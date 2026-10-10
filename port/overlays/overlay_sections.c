/*
 * Bind host overlay descriptors to named .data/.bss sections produced by
 * tools/prefix_stage_obj.py (stages) and ld -r + objcopy (mode overlays).
 *
 * GNU ld provides __start_<sec> / __stop_<sec> for referenced orphan sections.
 */
#ifndef PORT_HOST
#error "port/overlays/overlay_sections.c is host-only"
#endif

#include "overlay_host.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char __start_dw3data_fieldstg[];
extern char __stop_dw3data_fieldstg[];
extern char __start_dw3bss_fieldstg[];
extern char __stop_dw3bss_fieldstg[];

extern char __start_dw3data_stplnmet[];
extern char __stop_dw3data_stplnmet[];
extern char __start_dw3bss_stplnmet[];
extern char __stop_dw3bss_stplnmet[];

extern char __start_dw3data_wstag260[];
extern char __stop_dw3data_wstag260[];
extern char __start_dw3bss_wstag260[];
extern char __stop_dw3bss_wstag260[];

extern char __start_dw3data_wstag780[];
extern char __stop_dw3data_wstag780[];
extern char __start_dw3bss_wstag780[];
extern char __stop_dw3bss_wstag780[];

extern char __start_dw3data_wstag785[];
extern char __stop_dw3data_wstag785[];
extern char __start_dw3bss_wstag785[];
extern char __stop_dw3bss_wstag785[];

extern char __start_dw3data_wstag790[];
extern char __stop_dw3data_wstag790[];
extern char __start_dw3bss_wstag790[];
extern char __stop_dw3bss_wstag790[];

/* P3.8 batch 1 — early-game / connected from 780/785/790 */
extern char __start_dw3data_wstag200[];
extern char __stop_dw3data_wstag200[];
extern char __start_dw3bss_wstag200[];
extern char __stop_dw3bss_wstag200[];
extern char __start_dw3data_wstag201[];
extern char __stop_dw3data_wstag201[];
extern char __start_dw3bss_wstag201[];
extern char __stop_dw3bss_wstag201[];
extern char __start_dw3data_wstag206[];
extern char __stop_dw3data_wstag206[];
extern char __start_dw3bss_wstag206[];
extern char __stop_dw3bss_wstag206[];
extern char __start_dw3data_wstag210[];
extern char __stop_dw3data_wstag210[];
extern char __start_dw3bss_wstag210[];
extern char __stop_dw3bss_wstag210[];
extern char __start_dw3data_wstag212[];
extern char __stop_dw3data_wstag212[];
extern char __start_dw3bss_wstag212[];
extern char __stop_dw3bss_wstag212[];
extern char __start_dw3data_wstag220[];
extern char __stop_dw3data_wstag220[];
extern char __start_dw3bss_wstag220[];
extern char __stop_dw3bss_wstag220[];
extern char __start_dw3data_wstag225[];
extern char __stop_dw3data_wstag225[];
extern char __start_dw3bss_wstag225[];
extern char __stop_dw3bss_wstag225[];
extern char __start_dw3data_wstag230[];
extern char __stop_dw3data_wstag230[];
extern char __start_dw3bss_wstag230[];
extern char __stop_dw3bss_wstag230[];
extern char __start_dw3data_wstag235[];
extern char __stop_dw3data_wstag235[];
extern char __start_dw3bss_wstag235[];
extern char __stop_dw3bss_wstag235[];
extern char __start_dw3data_wstag240[];
extern char __stop_dw3data_wstag240[];
extern char __start_dw3bss_wstag240[];
extern char __stop_dw3bss_wstag240[];
extern char __start_dw3data_wstag245[];
extern char __stop_dw3data_wstag245[];
extern char __start_dw3bss_wstag245[];
extern char __stop_dw3bss_wstag245[];
extern char __start_dw3data_wstag250[];
extern char __stop_dw3data_wstag250[];
extern char __start_dw3bss_wstag250[];
extern char __stop_dw3bss_wstag250[];
extern char __start_dw3data_wstag255[];
extern char __stop_dw3data_wstag255[];
extern char __start_dw3bss_wstag255[];
extern char __stop_dw3bss_wstag255[];
/* P3.8 batch 2 — remaining early Central/Asuka + Amaterasu twins */
extern char __start_dw3data_wstag202[];
extern char __stop_dw3data_wstag202[];
extern char __start_dw3bss_wstag202[];
extern char __stop_dw3bss_wstag202[];
extern char __start_dw3data_wstag203[];
extern char __stop_dw3data_wstag203[];
extern char __start_dw3bss_wstag203[];
extern char __stop_dw3bss_wstag203[];
extern char __start_dw3data_wstag205[];
extern char __stop_dw3data_wstag205[];
extern char __start_dw3bss_wstag205[];
extern char __stop_dw3bss_wstag205[];
extern char __start_dw3data_wstag218[];
extern char __stop_dw3data_wstag218[];
extern char __start_dw3bss_wstag218[];
extern char __stop_dw3bss_wstag218[];
extern char __start_dw3data_wstag219[];
extern char __stop_dw3data_wstag219[];
extern char __start_dw3bss_wstag219[];
extern char __stop_dw3bss_wstag219[];
extern char __start_dw3data_wstag232[];
extern char __stop_dw3data_wstag232[];
extern char __start_dw3bss_wstag232[];
extern char __stop_dw3bss_wstag232[];
extern char __start_dw3data_wstag237[];
extern char __stop_dw3data_wstag237[];
extern char __start_dw3bss_wstag237[];
extern char __stop_dw3bss_wstag237[];
extern char __start_dw3data_wstag270[];
extern char __stop_dw3data_wstag270[];
extern char __start_dw3bss_wstag270[];
extern char __stop_dw3bss_wstag270[];
extern char __start_dw3data_wstag275[];
extern char __stop_dw3data_wstag275[];
extern char __start_dw3bss_wstag275[];
extern char __stop_dw3bss_wstag275[];
extern char __start_dw3data_wstag280[];
extern char __stop_dw3data_wstag280[];
extern char __start_dw3bss_wstag280[];
extern char __stop_dw3bss_wstag280[];
extern char __start_dw3data_wstag285[];
extern char __stop_dw3data_wstag285[];
extern char __start_dw3bss_wstag285[];
extern char __stop_dw3bss_wstag285[];
extern char __start_dw3data_wstag290[];
extern char __stop_dw3data_wstag290[];
extern char __start_dw3bss_wstag290[];
extern char __stop_dw3bss_wstag290[];
extern char __start_dw3data_wstag295[];
extern char __stop_dw3data_wstag295[];
extern char __start_dw3bss_wstag295[];
extern char __stop_dw3bss_wstag295[];
extern char __start_dw3data_wstag300[];
extern char __stop_dw3data_wstag300[];
extern char __start_dw3bss_wstag300[];
extern char __stop_dw3bss_wstag300[];
extern char __start_dw3data_wstag305[];
extern char __stop_dw3data_wstag305[];
extern char __start_dw3bss_wstag305[];
extern char __stop_dw3bss_wstag305[];
extern char __start_dw3data_wstag310[];
extern char __stop_dw3data_wstag310[];
extern char __start_dw3bss_wstag310[];
extern char __stop_dw3bss_wstag310[];
extern char __start_dw3data_wstag315[];
extern char __stop_dw3data_wstag315[];
extern char __start_dw3bss_wstag315[];
extern char __stop_dw3bss_wstag315[];
extern char __start_dw3data_wstag320[];
extern char __stop_dw3data_wstag320[];
extern char __start_dw3bss_wstag320[];
extern char __stop_dw3bss_wstag320[];
extern char __start_dw3data_wstag325[];
extern char __stop_dw3data_wstag325[];
extern char __start_dw3bss_wstag325[];
extern char __stop_dw3bss_wstag325[];
extern char __start_dw3data_wstag211[];
extern char __stop_dw3data_wstag211[];
extern char __start_dw3bss_wstag211[];
extern char __stop_dw3bss_wstag211[];
extern char __start_dw3data_wstag221[];
extern char __stop_dw3data_wstag221[];
extern char __start_dw3bss_wstag221[];
extern char __stop_dw3bss_wstag221[];
extern char __start_dw3data_wstag226[];
extern char __stop_dw3data_wstag226[];
extern char __start_dw3bss_wstag226[];
extern char __stop_dw3bss_wstag226[];
extern char __start_dw3data_wstag231[];
extern char __stop_dw3data_wstag231[];
extern char __start_dw3bss_wstag231[];
extern char __stop_dw3bss_wstag231[];
extern char __start_dw3data_wstag233[];
extern char __stop_dw3data_wstag233[];
extern char __start_dw3bss_wstag233[];
extern char __stop_dw3bss_wstag233[];
extern char __start_dw3data_wstag236[];
extern char __stop_dw3data_wstag236[];
extern char __start_dw3bss_wstag236[];
extern char __stop_dw3bss_wstag236[];
extern char __start_dw3data_wstag238[];
extern char __stop_dw3data_wstag238[];
extern char __start_dw3bss_wstag238[];
extern char __stop_dw3bss_wstag238[];
extern char __start_dw3data_wstag241[];
extern char __stop_dw3data_wstag241[];
extern char __start_dw3bss_wstag241[];
extern char __stop_dw3bss_wstag241[];
extern char __start_dw3data_wstag246[];
extern char __stop_dw3data_wstag246[];
extern char __start_dw3bss_wstag246[];
extern char __stop_dw3bss_wstag246[];
extern char __start_dw3data_wstag251[];
extern char __stop_dw3data_wstag251[];
extern char __start_dw3bss_wstag251[];
extern char __stop_dw3bss_wstag251[];
extern char __start_dw3data_wstag256[];
extern char __stop_dw3data_wstag256[];
extern char __start_dw3bss_wstag256[];
extern char __stop_dw3bss_wstag256[];
extern char __start_dw3data_wstag261[];
extern char __stop_dw3data_wstag261[];
extern char __start_dw3bss_wstag261[];
extern char __stop_dw3bss_wstag261[];

