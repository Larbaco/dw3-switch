/* The decomp's file cache (engine/file/file_cache.c) has FILE_CACHE_SLOTS (64)
 * slots. On the PS1 the 1.33 MB heap kept it small: allocMem evicts the least
 * recently used file whenever an allocation fails. The host heap is 8 MiB, so
 * nothing evicts and a long session fills all 64 slots, and the next
 * requestFile wrote through a NULL slot (the preview.31-rc1 battle report
 * crash). Policy and state in port/file_cache/file_cache_host.c; hooks in
 * patches 0095 (test hook) and 0096 (evict when the slots run out). Design:
 * docs/P5.1_MEMORY.md "File cache slots". */
#ifndef DW3_PORT_FILE_CACHE_H
#define DW3_PORT_FILE_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Test hook DW3_FILE_CACHE_SLOTS=<n> (1..FILE_CACHE_SLOTS): how many of the
 * cache slots findFreeFileSlot may hand out. Unset or invalid: all of them
 * (the value passed in), so a normal run is unchanged. */
int dw3_port_file_cache_slots(int total);

/* requestFile found every slot taken and the cache evicted its least recently
 * used loaded file (`evicted`) to take a slot for `file`. Logged (rate
 * limited) to dw3.log through psyz. */
void dw3_port_file_cache_evicted(int file, int evicted, int total);

/* requestFile found every slot taken and no loaded file to evict (all queued
 * or reading). Logs why and aborts, so the crash report names the cause. Does
 * not return. */
void dw3_port_file_cache_fatal(int file, int total);

#ifdef __cplusplus
}
#endif

#endif
