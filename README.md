# Custom Memory Allocator in C

A from-scratch heap allocator implementing `malloc`, `free`, `realloc`, and `calloc` using `sbrk`/`brk` system calls. Built as a learning project to understand how dynamic memory management works under the hood.

---

## How It Works

The allocator manages a contiguous region of memory obtained from the OS via `sbrk`. Each allocation is preceded by a metadata block that tracks the size, free status, and neighboring blocks in a doubly-linked list.

```
Heap layout:

[ Block Header | user memory ] → [ Block Header | user memory ] → NULL
  (metadata)                       (metadata)
```

**Block header fields:**
- `is_free` — whether this block is available for reuse
- `block_size` — size of the usable memory region (excluding header)
- `next_block` / `prev_block` — doubly-linked list pointers
- `memory_ptr` — pointer to the start of usable memory
- `memory[1]` — flexible array marking the start of user data

---

## Features

| Feature | Strategy |
|---|---|
| Allocation | First-fit search through the free list |
| Free block reuse | Block splitting when a free block is larger than needed |
| Memory reclamation | Adjacent free blocks are coalesced on `free()` |
| Heap shrinking | Trailing free blocks are returned to the OS via `brk()` |
| Overflow protection | `calloc` checks for `size_t` overflow before multiplying |

---

## Files

```
allocator.c    — Core allocator: malloc, free, realloc, calloc
benchmark.c    — Performance comparison against glibc + edge case tests
allocator      — Compiled binary
```

---

## Building & Running

```bash
# Compile
gcc -O2 -o allocator allocator.c benchmark.c

# Run the benchmark
./allocator
```

Example output:

```
── Custom Allocator Benchmark (10000 ops, pool=256, max_alloc=512B) ──

Performance:
  custom (first-fit)   142.30 ms   10000 ops   frag: 3.12%
  glibc malloc          18.74 ms   10000 ops   frag: n/a (glibc)

Summary:
  Throughput — custom: 70 ops/ms   glibc: 533 ops/ms
  glibc is 7.6x faster

  malloc(0)              → non-NULL (ok)
  free(NULL)             → no crash ✓
  realloc(NULL, 64)      → allocated ✓
  calloc(SIZE_MAX, 2)    → NULL ✓
  write/read 100 ints    → correct ✓
```

---

## Known Limitations

- **First-fit only** — no size-class bins; allocation is O(n) in heap size
- **4-byte alignment** — not safe for `double` or pointer-sized types on 64-bit systems (8-byte alignment required)
- **Not thread-safe** — no mutex around the free list or `sbrk` calls
- **`sbrk`-based** — `sbrk` is deprecated on some modern systems (e.g., macOS); Linux is fine
- **No guard pages** — out-of-bounds writes are not detected

---

## Internals

### Allocation (`custom_malloc`)

1. Align the requested size to a 4-byte boundary
2. If the heap is empty, call `grow_heap` to extend via `sbrk`
3. Otherwise, walk the free list looking for a block large enough (`find_free_block`)
4. If found and large enough to split (leaving at least `METADATA_SIZE + 4` bytes), call `split_block`
5. If no free block fits, extend the heap

### Free (`custom_free`)

1. Validate the pointer is within the heap and matches its own `memory_ptr` field
2. Mark the block as free
3. Coalesce with the previous block if it is also free
4. Coalesce with the next block if it exists and is free
5. If this block is now the last in the heap, shrink the heap with `brk()`

### Realloc (`custom_realloc`)

- If the current block is already large enough, optionally split and return as-is
- If the next block is free and the combined size suffices, merge in-place
- Otherwise, `malloc` a new block, copy the data, and `free` the old block

---

## References

- [Malloc tutorial — Marwan Burelle](http://www.inf.udec.cl/~leo/Malloc_tutorial.pdf)
- `man 2 sbrk` / `man 2 brk`
- glibc ptmalloc source — `malloc/malloc.c`
