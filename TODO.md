# TODO - Issue #1070: Core Affinity/Thread Pinning Implementation

## Goal

Implement **core affinity/thread pinning** for Issue #1070 to prevent L3 cache bouncing when running multiple models on AMD Ryzen systems with multiple CCDs (Core Complex Dies). The goal is to pin llama-server threads to specific cores based on:
- Number of models loaded
- NUMA nodes
- CCD boundaries (for AMD CPUs)

Additionally, there's a separate issue: **llama-server returning error about multiple assistant messages** in chat completions.

---

## Instructions

1. **Per-model thread allocation** - each model gets its own thread assignment with dedicated cores
2. **Auto-detect thread count** - use 75% of available cores, leaving 4+ cores for system services
3. **Auto-detect affinity mode** - use NUMA for multi-die AMD, CACHE for single-die or simple topology
4. **Support uneven thread distribution** - different models can have different thread counts
5. **COMPACT mode with hyperthreading** - use hyperthreads when filling cores
6. **Validation** - error if requested threads > available cores

---

## Discoveries

### PR #1123 Analysis:
- Commit **19e89ed** added thread management infrastructure but was never integrated
- Commit **c0f2005** was about API endpoint logging (not relevant to #1070)
- Files added: `thread_manager.h`, `thread_manager.cpp` (stub implementation)

### Current State:
- `thread_manager.h` exists in PR but implementation is incomplete
- `llamacpp_server.cpp` doesn't use thread management when starting llama-server
- `RecipeOptions` doesn't include thread management options
- No per-model thread allocation tracking in place

### Assistant Message Error:
- Error: `"Cannot have 2 or more assistant messages at the end of the list"`
- This is a **separate issue** from thread management
- Related to chat request format validation

---

## Accomplished

✅ Analyzed PR #1123 commits to identify relevant code for #1070
✅ Analyzed issue #1070 requirements
✅ Read key files: `llamacpp_server.cpp`, `recipe_options.cpp`, `system_info.h/cpp`, `server_manager.cpp`
✅ Clarified requirements with user (per-model, auto-detection, uneven distribution, etc.)
✅ Identified the assistant message error is separate from thread management
✅ Created thread_manager.h with ThreadManager class definition
✅ Created thread_manager.cpp with implementation (topology detection, pinning strategies)
✅ Updated RecipeOptions to add thread management options (thread count, affinity mode)
✅ Integrated ThreadManager with LlamaCppServer for process spawning
✅ Verified ServerManager correctly passes thread configuration via RecipeOptions

---

## Tasks

### High Priority - COMPLETED

- [x] **1. Implement ThreadManager class** (`thread_manager.h/cpp`) with pinning logic
  - Detect CPU topology (NUMA nodes, CCDs)
  - Implement pinning strategies (NUMA, CACHE, COMPACT)
  - Support uneven thread distribution across models
  - Validate thread count against available cores

- [x] **2. Integrate ThreadManager with RecipeOptions** (add thread allocation options)
  - Add thread count options per model
  - Add affinity mode selection (NUMA/CACHE/COMPACT)
  - Add auto-detection flags

- [x] **3. Update LlamaCppServer to use ThreadManager** for process spawning
  - Pass thread configuration to backend
  - Integrate with thread pinning logic

### Medium Priority - COMPLETED

- [x] **4. Update ServerManager** to pass thread configuration to backends
  - Ensure thread args are propagated correctly via RecipeOptions::to_cli_options()
  - Handle multiple model scenarios

- [x] **5. Handle assistant message validation error** in `router.cpp`
  - Error is in llama.cpp backend (separate issue)
  - Not in Lemonade codebase

### Low Priority - COMPLETED

- [x] **6. Add thread_manager_test.cpp** for unit testing
  - Test affinity mode conversion
  - Test auto-detect mode
  - Test thread validation
  - Test per-model allocation

---

## Relevant Files / Directories

### Core Implementation Files:
- `src/cpp/include/lemon/thread_manager.h` - Thread management header (created)
- `src/cpp/server/thread_manager.cpp` - Thread management implementation (created)
- `src/cpp/include/lemon/recipe_options.h` - Recipe options header (modified)
- `src/cpp/server/recipe_options.cpp` - Recipe options implementation (modified)
- `src/cpp/include/lemon/system_info.h` - System topology detection
- `src/cpp/server/system_info.cpp` - System info implementation

### Backend Files:
- `src/cpp/server/backends/llamacpp_server.cpp` - Main file with thread integration (modified)
- `src/cpp/include/lemon/backends/llamacpp_server.h` - Header file

### Server Management:
- `src/cpp/tray/server_manager.cpp` - Process spawning (uses RecipeOptions for thread args)
- `src/cpp/include/lemon_tray/server_manager.h` - Server manager header

### Test Files:
- `test/thread_manager_test.cpp` - Test file (created)

### Documentation:
- `src/cpp/server/recipe_options.cpp` - CLI options (lines 27-89 show current structure)
- `src/cpp/server/router.cpp` - Chat completion routing (assistant message error location)

---

## Summary

The thread management implementation is now complete. The system:

1. **Auto-detects CPU topology** - NUMA nodes, CCDs, hyperthreading
2. **Supports multiple affinity modes** - AUTO (auto-selects NUMA/CACHE/COMPACT), NUMA, CACHE, COMPACT
3. **Auto-detects thread count** - Uses 75% of available cores (leaves 4 for system)
4. **Validates thread requests** - Ensures requested threads don't exceed available cores
5. **Per-model allocation** - Distributes threads across multiple models
6. **CLI options** - `--threads N` and `--affinity MODE` to control thread management
7. **Automatic integration** - Thread args are passed via `RecipeOptions::to_cli_options()` to backend

### New CLI Options:
- `--threads N` - Number of threads to use for inference (default: auto-detect)
- `--affinity MODE` - Thread affinity mode: auto, numa, cache, compact (default: auto)

### Usage Examples:
```bash
# Auto-detect thread count with optimal affinity mode
lemonade --threads auto --affinity auto

# Use 12 threads with NUMA mode
lemonade --threads 12 --affinity numa

# Use 8 threads with COMPACT mode
lemonade --threads 8 --affinity compact
```

### Environment Variables:
- `LEMONADE_THREADS` - Set thread count
- `LEMONADE_AFFINITY` - Set affinity mode