extern char __start_dw3data_wstag271[];
extern char __stop_dw3data_wstag271[];
extern char __start_dw3bss_wstag271[];
extern char __stop_dw3bss_wstag271[];
extern char __start_dw3data_wstag276[];
extern char __stop_dw3data_wstag276[];
extern char __start_dw3bss_wstag276[];
extern char __stop_dw3bss_wstag276[];
extern char __start_dw3data_wstag281[];
extern char __stop_dw3data_wstag281[];
extern char __start_dw3bss_wstag281[];
extern char __stop_dw3bss_wstag281[];
extern char __start_dw3data_wstag286[];
extern char __stop_dw3data_wstag286[];
extern char __start_dw3bss_wstag286[];
extern char __stop_dw3bss_wstag286[];
extern char __start_dw3data_wstag291[];
extern char __stop_dw3data_wstag291[];
extern char __start_dw3bss_wstag291[];
extern char __stop_dw3bss_wstag291[];
extern char __start_dw3data_wstag296[];
extern char __stop_dw3data_wstag296[];
extern char __start_dw3bss_wstag296[];
extern char __stop_dw3bss_wstag296[];
extern char __start_dw3data_wstag301[];
extern char __stop_dw3data_wstag301[];
extern char __start_dw3bss_wstag301[];
extern char __stop_dw3bss_wstag301[];
extern char __start_dw3data_wstag306[];
extern char __stop_dw3data_wstag306[];
extern char __start_dw3bss_wstag306[];
extern char __stop_dw3bss_wstag306[];
extern char __start_dw3data_wstag311[];
extern char __stop_dw3data_wstag311[];
extern char __start_dw3bss_wstag311[];
extern char __stop_dw3bss_wstag311[];
extern char __start_dw3data_wstag316[];
extern char __stop_dw3data_wstag316[];
extern char __start_dw3bss_wstag316[];
extern char __stop_dw3bss_wstag316[];
extern char __start_dw3data_wstag321[];
extern char __stop_dw3data_wstag321[];
extern char __start_dw3bss_wstag321[];
extern char __stop_dw3bss_wstag321[];
extern char __start_dw3data_wstag326[];
extern char __stop_dw3data_wstag326[];
extern char __start_dw3bss_wstag326[];
extern char __stop_dw3bss_wstag326[];
extern char __start_dw3data_wstag330[];
extern char __stop_dw3data_wstag330[];
extern char __start_dw3bss_wstag330[];
extern char __stop_dw3bss_wstag330[];
extern char __start_dw3data_wstag335[];
extern char __stop_dw3data_wstag335[];
extern char __start_dw3bss_wstag335[];
extern char __stop_dw3bss_wstag335[];
extern char __start_dw3data_wstag340[];
extern char __stop_dw3data_wstag340[];
extern char __start_dw3bss_wstag340[];
extern char __stop_dw3bss_wstag340[];
extern char __start_dw3data_wstag345[];
extern char __stop_dw3data_wstag345[];
extern char __start_dw3bss_wstag345[];
extern char __stop_dw3bss_wstag345[];
extern char __start_dw3data_wstag350[];
extern char __stop_dw3data_wstag350[];
extern char __start_dw3bss_wstag350[];
extern char __stop_dw3bss_wstag350[];
extern char __start_dw3data_wstag355[];
extern char __stop_dw3data_wstag355[];
extern char __start_dw3bss_wstag355[];
extern char __stop_dw3bss_wstag355[];
extern char __start_dw3data_wstag360[];
extern char __stop_dw3data_wstag360[];
extern char __start_dw3bss_wstag360[];
extern char __stop_dw3bss_wstag360[];
extern char __start_dw3data_wstag365[];
extern char __stop_dw3data_wstag365[];
extern char __start_dw3bss_wstag365[];
extern char __stop_dw3bss_wstag365[];
extern char __start_dw3data_wstag370[];
extern char __stop_dw3data_wstag370[];
extern char __start_dw3bss_wstag370[];
extern char __stop_dw3bss_wstag370[];
extern char __start_dw3data_wstag375[];
extern char __stop_dw3data_wstag375[];
extern char __start_dw3bss_wstag375[];
extern char __stop_dw3bss_wstag375[];
extern char __start_dw3data_wstag380[];
extern char __stop_dw3data_wstag380[];
extern char __start_dw3bss_wstag380[];
extern char __stop_dw3bss_wstag380[];
extern char __start_dw3data_wstag385[];
extern char __stop_dw3data_wstag385[];
extern char __start_dw3bss_wstag385[];
extern char __stop_dw3bss_wstag385[];
extern char __start_dw3data_wstag395[];
extern char __stop_dw3data_wstag395[];
extern char __start_dw3bss_wstag395[];
extern char __stop_dw3bss_wstag395[];
extern char __start_dw3data_wstag400[];
extern char __stop_dw3data_wstag400[];
extern char __start_dw3bss_wstag400[];
extern char __stop_dw3bss_wstag400[];
extern char __start_dw3data_wstag405[];
extern char __stop_dw3data_wstag405[];
extern char __start_dw3bss_wstag405[];
extern char __stop_dw3bss_wstag405[];
extern char __start_dw3data_wstag410[];
extern char __stop_dw3data_wstag410[];
extern char __start_dw3bss_wstag410[];
extern char __stop_dw3bss_wstag410[];
extern char __start_dw3data_wstag415[];
extern char __stop_dw3data_wstag415[];
extern char __start_dw3bss_wstag415[];
extern char __stop_dw3bss_wstag415[];
extern char __start_dw3data_wstag420[];
extern char __stop_dw3data_wstag420[];
extern char __start_dw3bss_wstag420[];
extern char __stop_dw3bss_wstag420[];
extern char __start_dw3data_wstag425[];
extern char __stop_dw3data_wstag425[];
extern char __start_dw3bss_wstag425[];
extern char __stop_dw3bss_wstag425[];
extern char __start_dw3data_wstag430[];
extern char __stop_dw3data_wstag430[];
extern char __start_dw3bss_wstag430[];
extern char __stop_dw3bss_wstag430[];
extern char __start_dw3data_wstag435[];
extern char __stop_dw3data_wstag435[];
extern char __start_dw3bss_wstag435[];
extern char __stop_dw3bss_wstag435[];
extern char __start_dw3data_wstag440[];
extern char __stop_dw3data_wstag440[];
extern char __start_dw3bss_wstag440[];
extern char __stop_dw3bss_wstag440[];
extern char __start_dw3data_wstag445[];
extern char __stop_dw3data_wstag445[];
extern char __start_dw3bss_wstag445[];
extern char __stop_dw3bss_wstag445[];
extern char __start_dw3data_wstag450[];
extern char __stop_dw3data_wstag450[];
extern char __start_dw3bss_wstag450[];
extern char __stop_dw3bss_wstag450[];
extern char __start_dw3data_wstag455[];
extern char __stop_dw3data_wstag455[];
extern char __start_dw3bss_wstag455[];
extern char __stop_dw3bss_wstag455[];
extern char __start_dw3data_wstag475[];
extern char __stop_dw3data_wstag475[];
extern char __start_dw3bss_wstag475[];
extern char __stop_dw3bss_wstag475[];
extern char __start_dw3data_wstag465[];
extern char __stop_dw3data_wstag465[];
extern char __start_dw3bss_wstag465[];
extern char __stop_dw3bss_wstag465[];
extern char __start_dw3data_wstag470[];
extern char __stop_dw3data_wstag470[];
extern char __start_dw3bss_wstag470[];
extern char __stop_dw3bss_wstag470[];
/* P3.8 batch 4 — South/Suzaku/Network + West/Byakko/North */
extern char __start_dw3data_wstag480[];
extern char __stop_dw3data_wstag480[];
extern char __start_dw3bss_wstag480[];
extern char __stop_dw3bss_wstag480[];
extern char __start_dw3data_wstag485[];
extern char __stop_dw3data_wstag485[];
extern char __start_dw3bss_wstag485[];
extern char __stop_dw3bss_wstag485[];
extern char __start_dw3data_wstag490[];
extern char __stop_dw3data_wstag490[];
extern char __start_dw3bss_wstag490[];
extern char __stop_dw3bss_wstag490[];
extern char __start_dw3data_wstag495[];
extern char __stop_dw3data_wstag495[];
extern char __start_dw3bss_wstag495[];
extern char __stop_dw3bss_wstag495[];
extern char __start_dw3data_wstag500[];
extern char __stop_dw3data_wstag500[];
extern char __start_dw3bss_wstag500[];
extern char __stop_dw3bss_wstag500[];
extern char __start_dw3data_wstag505[];
extern char __stop_dw3data_wstag505[];
extern char __start_dw3bss_wstag505[];
extern char __stop_dw3bss_wstag505[];
extern char __start_dw3data_wstag520[];
extern char __stop_dw3data_wstag520[];
extern char __start_dw3bss_wstag520[];
extern char __stop_dw3bss_wstag520[];
extern char __start_dw3data_wstag525[];
extern char __stop_dw3data_wstag525[];
extern char __start_dw3bss_wstag525[];
extern char __stop_dw3bss_wstag525[];
extern char __start_dw3data_wstag530[];
extern char __stop_dw3data_wstag530[];
extern char __start_dw3bss_wstag530[];
extern char __stop_dw3bss_wstag530[];
extern char __start_dw3data_wstag535[];
extern char __stop_dw3data_wstag535[];
extern char __start_dw3bss_wstag535[];
extern char __stop_dw3bss_wstag535[];
extern char __start_dw3data_wstag537[];
extern char __stop_dw3data_wstag537[];
extern char __start_dw3bss_wstag537[];
extern char __stop_dw3bss_wstag537[];
extern char __start_dw3data_wstag540[];
extern char __stop_dw3data_wstag540[];
extern char __start_dw3bss_wstag540[];
extern char __stop_dw3bss_wstag540[];
extern char __start_dw3data_wstag545[];
extern char __stop_dw3data_wstag545[];
extern char __start_dw3bss_wstag545[];
extern char __stop_dw3bss_wstag545[];
extern char __start_dw3data_wstag550[];
extern char __stop_dw3data_wstag550[];
extern char __start_dw3bss_wstag550[];
extern char __stop_dw3bss_wstag550[];
extern char __start_dw3data_wstag555[];
extern char __stop_dw3data_wstag555[];
extern char __start_dw3bss_wstag555[];
extern char __stop_dw3bss_wstag555[];
extern char __start_dw3data_wstag560[];
extern char __stop_dw3data_wstag560[];
extern char __start_dw3bss_wstag560[];
extern char __stop_dw3bss_wstag560[];
extern char __start_dw3data_wstag565[];
extern char __stop_dw3data_wstag565[];
extern char __start_dw3bss_wstag565[];
extern char __stop_dw3bss_wstag565[];
extern char __start_dw3data_wstag570[];
extern char __stop_dw3data_wstag570[];
extern char __start_dw3bss_wstag570[];
extern char __stop_dw3bss_wstag570[];
extern char __start_dw3data_wstag575[];
extern char __stop_dw3data_wstag575[];
extern char __start_dw3bss_wstag575[];
extern char __stop_dw3bss_wstag575[];
extern char __start_dw3data_wstag580[];
extern char __stop_dw3data_wstag580[];
extern char __start_dw3bss_wstag580[];
extern char __stop_dw3bss_wstag580[];
extern char __start_dw3data_wstag585[];
extern char __stop_dw3data_wstag585[];
extern char __start_dw3bss_wstag585[];
extern char __stop_dw3bss_wstag585[];
extern char __start_dw3data_wstag590[];
extern char __stop_dw3data_wstag590[];
extern char __start_dw3bss_wstag590[];
extern char __stop_dw3bss_wstag590[];
extern char __start_dw3data_wstag595[];
extern char __stop_dw3data_wstag595[];
extern char __start_dw3bss_wstag595[];
extern char __stop_dw3bss_wstag595[];
extern char __start_dw3data_wstag600[];
extern char __stop_dw3data_wstag600[];
extern char __start_dw3bss_wstag600[];
extern char __stop_dw3bss_wstag600[];
extern char __start_dw3data_wstag605[];
extern char __stop_dw3data_wstag605[];
extern char __start_dw3bss_wstag605[];
extern char __stop_dw3bss_wstag605[];
extern char __start_dw3data_wstag610[];
extern char __stop_dw3data_wstag610[];
extern char __start_dw3bss_wstag610[];
extern char __stop_dw3bss_wstag610[];
extern char __start_dw3data_wstag615[];
extern char __stop_dw3data_wstag615[];
extern char __start_dw3bss_wstag615[];
extern char __stop_dw3bss_wstag615[];
extern char __start_dw3data_wstag620[];
extern char __stop_dw3data_wstag620[];
extern char __start_dw3bss_wstag620[];
extern char __stop_dw3bss_wstag620[];
extern char __start_dw3data_wstag625[];
extern char __stop_dw3data_wstag625[];
extern char __start_dw3bss_wstag625[];
extern char __stop_dw3bss_wstag625[];
extern char __start_dw3data_wstag630[];
extern char __stop_dw3data_wstag630[];
extern char __start_dw3bss_wstag630[];
extern char __stop_dw3bss_wstag630[];
extern char __start_dw3data_wstag635[];
extern char __stop_dw3data_wstag635[];
extern char __start_dw3bss_wstag635[];
extern char __stop_dw3bss_wstag635[];
extern char __start_dw3data_wstag640[];
extern char __stop_dw3data_wstag640[];
extern char __start_dw3bss_wstag640[];
extern char __stop_dw3bss_wstag640[];
extern char __start_dw3data_wstag645[];
extern char __stop_dw3data_wstag645[];
extern char __start_dw3bss_wstag645[];
extern char __stop_dw3bss_wstag645[];
extern char __start_dw3data_wstag650[];
extern char __stop_dw3data_wstag650[];
extern char __start_dw3bss_wstag650[];
extern char __stop_dw3bss_wstag650[];
extern char __start_dw3data_wstag655[];
extern char __stop_dw3data_wstag655[];
extern char __start_dw3bss_wstag655[];
extern char __stop_dw3bss_wstag655[];
extern char __start_dw3data_wstag660[];
extern char __stop_dw3data_wstag660[];
extern char __start_dw3bss_wstag660[];
extern char __stop_dw3bss_wstag660[];
extern char __start_dw3data_wstag675[];
extern char __stop_dw3data_wstag675[];
extern char __start_dw3bss_wstag675[];
extern char __stop_dw3bss_wstag675[];
extern char __start_dw3data_wstag680[];
extern char __stop_dw3data_wstag680[];
extern char __start_dw3bss_wstag680[];
extern char __stop_dw3bss_wstag680[];
extern char __start_dw3data_wstag685[];
extern char __stop_dw3data_wstag685[];
extern char __start_dw3bss_wstag685[];
extern char __stop_dw3bss_wstag685[];
extern char __start_dw3data_wstag690[];
extern char __stop_dw3data_wstag690[];
extern char __start_dw3bss_wstag690[];
extern char __stop_dw3bss_wstag690[];
extern char __start_dw3data_wstag695[];
extern char __stop_dw3data_wstag695[];
extern char __start_dw3bss_wstag695[];
extern char __stop_dw3bss_wstag695[];
extern char __start_dw3data_wstag700[];
extern char __stop_dw3data_wstag700[];
extern char __start_dw3bss_wstag700[];
extern char __stop_dw3bss_wstag700[];
extern char __start_dw3data_wstag705[];
extern char __stop_dw3data_wstag705[];
extern char __start_dw3bss_wstag705[];
extern char __stop_dw3bss_wstag705[];
extern char __start_dw3data_wstag710[];
extern char __stop_dw3data_wstag710[];
extern char __start_dw3bss_wstag710[];
extern char __stop_dw3bss_wstag710[];
extern char __start_dw3data_wstag715[];
extern char __stop_dw3data_wstag715[];
extern char __start_dw3bss_wstag715[];
extern char __stop_dw3bss_wstag715[];
extern char __start_dw3data_wstag720[];
extern char __stop_dw3data_wstag720[];
extern char __start_dw3bss_wstag720[];
extern char __stop_dw3bss_wstag720[];
extern char __start_dw3data_wstag725[];
extern char __stop_dw3data_wstag725[];
extern char __start_dw3bss_wstag725[];
extern char __stop_dw3bss_wstag725[];
extern char __start_dw3data_wstag730[];
extern char __stop_dw3data_wstag730[];
extern char __start_dw3bss_wstag730[];
extern char __stop_dw3bss_wstag730[];
extern char __start_dw3data_wstag735[];
extern char __stop_dw3data_wstag735[];
extern char __start_dw3bss_wstag735[];
extern char __stop_dw3bss_wstag735[];
extern char __start_dw3data_wstag740[];
extern char __stop_dw3data_wstag740[];
extern char __start_dw3bss_wstag740[];
extern char __stop_dw3bss_wstag740[];
extern char __start_dw3data_wstag745[];
extern char __stop_dw3data_wstag745[];
extern char __start_dw3bss_wstag745[];
extern char __stop_dw3bss_wstag745[];
extern char __start_dw3data_wstag750[];
extern char __stop_dw3data_wstag750[];
extern char __start_dw3bss_wstag750[];
extern char __stop_dw3bss_wstag750[];
extern char __start_dw3data_wstag755[];
extern char __stop_dw3data_wstag755[];
extern char __start_dw3bss_wstag755[];
extern char __stop_dw3bss_wstag755[];
extern char __start_dw3data_wstag760[];
extern char __stop_dw3data_wstag760[];
extern char __start_dw3bss_wstag760[];
extern char __stop_dw3bss_wstag760[];


