/* Separate TU so the linker rewrites harness external API calls exactly as
 * it rewrites pinned PSY-Z libcd.c/cdrom.c calls in the Switch link. */
int CdReady(int mode, unsigned char *result) { (void)mode; (void)result; return 1; }
void StSetRing(unsigned long *ring, unsigned long size) { (void)ring; (void)size; }
void StClearRing(void) {}
void data_ready_callback(void) {}
