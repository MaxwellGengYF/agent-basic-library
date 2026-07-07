---
name: mimalloc
description: Compact, fast general-purpose allocator from Microsoft. Use when: (1) replacing malloc/free for performance, (2) using isolated heaps/theaps for per-context allocation, (3) fine-grained control over aligned/zeroed/realloc family, (4) C++ STL allocator integration.
---

# mimalloc Usage Guide

mimalloc is a compact general-purpose allocator (~15k LOC) with excellent performance. It provides a full malloc-like API, extended allocation with alignment/zeroing, independent heaps, and thread-local theaps.

## Integration

```c
#include "mimalloc.h"           // main API
#include "mimalloc-stats.h"     // statistics API (optional)
```

**Ways to use:**
1. **Link against mimalloc** — replaces `malloc`/`free` globally at link time
2. **Include `mimalloc-override.h`** — statically redirects `malloc`, `free`, etc. to `mi_` variants via macros
3. **Include `mimalloc-new-delete.h`** (one source file only) — overrides C++ `operator new`/`delete`
4. **Use `mi_malloc`/`mi_free`** directly — explicit API

## Standard Allocation (malloc-compatible)

```c
void* mi_malloc(size_t size);
void* mi_calloc(size_t count, size_t size);    // zero-initialized
void* mi_realloc(void* p, size_t newsize);
void* mi_expand(void* p, size_t newsize);      // expand in-place if possible
void  mi_free(void* p);

char* mi_strdup(const char* s);
char* mi_strndup(const char* s, size_t n);
char* mi_realpath(const char* fname, char* resolved_name);

size_t mi_usable_size(const void* p);           // actual usable size of allocation
size_t mi_good_size(size_t size);               // rounded-up actual allocated size
```

## Extended Allocation

```c
// Zero-initialized (same as calloc but single-arg)
void* mi_zalloc(size_t size);

// Small object fast path (size <= MI_SMALL_SIZE_MAX = 128*sizeof(void*))
void* mi_malloc_small(size_t size);
void* mi_zalloc_small(size_t size);

// Array allocation with overflow check
void* mi_mallocn(size_t count, size_t size);
void* mi_reallocn(void* p, size_t count, size_t size);
void* mi_reallocf(void* p, size_t newsize);    // free on failure (like FreeBSD)

// Zero-initialized reallocation (only valid for memory from mi_calloc/mi_zalloc)
void* mi_rezalloc(void* p, size_t newsize);
void* mi_recalloc(void* p, size_t newcount, size_t size);

// Fast free for small allocations (from mi_(heap_)(m|z)alloc_small)
void mi_free_small(void* p);
```

## Aligned Allocation

**Note:** `alignment` always follows `size` (differs from `posix_memalign`).

```c
void* mi_malloc_aligned(size_t size, size_t alignment);
void* mi_malloc_aligned_at(size_t size, size_t alignment, size_t offset);
void* mi_zalloc_aligned(size_t size, size_t alignment);      // zero-init
void* mi_zalloc_aligned_at(size_t size, size_t alignment, size_t offset);
void* mi_calloc_aligned(size_t count, size_t size, size_t alignment);
void* mi_calloc_aligned_at(size_t count, size_t size, size_t alignment, size_t offset);
void* mi_realloc_aligned(void* p, size_t newsize, size_t alignment);
void* mi_realloc_aligned_at(void* p, size_t newsize, size_t alignment, size_t offset);

// Zero-init realloc for aligned memory
void* mi_rezalloc_aligned(void* p, size_t newsize, size_t alignment);
void* mi_recalloc_aligned(void* p, size_t newcount, size_t size, size_t alignment);
void* mi_rezalloc_aligned_at(void* p, size_t newsize, size_t alignment, size_t offset);
void* mi_recalloc_aligned_at(void* p, size_t newcount, size_t size, size_t alignment, size_t offset);
```

## Typed Allocation Macros