extern char __start_dw3data_fightstg[];
extern char __stop_dw3data_fightstg[];
extern char __start_dw3bss_fightstg[];
extern char __stop_dw3bss_fightstg[];

extern char __start_dw3data_wfightmn[];
extern char __stop_dw3data_wfightmn[];
extern char __start_dw3bss_wfightmn[];
extern char __stop_dw3bss_wfightmn[];

extern char __start_dw3data_stfgtrep[];
extern char __stop_dw3data_stfgtrep[];
extern char __start_dw3bss_stfgtrep[];
extern char __stop_dw3bss_stfgtrep[];

extern char __start_dw3data_ststatus[];
extern char __stop_dw3data_ststatus[];
extern char __start_dw3bss_ststatus[];
extern char __stop_dw3bss_ststatus[];

extern char __start_dw3data_stitshop[];
extern char __stop_dw3data_stitshop[];
extern char __start_dw3bss_stitshop[];
extern char __stop_dw3bss_stitshop[];

extern char __start_dw3data_stgtrain[];
extern char __stop_dw3data_stgtrain[];
extern char __start_dw3bss_stgtrain[];
extern char __stop_dw3bss_stgtrain[];

extern char __start_dw3data_stgdglab[];
extern char __stop_dw3data_stgdglab[];
extern char __start_dw3bss_stgdglab[];
extern char __stop_dw3bss_stgdglab[];

