/*
 * Host heap storage (P2.8). PS1 uses a fixed VRAM map ending at 0x801FF000;
 * on host we own a large BSS region and point HEAP_BASE / HEAP_END at it.
 */
#ifndef PORT_HOST
#error "port/overlays/host_heap.c is host-only"
#endif

enum { DW3_HOST_HEAP_BYTES = 8 * 1024 * 1024 };

unsigned char DW3_HOST_HEAP[DW3_HOST_HEAP_BYTES];
const unsigned long DW3_HOST_HEAP_SIZE = DW3_HOST_HEAP_BYTES;