```c
#define mi_malloc_tp(tp)           ((tp*)mi_malloc(sizeof(tp)))
#define mi_zalloc_tp(tp)           ((tp*)mi_zalloc(sizeof(tp)))
#define mi_calloc_tp(tp,n)         ((tp*)mi_calloc(n,sizeof(tp)))
#define mi_mallocn_tp(tp,n)        ((tp*)mi_mallocn(n,sizeof(tp)))
#define mi_reallocn_tp(tp,p,n)     ((tp*)mi_reallocn(p,n,sizeof(tp)))
#define mi_recalloc_tp(tp,p,n)     ((tp*)mi_recalloc(p,n,sizeof(tp)))
// Heap variants (see below)
#define mi_heap_malloc_tp(tp,hp)   ((tp*)mi_heap_malloc(hp,sizeof(tp)))
// ... etc.
```

## Returned Block Size API

Allocations that also return the actual block size:

```c
void* mi_umalloc(size_t size, size_t* block_size);
void* mi_ucalloc(size_t count, size_t size, size_t* block_size);
void* mi_urealloc(void* p, size_t newsize, size_t* block_size_pre, size_t* block_size_post);
void  mi_ufree(void* p, size_t* block_size);

void* mi_umalloc_aligned(size_t size, size_t alignment, size_t* block_size);
void* mi_uzalloc_aligned(size_t size, size_t alignment, size_t* block_size);
void* mi_umalloc_small(size_t size, size_t* block_size);
void* mi_uzalloc_small(size_t size, size_t* block_size);
```

## Heaps (First-Class, Thread-Safe)

Heaps allow isolated allocation contexts. Blocks allocated from a heap can be freed from any thread.

```c
mi_heap_t* mi_heap_new(void);
void  mi_heap_delete(mi_heap_t* heap);   // move live blocks to main heap
void  mi_heap_destroy(mi_heap_t* heap);  // free all live blocks
void  mi_heap_collect(mi_heap_t* heap, bool force);

mi_heap_t* mi_heap_main(void);
mi_heap_t* mi_heap_of(const void* p);       // heap containing p
bool   mi_heap_contains(const mi_heap_t* heap, const void* p);
bool   mi_any_heap_contains(const void* p);

// Heap allocation (same families as global)
void* mi_heap_malloc(mi_heap_t* heap, size_t size);
void* mi_heap_zalloc(mi_heap_t* heap, size_t size);
void* mi_heap_calloc(mi_heap_t* heap, size_t count, size_t size);
void* mi_heap_mallocn(mi_heap_t* heap, size_t count, size_t size);
void* mi_heap_malloc_small(mi_heap_t* heap, size_t size);
void* mi_heap_zalloc_small(mi_heap_t* heap, size_t size);
void* mi_heap_realloc(mi_heap_t* heap, void* p, size_t newsize);
void* mi_heap_reallocn(mi_heap_t* heap, void* p, size_t count, size_t size);
void* mi_heap_reallocf(mi_heap_t* heap, void* p, size_t newsize);
char* mi_heap_strdup(mi_heap_t* heap, const char* s);
char* mi_heap_strndup(mi_heap_t* heap, const char* s, size_t n);
char* mi_heap_realpath(mi_heap_t* heap, const char* fname, char* resolved_name);

// Heap aligned allocation
void* mi_heap_malloc_aligned(mi_heap_t* heap, size_t size, size_t alignment);
void* mi_heap_malloc_aligned_at(mi_heap_t* heap, size_t size, size_t alignment, size_t offset);
void* mi_heap_zalloc_aligned(mi_heap_t* heap, size_t size, size_t alignment);
void* mi_heap_zalloc_aligned_at(mi_heap_t* heap, size_t size, size_t alignment, size_t offset);
void* mi_heap_calloc_aligned(mi_heap_t* heap, size_t count, size_t size, size_t alignment);
void* mi_heap_realloc_aligned(mi_heap_t* heap, void* p, size_t newsize, size_t alignment);

// Heap zero-init realloc
void* mi_heap_rezalloc(mi_heap_t* heap, void* p, size_t newsize);
void* mi_heap_recalloc(mi_heap_t* heap, void* p, size_t newcount, size_t size);
// ... and aligned variants

// Example
mi_heap_t* heap = mi_heap_new();
int* arr = mi_heap_malloc_tp(int, heap);
arr[0] = 42;
mi_free(arr);                         // free from any thread
mi_heap_delete(heap);
```

