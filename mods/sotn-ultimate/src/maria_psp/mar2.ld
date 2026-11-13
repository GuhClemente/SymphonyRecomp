/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Saturn port mod: BIN/MARIA2.BIN (mods/sotn-ultimate/tools/build_mar2.sh); player.ld plus
   the BSS bounds the glue clears (the loaded file has no BSS) */
/* keep the entry table, which nothing in the overlay references (as
   upstream's mods/mariapsp-forpsx/symexport.txt does) */
EXTERN(MARIA_override);
SECTIONS {
    . = 0x8013C000;
    .data : {
        *(.data*)
    }
    .rodata : {
        *(.rodata*)
    }
    .text : {
        *(.text*)
    }
    end = .;
    .bss : {
        mar2_bss_start = .;
        *(.sbss*) *(.bss*) *(COMMON)
        . = ALIGN(4);
        mar2_bss_end = .;
    }
}