extern char __start_dw3data_stgmcard[];
extern char __stop_dw3data_stgmcard[];
extern char __start_dw3bss_stgmcard[];
extern char __stop_dw3bss_stgmcard[];

extern char __start_dw3data_stdgname[];
extern char __stop_dw3data_stdgname[];
extern char __start_dw3bss_stdgname[];
extern char __stop_dw3bss_stdgname[];

extern char __start_dw3data_cardgame[];
extern char __stop_dw3data_cardgame[];
extern char __start_dw3bss_cardgame[];
extern char __stop_dw3bss_cardgame[];

extern char __start_dw3data_stcrddek[];
extern char __stop_dw3data_stcrddek[];
extern char __start_dw3bss_stcrddek[];
extern char __stop_dw3bss_stcrddek[];

extern char __start_dw3data_stcrdshp[];
extern char __stop_dw3data_stcrdshp[];
extern char __start_dw3bss_stcrdshp[];
extern char __stop_dw3bss_stcrdshp[];

extern char __start_dw3data_stcrdabm[];
extern char __stop_dw3data_stcrdabm[];
extern char __start_dw3bss_stcrdabm[];
extern char __stop_dw3bss_stcrdabm[];

extern char __start_dw3data_stagslct[];
extern char __stop_dw3data_stagslct[];
extern char __start_dw3bss_stagslct[];
extern char __stop_dw3bss_stagslct[];

static void bind_section(Dw3OverlayDesc *desc, char *data, char *data_end, char *bss,
                         char *bss_end) {
    size_t data_size;
    size_t bss_size;
    void *init;

    if (desc == NULL) {
        return;
    }
    data_size = (size_t)(data_end - data);
    bss_size = (size_t)(bss_end - bss);

    desc->data = data;
    desc->data_size = data_size;
    desc->bss = bss;
    desc->bss_size = bss_size;

    if (data_size == 0) {
        desc->data_init = NULL;
        fprintf(stderr, "dw3: overlay file=0x%x empty .data section\n",
                (unsigned)desc->file_id);
        return;
    }
    init = malloc(data_size);
    if (init == NULL) {
        fprintf(stderr, "dw3: overlay snapshot malloc(%zu) failed file=0x%x\n", data_size,
                (unsigned)desc->file_id);
        desc->data_init = NULL;
        return;
    }
    memcpy(init, data, data_size);
    desc->data_init = init;
    fprintf(stderr, "dw3: overlay file=0x%x data=%zu bss=%zu snapshot=%p\n",
            (unsigned)desc->file_id, data_size, bss_size, init);
}

void dw3_overlay_bind_field_sections(Dw3OverlayDesc *fieldstg, Dw3OverlayDesc *stplnmet,
                                     Dw3OverlayDesc *wstag260, Dw3OverlayDesc *wstag780,
                                     Dw3OverlayDesc *wstag785, Dw3OverlayDesc *wstag790) {
    bind_section(fieldstg, __start_dw3data_fieldstg, __stop_dw3data_fieldstg,
                 __start_dw3bss_fieldstg, __stop_dw3bss_fieldstg);
    bind_section(stplnmet, __start_dw3data_stplnmet, __stop_dw3data_stplnmet,
                 __start_dw3bss_stplnmet, __stop_dw3bss_stplnmet);
    bind_section(wstag260, __start_dw3data_wstag260, __stop_dw3data_wstag260,
                 __start_dw3bss_wstag260, __stop_dw3bss_wstag260);
    bind_section(wstag780, __start_dw3data_wstag780, __stop_dw3data_wstag780,
                 __start_dw3bss_wstag780, __stop_dw3bss_wstag780);
    bind_section(wstag785, __start_dw3data_wstag785, __stop_dw3data_wstag785,
                 __start_dw3bss_wstag785, __stop_dw3bss_wstag785);
    bind_section(wstag790, __start_dw3data_wstag790, __stop_dw3data_wstag790,
                 __start_dw3bss_wstag790, __stop_dw3bss_wstag790);
}