## Theaps (Thread-Local, Fastest Path)

Theaps are thread-local heaps. Allocation through a `theap` is slightly faster than plain `mi_malloc` (skips thread-local lookup). A theap can only allocate from the thread that created it.

```c
mi_theap_t* mi_heap_theap(mi_heap_t* heap);      // get theap from a heap
mi_theap_t* mi_theap_set_default(mi_theap_t* theap);  // set as default for this thread
mi_theap_t* mi_theap_get_default(void);
void  mi_theap_collect(mi_theap_t* theap, bool force);

// Allocation through a theap
void* mi_theap_malloc(mi_theap_t* theap, size_t size);
void* mi_theap_zalloc(mi_theap_t* theap, size_t size);
void* mi_theap_calloc(mi_theap_t* theap, size_t count, size_t size);
void* mi_theap_malloc_small(mi_theap_t* theap, size_t size);
void* mi_theap_zalloc_small(mi_theap_t* theap, size_t size);
void* mi_theap_malloc_aligned(mi_theap_t* theap, size_t size, size_t alignment);
void* mi_theap_realloc(mi_theap_t* theap, void* p, size_t newsize);

// Guarded objects (experimental)
void mi_theap_guarded_set_sample_rate(mi_theap_t* theap, size_t sample_rate, size_t seed);
void mi_theap_guarded_set_size_bound(mi_theap_t* theap, size_t min, size_t max);
```

## Visiting Heap Blocks

```c
typedef struct mi_heap_area_s {
  void*  blocks;
  size_t reserved, committed, used;
  size_t block_size, full_block_size;
} mi_heap_area_t;

typedef bool (*mi_block_visit_fun)(const mi_heap_t* heap, const mi_heap_area_t* area, void* block, size_t block_size, void* arg);

bool mi_heap_visit_blocks(mi_heap_t* heap, bool visit_blocks, mi_block_visit_fun* visitor, void* arg);
bool mi_heap_visit_abandoned_blocks(mi_heap_t* heap, bool visit_blocks, mi_block_visit_fun* visitor, void* arg);
```

## POSIX / Windows Compatibility API

```c
int  mi_posix_memalign(void** p, size_t alignment, size_t size);
void* mi_memalign(size_t alignment, size_t size);
void* mi_valloc(size_t size);
void* mi_pvalloc(size_t size);
void* mi_aligned_alloc(size_t alignment, size_t size);
void* mi_reallocarray(void* p, size_t count, size_t size);
int   mi_reallocarr(void* p, size_t count, size_t size);
void* mi_aligned_recalloc(void* p, size_t newcount, size_t size, size_t alignment);
void* mi_aligned_offset_recalloc(void* p, size_t newcount, size_t size, size_t alignment, size_t offset);

unsigned short* mi_wcsdup(const unsigned short* s);
unsigned char*  mi_mbsdup(const unsigned char* s);
int   mi_dupenv_s(char** buf, size_t* size, const char* name);
int   mi_wdupenv_s(unsigned short** buf, size_t* size, const unsigned short* name);

// "Checked free" — checks if pointer is in our theap
void mi_cfree(void* p);

// Sized/aligned free (for allocators that track sizes)
void mi_free_size(void* p, size_t size);
void mi_free_size_aligned(void* p, size_t size, size_t alignment);
void mi_free_aligned(void* p, size_t alignment);
```

## C++ `new`/`delete` Override

Include `mimalloc-new-delete.h` in **exactly one** source file:

```cpp
#include "mimalloc-new-delete.h"  // overrides operator new/delete globally
```

This provides:
- `operator new(size_t)` → `mi_new(n)`
- `operator new[](size_t)` → `mi_new(n)`
- `operator delete(void*)` → `mi_free(p)`
- Aligned new/delete for C++17 `std::align_val_t`
- Sized delete for C++14

## C++ STL Allocator

