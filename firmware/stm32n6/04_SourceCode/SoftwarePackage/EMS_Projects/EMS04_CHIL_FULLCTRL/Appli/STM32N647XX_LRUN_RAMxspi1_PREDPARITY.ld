/*
******************************************************************************
**
** @file        : STM32N647XX_LRUN_RAMxspi1_PREDPARITY.ld
**
** @author      : GPM Application Team
**
** @brief       : Linker script for STM32N647XX Device from STM32N6 series
**                      2048 KBytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed as is, without any warranty
**                of any kind.
**
******************************************************************************
** @attention
**
** Copyright (c) 2023 STMicroelectronics.
** All rights reserved.
**
** This software is licensed under terms that can be found in the LICENSE file
** in the root directory of this software component.
** If no LICENSE file comes with this software, it is provided AS-IS.
**
******************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)


/* Memories definition */
MEMORY
{
  RAM    (xrw) : ORIGIN = 0x34000400, LENGTH = 2047K
  EXTRAM (rw)  : ORIGIN = 0x90000000, LENGTH = 32M
  /* EMS04-S3-C：HyperRAM 应用数据区——避开 LCD 帧缓冲 g_ltdc_lcd_framebuf
   *（rgblcd.h:90 硬编码 0x90000000，480x800x2 = 768KB，ems_lcd_poll 1Hz 整屏
   * 重写）。板上实证（S3C_BOARD_PREFLIGHT_FAIL.md）：s_svc@0x900000A0 与 fb
   * 重叠被像素覆盖 → svc 指针读回垃圾 → HardFault/静默。应用数据统一从
   * 0x90100000 起（fb 顶 0x900BB800 之上留余量）。 */
  HYPERRAM_APP (rw) : ORIGIN = 0x90100000, LENGTH = 31M
}
/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM); /* end of "RAM" Ram type memory */
_sstack = _estack - _Min_Stack_Size;

_Min_Heap_Size = 0x200;      /* required amount of heap  */
_Min_Stack_Size = 0x10000; /* EMS01: 64KB（T-507 STKOF 修复：INFER_TEST 栈帧 4.9KB + TICK 控制路径 ~7.5KB + REF workclf ~20KB；vendor 默认 0x800 不足） */