void dw3_overlay_bind_p38_batch1_sections(
    Dw3OverlayDesc *wstag200, Dw3OverlayDesc *wstag201, Dw3OverlayDesc *wstag206,
    Dw3OverlayDesc *wstag210, Dw3OverlayDesc *wstag212, Dw3OverlayDesc *wstag220,
    Dw3OverlayDesc *wstag225, Dw3OverlayDesc *wstag230, Dw3OverlayDesc *wstag235,
    Dw3OverlayDesc *wstag240, Dw3OverlayDesc *wstag245, Dw3OverlayDesc *wstag250,
    Dw3OverlayDesc *wstag255) {
    bind_section(wstag200, __start_dw3data_wstag200, __stop_dw3data_wstag200,
                 __start_dw3bss_wstag200, __stop_dw3bss_wstag200);
    bind_section(wstag201, __start_dw3data_wstag201, __stop_dw3data_wstag201,
                 __start_dw3bss_wstag201, __stop_dw3bss_wstag201);
    bind_section(wstag206, __start_dw3data_wstag206, __stop_dw3data_wstag206,
                 __start_dw3bss_wstag206, __stop_dw3bss_wstag206);
    bind_section(wstag210, __start_dw3data_wstag210, __stop_dw3data_wstag210,
                 __start_dw3bss_wstag210, __stop_dw3bss_wstag210);
    bind_section(wstag212, __start_dw3data_wstag212, __stop_dw3data_wstag212,
                 __start_dw3bss_wstag212, __stop_dw3bss_wstag212);
    bind_section(wstag220, __start_dw3data_wstag220, __stop_dw3data_wstag220,
                 __start_dw3bss_wstag220, __stop_dw3bss_wstag220);
    bind_section(wstag225, __start_dw3data_wstag225, __stop_dw3data_wstag225,
                 __start_dw3bss_wstag225, __stop_dw3bss_wstag225);
    bind_section(wstag230, __start_dw3data_wstag230, __stop_dw3data_wstag230,
                 __start_dw3bss_wstag230, __stop_dw3bss_wstag230);
    bind_section(wstag235, __start_dw3data_wstag235, __stop_dw3data_wstag235,
                 __start_dw3bss_wstag235, __stop_dw3bss_wstag235);
    bind_section(wstag240, __start_dw3data_wstag240, __stop_dw3data_wstag240,
                 __start_dw3bss_wstag240, __stop_dw3bss_wstag240);
    bind_section(wstag245, __start_dw3data_wstag245, __stop_dw3data_wstag245,
                 __start_dw3bss_wstag245, __stop_dw3bss_wstag245);
    bind_section(wstag250, __start_dw3data_wstag250, __stop_dw3data_wstag250,
                 __start_dw3bss_wstag250, __stop_dw3bss_wstag250);
    bind_section(wstag255, __start_dw3data_wstag255, __stop_dw3data_wstag255,
                 __start_dw3bss_wstag255, __stop_dw3bss_wstag255);
}

void dw3_overlay_bind_p38_batch2a_sections(
    Dw3OverlayDesc *wstag202,
    Dw3OverlayDesc *wstag203,
    Dw3OverlayDesc *wstag205,
    Dw3OverlayDesc *wstag218,
    Dw3OverlayDesc *wstag219,
    Dw3OverlayDesc *wstag232,
    Dw3OverlayDesc *wstag237,
    Dw3OverlayDesc *wstag270,
    Dw3OverlayDesc *wstag275,
    Dw3OverlayDesc *wstag280,
    Dw3OverlayDesc *wstag285,
    Dw3OverlayDesc *wstag290,
    Dw3OverlayDesc *wstag295,
    Dw3OverlayDesc *wstag300,
    Dw3OverlayDesc *wstag305,
    Dw3OverlayDesc *wstag310,
    Dw3OverlayDesc *wstag315,
    Dw3OverlayDesc *wstag320,
    Dw3OverlayDesc *wstag325) {
    bind_section(wstag202, __start_dw3data_wstag202, __stop_dw3data_wstag202,
                 __start_dw3bss_wstag202, __stop_dw3bss_wstag202);
    bind_section(wstag203, __start_dw3data_wstag203, __stop_dw3data_wstag203,
                 __start_dw3bss_wstag203, __stop_dw3bss_wstag203);
    bind_section(wstag205, __start_dw3data_wstag205, __stop_dw3data_wstag205,
                 __start_dw3bss_wstag205, __stop_dw3bss_wstag205);
    bind_section(wstag218, __start_dw3data_wstag218, __stop_dw3data_wstag218,
                 __start_dw3bss_wstag218, __stop_dw3bss_wstag218);
    bind_section(wstag219, __start_dw3data_wstag219, __stop_dw3data_wstag219,
                 __start_dw3bss_wstag219, __stop_dw3bss_wstag219);
    bind_section(wstag232, __start_dw3data_wstag232, __stop_dw3data_wstag232,
                 __start_dw3bss_wstag232, __stop_dw3bss_wstag232);
    bind_section(wstag237, __start_dw3data_wstag237, __stop_dw3data_wstag237,
                 __start_dw3bss_wstag237, __stop_dw3bss_wstag237);
    bind_section(wstag270, __start_dw3data_wstag270, __stop_dw3data_wstag270,
                 __start_dw3bss_wstag270, __stop_dw3bss_wstag270);
    bind_section(wstag275, __start_dw3data_wstag275, __stop_dw3data_wstag275,
                 __start_dw3bss_wstag275, __stop_dw3bss_wstag275);
    bind_section(wstag280, __start_dw3data_wstag280, __stop_dw3data_wstag280,
                 __start_dw3bss_wstag280, __stop_dw3bss_wstag280);
    bind_section(wstag285, __start_dw3data_wstag285, __stop_dw3data_wstag285,
                 __start_dw3bss_wstag285, __stop_dw3bss_wstag285);
    bind_section(wstag290, __start_dw3data_wstag290, __stop_dw3data_wstag290,
                 __start_dw3bss_wstag290, __stop_dw3bss_wstag290);
    bind_section(wstag295, __start_dw3data_wstag295, __stop_dw3data_wstag295,
                 __start_dw3bss_wstag295, __stop_dw3bss_wstag295);
    bind_section(wstag300, __start_dw3data_wstag300, __stop_dw3data_wstag300,
                 __start_dw3bss_wstag300, __stop_dw3bss_wstag300);
    bind_section(wstag305, __start_dw3data_wstag305, __stop_dw3data_wstag305,
                 __start_dw3bss_wstag305, __stop_dw3bss_wstag305);
    bind_section(wstag310, __start_dw3data_wstag310, __stop_dw3data_wstag310,
                 __start_dw3bss_wstag310, __stop_dw3bss_wstag310);
    bind_section(wstag315, __start_dw3data_wstag315, __stop_dw3data_wstag315,
                 __start_dw3bss_wstag315, __stop_dw3bss_wstag315);
    bind_section(wstag320, __start_dw3data_wstag320, __stop_dw3data_wstag320,
                 __start_dw3bss_wstag320, __stop_dw3bss_wstag320);
    bind_section(wstag325, __start_dw3data_wstag325, __stop_dw3data_wstag325,
                 __start_dw3bss_wstag325, __stop_dw3bss_wstag325);
}

void dw3_overlay_bind_p38_batch2b_sections(
    Dw3OverlayDesc *wstag211,
    Dw3OverlayDesc *wstag221,
    Dw3OverlayDesc *wstag226,
    Dw3OverlayDesc *wstag231,
    Dw3OverlayDesc *wstag233,
    Dw3OverlayDesc *wstag236,
    Dw3OverlayDesc *wstag238,
    Dw3OverlayDesc *wstag241,
    Dw3OverlayDesc *wstag246,
    Dw3OverlayDesc *wstag251,
    Dw3OverlayDesc *wstag256,
    Dw3OverlayDesc *wstag261) {
    bind_section(wstag211, __start_dw3data_wstag211, __stop_dw3data_wstag211,
                 __start_dw3bss_wstag211, __stop_dw3bss_wstag211);
    bind_section(wstag221, __start_dw3data_wstag221, __stop_dw3data_wstag221,
                 __start_dw3bss_wstag221, __stop_dw3bss_wstag221);
    bind_section(wstag226, __start_dw3data_wstag226, __stop_dw3data_wstag226,
                 __start_dw3bss_wstag226, __stop_dw3bss_wstag226);
    bind_section(wstag231, __start_dw3data_wstag231, __stop_dw3data_wstag231,
                 __start_dw3bss_wstag231, __stop_dw3bss_wstag231);
    bind_section(wstag233, __start_dw3data_wstag233, __stop_dw3data_wstag233,
                 __start_dw3bss_wstag233, __stop_dw3bss_wstag233);
    bind_section(wstag236, __start_dw3data_wstag236, __stop_dw3data_wstag236,
                 __start_dw3bss_wstag236, __stop_dw3bss_wstag236);
    bind_section(wstag238, __start_dw3data_wstag238, __stop_dw3data_wstag238,
                 __start_dw3bss_wstag238, __stop_dw3bss_wstag238);
    bind_section(wstag241, __start_dw3data_wstag241, __stop_dw3data_wstag241,
                 __start_dw3bss_wstag241, __stop_dw3bss_wstag241);
    bind_section(wstag246, __start_dw3data_wstag246, __stop_dw3data_wstag246,
                 __start_dw3bss_wstag246, __stop_dw3bss_wstag246);
    bind_section(wstag251, __start_dw3data_wstag251, __stop_dw3data_wstag251,
                 __start_dw3bss_wstag251, __stop_dw3bss_wstag251);
    bind_section(wstag256, __start_dw3data_wstag256, __stop_dw3data_wstag256,
                 __start_dw3bss_wstag256, __stop_dw3bss_wstag256);
    bind_section(wstag261, __start_dw3data_wstag261, __stop_dw3data_wstag261,
                 __start_dw3bss_wstag261, __stop_dw3bss_wstag261);
}


