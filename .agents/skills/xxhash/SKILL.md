---
name: xxhash
description: Extremely fast non-cryptographic hash functions (XXH32, XXH64, XXH3_64bits, XXH3_128bits). Use when: (1) hashing data for hash tables, checksums, or fingerprints, (2) generating high-speed 32/64/128-bit hashes, (3) incremental/streaming hashing.
---

# xxHash Usage Guide

xxHash is an extremely fast non-cryptographic hash algorithm, operating at RAM speed limits. Three families: **XXH32** (32-bit), **XXH64** (64-bit), and **XXH3** (modern 64/128-bit, ~2x faster).

## Integration

```c
// 1) Single-file header-only: inline all functions
#define XXH_INLINE_ALL
#include "xxhash.h"

// 2) Normal: compile xxhash.c separately, link xxhash.o
#include "xxhash.h"   // prototypes only
```

## Build Options

| Macro | Effect |
|-------|--------|
| `XXH_INLINE_ALL` | Inline all functions into the translation unit |
| `XXH_PRIVATE_API` | Include implementation without inline |
| `XXH_STATIC_LINKING_ONLY` | Expose internal state structs & experimental API |
| `XXH_IMPLEMENTATION` | Emit function definitions (used in `xxhash.c`) |
| `XXH_NAMESPACE=MyPrefix` | Prefix all public symbols to avoid collisions |

## Type & Hash Sizes

| Type | Description |
|------|-------------|
| `XXH32_hash_t` | uint32 — 32-bit hash value |
| `XXH64_hash_t` | uint64 — 64-bit hash value |
| `XXH128_hash_t` | struct `{ XXH64_hash_t low64; XXH64_hash_t high64; }` |
| `XXH_errorcode` | enum: `XXH_OK` (0) or `XXH_ERROR` |

---

## Single-Shot (One-Call) Hashing

```c
// XXH32 — 32-bit hash
XXH32_hash_t XXH32(const void* input, size_t length, XXH32_hash_t seed);

// XXH64 — 64-bit hash
XXH64_hash_t XXH64(const void* input, size_t length, XXH64_hash_t seed);

// XXH3 — 64-bit (default secret)
XXH64_hash_t XXH3_64bits(const void* data, size_t len);

// XXH3 — 64-bit (with seed, equivalent to XXH128() for 128-bit)
XXH64_hash_t XXH3_64bits_withSeed(const void* data, size_t len, XXH64_hash_t seed);

// XXH3 — 64-bit (with custom secret, secretSize >= XXH3_SECRET_SIZE_MIN=136)
XXH64_hash_t XXH3_64bits_withSecret(const void* data, size_t len, const void* secret, size_t secretSize);

// XXH3 — 64-bit (combines secret+seed for optimal speed on all key sizes)
XXH64_hash_t XXH3_64bits_withSecretandSeed(const void* data, size_t len, const void* secret, size_t secretSize, XXH64_hash_t seed);

// XXH3 — 128-bit variants (same patterns as 64-bit)
XXH128_hash_t XXH3_128bits(const void* data, size_t len);
XXH128_hash_t XXH3_128bits_withSeed(const void* data, size_t len, XXH64_hash_t seed);
XXH128_hash_t XXH3_128bits_withSecret(const void* data, size_t len, const void* secret, size_t secretSize);
XXH128_hash_t XXH3_128bits_withSecretandSeed(const void* data, size_t len, const void* secret, size_t secretSize, XXH64_hash_t seed);

// XXH128 is an alias for XXH3_128bits_withSeed()
XXH128_hash_t XXH128(const void* data, size_t len, XXH64_hash_t seed);

// Example
XXH32_hash_t h32 = XXH32("hello", 5, 0);
XXH64_hash_t h64 = XXH64("hello", 5, 0);
XXH64_hash_t h3  = XXH3_64bits_withSeed("hello", 5, 0);
```

## Streaming (Incremental) Hashing

Pattern: `createState` → `reset*` → `update`... → `digest` → `freeState`

### XXH32 / XXH64 Streaming

```c
XXH32_state_t* state = XXH32_createState();
XXH32_reset(state, seed);
XXH32_update(state, data, len);       // call multiple times
XXH32_hash_t hash = XXH32_digest(state);
XXH32_freeState(state);
// Same pattern: XXH64_createState, XXH64_reset, XXH64_update, XXH64_digest, XXH64_freeState
```

