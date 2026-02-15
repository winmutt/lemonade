#include "lemon/thread_manager.h"
#include "lemon/system_info.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <numeric>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#ifdef HAVE_LIBNUMA
#include <numa.h>
#endif
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

namespace lemon {

ThreadManager::ThreadManager() {}

int ThreadManager::get_available_threads() {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
#elif defined(__linux__)
    return sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(__APPLE__)
    int mib[2];
    size_t len = sizeof(int);
    int ncpus;
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    sysctl(mib, 2, &ncpus, &len, NULL, 0);
    return ncpus;
#else
    return 4;
#endif
}

bool ThreadManager::detect_is_amd(const std::string& cpu_name) {
    std::string cpu_lower = cpu_name;
    std::transform(cpu_lower.begin(), cpu_lower.end(), cpu_lower.begin(), ::tolower);
    return cpu_lower.find("amd") != std::string::npos ||
           cpu_lower.find("ryzen") != std::string::npos ||
           cpu_lower.find("threadripper") != std::string::npos;
}

int ThreadManager::detect_numa_nodes() {
#ifdef HAVE_LIBNUMA
    if (numa_available() >= 0) {
        return numa_max_node() + 1;
    }
#endif
    return 1;
}

int ThreadManager::detect_threads_per_core(int logical_threads, int physical_cores) {
    if (physical_cores == 0) return 1;
    return std::max(1, logical_threads / physical_cores);
}

int ThreadManager::detect_ccds(const CpuTopology& topo) {
    if (!topo.is_amd) return 1;

    if (topo.physical_cores <= 8) {
        return 1;
    } else if (topo.physical_cores <= 16) {
        return 2;
    } else if (topo.physical_cores <= 32) {
        return 2;
    } else {
        return 4;
    }
}

CpuTopology ThreadManager::detect_topology() {
    CpuTopology topo;

    auto system_info = create_system_info();
    auto cpu_info = system_info->get_cpu_device();

    topo.physical_cores = cpu_info.cores;
    topo.logical_threads = cpu_info.threads;
    topo.cpu_name = cpu_info.name;
    topo.threads_per_core = detect_threads_per_core(topo.logical_threads, topo.physical_cores);
    topo.has_hyperthreading = topo.threads_per_core > 1;
    topo.is_amd = detect_is_amd(topo.cpu_name);

    topo.num_numa_nodes = detect_numa_nodes();
    topo.num_ccds = detect_ccds(topo);

    if (topo.num_ccds > 0) {
        topo.cores_per_ccd = topo.physical_cores / topo.num_ccds;
    }

    return topo;
}

bool ThreadManager::validate_thread_count(int requested_threads, const CpuTopology& topo) {
    int available = topo.logical_threads;
    int max_allowed = available - 4;
    max_allowed = std::max(1, max_allowed);

    return requested_threads <= max_allowed;
}

AffinityMode ThreadManager::auto_detect_mode(const CpuTopology& topo) {
    if (topo.is_amd && (topo.num_ccds > 1 || topo.num_numa_nodes > 1)) {
        return AffinityMode::NUMA;
    }

    return AffinityMode::CACHE;
}

std::string ThreadManager::affinity_mode_to_string(AffinityMode mode) {
    switch (mode) {
        case AffinityMode::AUTO: return "auto";
        case AffinityMode::NUMA: return "numa";
        case AffinityMode::CACHE: return "cache";
        case AffinityMode::COMPACT: return "compact";
        default: return "auto";
    }
}

AffinityMode ThreadManager::string_to_affinity_mode(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "auto") return AffinityMode::AUTO;
    if (lower == "numa") return AffinityMode::NUMA;
    if (lower == "cache") return AffinityMode::CACHE;
    if (lower == "compact") return AffinityMode::COMPACT;

    return AffinityMode::AUTO;
}

ThreadAllocation ThreadManager::calculate_allocation(int thread_count,
                                                     const CpuTopology& topo,
                                                     AffinityMode mode) {
    ThreadAllocation allocation;
    allocation.thread_count = thread_count;
    allocation.mode = mode;

    if (mode == AffinityMode::AUTO) {
        mode = auto_detect_mode(topo);
        allocation.mode = mode;
    }

    switch (mode) {
        case AffinityMode::NUMA: {
            int threads_per_numa = thread_count / topo.num_numa_nodes;
            int remainder = thread_count % topo.num_numa_nodes;

            for (int numa = 0; numa < topo.num_numa_nodes; ++numa) {
                int count = threads_per_numa + (numa < remainder ? 1 : 0);
                for (int i = 0; i < count; ++i) {
                    allocation.numa_node_ids.push_back(numa);
                    int core_id = numa * topo.cores_per_ccd + (i % topo.cores_per_ccd);
                    allocation.core_ids.push_back(core_id);
                }
            }
            break;
        }

        case AffinityMode::CACHE: {
            int threads_per_ccd = thread_count / topo.num_ccds;
            int remainder = thread_count % topo.num_ccds;

            for (int ccd = 0; ccd < topo.num_ccds; ++ccd) {
                int count = threads_per_ccd + (ccd < remainder ? 1 : 0);
                for (int i = 0; i < count; ++i) {
                    allocation.ccd_ids.push_back(ccd);
                    int core_id = ccd * topo.cores_per_ccd + (i % topo.cores_per_ccd);
                    allocation.core_ids.push_back(core_id);
                }
            }
            break;
        }

        case AffinityMode::COMPACT: {
            for (int i = 0; i < thread_count; ++i) {
                int core_id = i / topo.threads_per_core;
                allocation.core_ids.push_back(core_id);
            }
            break;
        }
    }

    return allocation;
}

std::vector<std::string> ThreadManager::get_pinning_args(const ThreadAllocation& allocation,
                                                         const CpuTopology& topo) {
    std::vector<std::string> args;

    args.push_back("--parallel");
    args.push_back(std::to_string(allocation.thread_count));

    return args;
}

std::vector<int> ThreadManager::calculate_per_model_allocation(int total_models,
                                                               int available_threads,
                                                               const std::vector<int>& model_thread_counts) {
    std::vector<int> allocations;

    if (total_models == 0) return allocations;

    if (!model_thread_counts.empty() && model_thread_counts.size() == (size_t)total_models) {
        int total_requested = std::accumulate(model_thread_counts.begin(), model_thread_counts.end(), 0);
        if (total_requested <= available_threads) {
            return model_thread_counts;
        }
    }

    int base_threads = available_threads / total_models;
    int remainder = available_threads % total_models;

    for (int i = 0; i < total_models; ++i) {
        int threads = base_threads + (i < remainder ? 1 : 0);
        allocations.push_back(std::max(1, threads));
    }

    return allocations;
}

} // namespace lemon
