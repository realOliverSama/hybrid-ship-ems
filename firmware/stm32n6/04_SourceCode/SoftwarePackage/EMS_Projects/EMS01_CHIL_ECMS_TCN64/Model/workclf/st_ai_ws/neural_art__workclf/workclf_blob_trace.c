#include "ll_aton_NN_interface.h"
#include "ll_aton.h"
#include "ll_aton_ec_trace.h"

#if 0
// Workaround: the tracer does not know the target at this moment
// and cannot call the functions since are used in static code
#define ATON_LIB_PHYSICAL_TO_VIRTUAL_ADDR(address) LL_Address_Physical2Virtual(address)
#define ATON_LIB_VIRTUAL_TO_PHYSICAL_ADDR(address) LL_Address_Virtual2Physical(address)
#else
#define ATON_LIB_PHYSICAL_TO_VIRTUAL_ADDR(address) (address)
#define ATON_LIB_VIRTUAL_TO_PHYSICAL_ADDR(address) (address)
#endif

#define _mem_pool__user_io_input_0_workclf 1
#define _mem_pool__user_io_input_1_workclf 3
#define _mem_pool__user_io_output_0_workclf 5

// MCU cache line size: 32 (bytes)
// NPU cache line size: 64 (bytes)
// MCU+NPU cache line size equal to 64 bytes (power of 2 not less than 8)
unsigned int cache_line_size = 64;

mpool_reloc_info_t mpool_reloc_info[] = {
  {"AXISRAM6", "_mem_pool_AXISRAM6_workclf", 0x34350000, 1, 0},
  {"AXISRAM5", "_mem_pool_AXISRAM5_workclf", 0x342e0000, 1, 0},
  {"AXISRAM4", "_mem_pool_AXISRAM4_workclf", 0x34270000, 1, 0},
  {"AXISRAM3", "_mem_pool_AXISRAM3_workclf", 0x34200000, 1, 0},
  {"xSPI1", "_mem_pool_xSPI1_workclf", 0x90000000, 1, 0},
  {"xSPI2", "_mem_pool_xSPI2_workclf", 0x70400000, 1, 0},
  {"AXISRAM3_AXISRAM4_AXISRAM5_AXISRAM6", "_mem_pool_AXISRAM3_AXISRAM4_AXISRAM5_AXISRAM6_workclf", 0x34200000, 1, 0},
  {"_user_io_input_0", "_mem_pool__user_io_input_0_workclf", 0x1, 0, 1},
  {"_user_io_input_1", "_mem_pool__user_io_input_1_workclf", 0x3, 0, 1},
  {"_user_io_output_0", "_mem_pool__user_io_output_0_workclf", 0x5, 0, 1},
  {NULL, NULL, 0, 0, 0}
};


static const uint64_t __ec_params_memory_pool_base[129] = {
0xFF87B378FF83B1B5ULL,0xFF7E1C7AFF7E16FDULL,0xFF838C22FF7E4CADULL,0xFF804F4AFF82B7F8ULL,0xFF8541DCFF7F1EC6ULL,0xFF7D92F6FF80F9F8ULL,0xFF7EF4D5FF803076ULL,0xFF7F2FDBFF81404BULL,
0xFF81831DFF8146C4ULL,0xFF81821EFF7F2D28ULL,0xFF81E8D0FF80A15AULL,0xFF7F42FBFF81EF84ULL,0xFF7D5F1DFF8020D2ULL,0xFF8876AEFF829116ULL,0xFF81263EFF7FDFCFULL,0xFF85C5AFFF8078C5ULL,
0xFF831BF2FF7FE254ULL,0xFF83790BFF837E61ULL,0xFF834AFFFF8197ABULL,0xFF817511FF7C36A0ULL,0xFF7F4408FF81B204ULL,0xFF7C6B21FF7F3BE7ULL,0xFF7F1F35FF800000ULL,0xFF7DEC34FF81C3A7ULL,
0xFF7F7DA1FF81E8B7ULL,0xFF87C843FF7E93B6ULL,0xFF8093D8FF7E54CBULL,0xFF85C057FF828320ULL,0xFF7F0794FF82712FULL,0xFF800000FF80BA40ULL,0xFF7BCBAEFF800D56ULL,0xFF7F8D0CFF81B1BCULL,
0xFE665C0FFE1377E9ULL,0xFE3DE7E1FE5A7ED7ULL,0xFE01225DFE3E9B33ULL,0xFE167243FE032047ULL,0xFE108CB7FE338EB8ULL,0xFE0A3AD1FDA250C4ULL,0xFDE8406DFDF2C407ULL,0xFE05999DFE8E6D2BULL,
0xFE127DA2FE0B18B4ULL,0xFE0195CFFE3308ADULL,0xFE034112FDC3F0F0ULL,0xFE05F665FE050272ULL,0xFDE9AE87FD8457DDULL,0xFDF973E5FE171916ULL,0xFE5E1908FE0EF2FBULL,0xFE4C601BFE452771ULL,
0xFDFCBC4EFE17EB21ULL,0xFDF786E2FDFDEBCDULL,0xFDFC041BFE0F7223ULL,0xFDECECEEFE0CCEC2ULL,0xFE09912EFE060BEDULL,0xFE0B5582FDFCDA70ULL,0xFDFAB760FE0579F5ULL,0xFE0431AEFDFE8CBEULL,
0xFE034139FE193619ULL,0xFE11D54DFE126A1DULL,0xFE096C95FE0981B1ULL,0xFDF7F499FE05642EULL,0xFDE6898AFE0BB6E1ULL,0xFE09CF96FE0718AEULL,0xFDFE209FFDFD5E69ULL,0xFE007E0EFE163563ULL,
0x13D626BC22361F86ULL,0x1CD50BB211971A7CULL,0x137C0C581C0A0D02ULL,0x0B7C116E0B8E113BULL,0x19A20C73258A1D0CULL,0x1383175210891821ULL,0x242630E616272865ULL,0x2EBA196D1AE71313ULL,
0x1047158D0E5C1A5FULL,0x0DD10F7B1C651B11ULL,0x45331DE20AFA2E4CULL,0x1C2D14D30C2709DAULL,0x315820BF1D6113CCULL,0x338C241B31483EB6ULL,0x0AC6171A0B3A40F4ULL,0x0B260FF427F51557ULL,
0xF7C61F86F7B15B94ULL,0xF8F6FE67F6094256ULL,0xF766C1DCF7C4ECD7ULL,0xF7F21D8EF992CC9EULL,0xF64B52A5F6DEF815ULL,0xF7D820D6F7CEEEAFULL,0xF6F782A1F92668C3ULL,0xF7E1CF09F85D3E93ULL,
0x13F610AA139E1567ULL,0x10330EE417F32E7DULL,0x124C0EE2331F1218ULL,0x2F320EBE36961E26ULL,0x0FFE15A41182433CULL,0x1593129611A219E0ULL,0x0D65113C10E914B5ULL,0x142E1C45110611D7ULL,
0x140B2279126E1EEDULL,0x262B1C13110B20F1ULL,0x3C1933C317861DCEULL,0x29A5291C4C9C180BULL,0x66F422BA287663A1ULL,0x13451DB41C932DD6ULL,0x200825943013247DULL,0x2D5344C911502731ULL,
0xF7F09032F804B0B5ULL,0xF80479C3F843C1F9ULL,0xF85FC452F813EFE4ULL,0xF8AC9EF4F7DBDDBCULL,0x0FEF34B12F091D18ULL,0x228235335E4E3595ULL,0x28FE30D42999440EULL,0x225017FF270C28DBULL,
0x0493E634048A27C6ULL,0x0489B9790493CEF4ULL,0x048FCEF50485D29BULL,0x00000000048C0050ULL,0x436665704D7D58F1ULL,0x35CB62EF4432702CULL,0x41E92BD1153D4BC2ULL,0x00003A7F4E771AA7ULL,
0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
0x0000000000000000ULL,
};

void trace_ec__ec_blob_workclf_3(void) {
  ec_trace_start_blob("_ec_blob_workclf_3");
  ec_trace_start_epoch(3);
  {
  }
  {
  }
  ec_trace_end_epoch(3);
  ec_trace_end_blob("_ec_blob_workclf_3");
}

