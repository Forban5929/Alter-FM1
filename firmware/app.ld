/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA memory map.
 * Code + constants execute in place from XIP (app.bin offset 0 = 0x02000120).
 * RAMTEXT (ALTER v0.61, after JIANT 0.6.78: at the top of the pool, was 0x01C00000..0x01C06000 + 8 KiB of debug room):
 * .ram_text (code that must run while the flash is busy: the flash driver), then .c1_text in it (the second core's
 * waiting loop, bsp/chip/fm1_cpu1.h: an idle core 1 never fetches from the flash), then .hot_text (core 0's hottest
 * audio code, tools/build.py HOT_FNS). One window (_rt_start .. _rt_end: the PC limits, main's copy). A call reaches
 * +-4 MiB (pi32v2): from here it reaches the whole XIP image (build.py checks it); from 0x01C00000 it reached none of it.
 * RAM  : .data/.bss (72 KiB): what the audio and key-scan interrupts touch, small variables
 * POOL : big zero-initialised buffers (FX lines, canvases, the parts' engine memory, the main loop's work
 *        buffers), section .pool. The same SRAM: the RAM / POOL boundary only splits the budget (1.5: 96 / 336
 *        KiB -> 72 / 360 KiB); tools/build.py reads both lengths from here
 * STACKS with 256-byte guard bands; NOINIT survives resets and UBOOT entry.
 * 0x01C7FD50.. is reserved: boot_info, boot-state mailbox, vector table. */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8DFBC
    RAM    (rwx) : ORIGIN = 0x01C00000, LENGTH = 72K
    POOL   (rw)  : ORIGIN = 0x01C12000, LENGTH = 0x5A000
    RAMTEXT (rwx): ORIGIN = 0x01C6C000, LENGTH = 0x8000
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x3D50
}
SECTIONS
{
    /* ALTER (v0.60, after JIANT 0.9.12): what an interrupt reaches first, together, in name order (tools/build.py
     * audio_sections): the UI cannot scatter the sound's code through the flash cache */
    .text : { KEEP(*(.text.crt0)) *(SORT_BY_NAME(.text.a.*)) *(SORT_BY_NAME(.rodata.a.*)) *(.text*) *(.rodata*)
              . = ALIGN(4); _etext = .; } > XIP
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) . = ALIGN(4);
                  _c1_start = .; KEEP(*(.c1_text*)) . = ALIGN(4); _c1_end = .;
                  _hot_start = .; KEEP(*(.hot_text*)) . = ALIGN(4); _hot_end = .; _rt_end = .; } > RAMTEXT AT > XIP
    _rt_load = LOADADDR(.ram_text);
    .data : { . = ALIGN(4); _data_start = .; *(.data*) *(.sdata*) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); _bss_end = .; } > RAM
    .pool (NOLOAD) : { _pool_start = .; *(.pool*) . = ALIGN(4); _pool_end = .; } > POOL
    .noinit (NOLOAD) : { *(.noinit*) } > NOINIT
    _guard0 = 0x01C74000; _ustack_lo = 0x01C74100; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    /DISCARD/ : { *(.comment) *(.note*) }
}