void dw3_overlay_bind_p38_batch3a_sections(
    Dw3OverlayDesc *wstag271,
    Dw3OverlayDesc *wstag276,
    Dw3OverlayDesc *wstag281,
    Dw3OverlayDesc *wstag286,
    Dw3OverlayDesc *wstag291,
    Dw3OverlayDesc *wstag296,
    Dw3OverlayDesc *wstag301,
    Dw3OverlayDesc *wstag306,
    Dw3OverlayDesc *wstag311,
    Dw3OverlayDesc *wstag316,
    Dw3OverlayDesc *wstag321,
    Dw3OverlayDesc *wstag326) {
    bind_section(wstag271, __start_dw3data_wstag271, __stop_dw3data_wstag271,
                 __start_dw3bss_wstag271, __stop_dw3bss_wstag271);
    bind_section(wstag276, __start_dw3data_wstag276, __stop_dw3data_wstag276,
                 __start_dw3bss_wstag276, __stop_dw3bss_wstag276);
    bind_section(wstag281, __start_dw3data_wstag281, __stop_dw3data_wstag281,
                 __start_dw3bss_wstag281, __stop_dw3bss_wstag281);
    bind_section(wstag286, __start_dw3data_wstag286, __stop_dw3data_wstag286,
                 __start_dw3bss_wstag286, __stop_dw3bss_wstag286);
    bind_section(wstag291, __start_dw3data_wstag291, __stop_dw3data_wstag291,
                 __start_dw3bss_wstag291, __stop_dw3bss_wstag291);
    bind_section(wstag296, __start_dw3data_wstag296, __stop_dw3data_wstag296,
                 __start_dw3bss_wstag296, __stop_dw3bss_wstag296);
    bind_section(wstag301, __start_dw3data_wstag301, __stop_dw3data_wstag301,
                 __start_dw3bss_wstag301, __stop_dw3bss_wstag301);
    bind_section(wstag306, __start_dw3data_wstag306, __stop_dw3data_wstag306,
                 __start_dw3bss_wstag306, __stop_dw3bss_wstag306);
    bind_section(wstag311, __start_dw3data_wstag311, __stop_dw3data_wstag311,
                 __start_dw3bss_wstag311, __stop_dw3bss_wstag311);
    bind_section(wstag316, __start_dw3data_wstag316, __stop_dw3data_wstag316,
                 __start_dw3bss_wstag316, __stop_dw3bss_wstag316);
    bind_section(wstag321, __start_dw3data_wstag321, __stop_dw3data_wstag321,
                 __start_dw3bss_wstag321, __stop_dw3bss_wstag321);
    bind_section(wstag326, __start_dw3data_wstag326, __stop_dw3data_wstag326,
                 __start_dw3bss_wstag326, __stop_dw3bss_wstag326);
}

void dw3_overlay_bind_p38_batch3b_sections(
    Dw3OverlayDesc *wstag330,
    Dw3OverlayDesc *wstag335,
    Dw3OverlayDesc *wstag340,
    Dw3OverlayDesc *wstag345,
    Dw3OverlayDesc *wstag350,
    Dw3OverlayDesc *wstag355,
    Dw3OverlayDesc *wstag360,
    Dw3OverlayDesc *wstag365,
    Dw3OverlayDesc *wstag370,
    Dw3OverlayDesc *wstag375,
    Dw3OverlayDesc *wstag380,
    Dw3OverlayDesc *wstag385,
    Dw3OverlayDesc *wstag395,
    Dw3OverlayDesc *wstag400,
    Dw3OverlayDesc *wstag405,
    Dw3OverlayDesc *wstag410,
    Dw3OverlayDesc *wstag415,
    Dw3OverlayDesc *wstag420) {
    bind_section(wstag330, __start_dw3data_wstag330, __stop_dw3data_wstag330,
                 __start_dw3bss_wstag330, __stop_dw3bss_wstag330);
    bind_section(wstag335, __start_dw3data_wstag335, __stop_dw3data_wstag335,
                 __start_dw3bss_wstag335, __stop_dw3bss_wstag335);
    bind_section(wstag340, __start_dw3data_wstag340, __stop_dw3data_wstag340,
                 __start_dw3bss_wstag340, __stop_dw3bss_wstag340);
    bind_section(wstag345, __start_dw3data_wstag345, __stop_dw3data_wstag345,
                 __start_dw3bss_wstag345, __stop_dw3bss_wstag345);
    bind_section(wstag350, __start_dw3data_wstag350, __stop_dw3data_wstag350,
                 __start_dw3bss_wstag350, __stop_dw3bss_wstag350);
    bind_section(wstag355, __start_dw3data_wstag355, __stop_dw3data_wstag355,
                 __start_dw3bss_wstag355, __stop_dw3bss_wstag355);
    bind_section(wstag360, __start_dw3data_wstag360, __stop_dw3data_wstag360,
                 __start_dw3bss_wstag360, __stop_dw3bss_wstag360);
    bind_section(wstag365, __start_dw3data_wstag365, __stop_dw3data_wstag365,
                 __start_dw3bss_wstag365, __stop_dw3bss_wstag365);
    bind_section(wstag370, __start_dw3data_wstag370, __stop_dw3data_wstag370,
                 __start_dw3bss_wstag370, __stop_dw3bss_wstag370);
    bind_section(wstag375, __start_dw3data_wstag375, __stop_dw3data_wstag375,
                 __start_dw3bss_wstag375, __stop_dw3bss_wstag375);
    bind_section(wstag380, __start_dw3data_wstag380, __stop_dw3data_wstag380,
                 __start_dw3bss_wstag380, __stop_dw3bss_wstag380);
    bind_section(wstag385, __start_dw3data_wstag385, __stop_dw3data_wstag385,
                 __start_dw3bss_wstag385, __stop_dw3bss_wstag385);
    bind_section(wstag395, __start_dw3data_wstag395, __stop_dw3data_wstag395,
                 __start_dw3bss_wstag395, __stop_dw3bss_wstag395);
    bind_section(wstag400, __start_dw3data_wstag400, __stop_dw3data_wstag400,
                 __start_dw3bss_wstag400, __stop_dw3bss_wstag400);
    bind_section(wstag405, __start_dw3data_wstag405, __stop_dw3data_wstag405,
                 __start_dw3bss_wstag405, __stop_dw3bss_wstag405);
    bind_section(wstag410, __start_dw3data_wstag410, __stop_dw3data_wstag410,
                 __start_dw3bss_wstag410, __stop_dw3bss_wstag410);
    bind_section(wstag415, __start_dw3data_wstag415, __stop_dw3data_wstag415,
                 __start_dw3bss_wstag415, __stop_dw3bss_wstag415);
    bind_section(wstag420, __start_dw3data_wstag420, __stop_dw3data_wstag420,
                 __start_dw3bss_wstag420, __stop_dw3bss_wstag420);
}