void trace_ec__ec_blob_workclf_5(void) {
  ec_trace_start_blob("_ec_blob_workclf_5");
  ec_trace_start_epoch(5);
  {
    /* Unit= 10 [CONV_ACC_V2 0] */
    /* kind=Conv node=Gemm_44_conv_4 */
    static const LL_Convacc_InitTypeDef Gemm_44_conv_4_init5 = {
      .simd = 2,
      .fsub = -19,
      .vshift = 0,
      .accumulate = 0,
      .rounding_f = 0,
      .saturation_f = 0,
      .round_mode_f = 0,
      .f_unsigned = 0,
      .k_unsigned = 0,
      .deepmode = 0,
      .dss2mode = 0,
      .kseten = 0,
      .zfbias = 0,
      .inbytes_f = 2,
      .shift_f = 0,
      .shift_a = 1,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 2,
      .shift_o = 1,
      .raw_o = 0,
      .fWidth = 1,
      .fHeight = 1,
      .kernelWidth = 1,
      .kernelHeight = 1,
      .nKernels = 8,
      .batchDepth = 2,
      .hstride = 1,
      .vstride = 1,
      .left_padding = 0,
      .right_padding = 0,
      .top_padding = 0,
      .bot_padding = 0,
      .left_crop = 0,
      .right_crop = 0,
      .top_crop = 0,
      .bot_crop = 0,
    };

    /* Unit=CONV_ACC_V2 */
    LL_Convacc_Init(0, &Gemm_44_conv_4_init5);


    /* Unit= 18 [ARITH_ACC_V2 0] */
    /* kind=Add node=Gemm_44_conv_4_off_bias_51 */
    static const LL_Arithacc_InitTypeDef Gemm_44_conv_4_off_bias_51_init5 = {
      .rounding_x = 0,
      .saturation_x = 0,
      .round_mode_x = 0,
      .inbytes_x = 2,
      .outbytes_x = 2,
      .shift_x = 0,
      .rounding_y = 0,
      .saturation_y = 0,
      .round_mode_y = 0,
      .inbytes_y = 2,
      .outbytes_y = 2,
      .combinebc = 1,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .shift_o = 20,
      .scalar = 0,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_CHAN,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 1,
      .fHeight = 1,
      .fChannels = 8,
      .batchDepth = 8,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 0,
      .B_scalar = 0,
      .C_scalar = (short)0,
      .A_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 928))) /* Equivalent hex offset = 0x3a0 */},
      .B_vector = {0},
      .C_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 832))) /* Equivalent hex offset = 0x340 */},
      .vec_precision = {16, 16, 32},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(0, &Gemm_44_conv_4_off_bias_51_init5);


    /* Dma inputs units to cycle: */
    /* Unit= 5 [STREAM_ENG_V2 5] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_44_conv_4 input ports=0 range=1[592,594] */

    static const LL_Streng_TensorInitTypeDef Gemm_44_conv_4_dma_init_in_0_5 = {
      /* 1x1x2(8 bits) */
      .dir = 0,
      .raw = 1,
      .noblk = 0,
      .align_right = 1,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Gemm_44_conv_4_zero_off_out_46 */
      .offset_start = 592,
      .offset_end = 594,
      .offset_limit = 664,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 2,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 1,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 16,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(5, &Gemm_44_conv_4_dma_init_in_0_5, 1);

    /* Unit= 0 [STREAM_ENG_V2 0] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_44_conv_4 input ports=1 range=5[7920,7936] */

    static const LL_Streng_TensorInitTypeDef Gemm_44_conv_4_dma_init_in_1_5 = {
      /* 8x1x1x2(8 bits) */
      .dir = 0,
      .raw = 1,
      .continuous = 1,
      .noblk = 0,
      .align_right = 1,
      .nbits_unsigned = 0,
      .cacheable = 1,
      .cache_allocate = 0,
      .addr_base = {(unsigned char *)(0x70400000UL) /* Equivalent hex address = 0x70400000UL */}, /* Gemm_44_weights_transposed_3 */
      .offset_start = 7920,
      .offset_end = 7936,
      .offset_limit = 8000,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 0,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(0, &Gemm_44_conv_4_dma_init_in_1_5, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM5 -> 2 */
    /* octoFlash -> 16 */

    /* Dma output units from cycle: */
    /* Unit= 7 [STREAM_ENG_V2 7] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_44_conv_4_off_bias_51 output ports=0 range=1[800,808] */

    static const LL_Streng_TensorInitTypeDef Gemm_44_conv_4_off_bias_51_dma_init_out_0_5 = {
      /* to memory with batch=8 */
      .dir = 1,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Gemm_44_conv_4_off_bias_out_52 */
      .offset_start = 800,
      .offset_end = 808,
      .offset_limit = 872,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 8,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(7, &Gemm_44_conv_4_off_bias_51_dma_init_out_0_5, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM5 <- 8 */

    static const LL_Switch_InitTypeDef STREAM_SWITCH_0_init_in_5[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 5, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_44_conv_4 IN: in unit=CONV_ACC_V2 0 in port=0 out unit=STREAM_ENG_V2 5 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_44_conv_4 IN: in unit=CONV_ACC_V2 0 in port=1 out unit=STREAM_ENG_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_44_conv_4_off_bias_51 IN: in unit=ARITH_ACC_V2 0 in port=0 out unit=CONV_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 7, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_44_conv_4_off_bias_51 OUT: in unit=STREAM_ENG_V2 7 in port=0 out unit=ARITH_ACC_V2 0 out port=0 */
    };


    /* epoch=5 */
    LL_Switch_Init(STREAM_SWITCH_0_init_in_5, 4);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_5_all_units[] = {
      { {STRENG, 7} }, /* STREAM_ENG_V2 */
      { {CONVACC, 0} }, /* CONV_ACC_V2 */
      { {ARITH, 0} }, /* ARITH_ACC_V2 */
      { {STRENG, 0} }, /* STREAM_ENG_V2 */
      { {STRENG, 5} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_5_all_units, 5);

  }

  ec_trace_wait_epoch_end(0x80);

  {
    static const LL_Switch_DeinitTypeDef STREAM_SWITCH_0_deinit_in_5[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 5, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_44_conv_4 IN: in unit=CONV_ACC_V2 0 in port=0 out unit=STREAM_ENG_V2 5 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_44_conv_4 IN: in unit=CONV_ACC_V2 0 in port=1 out unit=STREAM_ENG_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_44_conv_4_off_bias_51 IN: in unit=ARITH_ACC_V2 0 in port=0 out unit=CONV_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 7, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_44_conv_4_off_bias_51 OUT: in unit=STREAM_ENG_V2 7 in port=0 out unit=ARITH_ACC_V2 0 out port=0 */
    };


    /* epoch=5 */
    LL_Switch_Deinit(STREAM_SWITCH_0_deinit_in_5, 4);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_5_all_units[] = {
      { {STRENG, 7} }, /* STREAM_ENG_V2 */
      { {CONVACC, 0} }, /* CONV_ACC_V2 */
      { {ARITH, 0} }, /* ARITH_ACC_V2 */
      { {STRENG, 0} }, /* STREAM_ENG_V2 */
      { {STRENG, 5} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_5_all_units, 5);

  }
  ec_trace_end_epoch(5);
  ec_trace_start_epoch(6);
  {
    /* Unit= 11 [CONV_ACC_V2 1] */
    /* kind=Conv node=Conv2D_8 */
    static const LL_Convacc_InitTypeDef Conv2D_8_init6 = {
      .simd = 2,
      .fsub = -22,
      .vshift = 0,
      .accumulate = 0,
      .rounding_f = 0,
      .saturation_f = 0,
      .round_mode_f = 0,
      .f_unsigned = 0,
      .k_unsigned = 0,
      .deepmode = 0,
      .dss2mode = 0,
      .kseten = 0,
      .zfbias = 0,
      .inbytes_f = 2,
      .shift_f = 0,
      .shift_a = 2,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 2,
      .shift_o = 2,
      .raw_o = 0,
      .fWidth = 34,
      .fHeight = 1,
      .kernelWidth = 5,
      .kernelHeight = 1,
      .nKernels = 4,
      .batchDepth = 1,
      .hstride = 1,
      .vstride = 1,
      .left_padding = 0,
      .right_padding = 0,
      .top_padding = 0,
      .bot_padding = 0,
      .left_crop = 0,
      .right_crop = 33,
      .top_crop = 0,
      .bot_crop = 0,
    };

    /* Unit=CONV_ACC_V2 */
    LL_Convacc_Init(1, &Conv2D_8_init6);


    /* Unit= 19 [ARITH_ACC_V2 1] */
    /* kind=Add node=Conv2D_8_off_bias_24 */
    static const LL_Arithacc_InitTypeDef Conv2D_8_off_bias_24_init6 = {
      .rounding_x = 0,
      .saturation_x = 0,
      .round_mode_x = 0,
      .inbytes_x = 2,
      .outbytes_x = 2,
      .shift_x = 0,
      .rounding_y = 0,
      .saturation_y = 0,
      .round_mode_y = 0,
      .inbytes_y = 2,
      .outbytes_y = 2,
      .combinebc = 1,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .shift_o = 20,
      .scalar = 0,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_CHAN,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 30,
      .fHeight = 1,
      .fChannels = 16,
      .batchDepth = 4,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 0,
      .B_scalar = 0,
      .C_scalar = (short)0,
      .A_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 864))) /* Equivalent hex offset = 0x360 */},
      .B_vector = {0},
      .C_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 640))) /* Equivalent hex offset = 0x280 */},
      .vec_precision = {16, 16, 32},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(1, &Conv2D_8_off_bias_24_init6);


    /* Unit= 22 [POOL_ACC_V2 0] */
    /* kind=MaxPool node=MaxPool_14 */
    static const LL_Poolacc_InitTypeDef MaxPool_14_init6 = {
      .operation = POOL_MAX,
      .avgnopad = 0,
      .inputX = 30,
      .inputY = 1,
      .outputX = 15,
      .outputY = 1,
      .poolWinX = 2,
      .poolWinY = 1,
      .strideX = 2,
      .strideY = 1,
      .topCrop = 0,
      .bottomCrop = 0,
      .leftCrop = 0,
      .rightCrop = 29,
      .topPad = 0,
      .bottomPad = 0,
      .leftPad = 0,
      .rightPad = 0,
      .batchSize = 4,
      .shift_f = 0,
      .shift_o = 0,
      .dualLine = 1,
      .rounding_f = 0,
      .saturation_f = 0,
      .round_mode_f = 0,
      .inbytes_f = 1,
      .outbytes_f = 1,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .mulval = 0,
      .pad_val_en = 0,
      .pad_val = 0,
    };

    /* Unit=POOL_ACC_V2 */
    LL_Poolacc_Init(0, &MaxPool_14_init6);


    /* Dma inputs units to cycle: */
    /* Unit= 1 [STREAM_ENG_V2 1] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_8 input ports=0 range=1[512,546] */

    static const LL_Streng_TensorInitTypeDef Conv2D_8_dma_init_in_0_6 = {
      /* 34x1x1(8 bits) */
      .dir = 0,
      .raw = 1,
      .noblk = 0,
      .align_right = 1,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Conv2D_8_zero_off_out_19 */
      .offset_start = 512,
      .offset_end = 546,
      .offset_limit = 616,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 34,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 1,
      .frame_tot_cnt = 4,
      .nbits_in = 8,
      .nbits_out = 16,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(1, &Conv2D_8_dma_init_in_0_6, 1);

    /* Unit= 4 [STREAM_ENG_V2 4] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_8 input ports=1 range=5[7648,7728] */

    static const LL_Streng_TensorInitTypeDef Conv2D_8_dma_init_in_1_6 = {
      /* 16x5x1x1(8 bits) */
      .dir = 0,
      .raw = 1,
      .continuous = 1,
      .noblk = 0,
      .align_right = 1,
      .nbits_unsigned = 0,
      .cacheable = 1,
      .cache_allocate = 0,
      .addr_base = {(unsigned char *)(0x70400000UL) /* Equivalent hex address = 0x70400000UL */}, /* Conv2D_8_weights */
      .offset_start = 7648,
      .offset_end = 7728,
      .offset_limit = 7792,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 0,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(4, &Conv2D_8_dma_init_in_1_6, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM5 -> 34 */
    /* octoFlash -> 80 */

    /* Dma output units from cycle: */
    /* Unit= 8 [STREAM_ENG_V2 8] */
    /* Emit conf for STREAM_ENG_V2 node=MaxPool_14 output ports=0 range=1[272,512] */

    static const LL_Streng_TensorInitTypeDef MaxPool_14_dma_init_out_0_6 = {
      /* to memory canonical from batch=4 */
      .dir = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* MaxPool_14_out_0 */
      .offset_start = 272,
      .offset_limit = 576,
      .frame_count = 0,
      .fwidth = 15,
      .fheight = 1,
      .batch_depth = 4,
      .batch_offset = 16,
      .frame_offset = 4,
      .line_offset = 0,
      .loop_offset = 240,
      .frame_loop_cnt = 4,
      .frame_tot_cnt = 4,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(8, &MaxPool_14_dma_init_out_0_6, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM5 <- 240 */

    static const LL_Switch_InitTypeDef STREAM_SWITCH_0_init_in_6[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_8 IN: in unit=CONV_ACC_V2 1 in port=0 out unit=STREAM_ENG_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 1, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 4, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_8 IN: in unit=CONV_ACC_V2 1 in port=1 out unit=STREAM_ENG_V2 4 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_8_off_bias_24 IN: in unit=ARITH_ACC_V2 1 in port=0 out unit=CONV_ACC_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, POOL, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* MaxPool_14 IN: in unit=POOL_ACC_V2 0 in port=0 out unit=ARITH_ACC_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 8, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, POOL, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* MaxPool_14 OUT: in unit=STREAM_ENG_V2 8 in port=0 out unit=POOL_ACC_V2 0 out port=0 */
    };


    /* epoch=6 */
    LL_Switch_Init(STREAM_SWITCH_0_init_in_6, 5);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_6_all_units[] = {
      { {STRENG, 8} }, /* STREAM_ENG_V2 */
      { {CONVACC, 1} }, /* CONV_ACC_V2 */
      { {ARITH, 1} }, /* ARITH_ACC_V2 */
      { {POOL, 0} }, /* POOL_ACC_V2 */
      { {STRENG, 1} }, /* STREAM_ENG_V2 */
      { {STRENG, 4} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_6_all_units, 6);

  }

  ec_trace_wait_epoch_end(0x100);

  {
    static const LL_Switch_DeinitTypeDef STREAM_SWITCH_0_deinit_in_6[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_8 IN: in unit=CONV_ACC_V2 1 in port=0 out unit=STREAM_ENG_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 1, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 4, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_8 IN: in unit=CONV_ACC_V2 1 in port=1 out unit=STREAM_ENG_V2 4 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_8_off_bias_24 IN: in unit=ARITH_ACC_V2 1 in port=0 out unit=CONV_ACC_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, POOL, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* MaxPool_14 IN: in unit=POOL_ACC_V2 0 in port=0 out unit=ARITH_ACC_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 8, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, POOL, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* MaxPool_14 OUT: in unit=STREAM_ENG_V2 8 in port=0 out unit=POOL_ACC_V2 0 out port=0 */
    };


    /* epoch=6 */
    LL_Switch_Deinit(STREAM_SWITCH_0_deinit_in_6, 5);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_6_all_units[] = {
      { {STRENG, 8} }, /* STREAM_ENG_V2 */
      { {CONVACC, 1} }, /* CONV_ACC_V2 */
      { {ARITH, 1} }, /* ARITH_ACC_V2 */
      { {POOL, 0} }, /* POOL_ACC_V2 */
      { {STRENG, 1} }, /* STREAM_ENG_V2 */
      { {STRENG, 4} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_6_all_units, 6);

  }
  ec_trace_end_epoch(6);
  ec_trace_end_blob("_ec_blob_workclf_5");
}

void trace_ec__ec_blob_workclf_8(void) {
  ec_trace_start_blob("_ec_blob_workclf_8");
  ec_trace_start_epoch(8);
  {
  }
  {
  }
  ec_trace_end_epoch(8);
  ec_trace_start_epoch(9);
  {
    /* Unit= 20 [ARITH_ACC_V2 2] */
    /* kind=Sub node=Conv2D_20_suboff_27 */
    static const LL_Arithacc_InitTypeDef Conv2D_20_suboff_27_init9 = {
      .rounding_x = 0,
      .saturation_x = 0,
      .round_mode_x = 0,
      .inbytes_x = 1,
      .outbytes_x = 1,
      .shift_x = 0,
      .rounding_y = 0,
      .saturation_y = 0,
      .round_mode_y = 0,
      .inbytes_y = 1,
      .outbytes_y = 1,
      .combinebc = 1,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 0,
      .saturation_o = 0,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .shift_o = 14,
      .scalar = 1,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_SCALAR,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 17,
      .fHeight = 1,
      .fChannels = 16,
      .batchDepth = 16,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 16384,
      .B_scalar = 32,
      .C_scalar = (short)0,
      .A_vector = {0},
      .B_vector = {0},
      .C_vector = {0},
      .vec_precision = {8, 8, 8},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(2, &Conv2D_20_suboff_27_init9);


    /* Dma inputs units to cycle: */
    /* Unit= 3 [STREAM_ENG_V2 3] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_20_suboff_27 input ports=0 range=1[0,272] */

    static const LL_Streng_TensorInitTypeDef Conv2D_20_suboff_27_dma_init_in_0_9 = {
      /* from memory with batch=16 */
      .dir = 0,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Reshape_19_out_0 */
      .offset_start = 0,
      .offset_end = 272,
      .offset_limit = 336,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 272,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(3, &Conv2D_20_suboff_27_dma_init_in_0_9, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM5 -> 272 */

    /* Dma output units from cycle: */
    /* Unit= 8 [STREAM_ENG_V2 8] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_20_suboff_27 output ports=0 range=1[272,544] */

    static const LL_Streng_TensorInitTypeDef Conv2D_20_suboff_27_dma_init_out_0_9 = {
      /* to memory with batch=16 */
      .dir = 1,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 1,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Conv2D_20_zero_off_out_28 */
      .offset_start = 272,
      .offset_end = 544,
      .offset_limit = 608,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 272,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(8, &Conv2D_20_suboff_27_dma_init_out_0_9, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM5 <- 272 */

    static const LL_Switch_InitTypeDef STREAM_SWITCH_0_init_in_9[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_20_suboff_27 IN: in unit=ARITH_ACC_V2 2 in port=0 out unit=STREAM_ENG_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 8, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_20_suboff_27 OUT: in unit=STREAM_ENG_V2 8 in port=0 out unit=ARITH_ACC_V2 2 out port=0 */
    };


    /* epoch=9 */
    LL_Switch_Init(STREAM_SWITCH_0_init_in_9, 2);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_9_all_units[] = {
      { {STRENG, 8} }, /* STREAM_ENG_V2 */
      { {ARITH, 2} }, /* ARITH_ACC_V2 */
      { {STRENG, 3} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_9_all_units, 3);

  }

  ec_trace_wait_epoch_end(0x100);

  {
    static const LL_Switch_DeinitTypeDef STREAM_SWITCH_0_deinit_in_9[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_20_suboff_27 IN: in unit=ARITH_ACC_V2 2 in port=0 out unit=STREAM_ENG_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 8, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_20_suboff_27 OUT: in unit=STREAM_ENG_V2 8 in port=0 out unit=ARITH_ACC_V2 2 out port=0 */
    };


    /* epoch=9 */
    LL_Switch_Deinit(STREAM_SWITCH_0_deinit_in_9, 2);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_9_all_units[] = {
      { {STRENG, 8} }, /* STREAM_ENG_V2 */
      { {ARITH, 2} }, /* ARITH_ACC_V2 */
      { {STRENG, 3} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_9_all_units, 3);

  }
  ec_trace_end_epoch(9);
  ec_trace_start_epoch(10);
  {
    /* Unit= 12 [CONV_ACC_V2 2] */
    /* kind=Conv node=Conv2D_20 */
    static const LL_Convacc_InitTypeDef Conv2D_20_init10 = {
      .simd = 1,
      .fsub = 0,
      .vshift = 0,
      .accumulate = 0,
      .rounding_f = 0,
      .saturation_f = 0,
      .round_mode_f = 0,
      .f_unsigned = 1,
      .k_unsigned = 0,
      .deepmode = 0,
      .dss2mode = 0,
      .kseten = 0,
      .zfbias = 0,
      .inbytes_f = 1,
      .shift_f = 0,
      .shift_a = 3,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 2,
      .shift_o = 3,
      .raw_o = 0,
      .fWidth = 17,
      .fHeight = 1,
      .kernelWidth = 3,
      .kernelHeight = 1,
      .nKernels = 8,
      .batchDepth = 16,
      .hstride = 1,
      .vstride = 1,
      .left_padding = 0,
      .right_padding = 0,
      .top_padding = 0,
      .bot_padding = 0,
      .left_crop = 0,
      .right_crop = 16,
      .top_crop = 0,
      .bot_crop = 0,
    };

    /* Unit=CONV_ACC_V2 */
    LL_Convacc_Init(2, &Conv2D_20_init10);


    /* Unit= 21 [ARITH_ACC_V2 3] */
    /* kind=Add node=Conv2D_20_off_bias_33 */
    static const LL_Arithacc_InitTypeDef Conv2D_20_off_bias_33_init10 = {
      .rounding_x = 0,
      .saturation_x = 0,
      .round_mode_x = 0,
      .inbytes_x = 2,
      .outbytes_x = 2,
      .shift_x = 0,
      .rounding_y = 0,
      .saturation_y = 0,
      .round_mode_y = 0,
      .inbytes_y = 2,
      .outbytes_y = 2,
      .combinebc = 1,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .shift_o = 18,
      .scalar = 0,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_CHAN,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 15,
      .fHeight = 1,
      .fChannels = 32,
      .batchDepth = 8,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 0,
      .B_scalar = 0,
      .C_scalar = (short)0,
      .A_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 704))) /* Equivalent hex offset = 0x2c0 */},
      .B_vector = {0},
      .C_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 256))) /* Equivalent hex offset = 0x100 */},
      .vec_precision = {16, 16, 32},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(3, &Conv2D_20_off_bias_33_init10);


    /* Unit= 23 [POOL_ACC_V2 1] */
    /* kind=MaxPool node=MaxPool_26 */
    static const LL_Poolacc_InitTypeDef MaxPool_26_init10 = {
      .operation = POOL_MAX,
      .avgnopad = 0,
      .inputX = 15,
      .inputY = 1,
      .outputX = 7,
      .outputY = 1,
      .poolWinX = 2,
      .poolWinY = 1,
      .strideX = 2,
      .strideY = 1,
      .topCrop = 0,
      .bottomCrop = 0,
      .leftCrop = 0,
      .rightCrop = 13,
      .topPad = 0,
      .bottomPad = 0,
      .leftPad = 0,
      .rightPad = 0,
      .batchSize = 8,
      .shift_f = 0,
      .shift_o = 0,
      .dualLine = 1,
      .rounding_f = 0,
      .saturation_f = 0,
      .round_mode_f = 0,
      .inbytes_f = 1,
      .outbytes_f = 1,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .mulval = 0,
      .pad_val_en = 0,
      .pad_val = 0,
    };

    /* Unit=POOL_ACC_V2 */
    LL_Poolacc_Init(1, &MaxPool_26_init10);


    /* Dma inputs units to cycle: */
    /* Unit= 1 [STREAM_ENG_V2 1] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_20 input ports=0 range=1[272,544] */

    static const LL_Streng_TensorInitTypeDef Conv2D_20_dma_init_in_0_10 = {
      /* 17x1x16(8 bits) */
      .dir = 0,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 1,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Conv2D_20_zero_off_out_28 */
      .offset_start = 272,
      .offset_end = 544,
      .offset_limit = 608,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 272,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 1,
      .frame_tot_cnt = 4,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(1, &Conv2D_20_dma_init_in_0_10, 1);

    /* Unit= 6 [STREAM_ENG_V2 6] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_20 input ports=1 range=5[5648,7184] */

    static const LL_Streng_TensorInitTypeDef Conv2D_20_dma_init_in_1_10 = {
      /* 32x3x1x16(8 bits) */
      .dir = 0,
      .raw = 1,
      .continuous = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .cacheable = 1,
      .cache_allocate = 0,
      .addr_base = {(unsigned char *)(0x70400000UL) /* Equivalent hex address = 0x70400000UL */}, /* Conv2D_20_weights */
      .offset_start = 5648,
      .offset_end = 7184,
      .offset_limit = 7248,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 0,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(6, &Conv2D_20_dma_init_in_1_10, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM5 -> 272 */
    /* octoFlash -> 1536 */

    /* Dma output units from cycle: */
    /* Unit= 4 [STREAM_ENG_V2 4] */
    /* Emit conf for STREAM_ENG_V2 node=MaxPool_26 output ports=0 range=1[544,768] */

    static const LL_Streng_TensorInitTypeDef MaxPool_26_dma_init_out_0_10 = {
      /* to memory canonical from batch=8 */
      .dir = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* MaxPool_26_out_0 */
      .offset_start = 544,
      .offset_limit = 832,
      .frame_count = 0,
      .fwidth = 7,
      .fheight = 1,
      .batch_depth = 8,
      .batch_offset = 32,
      .frame_offset = 8,
      .line_offset = 0,
      .loop_offset = 224,
      .frame_loop_cnt = 4,
      .frame_tot_cnt = 4,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(4, &MaxPool_26_dma_init_out_0_10, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM5 <- 224 */

    static const LL_Switch_InitTypeDef STREAM_SWITCH_0_init_in_10[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_20 IN: in unit=CONV_ACC_V2 2 in port=0 out unit=STREAM_ENG_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 2, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 6, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_20 IN: in unit=CONV_ACC_V2 2 in port=1 out unit=STREAM_ENG_V2 6 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 3, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_20_off_bias_33 IN: in unit=ARITH_ACC_V2 3 in port=0 out unit=CONV_ACC_V2 2 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, POOL, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* MaxPool_26 IN: in unit=POOL_ACC_V2 1 in port=0 out unit=ARITH_ACC_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 4, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, POOL, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* MaxPool_26 OUT: in unit=STREAM_ENG_V2 4 in port=0 out unit=POOL_ACC_V2 1 out port=0 */
    };


    /* epoch=10 */
    LL_Switch_Init(STREAM_SWITCH_0_init_in_10, 5);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_10_all_units[] = {
      { {STRENG, 4} }, /* STREAM_ENG_V2 */
      { {CONVACC, 2} }, /* CONV_ACC_V2 */
      { {ARITH, 3} }, /* ARITH_ACC_V2 */
      { {POOL, 1} }, /* POOL_ACC_V2 */
      { {STRENG, 1} }, /* STREAM_ENG_V2 */
      { {STRENG, 6} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_10_all_units, 6);

  }

  ec_trace_wait_epoch_end(0x10);

  {
    static const LL_Switch_DeinitTypeDef STREAM_SWITCH_0_deinit_in_10[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_20 IN: in unit=CONV_ACC_V2 2 in port=0 out unit=STREAM_ENG_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 2, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 6, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_20 IN: in unit=CONV_ACC_V2 2 in port=1 out unit=STREAM_ENG_V2 6 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 3, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_20_off_bias_33 IN: in unit=ARITH_ACC_V2 3 in port=0 out unit=CONV_ACC_V2 2 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, POOL, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* MaxPool_26 IN: in unit=POOL_ACC_V2 1 in port=0 out unit=ARITH_ACC_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 4, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, POOL, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* MaxPool_26 OUT: in unit=STREAM_ENG_V2 4 in port=0 out unit=POOL_ACC_V2 1 out port=0 */
    };


    /* epoch=10 */
    LL_Switch_Deinit(STREAM_SWITCH_0_deinit_in_10, 5);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_10_all_units[] = {
      { {STRENG, 4} }, /* STREAM_ENG_V2 */
      { {CONVACC, 2} }, /* CONV_ACC_V2 */
      { {ARITH, 3} }, /* ARITH_ACC_V2 */
      { {POOL, 1} }, /* POOL_ACC_V2 */
      { {STRENG, 1} }, /* STREAM_ENG_V2 */
      { {STRENG, 6} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_10_all_units, 6);

  }
  ec_trace_end_epoch(10);
  ec_trace_end_blob("_ec_blob_workclf_8");
}

void trace_ec__ec_blob_workclf_12(void) {
  ec_trace_start_blob("_ec_blob_workclf_12");
  ec_trace_start_epoch(12);
  {
  }
  {
  }
  ec_trace_end_epoch(12);
  ec_trace_start_epoch(13);
  {
    /* Unit= 18 [ARITH_ACC_V2 0] */
    /* kind=Sub node=Conv2D_32_suboff_36 */
    static const LL_Arithacc_InitTypeDef Conv2D_32_suboff_36_init13 = {
      .rounding_x = 0,
      .saturation_x = 0,
      .round_mode_x = 0,
      .inbytes_x = 1,
      .outbytes_x = 1,
      .shift_x = 0,
      .rounding_y = 0,
      .saturation_y = 0,
      .round_mode_y = 0,
      .inbytes_y = 1,
      .outbytes_y = 1,
      .combinebc = 1,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 0,
      .saturation_o = 0,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .shift_o = 14,
      .scalar = 1,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_SCALAR,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 9,
      .fHeight = 1,
      .fChannels = 32,
      .batchDepth = 32,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 16384,
      .B_scalar = 32,
      .C_scalar = (short)0,
      .A_vector = {0},
      .B_vector = {0},
      .C_vector = {0},
      .vec_precision = {8, 8, 8},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(0, &Conv2D_32_suboff_36_init13);


    /* Dma inputs units to cycle: */
    /* Unit= 2 [STREAM_ENG_V2 2] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_32_suboff_36 input ports=0 range=1[0,288] */

    static const LL_Streng_TensorInitTypeDef Conv2D_32_suboff_36_dma_init_in_0_13 = {
      /* from memory with batch=32 */
      .dir = 0,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Reshape_31_out_0 */
      .offset_start = 0,
      .offset_end = 288,
      .offset_limit = 352,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 288,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(2, &Conv2D_32_suboff_36_dma_init_in_0_13, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM5 -> 288 */

    /* Dma output units from cycle: */
    /* Unit= 7 [STREAM_ENG_V2 7] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_32_suboff_36 output ports=0 range=1[288,576] */

    static const LL_Streng_TensorInitTypeDef Conv2D_32_suboff_36_dma_init_out_0_13 = {
      /* to memory from canonical with batch=16 */
      .dir = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 1,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Conv2D_32_zero_off_out_37 */
      .offset_start = 288,
      .offset_limit = 640,
      .frame_count = 0,
      .fwidth = 2,
      .fheight = 9,
      .batch_depth = 16,
      .batch_offset = 144,
      .frame_offset = 144,
      .line_offset = 16,
      .loop_offset = 288,
      .frame_loop_cnt = 1,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(7, &Conv2D_32_suboff_36_dma_init_out_0_13, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM5 <- 288 */

    static const LL_Switch_InitTypeDef STREAM_SWITCH_0_init_in_13[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32_suboff_36 IN: in unit=ARITH_ACC_V2 0 in port=0 out unit=STREAM_ENG_V2 2 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 7, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32_suboff_36 OUT: in unit=STREAM_ENG_V2 7 in port=0 out unit=ARITH_ACC_V2 0 out port=0 */
    };


    /* epoch=13 */
    LL_Switch_Init(STREAM_SWITCH_0_init_in_13, 2);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_13_all_units[] = {
      { {STRENG, 7} }, /* STREAM_ENG_V2 */
      { {ARITH, 0} }, /* ARITH_ACC_V2 */
      { {STRENG, 2} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_13_all_units, 3);

  }

  ec_trace_wait_epoch_end(0x80);

  {
    static const LL_Switch_DeinitTypeDef STREAM_SWITCH_0_deinit_in_13[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32_suboff_36 IN: in unit=ARITH_ACC_V2 0 in port=0 out unit=STREAM_ENG_V2 2 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 7, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32_suboff_36 OUT: in unit=STREAM_ENG_V2 7 in port=0 out unit=ARITH_ACC_V2 0 out port=0 */
    };


    /* epoch=13 */
    LL_Switch_Deinit(STREAM_SWITCH_0_deinit_in_13, 2);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_13_all_units[] = {
      { {STRENG, 7} }, /* STREAM_ENG_V2 */
      { {ARITH, 0} }, /* ARITH_ACC_V2 */
      { {STRENG, 2} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_13_all_units, 3);

  }
  ec_trace_end_epoch(13);
  ec_trace_start_epoch(14);
  {
    /* Unit= 13 [CONV_ACC_V2 3] */
    /* kind=Conv node=Conv2D_32 */
    static const LL_Convacc_InitTypeDef Conv2D_32_init14 = {
      .simd = 1,
      .fsub = 0,
      .vshift = 0,
      .accumulate = 1,
      .afilt_mode = AFILT_MODE_FRAMEZERO,
      .afilt_tot = 2,
      .afilt_first = 1,
      .afilt_last = 1,
      .rounding_f = 0,
      .saturation_f = 0,
      .round_mode_f = 0,
      .f_unsigned = 1,
      .k_unsigned = 0,
      .deepmode = 0,
      .dss2mode = 0,
      .kseten = 0,
      .zfbias = 0,
      .inbytes_f = 1,
      .shift_f = 0,
      .shift_a = 5,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 2,
      .shift_o = 5,
      .raw_o = 0,
      .fWidth = 9,
      .fHeight = 1,
      .kernelWidth = 3,
      .kernelHeight = 1,
      .nKernels = 8,
      .batchDepth = 16,
      .hstride = 1,
      .vstride = 1,
      .left_padding = 0,
      .right_padding = 0,
      .top_padding = 0,
      .bot_padding = 0,
      .left_crop = 0,
      .right_crop = 8,
      .top_crop = 0,
      .bot_crop = 0,
    };

    /* Unit=CONV_ACC_V2 */
    LL_Convacc_Init(3, &Conv2D_32_init14);


    /* Unit= 19 [ARITH_ACC_V2 1] */
    /* kind=Add node=Conv2D_32_off_bias_42 */
    static const LL_Arithacc_InitTypeDef Conv2D_32_off_bias_42_init14 = {
      .rounding_x = 0,
      .saturation_x = 0,
      .round_mode_x = 0,
      .inbytes_x = 2,
      .outbytes_x = 2,
      .shift_x = 0,
      .rounding_y = 0,
      .saturation_y = 0,
      .round_mode_y = 0,
      .inbytes_y = 2,
      .outbytes_y = 2,
      .combinebc = 1,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .shift_o = 18,
      .scalar = 0,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_CHAN,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 7,
      .fHeight = 1,
      .fChannels = 32,
      .batchDepth = 8,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 0,
      .B_scalar = 0,
      .C_scalar = (short)0,
      .A_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 768))) /* Equivalent hex offset = 0x300 */},
      .B_vector = {0},
      .C_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 384))) /* Equivalent hex offset = 0x180 */},
      .vec_precision = {16, 16, 32},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(1, &Conv2D_32_off_bias_42_init14);


    /* Dma inputs units to cycle: */
    /* Unit= 2 [STREAM_ENG_V2 2] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_32 input ports=0 range=1[288,576] */

    static const LL_Streng_TensorInitTypeDef Conv2D_32_dma_init_in_0_14 = {
      /* 9x1x16(8 bits) */
      .dir = 0,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 1,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Conv2D_32_zero_off_out_37 */
      .offset_start = 288,
      .offset_end = 432,
      .offset_limit = 640,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 144,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 2,
      .frame_tot_cnt = 8,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(2, &Conv2D_32_dma_init_in_0_14, 1);

    /* Unit= 5 [STREAM_ENG_V2 5] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_32 input ports=1 range=5[0,3072] */

    static const LL_Streng_TensorInitTypeDef Conv2D_32_dma_init_in_1_14 = {
      /* 32x3x1x32(8 bits) */
      .dir = 0,
      .raw = 1,
      .continuous = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .cacheable = 1,
      .cache_allocate = 0,
      .addr_base = {(unsigned char *)(0x70400000UL) /* Equivalent hex address = 0x70400000UL */}, /* Conv2D_32_weights */
      .offset_start = 0,
      .offset_end = 3072,
      .offset_limit = 3136,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 0,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(5, &Conv2D_32_dma_init_in_1_14, 1);

    /* Unit= 4 [STREAM_ENG_V2 4] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_32 input ports=2 range=1[576,688] */

    static const LL_Streng_TensorInitTypeDef Conv2D_32_dma_init_in_2_14 = {
      /* partial accumulator 112 (16 bits) */
      .dir = 0,
      .raw = 1,
      .noblk = 0,
      .align_right = 1,
      .sync_with_other = 1,
      .nbits_unsigned = 0,
      .sync_dma = 3,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* ATONN_ACCUMULATOR_PORT */
      .offset_start = 576,
      .offset_end = 688,
      .offset_limit = 752,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 0,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 8,
      .nbits_in = 16,
      .nbits_out = 16,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(4, &Conv2D_32_dma_init_in_2_14, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM5 -> 1184 */
    /* octoFlash -> 3072 */

    /* Dma output units from cycle: */
    /* Unit= 3 [STREAM_ENG_V2 3] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_32 output ports=0 range=1[576,688] */

    static const LL_Streng_TensorInitTypeDef Conv2D_32_dma_init_out_0_14 = {
      /* partial accumulator 112 (16 bits) */
      .dir = 1,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Conv2D_32_out_0 */
      .offset_start = 576,
      .offset_end = 688,
      .offset_limit = 752,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 0,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 8,
      .nbits_in = 16,
      .nbits_out = 16,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(3, &Conv2D_32_dma_init_out_0_14, 1);

    /* Unit= 1 [STREAM_ENG_V2 1] */
    /* Emit conf for STREAM_ENG_V2 node=Conv2D_32_off_bias_42 output ports=0 range=1[0,224] */

    static const LL_Streng_TensorInitTypeDef Conv2D_32_off_bias_42_dma_init_out_0_14 = {
      /* to memory canonical from batch=8 */
      .dir = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Conv2D_32_off_bias_out_43 */
      .offset_start = 0,
      .offset_limit = 288,
      .frame_count = 0,
      .fwidth = 7,
      .fheight = 1,
      .batch_depth = 8,
      .batch_offset = 32,
      .frame_offset = 8,
      .line_offset = 0,
      .loop_offset = 224,
      .frame_loop_cnt = 4,
      .frame_tot_cnt = 4,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(1, &Conv2D_32_off_bias_42_dma_init_out_0_14, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM5 <- 1344 */

    static const LL_Switch_InitTypeDef STREAM_SWITCH_0_init_in_14[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 3, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32 IN: in unit=CONV_ACC_V2 3 in port=0 out unit=STREAM_ENG_V2 2 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 3, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 5, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32 IN: in unit=CONV_ACC_V2 3 in port=1 out unit=STREAM_ENG_V2 5 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 3, 2), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 4, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32 IN: in unit=CONV_ACC_V2 3 in port=2 out unit=STREAM_ENG_V2 4 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 3, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32 OUT: in unit=STREAM_ENG_V2 3 in port=0 out unit=CONV_ACC_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 3, 0), LL_Switch_Init_Context(0) = 0, LL_Switch_Init_Frames(0) = 1, LL_Switch_Init_Source(1) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 3, 0), LL_Switch_Init_Context(1) = 1, LL_Switch_Init_Frames(1) = 1, }, /* Conv2D_32_off_bias_42 IN: in unit=ARITH_ACC_V2 1 in port=0 out unit=CONV_ACC_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32_off_bias_42 OUT: in unit=STREAM_ENG_V2 1 in port=0 out unit=ARITH_ACC_V2 1 out port=0 */
    };


    /* epoch=14 */
    LL_Switch_Init(STREAM_SWITCH_0_init_in_14, 6);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_14_all_units[] = {
      { {STRENG, 1} }, /* STREAM_ENG_V2 */
      { {STRENG, 3} }, /* STREAM_ENG_V2 */
      { {CONVACC, 3} }, /* CONV_ACC_V2 */
      { {ARITH, 1} }, /* ARITH_ACC_V2 */
      { {STRENG, 2} }, /* STREAM_ENG_V2 */
      { {STRENG, 4} }, /* STREAM_ENG_V2 */
      { {STRENG, 5} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_14_all_units, 7);

  }

  ec_trace_wait_epoch_end(0xa);

  {
    static const LL_Switch_DeinitTypeDef STREAM_SWITCH_0_deinit_in_14[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 3, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32 IN: in unit=CONV_ACC_V2 3 in port=0 out unit=STREAM_ENG_V2 2 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 3, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 5, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32 IN: in unit=CONV_ACC_V2 3 in port=1 out unit=STREAM_ENG_V2 5 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 3, 2), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 4, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32 IN: in unit=CONV_ACC_V2 3 in port=2 out unit=STREAM_ENG_V2 4 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 3, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32 OUT: in unit=STREAM_ENG_V2 3 in port=0 out unit=CONV_ACC_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 3, 0), LL_Switch_Init_Context(0) = 0, LL_Switch_Init_Frames(0) = 1, LL_Switch_Init_Source(1) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 3, 0), LL_Switch_Init_Context(1) = 1, LL_Switch_Init_Frames(1) = 1, }, /* Conv2D_32_off_bias_42 IN: in unit=ARITH_ACC_V2 1 in port=0 out unit=CONV_ACC_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Conv2D_32_off_bias_42 OUT: in unit=STREAM_ENG_V2 1 in port=0 out unit=ARITH_ACC_V2 1 out port=0 */
    };


    /* epoch=14 */
    LL_Switch_Deinit(STREAM_SWITCH_0_deinit_in_14, 6);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_14_all_units[] = {
      { {STRENG, 1} }, /* STREAM_ENG_V2 */
      { {STRENG, 3} }, /* STREAM_ENG_V2 */
      { {CONVACC, 3} }, /* CONV_ACC_V2 */
      { {ARITH, 1} }, /* ARITH_ACC_V2 */
      { {STRENG, 2} }, /* STREAM_ENG_V2 */
      { {STRENG, 4} }, /* STREAM_ENG_V2 */
      { {STRENG, 5} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_14_all_units, 7);

  }
  ec_trace_end_epoch(14);
  ec_trace_start_epoch(15);
  {
    /* Unit= 28 [NULL_UNIT 0] */
    /* kind=Reshape node=Reshape_37 */
    /* node=Reshape_37 satisfies input and output adjacency (DMA->DMA) and can be omitted */

    /* Dma inputs units to cycle: */
    /* Unit= 7 [STREAM_ENG_V2 7] */
    /* Emit conf for STREAM_ENG_V2 node=Reshape_37 input ports=0 range=1[0,224] */

    static const LL_Streng_TensorInitTypeDef Reshape_37_dma_init_in_0_15 = {
      /* memory canonical to batch=1 */
      .dir = 0,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Relu_35_out_0 */
      .offset_start = 0,
      .offset_limit = 288,
      .frame_count = 0,
      .fwidth = 7,
      .fheight = 1,
      .batch_depth = 1,
      .batch_offset = 32,
      .frame_offset = 1,
      .line_offset = 0,
      .loop_offset = 224,
      .frame_loop_cnt = 32,
      .frame_tot_cnt = 32,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(7, &Reshape_37_dma_init_in_0_15, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM5 -> 224 */

    /* Dma output units from cycle: */
    /* Unit= 0 [STREAM_ENG_V2 0] */
    /* Emit conf for STREAM_ENG_V2 node=Reshape_37 output ports=0 range=1[224,448] */

    static const LL_Streng_TensorInitTypeDef Reshape_37_dma_init_out_0_15 = {
      /* to memory canonical from batch=1 */
      .dir = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Reshape_37_out_0 */
      .offset_start = 224,
      .offset_limit = 512,
      .frame_count = 0,
      .fwidth = 1,
      .fheight = 7,
      .batch_depth = 1,
      .batch_offset = 32,
      .frame_offset = 1,
      .line_offset = 0,
      .loop_offset = 224,
      .frame_loop_cnt = 32,
      .frame_tot_cnt = 32,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(0, &Reshape_37_dma_init_out_0_15, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM5 <- 224 */

    static const LL_Switch_InitTypeDef STREAM_SWITCH_0_init_in_15[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 7, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Reshape_37 OUT: in unit=STREAM_ENG_V2 0 in port=0 out unit=STREAM_ENG_V2 7 out port=0 */
    };


    /* epoch=15 */
    LL_Switch_Init(STREAM_SWITCH_0_init_in_15, 1);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_15_all_units[] = {
      { {STRENG, 0} }, /* STREAM_ENG_V2 */
      { {STRENG, 7} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_15_all_units, 2);

  }

  ec_trace_wait_epoch_end(0x1);

  {
    static const LL_Switch_DeinitTypeDef STREAM_SWITCH_0_deinit_in_15[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 7, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Reshape_37 OUT: in unit=STREAM_ENG_V2 0 in port=0 out unit=STREAM_ENG_V2 7 out port=0 */
    };


    /* epoch=15 */
    LL_Switch_Deinit(STREAM_SWITCH_0_deinit_in_15, 1);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_15_all_units[] = {
      { {STRENG, 0} }, /* STREAM_ENG_V2 */
      { {STRENG, 7} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_15_all_units, 2);

  }
  ec_trace_end_epoch(15);
  ec_trace_start_epoch(16);
  {
    /* Unit= 22 [POOL_ACC_V2 0] */
    /* kind=AveragePool node=AveragePool_38 */
    static const LL_Poolacc_InitTypeDef AveragePool_38_init16 = {
      .operation = POOL_GAVG,
      .avgnopad = 0,
      .inputX = 1,
      .inputY = 7,
      .outputX = 2,
      .outputY = 8,
      .poolWinX = 0,
      .poolWinY = 0,
      .strideX = 1,
      .strideY = 1,
      .topCrop = 0,
      .bottomCrop = 6,
      .leftCrop = 0,
      .rightCrop = 0,
      .topPad = 0,
      .bottomPad = 0,
      .leftPad = 0,
      .rightPad = 0,
      .batchSize = 1,
      .shift_f = 0,
      .shift_o = 9,
      .dualLine = 1,
      .rounding_f = 0,
      .saturation_f = 0,
      .round_mode_f = 0,
      .inbytes_f = 1,
      .outbytes_f = 1,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 2,
      .mulval = 18725,
      .pad_val_en = 0,
      .pad_val = 0,
    };

    /* Unit=POOL_ACC_V2 */
    LL_Poolacc_Init(0, &AveragePool_38_init16);


    /* Unit= 20 [ARITH_ACC_V2 2] */
    /* kind=Add node=RequantizeLinear_inserted_id240 */
    static const LL_Arithacc_InitTypeDef RequantizeLinear_inserted_id240_init16 = {
      .rounding_x = 0,
      .saturation_x = 0,
      .round_mode_x = 0,
      .inbytes_x = 2,
      .outbytes_x = 2,
      .shift_x = 0,
      .rounding_y = 0,
      .saturation_y = 0,
      .round_mode_y = 0,
      .inbytes_y = 1,
      .outbytes_y = 1,
      .combinebc = 1,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .shift_o = 15,
      .scalar = 1,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_SCALAR,
      .Ax_shift = 8,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 1,
      .fHeight = 1,
      .fChannels = 32,
      .batchDepth = 1,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 21450,
      .B_scalar = -23,
      .C_scalar = (short)58563,
      .A_vector = {0},
      .B_vector = {0},
      .C_vector = {0},
      .vec_precision = {8, 8, 8},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(2, &RequantizeLinear_inserted_id240_init16);


    /* Dma inputs units to cycle: */
    /* Unit= 3 [STREAM_ENG_V2 3] */
    /* Emit conf for STREAM_ENG_V2 node=AveragePool_38 input ports=0 range=1[224,448] */

    static const LL_Streng_TensorInitTypeDef AveragePool_38_dma_init_in_0_16 = {
      /* memory canonical to batch=1 */
      .dir = 0,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Reshape_37_out_0 */
      .offset_start = 224,
      .offset_limit = 512,
      .frame_count = 0,
      .fwidth = 1,
      .fheight = 7,
      .batch_depth = 1,
      .batch_offset = 32,
      .frame_offset = 1,
      .line_offset = 0,
      .loop_offset = 224,
      .frame_loop_cnt = 32,
      .frame_tot_cnt = 32,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(3, &AveragePool_38_dma_init_in_0_16, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM5 -> 224 */

    /* Dma output units from cycle: */
    /* Unit= 9 [STREAM_ENG_V2 9] */
    /* Emit conf for STREAM_ENG_V2 node=RequantizeLinear_inserted_id240 output ports=0 range=1[768,800] */

    static const LL_Streng_TensorInitTypeDef RequantizeLinear_inserted_id240_dma_init_out_0_16 = {
      /* to memory with batch=1 */
      .dir = 1,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* AveragePool_38_out_0_cp_in_1 */
      .offset_start = 768,
      .offset_end = 769,
      .offset_limit = 864,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 1,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 32,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(9, &RequantizeLinear_inserted_id240_dma_init_out_0_16, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM5 <- 32 */

    static const LL_Switch_InitTypeDef STREAM_SWITCH_0_init_in_16[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, POOL, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* AveragePool_38 IN: in unit=POOL_ACC_V2 0 in port=0 out unit=STREAM_ENG_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, POOL, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* RequantizeLinear_inserted_id240 IN: in unit=ARITH_ACC_V2 2 in port=0 out unit=POOL_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 9, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* RequantizeLinear_inserted_id240 OUT: in unit=STREAM_ENG_V2 9 in port=0 out unit=ARITH_ACC_V2 2 out port=0 */
    };


    /* epoch=16 */
    LL_Switch_Init(STREAM_SWITCH_0_init_in_16, 3);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_16_all_units[] = {
      { {STRENG, 9} }, /* STREAM_ENG_V2 */
      { {ARITH, 2} }, /* ARITH_ACC_V2 */
      { {POOL, 0} }, /* POOL_ACC_V2 */
      { {STRENG, 3} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_16_all_units, 4);

  }

  ec_trace_wait_epoch_end(0x200);

  {
    static const LL_Switch_DeinitTypeDef STREAM_SWITCH_0_deinit_in_16[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, POOL, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* AveragePool_38 IN: in unit=POOL_ACC_V2 0 in port=0 out unit=STREAM_ENG_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, POOL, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* RequantizeLinear_inserted_id240 IN: in unit=ARITH_ACC_V2 2 in port=0 out unit=POOL_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 9, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* RequantizeLinear_inserted_id240 OUT: in unit=STREAM_ENG_V2 9 in port=0 out unit=ARITH_ACC_V2 2 out port=0 */
    };


    /* epoch=16 */
    LL_Switch_Deinit(STREAM_SWITCH_0_deinit_in_16, 3);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_16_all_units[] = {
      { {STRENG, 9} }, /* STREAM_ENG_V2 */
      { {ARITH, 2} }, /* ARITH_ACC_V2 */
      { {POOL, 0} }, /* POOL_ACC_V2 */
      { {STRENG, 3} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_16_all_units, 4);

  }
  ec_trace_end_epoch(16);
  ec_trace_start_epoch(17);
  {
  }
  {
  }
  ec_trace_end_epoch(17);
  ec_trace_start_epoch(18);
  {
    /* Unit= 21 [ARITH_ACC_V2 3] */
    /* kind=Sub node=Gemm_49_conv_10_suboff_54 */
    static const LL_Arithacc_InitTypeDef Gemm_49_conv_10_suboff_54_init18 = {
      .rounding_x = 0,
      .saturation_x = 0,
      .round_mode_x = 0,
      .inbytes_x = 1,
      .outbytes_x = 1,
      .shift_x = 0,
      .rounding_y = 0,
      .saturation_y = 0,
      .round_mode_y = 0,
      .inbytes_y = 1,
      .outbytes_y = 1,
      .combinebc = 1,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 0,
      .saturation_o = 0,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .shift_o = 14,
      .scalar = 1,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_SCALAR,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 1,
      .fHeight = 1,
      .fChannels = 40,
      .batchDepth = 40,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 16384,
      .B_scalar = 32,
      .C_scalar = (short)0,
      .A_vector = {0},
      .B_vector = {0},
      .C_vector = {0},
      .vec_precision = {8, 8, 8},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(3, &Gemm_49_conv_10_suboff_54_init18);


    /* Dma inputs units to cycle: */
    /* Unit= 9 [STREAM_ENG_V2 9] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_49_conv_10_suboff_54 input ports=0 range=1[768,808] */

    static const LL_Streng_TensorInitTypeDef Gemm_49_conv_10_suboff_54_dma_init_in_0_18 = {
      /* from memory with batch=40 */
      .dir = 0,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Gemm_49_reshape_x_8 */
      .offset_start = 768,
      .offset_end = 808,
      .offset_limit = 872,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 40,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(9, &Gemm_49_conv_10_suboff_54_dma_init_in_0_18, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM5 -> 40 */

    /* Dma output units from cycle: */
    /* Unit= 2 [STREAM_ENG_V2 2] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_49_conv_10_suboff_54 output ports=0 range=1[64,104] */

    static const LL_Streng_TensorInitTypeDef Gemm_49_conv_10_suboff_54_dma_init_out_0_18 = {
      /* to memory with batch=40 */
      .dir = 1,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 1,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Gemm_49_conv_10_zero_off_out_55 */
      .offset_start = 64,
      .offset_end = 104,
      .offset_limit = 168,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 40,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(2, &Gemm_49_conv_10_suboff_54_dma_init_out_0_18, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM5 <- 40 */

    static const LL_Switch_InitTypeDef STREAM_SWITCH_0_init_in_18[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 3, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 9, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_49_conv_10_suboff_54 IN: in unit=ARITH_ACC_V2 3 in port=0 out unit=STREAM_ENG_V2 9 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_49_conv_10_suboff_54 OUT: in unit=STREAM_ENG_V2 2 in port=0 out unit=ARITH_ACC_V2 3 out port=0 */
    };


    /* epoch=18 */
    LL_Switch_Init(STREAM_SWITCH_0_init_in_18, 2);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_18_all_units[] = {
      { {STRENG, 2} }, /* STREAM_ENG_V2 */
      { {ARITH, 3} }, /* ARITH_ACC_V2 */
      { {STRENG, 9} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_18_all_units, 3);

  }

  ec_trace_wait_epoch_end(0x4);

  {
    static const LL_Switch_DeinitTypeDef STREAM_SWITCH_0_deinit_in_18[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 3, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 9, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_49_conv_10_suboff_54 IN: in unit=ARITH_ACC_V2 3 in port=0 out unit=STREAM_ENG_V2 9 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_49_conv_10_suboff_54 OUT: in unit=STREAM_ENG_V2 2 in port=0 out unit=ARITH_ACC_V2 3 out port=0 */
    };


    /* epoch=18 */
    LL_Switch_Deinit(STREAM_SWITCH_0_deinit_in_18, 2);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_18_all_units[] = {
      { {STRENG, 2} }, /* STREAM_ENG_V2 */
      { {ARITH, 3} }, /* ARITH_ACC_V2 */
      { {STRENG, 9} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_18_all_units, 3);

  }
  ec_trace_end_epoch(18);
  ec_trace_start_epoch(19);
  {
    /* Unit= 10 [CONV_ACC_V2 0] */
    /* kind=Conv node=Gemm_49_conv_10 */
    static const LL_Convacc_InitTypeDef Gemm_49_conv_10_init19 = {
      .simd = 1,
      .fsub = 0,
      .vshift = 0,
      .accumulate = 0,
      .rounding_f = 0,
      .saturation_f = 0,
      .round_mode_f = 0,
      .f_unsigned = 1,
      .k_unsigned = 0,
      .deepmode = 1,
      .dss2mode = 0,
      .kseten = 3,
      .zfbias = 0,
      .inbytes_f = 3,
      .shift_f = 0,
      .shift_a = 4,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 2,
      .shift_o = 4,
      .raw_o = 0,
      .fWidth = 1,
      .fHeight = 1,
      .kernelWidth = 1,
      .kernelHeight = 1,
      .nKernels = 16,
      .batchDepth = 40,
      .hstride = 1,
      .vstride = 1,
      .left_padding = 0,
      .right_padding = 0,
      .top_padding = 0,
      .bot_padding = 0,
      .left_crop = 0,
      .right_crop = 0,
      .top_crop = 0,
      .bot_crop = 0,
    };

    /* Unit=CONV_ACC_V2 */
    LL_Convacc_Init(0, &Gemm_49_conv_10_init19);


    /* Unit= 18 [ARITH_ACC_V2 0] */
    /* kind=Add node=Gemm_49_conv_10_off_bias_60 */
    static const LL_Arithacc_InitTypeDef Gemm_49_conv_10_off_bias_60_init19 = {
      .rounding_x = 0,
      .saturation_x = 0,
      .round_mode_x = 0,
      .inbytes_x = 2,
      .outbytes_x = 2,
      .shift_x = 0,
      .rounding_y = 0,
      .saturation_y = 0,
      .round_mode_y = 0,
      .inbytes_y = 2,
      .outbytes_y = 2,
      .combinebc = 1,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .shift_o = 16,
      .scalar = 0,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_CHAN,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 1,
      .fHeight = 1,
      .fChannels = 64,
      .batchDepth = 16,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 0,
      .B_scalar = 0,
      .C_scalar = (short)0,
      .A_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 512))) /* Equivalent hex offset = 0x200 */},
      .B_vector = {0},
      .C_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 0))) /* Equivalent hex offset = 0x0 */},
      .vec_precision = {16, 16, 32},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(0, &Gemm_49_conv_10_off_bias_60_init19);


    /* Unit= 19 [ARITH_ACC_V2 1] */
    /* kind=Sub node=Gemm_53_conv_16_suboff_63 */
    static const LL_Arithacc_InitTypeDef Gemm_53_conv_16_suboff_63_init19 = {
      .rounding_x = 0,
      .saturation_x = 0,
      .round_mode_x = 0,
      .inbytes_x = 1,
      .outbytes_x = 1,
      .shift_x = 0,
      .rounding_y = 0,
      .saturation_y = 0,
      .round_mode_y = 0,
      .inbytes_y = 1,
      .outbytes_y = 1,
      .combinebc = 1,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 0,
      .saturation_o = 0,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .shift_o = 14,
      .scalar = 1,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_SCALAR,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 1,
      .fHeight = 1,
      .fChannels = 64,
      .batchDepth = 16,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 16384,
      .B_scalar = 32,
      .C_scalar = (short)0,
      .A_vector = {0},
      .B_vector = {0},
      .C_vector = {0},
      .vec_precision = {8, 8, 8},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(1, &Gemm_53_conv_16_suboff_63_init19);


    /* Dma inputs units to cycle: */
    /* Unit= 3 [STREAM_ENG_V2 3] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_49_conv_10 input ports=0 range=1[64,104] */

    static const LL_Streng_TensorInitTypeDef Gemm_49_conv_10_dma_init_in_0_19 = {
      /* 1x1x40(8 bits) */
      .dir = 0,
      .raw = 1,
      .raw_out = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 1,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Gemm_49_conv_10_zero_off_out_55 */
      .offset_start = 64,
      .offset_end = 106,
      .offset_limit = 168,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 0,
      .line_offset = 0,
      .loop_offset = 40,
      .frame_loop_cnt = 4,
      .frame_tot_cnt = 4,
      .nbits_in = 24,
      .nbits_out = 24,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(3, &Gemm_49_conv_10_dma_init_in_0_19, 1);

    /* Unit= 7 [STREAM_ENG_V2 7] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_49_conv_10 input ports=1 range=5[3072,5640] */

    static const LL_Streng_TensorInitTypeDef Gemm_49_conv_10_dma_init_in_1_19 = {
      /* 64x1x1x40(8 bits) */
      .dir = 0,
      .raw = 1,
      .continuous = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .cacheable = 1,
      .cache_allocate = 0,
      .addr_base = {(unsigned char *)(0x70400000UL) /* Equivalent hex address = 0x70400000UL */}, /* Gemm_49_weights_transposed_9 */
      .offset_start = 3072,
      .offset_end = 5640,
      .offset_limit = 5704,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 0,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 24,
      .nbits_out = 24,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(7, &Gemm_49_conv_10_dma_init_in_1_19, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM5 -> 160 */
    /* octoFlash -> 2560 */

    /* Dma output units from cycle: */
    /* Unit= 6 [STREAM_ENG_V2 6] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_53_conv_16_suboff_63 output ports=0 range=1[0,64] */

    static const LL_Streng_TensorInitTypeDef Gemm_53_conv_16_suboff_63_dma_init_out_0_19 = {
      /* to memory with batch=16 */
      .dir = 1,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 1,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Gemm_53_conv_16_zero_off_out_64 */
      .offset_start = 0,
      .offset_end = 16,
      .offset_limit = 128,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 16,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 4,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(6, &Gemm_53_conv_16_suboff_63_dma_init_out_0_19, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM5 <- 64 */

    static const LL_Switch_InitTypeDef STREAM_SWITCH_0_init_in_19[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_49_conv_10 IN: in unit=CONV_ACC_V2 0 in port=0 out unit=STREAM_ENG_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 7, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_49_conv_10 IN: in unit=CONV_ACC_V2 0 in port=1 out unit=STREAM_ENG_V2 7 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_49_conv_10_off_bias_60 IN: in unit=ARITH_ACC_V2 0 in port=0 out unit=CONV_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_53_conv_16_suboff_63 IN: in unit=ARITH_ACC_V2 1 in port=0 out unit=ARITH_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 6, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_53_conv_16_suboff_63 OUT: in unit=STREAM_ENG_V2 6 in port=0 out unit=ARITH_ACC_V2 1 out port=0 */
    };


    /* epoch=19 */
    LL_Switch_Init(STREAM_SWITCH_0_init_in_19, 5);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_19_all_units[] = {
      { {STRENG, 6} }, /* STREAM_ENG_V2 */
      { {CONVACC, 0} }, /* CONV_ACC_V2 */
      { {ARITH, 0} }, /* ARITH_ACC_V2 */
      { {ARITH, 1} }, /* ARITH_ACC_V2 */
      { {STRENG, 3} }, /* STREAM_ENG_V2 */
      { {STRENG, 7} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_19_all_units, 6);

  }

  ec_trace_wait_epoch_end(0x40);

  {
    static const LL_Switch_DeinitTypeDef STREAM_SWITCH_0_deinit_in_19[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_49_conv_10 IN: in unit=CONV_ACC_V2 0 in port=0 out unit=STREAM_ENG_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 7, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_49_conv_10 IN: in unit=CONV_ACC_V2 0 in port=1 out unit=STREAM_ENG_V2 7 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_49_conv_10_off_bias_60 IN: in unit=ARITH_ACC_V2 0 in port=0 out unit=CONV_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_53_conv_16_suboff_63 IN: in unit=ARITH_ACC_V2 1 in port=0 out unit=ARITH_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 6, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_53_conv_16_suboff_63 OUT: in unit=STREAM_ENG_V2 6 in port=0 out unit=ARITH_ACC_V2 1 out port=0 */
    };


    /* epoch=19 */
    LL_Switch_Deinit(STREAM_SWITCH_0_deinit_in_19, 5);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_19_all_units[] = {
      { {STRENG, 6} }, /* STREAM_ENG_V2 */
      { {CONVACC, 0} }, /* CONV_ACC_V2 */
      { {ARITH, 0} }, /* ARITH_ACC_V2 */
      { {ARITH, 1} }, /* ARITH_ACC_V2 */
      { {STRENG, 3} }, /* STREAM_ENG_V2 */
      { {STRENG, 7} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_19_all_units, 6);

  }
  ec_trace_end_epoch(19);
  ec_trace_start_epoch(20);
  {
    /* Unit= 11 [CONV_ACC_V2 1] */
    /* kind=Conv node=Gemm_53_conv_16 */
    static const LL_Convacc_InitTypeDef Gemm_53_conv_16_init20 = {
      .simd = 1,
      .fsub = 0,
      .vshift = 0,
      .accumulate = 0,
      .rounding_f = 0,
      .saturation_f = 0,
      .round_mode_f = 0,
      .f_unsigned = 1,
      .k_unsigned = 0,
      .deepmode = 1,
      .dss2mode = 0,
      .kseten = 3,
      .zfbias = 0,
      .inbytes_f = 3,
      .shift_f = 0,
      .shift_a = 3,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 2,
      .shift_o = 3,
      .raw_o = 0,
      .fWidth = 1,
      .fHeight = 1,
      .kernelWidth = 1,
      .kernelHeight = 1,
      .nKernels = 7,
      .batchDepth = 64,
      .hstride = 1,
      .vstride = 1,
      .left_padding = 0,
      .right_padding = 0,
      .top_padding = 0,
      .bot_padding = 0,
      .left_crop = 0,
      .right_crop = 0,
      .top_crop = 0,
      .bot_crop = 0,
    };

    /* Unit=CONV_ACC_V2 */
    LL_Convacc_Init(1, &Gemm_53_conv_16_init20);


    /* Unit= 20 [ARITH_ACC_V2 2] */
    /* kind=Add node=Gemm_53_conv_16_off_bias_69 */
    static const LL_Arithacc_InitTypeDef Gemm_53_conv_16_off_bias_69_init20 = {
      .rounding_x = 0,
      .saturation_x = 0,
      .round_mode_x = 0,
      .inbytes_x = 2,
      .outbytes_x = 2,
      .shift_x = 0,
      .rounding_y = 0,
      .saturation_y = 0,
      .round_mode_y = 0,
      .inbytes_y = 2,
      .outbytes_y = 2,
      .combinebc = 1,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 1,
      .shift_o = 20,
      .scalar = 0,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_CHAN,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 1,
      .fHeight = 1,
      .fChannels = 7,
      .batchDepth = 7,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 0,
      .B_scalar = 0,
      .C_scalar = (short)0,
      .A_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 944))) /* Equivalent hex offset = 0x3b0 */},
      .B_vector = {0},
      .C_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 896))) /* Equivalent hex offset = 0x380 */},
      .vec_precision = {16, 16, 32},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(2, &Gemm_53_conv_16_off_bias_69_init20);


    /* Dma inputs units to cycle: */
    /* Unit= 9 [STREAM_ENG_V2 9] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_53_conv_16 input ports=0 range=1[0,64] */

    static const LL_Streng_TensorInitTypeDef Gemm_53_conv_16_dma_init_in_0_20 = {
      /* 1x1x64(8 bits) */
      .dir = 0,
      .raw = 1,
      .raw_out = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 1,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Gemm_53_conv_16_zero_off_out_64 */
      .offset_start = 0,
      .offset_end = 66,
      .offset_limit = 128,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 0,
      .line_offset = 0,
      .loop_offset = 64,
      .frame_loop_cnt = 1,
      .frame_tot_cnt = 1,
      .nbits_in = 24,
      .nbits_out = 24,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(9, &Gemm_53_conv_16_dma_init_in_0_20, 1);

    /* Unit= 2 [STREAM_ENG_V2 2] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_53_conv_16 input ports=1 range=5[7184,7634] */

    static const LL_Streng_TensorInitTypeDef Gemm_53_conv_16_dma_init_in_1_20 = {
      /* 7x1x1x64(8 bits) */
      .dir = 0,
      .raw = 1,
      .continuous = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .cacheable = 1,
      .cache_allocate = 0,
      .addr_base = {(unsigned char *)(0x70400000UL) /* Equivalent hex address = 0x70400000UL */}, /* Gemm_53_weights_transposed_15 */
      .offset_start = 7184,
      .offset_end = 7634,
      .offset_limit = 7704,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 0,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 24,
      .nbits_out = 24,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(2, &Gemm_53_conv_16_dma_init_in_1_20, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM5 -> 64 */
    /* octoFlash -> 448 */

    /* Dma output units from cycle: */
    /* Unit= 1 [STREAM_ENG_V2 1] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_53_conv_16_off_bias_69 output ports=0 range=1[512,519] */

    static const LL_Streng_TensorInitTypeDef Gemm_53_conv_16_off_bias_69_dma_init_out_0_20 = {
      /* to memory with batch=7 */
      .dir = 1,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x342e0000UL) /* Equivalent hex address = 0x342e0000UL */}, /* Gemm_53_conv_16_off_bias_out_70 */
      .offset_start = 512,
      .offset_end = 519,
      .offset_limit = 584,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 7,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(1, &Gemm_53_conv_16_off_bias_69_dma_init_out_0_20, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM5 <- 7 */

    static const LL_Switch_InitTypeDef STREAM_SWITCH_0_init_in_20[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 9, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_53_conv_16 IN: in unit=CONV_ACC_V2 1 in port=0 out unit=STREAM_ENG_V2 9 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 1, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_53_conv_16 IN: in unit=CONV_ACC_V2 1 in port=1 out unit=STREAM_ENG_V2 2 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_53_conv_16_off_bias_69 IN: in unit=ARITH_ACC_V2 2 in port=0 out unit=CONV_ACC_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_53_conv_16_off_bias_69 OUT: in unit=STREAM_ENG_V2 1 in port=0 out unit=ARITH_ACC_V2 2 out port=0 */
    };


    /* epoch=20 */
    LL_Switch_Init(STREAM_SWITCH_0_init_in_20, 4);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_20_all_units[] = {
      { {STRENG, 1} }, /* STREAM_ENG_V2 */
      { {CONVACC, 1} }, /* CONV_ACC_V2 */
      { {ARITH, 2} }, /* ARITH_ACC_V2 */
      { {STRENG, 2} }, /* STREAM_ENG_V2 */
      { {STRENG, 9} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_20_all_units, 5);

  }

  ec_trace_wait_epoch_end(0x2);

  {
    static const LL_Switch_DeinitTypeDef STREAM_SWITCH_0_deinit_in_20[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 9, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_53_conv_16 IN: in unit=CONV_ACC_V2 1 in port=0 out unit=STREAM_ENG_V2 9 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 1, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_53_conv_16 IN: in unit=CONV_ACC_V2 1 in port=1 out unit=STREAM_ENG_V2 2 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_53_conv_16_off_bias_69 IN: in unit=ARITH_ACC_V2 2 in port=0 out unit=CONV_ACC_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_53_conv_16_off_bias_69 OUT: in unit=STREAM_ENG_V2 1 in port=0 out unit=ARITH_ACC_V2 2 out port=0 */
    };


    /* epoch=20 */
    LL_Switch_Deinit(STREAM_SWITCH_0_deinit_in_20, 4);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_20_all_units[] = {
      { {STRENG, 1} }, /* STREAM_ENG_V2 */
      { {CONVACC, 1} }, /* CONV_ACC_V2 */
      { {ARITH, 2} }, /* ARITH_ACC_V2 */
      { {STRENG, 2} }, /* STREAM_ENG_V2 */
      { {STRENG, 9} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_20_all_units, 5);

  }
  ec_trace_end_epoch(20);
  ec_trace_end_blob("_ec_blob_workclf_12");
}


int main () {
  ec_trace_init("workclf_ecblobs.h", "workclf", false, 0, false);
  trace_ec__ec_blob_workclf_3();
  trace_ec__ec_blob_workclf_5();
  trace_ec__ec_blob_workclf_8();
  trace_ec__ec_blob_workclf_12();
  ec_trace_all_blobs_done();
}
