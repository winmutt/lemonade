#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace lemon {

using json = nlohmann::json;

enum class AffinityMode {
    AUTO,      // Auto-detect best mode (NUMA for multi-die AMD, CACHE for simple topology)
    NUMA,      // Pin threads to NUMA nodes
    CACHE,     // Pin threads to cache domains (CCDs on AMD)
    COMPACT    // Fill cores compactly (use hyperthreads when filling)
};

struct CpuTopology {
    int physical_cores = 0;
    int logical_threads = 0;
    int num_numa_nodes = 0;
    int num_ccds = 0;
    int cores_per_ccd = 0;
    int threads_per_core = 0;
    bool has_hyperthreading = false;
    bool is_amd = false;
    std::string cpu_name;
};

struct ThreadAllocation {
    int thread_count = 0;
    AffinityMode mode = AffinityMode::AUTO;
    std::vector<int> core_ids;
    std::vector<int> numa_node_ids;
    std::vector<int> ccd_ids;
};

class ThreadManager {
public:
    ThreadManager();
    ~ThreadManager() = default;

    // Detect CPU topology
    CpuTopology detect_topology();

    // Validate thread count against available cores
    bool validate_thread_count(int requested_threads, const CpuTopology& topo);

    // Calculate thread allocation based on mode
    ThreadAllocation calculate_allocation(int thread_count,
                                         const CpuTopology& topo,
                                         AffinityMode mode);

    // Get CLI arguments for thread pinning
    std::vector<std::string> get_pinning_args(const ThreadAllocation& allocation,
                                             const CpuTopology& topo);

    // Convert AffinityMode to string
    static std::string affinity_mode_to_string(AffinityMode mode);

    // Convert string to AffinityMode
    static AffinityMode string_to_affinity_mode(const std::string& str);

    // Auto-detect affinity mode based on topology
    static AffinityMode auto_detect_mode(const CpuTopology& topo);

    // Get system available threads
    static int get_available_threads();

    // Calculate per-model allocation
    static std::vector<int> calculate_per_model_allocation(int total_models,
                                                          int available_threads,
                                                          const std::vector<int>& model_thread_counts = {});

private:
    // Detect if CPU is AMD
    bool detect_is_amd(const std::string& cpu_name);

    // Detect NUMA nodes
    int detect_numa_nodes();

    // Detect CCDs (Core Complex Dies) for AMD
    int detect_ccds(const CpuTopology& topo);

    // Get logical threads per physical core
    int detect_threads_per_core(int logical_threads, int physical_cores);
};

} // namespace lemon
