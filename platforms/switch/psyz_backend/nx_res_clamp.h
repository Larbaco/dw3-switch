/* P6.11: the highest Internal resolution each console mode offers (1, 2 or 3).
 * The Options row can ask for 3x anywhere; the port clamps the request to this
 * table and logs it (`dw3-res: clamp 3x -> 2x (handheld max)`).
 *
 * Until the OLED measurement run (docs/P6.11_INTERNAL_RES.md, rows R0.1 and
 * R0.2) fills it, both modes are 2x. To fill it, run
 *
 *     python3 tools/res_probe_summary.py tmp/evidence/<date>-p611-oled/dw3*.log
 *
 * and copy the two `#define` lines it prints at the end over the ones below.
 * Nothing else changes: the numbers are read at run time, every frame, so a
 * dock change picks up the other mode's limit. */
#ifndef DW3_NX_RES_CLAMP_H
#define DW3_NX_RES_CLAMP_H

#define DW3_NX_RES_MAX_HANDHELD 2
#define DW3_NX_RES_MAX_DOCKED 2

#endif /* DW3_NX_RES_CLAMP_H */
