#ifndef DW3_BOOT_TIM_HOST_H
#define DW3_BOOT_TIM_HOST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Load a TIM file into STAGE_VRAM. Returns 0 on success, -1 on error. */
int dw3_host_load_boot_tim(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* DW3_BOOT_TIM_HOST_H */