/* Sections */
SECTIONS
{
  /* The startup code into "RAM" Ram type memory */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >RAM

  /* The program code and other data into "RAM" Ram type memory */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >RAM

  /* Constant data into "RAM" Ram type memory */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >RAM

  .ARM.extab   : {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >RAM

  .ARM : {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >RAM

  .preinit_array     :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >RAM

  .init_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >RAM

  .fini_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >RAM

  /* Used by the startup to initialize data */
  .gnu.sgstubs ALIGN(32) :
  {
    _sNSCVeneer = .;
    *(.gnu.sgstubs*)   /* Secure Gateway stubs */
    . = ALIGN(32);
    FILL(0x00);
  } >RAM
  _eNSCVeneer = .;

  /* Uninitialized data section into "RAM" Ram type memory */
  _sidata = LOADADDR(.data);

  /* Initialized data sections into "RAM" Ram type memory */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);

  } >RAM AT>RAM

  /* Initialized TLS data section */
  .tdata : ALIGN(4)
  {
    *(.tdata .tdata.* .gnu.linkonce.td.*)
    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
    PROVIDE(__data_end = .);
    PROVIDE(__tdata_end = .);
  } >RAM AT>RAM

  PROVIDE( __tdata_start = ADDR(.tdata) );
  PROVIDE( __tdata_size = __tdata_end - __tdata_start );

  PROVIDE( __data_start = ADDR(.data) );
  PROVIDE( __data_size = __data_end - __data_start );

  PROVIDE( __tdata_source = LOADADDR(.tdata) );
  PROVIDE( __tdata_source_end = LOADADDR(.tdata) + SIZEOF(.tdata) );
  PROVIDE( __tdata_source_size = __tdata_source_end - __tdata_source );

  PROVIDE( __data_source = LOADADDR(.data) );
  PROVIDE( __data_source_end = __tdata_source_end );
  PROVIDE( __data_source_size = __data_source_end - __data_source );


  .noncacheable :
  {
    . = ALIGN(8);
    __snoncacheable = .;/* create symbol for start of section */
    KEEP(*(.noncacheable))
    . = ALIGN(8);
    __enoncacheable = .;  /* create symbol for end of section */
  } > RAM

  /* Uninitialized TLS data section */
  .tbss (NOLOAD) : ALIGN(4)
  {
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.tbss .tbss.*)
    . = ALIGN(4);
    PROVIDE( __tbss_end = . );
  } >RAM

  PROVIDE( __tbss_start = ADDR(.tbss) );
  PROVIDE( __tbss_size = __tbss_end - __tbss_start );
  PROVIDE( __tbss_offset = ADDR(.tbss) - ADDR(.tdata) );

  PROVIDE( __tls_base = __tdata_start );
  PROVIDE( __tls_end = __tbss_end );
  PROVIDE( __tls_size = __tls_end - __tls_base );
  PROVIDE( __tls_align = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss)) );
  PROVIDE( __tls_size_align = (__tls_size + __tls_align - 1) & ~(__tls_align - 1) );
  PROVIDE( __arm32_tls_tcb_offset = MAX(8, __tls_align) );
  PROVIDE( __arm64_tls_tcb_offset = MAX(16, __tls_align) );

  .bss (NOLOAD) : ALIGN(4)
  {
    /* This is used by the startup in order to initialize the .bss section */
    /* EMS04-S3-C：ems04_fp_board.o 的 BSS（s_svc 485,832 B + 胶水状态）排除在外，
     * 改由下方 .hyperram_bss 承载（SRAM1-2 2047K 窗在 REATT SPLIT + FP 同载下
     * 溢出 278 KB，实测见 build/PredParity 首次链接；s_svc > AXISRAM3 池 457,720 B
     * 且 NPU arena 整窗占用 SRAM3-6，HyperRAM 为唯一物理可行目的地）。
     * 依赖链：main() SysInit 的 HyperRAM init（EMS04_PRED_PARITY 条件）先于
     * ems04_fp_board_init()（memset 全量初始化，ems04_fp_svc.c:87），启动代码的
     * [_sbss,_ebss) 线性清零不触本段。 */
    *(EXCLUDE_FILE(*ems04_fp_board*) .bss)
    *(EXCLUDE_FILE(*ems04_fp_board*) .bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
      PROVIDE( __bss_end = .);
  } >RAM
  PROVIDE( __non_tls_bss_start = ADDR(.bss) );

  PROVIDE( __bss_start = __tbss_start );
  PROVIDE( __bss_size = __bss_end - __bss_start );


  /* User_heap_stack section, used to check that there is enough "RAM" Ram  type memory left */
  ._user_heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    libc.a:* ( * )
    libm.a:* ( * )
    libgcc.a:* ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }

  /* External RAM: generated by atk_stm32n6_cmake_prepare.py */
  .EXTRAM (NOLOAD) :
  {
    . = ALIGN(8);
    *(.EXTRAM)
    . = ALIGN(8);
  } >EXTRAM

  /* EMS04-S3-C：FP 服务大实例 → HyperRAM（XSPI1 @0x90000000）。
   * NOLOAD（filesz=0）：启动代码不清零、调试器加载器跳过（mcp-contract
   * noload_zero_filesz_segment_outside_whitelist_is_skipped 同款语义）；
   * 运行期首次清零/初始化由 ems04_fp_board_init() → ems04_fp_svc_init()
   * 的 memset 全量完成（前置核查：ems04_fp_svc.c:87-93，2026-09-26）。
   * 摆放位置在 .bss 之后，避免插入启动代码的线性清零区间 [_sbss,_ebss)。 */
  .hyperram_bss (NOLOAD) : ALIGN(32)
  {
    __hyperram_bss_start__ = .;
    *ems04_fp_board* (.bss)
    *ems04_fp_board* (.bss*)
    . = ALIGN(32);
    __hyperram_bss_end__ = .;
  } >HYPERRAM_APP

  /* ATK_RAM_ORIGIN_ASSERT */
  ASSERT(ORIGIN(RAM) == 0x34000400,
         "ATK: RAM origin must be 0x34000400")

  /* EMS04-S3-C：HyperRAM 实例段越界即链接失败（fail-closed，不静默） */
  ASSERT(__hyperram_bss_end__ <= (ORIGIN(HYPERRAM_APP) + LENGTH(HYPERRAM_APP)),
         "EMS04 S3-C: .hyperram_bss overflow HyperRAM app window")
  ASSERT(__hyperram_bss_start__ >= ORIGIN(HYPERRAM_APP),
         "EMS04 S3-C: .hyperram_bss below HyperRAM app origin")

}