```cpp
// Global allocator (uses mi_malloc/mi_free)
std::vector<int, mi_stl_allocator<int>> vec;
vec.push_back(1);

// Heap-specific allocator (C++11+)
mi_heap_t* heap = mi_heap_new();
std::vector<int, mi_heap_stl_allocator<int>> heap_vec( mi_heap_stl_allocator<int>(heap) );
heap_vec.push_back(42);
// heap is deleted when the allocator's shared_ptr refcount drops to zero

// Destroy allocator — free() does nothing, heap is destroyed in one go
std::vector<int, mi_heap_destroy_stl_allocator<int>> destroy_vec(
    mi_heap_destroy_stl_allocator<int>(heap));
```

## Options

```c
typedef enum mi_option_e {
  mi_option_show_errors,               // print error messages
  mi_option_show_stats,                // print statistics on termination
  mi_option_verbose,                   // print verbose messages
  mi_option_arena_eager_commit,        // eager commit arenas? Use 2 for just on overcommit
  mi_option_purge_decommits,           // purge decommits? (1) or just memory reset (0)
  mi_option_allow_large_os_pages,      // allow large (2/4 MiB) OS pages
  mi_option_reserve_huge_os_pages,     // reserve N huge (1 GiB) OS pages at startup
  mi_option_reserve_huge_os_pages_at,  // reserve huge OS pages at specific NUMA node
  mi_option_reserve_os_memory,         // reserve specified KiB of OS memory at startup
  mi_option_purge_delay,               // purge delay in ms (10), 0=immediate, -1=no purging
  mi_option_use_numa_nodes,            // 0=all available, otherwise at most N nodes
  mi_option_disallow_os_alloc,         // 1=no OS memory allocation (only reserved arenas)
  mi_option_os_tag,                    // OS logging tag (macOS)
  mi_option_max_errors,                // max error messages
  mi_option_max_warnings,              // max warning messages
  mi_option_destroy_on_exit,           // release all memory on exit
  mi_option_arena_reserve,             // initial arena reservation size (KiB)
  mi_option_arena_purge_mult,          // multiplier for purge_delay for arenas (=10)
  mi_option_disallow_arena_alloc,      // 1=don't use arena's for allocation
  mi_option_retry_on_oom,              // retry OOM for N ms (=400) (Windows only)
  mi_option_allow_thp,                 // allow transparent huge pages? (=1)
  mi_option_guarded_sample_rate,       // 1 out of N allocations in range will be guarded
  // ... and more
} mi_option_t;

bool mi_option_is_enabled(mi_option_t option);
void mi_option_enable(mi_option_t option);
void mi_option_disable(mi_option_t option);
void mi_option_set_enabled(mi_option_t option, bool enable);
void mi_option_set_enabled_default(mi_option_t option, bool enable);

long   mi_option_get(mi_option_t option);
long   mi_option_get_clamp(mi_option_t option, long min, long max);
size_t mi_option_get_size(mi_option_t option);    // returns value in bytes
void   mi_option_set(mi_option_t option, long value);
void   mi_option_set_default(mi_option_t option, long value);

// Example: enable verbose stats output
mi_option_enable(mi_option_show_stats);
mi_option_enable(mi_option_verbose);
```

## Arena Memory Management

```c
// Reserve OS memory upfront
int  mi_reserve_huge_os_pages_interleave(size_t pages, size_t numa_nodes, size_t timeout_ms);
int  mi_reserve_huge_os_pages_at(size_t pages, int numa_node, size_t timeout_ms);
int  mi_reserve_os_memory(size_t size, bool commit, bool allow_large);
bool mi_manage_os_memory(void* start, size_t size, bool is_committed, bool is_pinned, bool is_zero, int numa_node);

// Arena IDs for exclusive usage
int  mi_reserve_huge_os_pages_at_ex(size_t pages, int numa_node, size_t timeout_ms, bool exclusive, mi_arena_id_t* arena_id);
int  mi_reserve_os_memory_ex(size_t size, bool commit, bool allow_large, bool exclusive, mi_arena_id_t* arena_id);
bool mi_manage_os_memory_ex(void* start, size_t size, bool is_committed, bool is_pinned, bool is_zero, int numa_node, bool exclusive, mi_arena_id_t* arena_id);
bool mi_arena_contains(mi_arena_id_t arena_id, const void* p);

// Create a heap that only allocates in a specific arena
mi_heap_t* mi_heap_new_in_arena(mi_arena_id_t arena_id);

size_t mi_arena_min_alignment(void);
size_t mi_arena_min_size(void);
void*  mi_arena_area(mi_arena_id_t arena_id, size_t* size);
void   mi_arenas_print(void);
```