void dw3_overlay_bind_p38_batch3c_sections(
    Dw3OverlayDesc *wstag425,
    Dw3OverlayDesc *wstag430,
    Dw3OverlayDesc *wstag435,
    Dw3OverlayDesc *wstag440,
    Dw3OverlayDesc *wstag445,
    Dw3OverlayDesc *wstag450,
    Dw3OverlayDesc *wstag455,
    Dw3OverlayDesc *wstag475,
    Dw3OverlayDesc *wstag465,
    Dw3OverlayDesc *wstag470) {
    bind_section(wstag425, __start_dw3data_wstag425, __stop_dw3data_wstag425,
                 __start_dw3bss_wstag425, __stop_dw3bss_wstag425);
    bind_section(wstag430, __start_dw3data_wstag430, __stop_dw3data_wstag430,
                 __start_dw3bss_wstag430, __stop_dw3bss_wstag430);
    bind_section(wstag435, __start_dw3data_wstag435, __stop_dw3data_wstag435,
                 __start_dw3bss_wstag435, __stop_dw3bss_wstag435);
    bind_section(wstag440, __start_dw3data_wstag440, __stop_dw3data_wstag440,
                 __start_dw3bss_wstag440, __stop_dw3bss_wstag440);
    bind_section(wstag445, __start_dw3data_wstag445, __stop_dw3data_wstag445,
                 __start_dw3bss_wstag445, __stop_dw3bss_wstag445);
    bind_section(wstag450, __start_dw3data_wstag450, __stop_dw3data_wstag450,
                 __start_dw3bss_wstag450, __stop_dw3bss_wstag450);
    bind_section(wstag455, __start_dw3data_wstag455, __stop_dw3data_wstag455,
                 __start_dw3bss_wstag455, __stop_dw3bss_wstag455);
    bind_section(wstag475, __start_dw3data_wstag475, __stop_dw3data_wstag475,
                 __start_dw3bss_wstag475, __stop_dw3bss_wstag475);
    bind_section(wstag465, __start_dw3data_wstag465, __stop_dw3data_wstag465,
                 __start_dw3bss_wstag465, __stop_dw3bss_wstag465);
    bind_section(wstag470, __start_dw3data_wstag470, __stop_dw3data_wstag470,
                 __start_dw3bss_wstag470, __stop_dw3bss_wstag470);
}
void dw3_overlay_bind_p38_batch4a_sections(
    Dw3OverlayDesc *wstag480,
    Dw3OverlayDesc *wstag485,
    Dw3OverlayDesc *wstag490,
    Dw3OverlayDesc *wstag495,
    Dw3OverlayDesc *wstag500,
    Dw3OverlayDesc *wstag505,
    Dw3OverlayDesc *wstag520,
    Dw3OverlayDesc *wstag525,
    Dw3OverlayDesc *wstag530,
    Dw3OverlayDesc *wstag535,
    Dw3OverlayDesc *wstag537,
    Dw3OverlayDesc *wstag540,
    Dw3OverlayDesc *wstag545) {
    bind_section(wstag480, __start_dw3data_wstag480, __stop_dw3data_wstag480,
                 __start_dw3bss_wstag480, __stop_dw3bss_wstag480);
    bind_section(wstag485, __start_dw3data_wstag485, __stop_dw3data_wstag485,
                 __start_dw3bss_wstag485, __stop_dw3bss_wstag485);
    bind_section(wstag490, __start_dw3data_wstag490, __stop_dw3data_wstag490,
                 __start_dw3bss_wstag490, __stop_dw3bss_wstag490);
    bind_section(wstag495, __start_dw3data_wstag495, __stop_dw3data_wstag495,
                 __start_dw3bss_wstag495, __stop_dw3bss_wstag495);
    bind_section(wstag500, __start_dw3data_wstag500, __stop_dw3data_wstag500,
                 __start_dw3bss_wstag500, __stop_dw3bss_wstag500);
    bind_section(wstag505, __start_dw3data_wstag505, __stop_dw3data_wstag505,
                 __start_dw3bss_wstag505, __stop_dw3bss_wstag505);
    bind_section(wstag520, __start_dw3data_wstag520, __stop_dw3data_wstag520,
                 __start_dw3bss_wstag520, __stop_dw3bss_wstag520);
    bind_section(wstag525, __start_dw3data_wstag525, __stop_dw3data_wstag525,
                 __start_dw3bss_wstag525, __stop_dw3bss_wstag525);
    bind_section(wstag530, __start_dw3data_wstag530, __stop_dw3data_wstag530,
                 __start_dw3bss_wstag530, __stop_dw3bss_wstag530);
    bind_section(wstag535, __start_dw3data_wstag535, __stop_dw3data_wstag535,
                 __start_dw3bss_wstag535, __stop_dw3bss_wstag535);
    bind_section(wstag537, __start_dw3data_wstag537, __stop_dw3data_wstag537,
                 __start_dw3bss_wstag537, __stop_dw3bss_wstag537);
    bind_section(wstag540, __start_dw3data_wstag540, __stop_dw3data_wstag540,
                 __start_dw3bss_wstag540, __stop_dw3bss_wstag540);
    bind_section(wstag545, __start_dw3data_wstag545, __stop_dw3data_wstag545,
                 __start_dw3bss_wstag545, __stop_dw3bss_wstag545);
}

void dw3_overlay_bind_p38_batch4b_sections(
    Dw3OverlayDesc *wstag550,
    Dw3OverlayDesc *wstag555,
    Dw3OverlayDesc *wstag560,
    Dw3OverlayDesc *wstag565,
    Dw3OverlayDesc *wstag570,
    Dw3OverlayDesc *wstag575,
    Dw3OverlayDesc *wstag580,
    Dw3OverlayDesc *wstag585,
    Dw3OverlayDesc *wstag590,
    Dw3OverlayDesc *wstag595,
    Dw3OverlayDesc *wstag600,
    Dw3OverlayDesc *wstag605,
    Dw3OverlayDesc *wstag610,
    Dw3OverlayDesc *wstag615,
    Dw3OverlayDesc *wstag620) {
    bind_section(wstag550, __start_dw3data_wstag550, __stop_dw3data_wstag550,
                 __start_dw3bss_wstag550, __stop_dw3bss_wstag550);
    bind_section(wstag555, __start_dw3data_wstag555, __stop_dw3data_wstag555,
                 __start_dw3bss_wstag555, __stop_dw3bss_wstag555);
    bind_section(wstag560, __start_dw3data_wstag560, __stop_dw3data_wstag560,
                 __start_dw3bss_wstag560, __stop_dw3bss_wstag560);
    bind_section(wstag565, __start_dw3data_wstag565, __stop_dw3data_wstag565,
                 __start_dw3bss_wstag565, __stop_dw3bss_wstag565);
    bind_section(wstag570, __start_dw3data_wstag570, __stop_dw3data_wstag570,
                 __start_dw3bss_wstag570, __stop_dw3bss_wstag570);
    bind_section(wstag575, __start_dw3data_wstag575, __stop_dw3data_wstag575,
                 __start_dw3bss_wstag575, __stop_dw3bss_wstag575);
    bind_section(wstag580, __start_dw3data_wstag580, __stop_dw3data_wstag580,
                 __start_dw3bss_wstag580, __stop_dw3bss_wstag580);
    bind_section(wstag585, __start_dw3data_wstag585, __stop_dw3data_wstag585,
                 __start_dw3bss_wstag585, __stop_dw3bss_wstag585);
    bind_section(wstag590, __start_dw3data_wstag590, __stop_dw3data_wstag590,
                 __start_dw3bss_wstag590, __stop_dw3bss_wstag590);
    bind_section(wstag595, __start_dw3data_wstag595, __stop_dw3data_wstag595,
                 __start_dw3bss_wstag595, __stop_dw3bss_wstag595);
    bind_section(wstag600, __start_dw3data_wstag600, __stop_dw3data_wstag600,
                 __start_dw3bss_wstag600, __stop_dw3bss_wstag600);
    bind_section(wstag605, __start_dw3data_wstag605, __stop_dw3data_wstag605,
                 __start_dw3bss_wstag605, __stop_dw3bss_wstag605);
    bind_section(wstag610, __start_dw3data_wstag610, __stop_dw3data_wstag610,
                 __start_dw3bss_wstag610, __stop_dw3bss_wstag610);
    bind_section(wstag615, __start_dw3data_wstag615, __stop_dw3data_wstag615,
                 __start_dw3bss_wstag615, __stop_dw3bss_wstag615);
    bind_section(wstag620, __start_dw3data_wstag620, __stop_dw3data_wstag620,
                 __start_dw3bss_wstag620, __stop_dw3bss_wstag620);
}