### XXH3 Streaming (64-bit & 128-bit share the same `XXH3_state_t`)

```c
XXH3_state_t* state = XXH3_createState();

// Default secret
XXH3_64bits_reset(state);
// With seed
XXH3_64bits_reset_withSeed(state, seed);
// With custom secret
XXH3_64bits_reset_withSecret(state, secret, secretSize);
// With secret + seed (best of both)
XXH3_64bits_reset_withSecretandSeed(state, secret, secretSize, seed);

XXH3_64bits_update(state, data, len);  // call repeatedly
XXH64_hash_t hash = XXH3_64bits_digest(state);
XXH3_freeState(state);

// 128-bit streaming: same state type, same pattern
XXH3_128bits_reset_withSeed(state, seed);
XXH3_128bits_update(state, data, len);
XXH128_hash_t h128 = XXH3_128bits_digest(state);
```

**Stack-allocated state** (requires `XXH_STATIC_LINKING_ONLY`):

```c
XXH3_state_t state;
XXH3_INITSTATE(&state);               // only needed before _withSeed reset
XXH3_64bits_reset_withSeed(&state, seed);
XXH3_64bits_update(&state, data, len);
XXH64_hash_t h = XXH3_64bits_digest(&state);
```

### State Copying

```c
XXH32_copyState(dst, src);
XXH64_copyState(dst, src);
XXH3_copyState(dst, src);
```

## Canonical (Big-Endian) Representation

For portable storage/network transmission:

```c
// Convert hash → canonical (big-endian bytes)
XXH32_canonical_t c32;
XXH32_canonicalFromHash(&c32, hash32);

XXH64_canonical_t c64;
XXH64_canonicalFromHash(&c64, hash64);

XXH128_canonical_t c128;
XXH128_canonicalFromHash(&c128, h128);

// Convert canonical → native hash
XXH32_hash_t h32 = XXH32_hashFromCanonical(&c32);
XXH64_hash_t h64 = XXH64_hashFromCanonical(&c64);
XXH128_hash_t h128 = XXH128_hashFromCanonical(&c128);
```

Canonical types are structs with `unsigned char digest[N]` field (big-endian).

## Custom Secret Generation

```c
// Generate high-entropy secret from arbitrary content
char secret[XXH3_SECRET_SIZE_MIN];    // >= 136 bytes
XXH3_generateSecret(secret, sizeof(secret), customSeed, customSeedLen);

// Generate secret equivalent to what _withSeed() uses internally
char secret[XXH3_SECRET_DEFAULT_SIZE]; // 192 bytes
XXH3_generateSecret_fromSeed(secret, seed);
```

## XXH128 Helpers

```c
int XXH128_isEqual(XXH128_hash_t h1, XXH128_hash_t h2);  // 1 if equal
int XXH128_cmp(const void* h1, const void* h2);           // comparator for qsort/bsearch
```

## Key Macros / Constants

| Macro | Value | Description |
|-------|-------|-------------|
| `XXH3_SECRET_SIZE_MIN` | 136 | Minimum secret size for `_withSecret` variants |
| `XXH3_SECRET_DEFAULT_SIZE` | 192 | Default secret size |
| `XXH3_INTERNALBUFFER_SIZE` | 256 | Optimal update chunk size for XXH3 streaming |
| `XXH3_MIDSIZE_MAX` | 240 | Threshold where `_withSecretandSeed` switches strategy |

## Performance Notes

- **XXH3** is ~2x faster than XXH64 on large inputs, >3x faster on small inputs.
- **Single-shot** is faster than streaming — use it when all data is available.
- **XXH3_64bits_withSecretandSeed()** auto-selects the fastest path: uses seed for short keys (<240B), secret for large keys (>=240B).
- For repeated hashing with the same seed, cache the secret via `XXH3_generateSecret_fromSeed()` and use `_withSecret` variants.
- Define `XXH_INLINE_ALL` for best small-input performance (compile-time constant propagation).
- Setting `XXH_FORCE_MEMORY_ACCESS=2` may improve speed on some compilers.

## Inclusion Safety

- xxhash.h can be included multiple times, with and without `XXH_STATIC_LINKING_ONLY` / `XXH_INLINE_ALL`, in any order.
- The newer `xxh3.h` is deprecated — always include `xxhash.h` directly.