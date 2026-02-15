#include <lemon/thread_manager.h>
#include <iostream>
#include <cassert>

using namespace lemon;

void test_affinity_mode_conversion() {
    std::cout << "[TEST] Testing affinity mode conversion..." << std::endl;

    assert(ThreadManager::affinity_mode_to_string(AffinityMode::AUTO) == "auto");
    assert(ThreadManager::affinity_mode_to_string(AffinityMode::NUMA) == "numa");
    assert(ThreadManager::affinity_mode_to_string(AffinityMode::CACHE) == "cache");
    assert(ThreadManager::affinity_mode_to_string(AffinityMode::COMPACT) == "compact");

    assert(ThreadManager::string_to_affinity_mode("auto") == AffinityMode::AUTO);
    assert(ThreadManager::string_to_affinity_mode("numa") == AffinityMode::NUMA);
    assert(ThreadManager::string_to_affinity_mode("cache") == AffinityMode::CACHE);
    assert(ThreadManager::string_to_affinity_mode("compact") == AffinityMode::COMPACT);

    std::cout << "[PASS] Affinity mode conversion" << std::endl;
}

void test_auto_detect_mode() {
    std::cout << "[TEST] Testing auto-detect mode..." << std::endl;

    // Test with single CCD AMD
    CpuTopology topo1;
    topo1.physical_cores = 8;
    topo1.logical_threads = 16;
    topo1.num_numa_nodes = 1;
    topo1.num_ccds = 1;
    topo1.is_amd = true;

    AffinityMode mode1 = ThreadManager::auto_detect_mode(topo1);
    assert(mode1 == AffinityMode::CACHE);

    // Test with multiple CCD AMD
    CpuTopology topo2;
    topo2.physical_cores = 16;
    topo2.logical_threads = 32;
    topo2.num_numa_nodes = 2;
    topo2.num_ccds = 2;
    topo2.is_amd = true;

    AffinityMode mode2 = ThreadManager::auto_detect_mode(topo2);
    assert(mode2 == AffinityMode::NUMA);

    // Test with non-AMD
    CpuTopology topo3;
    topo3.physical_cores = 8;
    topo3.logical_threads = 16;
    topo3.num_numa_nodes = 1;
    topo3.num_ccds = 1;
    topo3.is_amd = false;

    AffinityMode mode3 = ThreadManager::auto_detect_mode(topo3);
    assert(mode3 == AffinityMode::CACHE);

    std::cout << "[PASS] Auto-detect mode" << std::endl;
}

void test_thread_validation() {
    std::cout << "[TEST] Testing thread validation..." << std::endl;

    ThreadManager manager;
    CpuTopology topo;
    topo.physical_cores = 8;
    topo.logical_threads = 16;

    // Valid thread count
    assert(manager.validate_thread_count(12, topo));

    // Invalid thread count (too high)
    assert(!manager.validate_thread_count(20, topo));

    std::cout << "[PASS] Thread validation" << std::endl;
}

void test_per_model_allocation() {
    std::cout << "[TEST] Testing per-model allocation..." << std::endl;

    int available_threads = 16;
    int num_models = 2;

    std::vector<int> allocation = ThreadManager::calculate_per_model_allocation(num_models, available_threads);

    assert(allocation.size() == 2);
    assert(allocation[0] + allocation[1] == available_threads);
    assert(allocation[0] >= 1 && allocation[1] >= 1);

    std::cout << "  Allocation for 2 models: [" << allocation[0] << ", " << allocation[1] << "]" << std::endl;

    // Test with explicit thread counts
    std::vector<int> explicit_counts = {10, 6};
    std::vector<int> allocation2 = ThreadManager::calculate_per_model_allocation(num_models, available_threads, explicit_counts);

    assert(allocation2.size() == 2);
    assert(allocation2[0] == 10 && allocation2[1] == 6);

    std::cout << "  Explicit allocation: [" << allocation2[0] << ", " << allocation2[1] << "]" << std::endl;

    std::cout << "[PASS] Per-model allocation" << std::endl;
}

int main() {
    std::cout << "=== ThreadManager Tests ===" << std::endl;
    std::cout << std::endl;

    try {
        test_affinity_mode_conversion();
        std::cout << std::endl;

        test_auto_detect_mode();
        std::cout << std::endl;

        test_thread_validation();
        std::cout << std::endl;

        test_per_model_allocation();
        std::cout << std::endl;

        std::cout << "=== All tests passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Test failed: " << e.what() << std::endl;
        return 1;
    }
}
