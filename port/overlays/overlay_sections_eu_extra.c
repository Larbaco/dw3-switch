/*
 * EU extra chapter (FIELDSTG_extraStages, GAME.progress 0x2D): the named
 * .data/.bss sections of WSTAG920-974, bound to their descriptors at register
 * time like overlay_sections.c does for the other stages. Compiled only into
 * the EU game (the US game has no such stages). Same rule as there: the
 * stage's initial .data is restored and its .bss zeroed when its file id
 * changes.
 */
#ifndef PORT_HOST
#error "port/overlays/overlay_sections_eu_extra.c is host-only"
#endif

#include "overlay_host.h"

/* overlay_sections.c: bind_section, for a file the US game does not compile */
void dw3_overlay_bind_section(Dw3OverlayDesc *desc, char *data, char *data_end, char *bss,
                              char *bss_end);

extern char __start_dw3data_wstag920[];
extern char __stop_dw3data_wstag920[];
extern char __start_dw3bss_wstag920[];
extern char __stop_dw3bss_wstag920[];
extern char __start_dw3data_wstag921[];
extern char __stop_dw3data_wstag921[];
extern char __start_dw3bss_wstag921[];
extern char __stop_dw3bss_wstag921[];
extern char __start_dw3data_wstag922[];
extern char __stop_dw3data_wstag922[];
extern char __start_dw3bss_wstag922[];
extern char __stop_dw3bss_wstag922[];
extern char __start_dw3data_wstag923[];
extern char __stop_dw3data_wstag923[];
extern char __start_dw3bss_wstag923[];
extern char __stop_dw3bss_wstag923[];
extern char __start_dw3data_wstag924[];
extern char __stop_dw3data_wstag924[];
extern char __start_dw3bss_wstag924[];
extern char __stop_dw3bss_wstag924[];
extern char __start_dw3data_wstag925[];
extern char __stop_dw3data_wstag925[];
extern char __start_dw3bss_wstag925[];
extern char __stop_dw3bss_wstag925[];
extern char __start_dw3data_wstag926[];
extern char __stop_dw3data_wstag926[];
extern char __start_dw3bss_wstag926[];
extern char __stop_dw3bss_wstag926[];
extern char __start_dw3data_wstag927[];
extern char __stop_dw3data_wstag927[];
extern char __start_dw3bss_wstag927[];
extern char __stop_dw3bss_wstag927[];
extern char __start_dw3data_wstag928[];
extern char __stop_dw3data_wstag928[];
extern char __start_dw3bss_wstag928[];
extern char __stop_dw3bss_wstag928[];
extern char __start_dw3data_wstag929[];
extern char __stop_dw3data_wstag929[];
extern char __start_dw3bss_wstag929[];
extern char __stop_dw3bss_wstag929[];
extern char __start_dw3data_wstag930[];
extern char __stop_dw3data_wstag930[];
extern char __start_dw3bss_wstag930[];
extern char __stop_dw3bss_wstag930[];
extern char __start_dw3data_wstag931[];
extern char __stop_dw3data_wstag931[];
extern char __start_dw3bss_wstag931[];
extern char __stop_dw3bss_wstag931[];
extern char __start_dw3data_wstag932[];
extern char __stop_dw3data_wstag932[];
extern char __start_dw3bss_wstag932[];
extern char __stop_dw3bss_wstag932[];
extern char __start_dw3data_wstag933[];
extern char __stop_dw3data_wstag933[];
extern char __start_dw3bss_wstag933[];
extern char __stop_dw3bss_wstag933[];
extern char __start_dw3data_wstag934[];
extern char __stop_dw3data_wstag934[];
extern char __start_dw3bss_wstag934[];
extern char __stop_dw3bss_wstag934[];
extern char __start_dw3data_wstag935[];
extern char __stop_dw3data_wstag935[];
extern char __start_dw3bss_wstag935[];
extern char __stop_dw3bss_wstag935[];
extern char __start_dw3data_wstag936[];
extern char __stop_dw3data_wstag936[];
extern char __start_dw3bss_wstag936[];
extern char __stop_dw3bss_wstag936[];
extern char __start_dw3data_wstag937[];
extern char __stop_dw3data_wstag937[];
extern char __start_dw3bss_wstag937[];
extern char __stop_dw3bss_wstag937[];
extern char __start_dw3data_wstag938[];
extern char __stop_dw3data_wstag938[];
extern char __start_dw3bss_wstag938[];
extern char __stop_dw3bss_wstag938[];
extern char __start_dw3data_wstag939[];
extern char __stop_dw3data_wstag939[];
extern char __start_dw3bss_wstag939[];
extern char __stop_dw3bss_wstag939[];
extern char __start_dw3data_wstag940[];
extern char __stop_dw3data_wstag940[];
extern char __start_dw3bss_wstag940[];
extern char __stop_dw3bss_wstag940[];
extern char __start_dw3data_wstag941[];
extern char __stop_dw3data_wstag941[];
extern char __start_dw3bss_wstag941[];
extern char __stop_dw3bss_wstag941[];
extern char __start_dw3data_wstag942[];
extern char __stop_dw3data_wstag942[];
extern char __start_dw3bss_wstag942[];
extern char __stop_dw3bss_wstag942[];
extern char __start_dw3data_wstag943[];
extern char __stop_dw3data_wstag943[];
extern char __start_dw3bss_wstag943[];
extern char __stop_dw3bss_wstag943[];
extern char __start_dw3data_wstag944[];
extern char __stop_dw3data_wstag944[];
extern char __start_dw3bss_wstag944[];
extern char __stop_dw3bss_wstag944[];
extern char __start_dw3data_wstag945[];
extern char __stop_dw3data_wstag945[];
extern char __start_dw3bss_wstag945[];
extern char __stop_dw3bss_wstag945[];
extern char __start_dw3data_wstag946[];
extern char __stop_dw3data_wstag946[];
extern char __start_dw3bss_wstag946[];
extern char __stop_dw3bss_wstag946[];
extern char __start_dw3data_wstag947[];
extern char __stop_dw3data_wstag947[];
extern char __start_dw3bss_wstag947[];
extern char __stop_dw3bss_wstag947[];
extern char __start_dw3data_wstag948[];
extern char __stop_dw3data_wstag948[];
extern char __start_dw3bss_wstag948[];
extern char __stop_dw3bss_wstag948[];
extern char __start_dw3data_wstag949[];
extern char __stop_dw3data_wstag949[];
extern char __start_dw3bss_wstag949[];
extern char __stop_dw3bss_wstag949[];
extern char __start_dw3data_wstag950[];
extern char __stop_dw3data_wstag950[];
extern char __start_dw3bss_wstag950[];
extern char __stop_dw3bss_wstag950[];
extern char __start_dw3data_wstag951[];
extern char __stop_dw3data_wstag951[];
extern char __start_dw3bss_wstag951[];
extern char __stop_dw3bss_wstag951[];
extern char __start_dw3data_wstag952[];
extern char __stop_dw3data_wstag952[];
extern char __start_dw3bss_wstag952[];
extern char __stop_dw3bss_wstag952[];
extern char __start_dw3data_wstag953[];
extern char __stop_dw3data_wstag953[];
extern char __start_dw3bss_wstag953[];
extern char __stop_dw3bss_wstag953[];
extern char __start_dw3data_wstag954[];
extern char __stop_dw3data_wstag954[];
extern char __start_dw3bss_wstag954[];
extern char __stop_dw3bss_wstag954[];
extern char __start_dw3data_wstag955[];
extern char __stop_dw3data_wstag955[];
extern char __start_dw3bss_wstag955[];
extern char __stop_dw3bss_wstag955[];
extern char __start_dw3data_wstag956[];
extern char __stop_dw3data_wstag956[];
extern char __start_dw3bss_wstag956[];
extern char __stop_dw3bss_wstag956[];
extern char __start_dw3data_wstag957[];
extern char __stop_dw3data_wstag957[];
extern char __start_dw3bss_wstag957[];
extern char __stop_dw3bss_wstag957[];
extern char __start_dw3data_wstag958[];
extern char __stop_dw3data_wstag958[];
extern char __start_dw3bss_wstag958[];
extern char __stop_dw3bss_wstag958[];
extern char __start_dw3data_wstag959[];
extern char __stop_dw3data_wstag959[];
extern char __start_dw3bss_wstag959[];
extern char __stop_dw3bss_wstag959[];
extern char __start_dw3data_wstag960[];
extern char __stop_dw3data_wstag960[];
extern char __start_dw3bss_wstag960[];
extern char __stop_dw3bss_wstag960[];
extern char __start_dw3data_wstag961[];
extern char __stop_dw3data_wstag961[];
extern char __start_dw3bss_wstag961[];
extern char __stop_dw3bss_wstag961[];
extern char __start_dw3data_wstag962[];
extern char __stop_dw3data_wstag962[];
extern char __start_dw3bss_wstag962[];
extern char __stop_dw3bss_wstag962[];
extern char __start_dw3data_wstag963[];
extern char __stop_dw3data_wstag963[];
extern char __start_dw3bss_wstag963[];
extern char __stop_dw3bss_wstag963[];
extern char __start_dw3data_wstag964[];
extern char __stop_dw3data_wstag964[];
extern char __start_dw3bss_wstag964[];
extern char __stop_dw3bss_wstag964[];
extern char __start_dw3data_wstag965[];
extern char __stop_dw3data_wstag965[];
extern char __start_dw3bss_wstag965[];
extern char __stop_dw3bss_wstag965[];
extern char __start_dw3data_wstag966[];
extern char __stop_dw3data_wstag966[];
extern char __start_dw3bss_wstag966[];
extern char __stop_dw3bss_wstag966[];
extern char __start_dw3data_wstag967[];
extern char __stop_dw3data_wstag967[];
extern char __start_dw3bss_wstag967[];
extern char __stop_dw3bss_wstag967[];
extern char __start_dw3data_wstag968[];
extern char __stop_dw3data_wstag968[];
extern char __start_dw3bss_wstag968[];
extern char __stop_dw3bss_wstag968[];
extern char __start_dw3data_wstag969[];
extern char __stop_dw3data_wstag969[];
extern char __start_dw3bss_wstag969[];
extern char __stop_dw3bss_wstag969[];
extern char __start_dw3data_wstag970[];
extern char __stop_dw3data_wstag970[];
extern char __start_dw3bss_wstag970[];
extern char __stop_dw3bss_wstag970[];
extern char __start_dw3data_wstag971[];
extern char __stop_dw3data_wstag971[];
extern char __start_dw3bss_wstag971[];
extern char __stop_dw3bss_wstag971[];
extern char __start_dw3data_wstag972[];
extern char __stop_dw3data_wstag972[];
extern char __start_dw3bss_wstag972[];
extern char __stop_dw3bss_wstag972[];
extern char __start_dw3data_wstag973[];
extern char __stop_dw3data_wstag973[];
extern char __start_dw3bss_wstag973[];
extern char __stop_dw3bss_wstag973[];
extern char __start_dw3data_wstag974[];
extern char __stop_dw3data_wstag974[];
extern char __start_dw3bss_wstag974[];
extern char __stop_dw3bss_wstag974[];