void dw3_overlay_bind_p38_batch4c_sections(
    Dw3OverlayDesc *wstag625,
    Dw3OverlayDesc *wstag630,
    Dw3OverlayDesc *wstag635,
    Dw3OverlayDesc *wstag640,
    Dw3OverlayDesc *wstag645,
    Dw3OverlayDesc *wstag650,
    Dw3OverlayDesc *wstag655,
    Dw3OverlayDesc *wstag660,
    Dw3OverlayDesc *wstag675,
    Dw3OverlayDesc *wstag680,
    Dw3OverlayDesc *wstag685) {
    bind_section(wstag625, __start_dw3data_wstag625, __stop_dw3data_wstag625,
                 __start_dw3bss_wstag625, __stop_dw3bss_wstag625);
    bind_section(wstag630, __start_dw3data_wstag630, __stop_dw3data_wstag630,
                 __start_dw3bss_wstag630, __stop_dw3bss_wstag630);
    bind_section(wstag635, __start_dw3data_wstag635, __stop_dw3data_wstag635,
                 __start_dw3bss_wstag635, __stop_dw3bss_wstag635);
    bind_section(wstag640, __start_dw3data_wstag640, __stop_dw3data_wstag640,
                 __start_dw3bss_wstag640, __stop_dw3bss_wstag640);
    bind_section(wstag645, __start_dw3data_wstag645, __stop_dw3data_wstag645,
                 __start_dw3bss_wstag645, __stop_dw3bss_wstag645);
    bind_section(wstag650, __start_dw3data_wstag650, __stop_dw3data_wstag650,
                 __start_dw3bss_wstag650, __stop_dw3bss_wstag650);
    bind_section(wstag655, __start_dw3data_wstag655, __stop_dw3data_wstag655,
                 __start_dw3bss_wstag655, __stop_dw3bss_wstag655);
    bind_section(wstag660, __start_dw3data_wstag660, __stop_dw3data_wstag660,
                 __start_dw3bss_wstag660, __stop_dw3bss_wstag660);
    bind_section(wstag675, __start_dw3data_wstag675, __stop_dw3data_wstag675,
                 __start_dw3bss_wstag675, __stop_dw3bss_wstag675);
    bind_section(wstag680, __start_dw3data_wstag680, __stop_dw3data_wstag680,
                 __start_dw3bss_wstag680, __stop_dw3bss_wstag680);
    bind_section(wstag685, __start_dw3data_wstag685, __stop_dw3data_wstag685,
                 __start_dw3bss_wstag685, __stop_dw3bss_wstag685);
}

void dw3_overlay_bind_p38_batch4d_sections(
    Dw3OverlayDesc *wstag690,
    Dw3OverlayDesc *wstag695,
    Dw3OverlayDesc *wstag700,
    Dw3OverlayDesc *wstag705,
    Dw3OverlayDesc *wstag710,
    Dw3OverlayDesc *wstag715,
    Dw3OverlayDesc *wstag720,
    Dw3OverlayDesc *wstag725,
    Dw3OverlayDesc *wstag730,
    Dw3OverlayDesc *wstag735,
    Dw3OverlayDesc *wstag740,
    Dw3OverlayDesc *wstag745,
    Dw3OverlayDesc *wstag750,
    Dw3OverlayDesc *wstag755,
    Dw3OverlayDesc *wstag760) {
    bind_section(wstag690, __start_dw3data_wstag690, __stop_dw3data_wstag690,
                 __start_dw3bss_wstag690, __stop_dw3bss_wstag690);
    bind_section(wstag695, __start_dw3data_wstag695, __stop_dw3data_wstag695,
                 __start_dw3bss_wstag695, __stop_dw3bss_wstag695);
    bind_section(wstag700, __start_dw3data_wstag700, __stop_dw3data_wstag700,
                 __start_dw3bss_wstag700, __stop_dw3bss_wstag700);
    bind_section(wstag705, __start_dw3data_wstag705, __stop_dw3data_wstag705,
                 __start_dw3bss_wstag705, __stop_dw3bss_wstag705);
    bind_section(wstag710, __start_dw3data_wstag710, __stop_dw3data_wstag710,
                 __start_dw3bss_wstag710, __stop_dw3bss_wstag710);
    bind_section(wstag715, __start_dw3data_wstag715, __stop_dw3data_wstag715,
                 __start_dw3bss_wstag715, __stop_dw3bss_wstag715);
    bind_section(wstag720, __start_dw3data_wstag720, __stop_dw3data_wstag720,
                 __start_dw3bss_wstag720, __stop_dw3bss_wstag720);
    bind_section(wstag725, __start_dw3data_wstag725, __stop_dw3data_wstag725,
                 __start_dw3bss_wstag725, __stop_dw3bss_wstag725);
    bind_section(wstag730, __start_dw3data_wstag730, __stop_dw3data_wstag730,
                 __start_dw3bss_wstag730, __stop_dw3bss_wstag730);
    bind_section(wstag735, __start_dw3data_wstag735, __stop_dw3data_wstag735,
                 __start_dw3bss_wstag735, __stop_dw3bss_wstag735);
    bind_section(wstag740, __start_dw3data_wstag740, __stop_dw3data_wstag740,
                 __start_dw3bss_wstag740, __stop_dw3bss_wstag740);
    bind_section(wstag745, __start_dw3data_wstag745, __stop_dw3data_wstag745,
                 __start_dw3bss_wstag745, __stop_dw3bss_wstag745);
    bind_section(wstag750, __start_dw3data_wstag750, __stop_dw3data_wstag750,
                 __start_dw3bss_wstag750, __stop_dw3bss_wstag750);
    bind_section(wstag755, __start_dw3data_wstag755, __stop_dw3data_wstag755,
                 __start_dw3bss_wstag755, __stop_dw3bss_wstag755);
    bind_section(wstag760, __start_dw3data_wstag760, __stop_dw3data_wstag760,
                 __start_dw3bss_wstag760, __stop_dw3bss_wstag760);
}

void dw3_overlay_bind_battle_sections(Dw3OverlayDesc *fightstg, Dw3OverlayDesc *wfightmn,
                                      Dw3OverlayDesc *stfgtrep) {
    bind_section(fightstg, __start_dw3data_fightstg, __stop_dw3data_fightstg,
                 __start_dw3bss_fightstg, __stop_dw3bss_fightstg);
    bind_section(wfightmn, __start_dw3data_wfightmn, __stop_dw3data_wfightmn,
                 __start_dw3bss_wfightmn, __stop_dw3bss_wfightmn);
    bind_section(stfgtrep, __start_dw3data_stfgtrep, __stop_dw3data_stfgtrep,
                 __start_dw3bss_stfgtrep, __stop_dw3bss_stfgtrep);
}

void dw3_overlay_bind_menu_sections(Dw3OverlayDesc *ststatus, Dw3OverlayDesc *stitshop,
                                    Dw3OverlayDesc *stgtrain, Dw3OverlayDesc *stgdglab,
                                    Dw3OverlayDesc *stgmcard, Dw3OverlayDesc *stdgname) {
    bind_section(ststatus, __start_dw3data_ststatus, __stop_dw3data_ststatus,
                 __start_dw3bss_ststatus, __stop_dw3bss_ststatus);
    bind_section(stitshop, __start_dw3data_stitshop, __stop_dw3data_stitshop,
                 __start_dw3bss_stitshop, __stop_dw3bss_stitshop);
    bind_section(stgtrain, __start_dw3data_stgtrain, __stop_dw3data_stgtrain,
                 __start_dw3bss_stgtrain, __stop_dw3bss_stgtrain);
    bind_section(stgdglab, __start_dw3data_stgdglab, __stop_dw3data_stgdglab,
                 __start_dw3bss_stgdglab, __stop_dw3bss_stgdglab);
    bind_section(stgmcard, __start_dw3data_stgmcard, __stop_dw3data_stgmcard,
                 __start_dw3bss_stgmcard, __stop_dw3bss_stgmcard);
    bind_section(stdgname, __start_dw3data_stdgname, __stop_dw3data_stdgname,
                 __start_dw3bss_stdgname, __stop_dw3bss_stdgname);
}

void dw3_overlay_bind_cardgame_sections(Dw3OverlayDesc *cardgame, Dw3OverlayDesc *stcrddek,
                                        Dw3OverlayDesc *stcrdshp, Dw3OverlayDesc *stcrdabm) {
    bind_section(cardgame, __start_dw3data_cardgame, __stop_dw3data_cardgame,
                 __start_dw3bss_cardgame, __stop_dw3bss_cardgame);
    bind_section(stcrddek, __start_dw3data_stcrddek, __stop_dw3data_stcrddek,
                 __start_dw3bss_stcrddek, __stop_dw3bss_stcrddek);
    bind_section(stcrdshp, __start_dw3data_stcrdshp, __stop_dw3data_stcrdshp,
                 __start_dw3bss_stcrdshp, __stop_dw3bss_stcrdshp);
    bind_section(stcrdabm, __start_dw3data_stcrdabm, __stop_dw3data_stcrdabm,
                 __start_dw3bss_stcrdabm, __stop_dw3bss_stcrdabm);
}

void dw3_overlay_bind_stagslct_sections(Dw3OverlayDesc *stagslct) {
    bind_section(stagslct, __start_dw3data_stagslct, __stop_dw3data_stagslct,
                 __start_dw3bss_stagslct, __stop_dw3bss_stagslct);
}

