/* Relocatable merge of writable sections into one named pair (P3.2).
 * Final link provides __start_dw3data_<name> / __stop_* via GNU ld orphans.
 */
SECTIONS {
  dw3data___DW3_OVL__ : {
    *(.data .data.* .sdata .sdata.*)
    *(.data.rel .data.rel.* .data.rel.ro .data.rel.ro.*)
  }
  dw3bss___DW3_OVL__ : {
    *(.bss .bss.* .sbss .sbss.*)
    *(COMMON)
  }
}