## Subprocesses

Separate memory arenas for sub-processes (e.g., separate interpreters in one process):

```c
mi_subproc_id_t mi_subproc_main(void);
mi_subproc_id_t mi_subproc_current(void);
mi_subproc_id_t mi_subproc_new(void);
void mi_subproc_destroy(mi_subproc_id_t subproc);
void mi_subproc_add_current_thread(mi_subproc_id_t subproc);

bool mi_subproc_visit_heaps(mi_subproc_id_t subproc, mi_heap_visit_fun* visitor, void* arg);
```

## Statistics

```c
// Print
void mi_stats_print_out(mi_output_fun* out, void* arg);
void mi_process_info_print(void);
void mi_options_print(void);

// JSON output
char* mi_stats_get_json(size_t buf_size, char* buf);   // use mi_free if buf==NULL
char* mi_heap_stats_get_json(mi_heap_t* heap, size_t buf_size, char* buf);

// Structured
bool mi_stats_get(mi_stats_t* stats);
bool mi_heap_stats_get(mi_heap_t* heap, mi_stats_t* stats);
bool mi_subproc_stats_get(mi_subproc_id_t subproc, mi_stats_t* stats);

// Register callbacks
void mi_register_output(mi_output_fun* out, void* arg);
void mi_register_error(mi_error_fun* fun, void* arg);
void mi_register_deferred_free(mi_deferred_free_fun* deferred_free, void* arg);
```

## Utility Functions

```c
void mi_collect(bool force);           // collect free pages
int  mi_version(void);                 // version number (e.g., 30300)
void mi_process_init(void);            // manual init
void mi_process_done(void);            // manual cleanup
void mi_thread_init(void);             // per-thread init
void mi_thread_done(void);             // per-thread cleanup
void mi_thread_set_in_threadpool(void); // mark thread as threadpool thread
```

## CMake Build

```cmake
add_subdirectory(path/to/mimalloc)
target_link_libraries(myapp PRIVATE mimalloc)
# Or static library:
target_link_libraries(myapp PRIVATE mimalloc-static)
```

## Key Macros for Build Configuration

| Macro | Effect |
|-------|--------|
| `MI_SECURE=1` | Guard pages around metadata, check pointer validity on free |
| `MI_SECURE=3` | + randomize allocations, encode free lists |
| `MI_SECURE=4` | + double-free detection |
| `MI_DEBUG=1` | Basic assertions, double-free check, corrupted free list detect |
| `MI_DEBUG=2` | + internal invariant checks |
| `MI_DEBUG=3` | + extensive internal invariant checking |
| `MI_STAT=1` | Maintain statistics |
| `MI_STAT=2` | Detailed statistics (more expensive) |
| `MI_MAX_ALIGN_SIZE` | Minimal alignment (default 16) |
| `MI_TRACK_VALGRIND` | Enable Valgrind tracking |
| `MI_TRACK_ASAN` | Enable AddressSanitizer tracking |
| `MI_PADDING` | Padding at end of blocks for buffer overflow detection |
| `MI_ENCODE_FREELIST` | Encoded free lists (with MI_SECURE>=3 or MI_DEBUG>=1) |

## Performance Notes

- **mi_malloc** is designed for general-purpose workloads; use **heaps** for isolated allocation contexts
- **Theaps** are the fastest path (avoid thread-local lookup), use when you control the thread context
- `mi_malloc_small` / `mi_zalloc_small` optimize for sizes ≤ `MI_SMALL_SIZE_MAX` (128×sizeof(void*))
- `mi_free_small` is slightly faster than `mi_free` for small allocations
- `mi_expand` expands in-place if possible (no data copy)
- `mi_option_show_stats` prints allocation statistics at process exit
- `mi_collect(true)` aggressively reclaims free pages back to the OS
- For custom memory management, use `mi_reserve_os_memory` + `mi_manage_os_memory` to provide pre-allocated memory pools