void dw3_overlay_bind_eu_extra_sections(Dw3OverlayDesc *first) {
    dw3_overlay_bind_section(&first[0], __start_dw3data_wstag920, __stop_dw3data_wstag920,
                             __start_dw3bss_wstag920, __stop_dw3bss_wstag920);
    dw3_overlay_bind_section(&first[1], __start_dw3data_wstag921, __stop_dw3data_wstag921,
                             __start_dw3bss_wstag921, __stop_dw3bss_wstag921);
    dw3_overlay_bind_section(&first[2], __start_dw3data_wstag922, __stop_dw3data_wstag922,
                             __start_dw3bss_wstag922, __stop_dw3bss_wstag922);
    dw3_overlay_bind_section(&first[3], __start_dw3data_wstag923, __stop_dw3data_wstag923,
                             __start_dw3bss_wstag923, __stop_dw3bss_wstag923);
    dw3_overlay_bind_section(&first[4], __start_dw3data_wstag924, __stop_dw3data_wstag924,
                             __start_dw3bss_wstag924, __stop_dw3bss_wstag924);
    dw3_overlay_bind_section(&first[5], __start_dw3data_wstag925, __stop_dw3data_wstag925,
                             __start_dw3bss_wstag925, __stop_dw3bss_wstag925);
    dw3_overlay_bind_section(&first[6], __start_dw3data_wstag926, __stop_dw3data_wstag926,
                             __start_dw3bss_wstag926, __stop_dw3bss_wstag926);
    dw3_overlay_bind_section(&first[7], __start_dw3data_wstag927, __stop_dw3data_wstag927,
                             __start_dw3bss_wstag927, __stop_dw3bss_wstag927);
    dw3_overlay_bind_section(&first[8], __start_dw3data_wstag928, __stop_dw3data_wstag928,
                             __start_dw3bss_wstag928, __stop_dw3bss_wstag928);
    dw3_overlay_bind_section(&first[9], __start_dw3data_wstag929, __stop_dw3data_wstag929,
                             __start_dw3bss_wstag929, __stop_dw3bss_wstag929);
    dw3_overlay_bind_section(&first[10], __start_dw3data_wstag930, __stop_dw3data_wstag930,
                             __start_dw3bss_wstag930, __stop_dw3bss_wstag930);
    dw3_overlay_bind_section(&first[11], __start_dw3data_wstag931, __stop_dw3data_wstag931,
                             __start_dw3bss_wstag931, __stop_dw3bss_wstag931);
    dw3_overlay_bind_section(&first[12], __start_dw3data_wstag932, __stop_dw3data_wstag932,
                             __start_dw3bss_wstag932, __stop_dw3bss_wstag932);
    dw3_overlay_bind_section(&first[13], __start_dw3data_wstag933, __stop_dw3data_wstag933,
                             __start_dw3bss_wstag933, __stop_dw3bss_wstag933);
    dw3_overlay_bind_section(&first[14], __start_dw3data_wstag934, __stop_dw3data_wstag934,
                             __start_dw3bss_wstag934, __stop_dw3bss_wstag934);
    dw3_overlay_bind_section(&first[15], __start_dw3data_wstag935, __stop_dw3data_wstag935,
                             __start_dw3bss_wstag935, __stop_dw3bss_wstag935);
    dw3_overlay_bind_section(&first[16], __start_dw3data_wstag936, __stop_dw3data_wstag936,
                             __start_dw3bss_wstag936, __stop_dw3bss_wstag936);
    dw3_overlay_bind_section(&first[17], __start_dw3data_wstag937, __stop_dw3data_wstag937,
                             __start_dw3bss_wstag937, __stop_dw3bss_wstag937);
    dw3_overlay_bind_section(&first[18], __start_dw3data_wstag938, __stop_dw3data_wstag938,
                             __start_dw3bss_wstag938, __stop_dw3bss_wstag938);
    dw3_overlay_bind_section(&first[19], __start_dw3data_wstag939, __stop_dw3data_wstag939,
                             __start_dw3bss_wstag939, __stop_dw3bss_wstag939);
    dw3_overlay_bind_section(&first[20], __start_dw3data_wstag940, __stop_dw3data_wstag940,
                             __start_dw3bss_wstag940, __stop_dw3bss_wstag940);
    dw3_overlay_bind_section(&first[21], __start_dw3data_wstag941, __stop_dw3data_wstag941,
                             __start_dw3bss_wstag941, __stop_dw3bss_wstag941);
    dw3_overlay_bind_section(&first[22], __start_dw3data_wstag942, __stop_dw3data_wstag942,
                             __start_dw3bss_wstag942, __stop_dw3bss_wstag942);
    dw3_overlay_bind_section(&first[23], __start_dw3data_wstag943, __stop_dw3data_wstag943,
                             __start_dw3bss_wstag943, __stop_dw3bss_wstag943);
    dw3_overlay_bind_section(&first[24], __start_dw3data_wstag944, __stop_dw3data_wstag944,
                             __start_dw3bss_wstag944, __stop_dw3bss_wstag944);
    dw3_overlay_bind_section(&first[25], __start_dw3data_wstag945, __stop_dw3data_wstag945,
                             __start_dw3bss_wstag945, __stop_dw3bss_wstag945);
    dw3_overlay_bind_section(&first[26], __start_dw3data_wstag946, __stop_dw3data_wstag946,
                             __start_dw3bss_wstag946, __stop_dw3bss_wstag946);
    dw3_overlay_bind_section(&first[27], __start_dw3data_wstag947, __stop_dw3data_wstag947,
                             __start_dw3bss_wstag947, __stop_dw3bss_wstag947);
    dw3_overlay_bind_section(&first[28], __start_dw3data_wstag948, __stop_dw3data_wstag948,
                             __start_dw3bss_wstag948, __stop_dw3bss_wstag948);
    dw3_overlay_bind_section(&first[29], __start_dw3data_wstag949, __stop_dw3data_wstag949,
                             __start_dw3bss_wstag949, __stop_dw3bss_wstag949);
    dw3_overlay_bind_section(&first[30], __start_dw3data_wstag950, __stop_dw3data_wstag950,
                             __start_dw3bss_wstag950, __stop_dw3bss_wstag950);
    dw3_overlay_bind_section(&first[31], __start_dw3data_wstag951, __stop_dw3data_wstag951,
                             __start_dw3bss_wstag951, __stop_dw3bss_wstag951);
    dw3_overlay_bind_section(&first[32], __start_dw3data_wstag952, __stop_dw3data_wstag952,
                             __start_dw3bss_wstag952, __stop_dw3bss_wstag952);
    dw3_overlay_bind_section(&first[33], __start_dw3data_wstag953, __stop_dw3data_wstag953,
                             __start_dw3bss_wstag953, __stop_dw3bss_wstag953);
    dw3_overlay_bind_section(&first[34], __start_dw3data_wstag954, __stop_dw3data_wstag954,
                             __start_dw3bss_wstag954, __stop_dw3bss_wstag954);
    dw3_overlay_bind_section(&first[35], __start_dw3data_wstag955, __stop_dw3data_wstag955,
                             __start_dw3bss_wstag955, __stop_dw3bss_wstag955);
    dw3_overlay_bind_section(&first[36], __start_dw3data_wstag956, __stop_dw3data_wstag956,
                             __start_dw3bss_wstag956, __stop_dw3bss_wstag956);
    dw3_overlay_bind_section(&first[37], __start_dw3data_wstag957, __stop_dw3data_wstag957,
                             __start_dw3bss_wstag957, __stop_dw3bss_wstag957);
    dw3_overlay_bind_section(&first[38], __start_dw3data_wstag958, __stop_dw3data_wstag958,
                             __start_dw3bss_wstag958, __stop_dw3bss_wstag958);
    dw3_overlay_bind_section(&first[39], __start_dw3data_wstag959, __stop_dw3data_wstag959,
                             __start_dw3bss_wstag959, __stop_dw3bss_wstag959);
    dw3_overlay_bind_section(&first[40], __start_dw3data_wstag960, __stop_dw3data_wstag960,
                             __start_dw3bss_wstag960, __stop_dw3bss_wstag960);
    dw3_overlay_bind_section(&first[41], __start_dw3data_wstag961, __stop_dw3data_wstag961,
                             __start_dw3bss_wstag961, __stop_dw3bss_wstag961);
    dw3_overlay_bind_section(&first[42], __start_dw3data_wstag962, __stop_dw3data_wstag962,
                             __start_dw3bss_wstag962, __stop_dw3bss_wstag962);
    dw3_overlay_bind_section(&first[43], __start_dw3data_wstag963, __stop_dw3data_wstag963,
                             __start_dw3bss_wstag963, __stop_dw3bss_wstag963);
    dw3_overlay_bind_section(&first[44], __start_dw3data_wstag964, __stop_dw3data_wstag964,
                             __start_dw3bss_wstag964, __stop_dw3bss_wstag964);
    dw3_overlay_bind_section(&first[45], __start_dw3data_wstag965, __stop_dw3data_wstag965,
                             __start_dw3bss_wstag965, __stop_dw3bss_wstag965);
    dw3_overlay_bind_section(&first[46], __start_dw3data_wstag966, __stop_dw3data_wstag966,
                             __start_dw3bss_wstag966, __stop_dw3bss_wstag966);
    dw3_overlay_bind_section(&first[47], __start_dw3data_wstag967, __stop_dw3data_wstag967,
                             __start_dw3bss_wstag967, __stop_dw3bss_wstag967);
    dw3_overlay_bind_section(&first[48], __start_dw3data_wstag968, __stop_dw3data_wstag968,
                             __start_dw3bss_wstag968, __stop_dw3bss_wstag968);
    dw3_overlay_bind_section(&first[49], __start_dw3data_wstag969, __stop_dw3data_wstag969,
                             __start_dw3bss_wstag969, __stop_dw3bss_wstag969);
    dw3_overlay_bind_section(&first[50], __start_dw3data_wstag970, __stop_dw3data_wstag970,
                             __start_dw3bss_wstag970, __stop_dw3bss_wstag970);
    dw3_overlay_bind_section(&first[51], __start_dw3data_wstag971, __stop_dw3data_wstag971,
                             __start_dw3bss_wstag971, __stop_dw3bss_wstag971);
    dw3_overlay_bind_section(&first[52], __start_dw3data_wstag972, __stop_dw3data_wstag972,
                             __start_dw3bss_wstag972, __stop_dw3bss_wstag972);
    dw3_overlay_bind_section(&first[53], __start_dw3data_wstag973, __stop_dw3data_wstag973,
                             __start_dw3bss_wstag973, __stop_dw3bss_wstag973);
    dw3_overlay_bind_section(&first[54], __start_dw3data_wstag974, __stop_dw3data_wstag974,
                             __start_dw3bss_wstag974, __stop_dw3bss_wstag974);
}
