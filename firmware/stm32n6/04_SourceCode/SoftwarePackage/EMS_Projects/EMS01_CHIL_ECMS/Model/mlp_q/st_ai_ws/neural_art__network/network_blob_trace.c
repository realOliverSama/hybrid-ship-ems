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

#define _mem_pool__user_io_input_0_Default 1
#define _mem_pool__user_io_output_0_Default 3

mpool_reloc_info_t mpool_reloc_info[] = {
  {"AXISRAM6", "_mem_pool_AXISRAM6_Default", 0x34350000, 1, 0},
  {"AXISRAM5", "_mem_pool_AXISRAM5_Default", 0x342e0000, 1, 0},
  {"AXISRAM4", "_mem_pool_AXISRAM4_Default", 0x34270000, 1, 0},
  {"AXISRAM3", "_mem_pool_AXISRAM3_Default", 0x34200000, 1, 0},
  {"xSPI1", "_mem_pool_xSPI1_Default", 0x90000000, 1, 0},
  {"xSPI2", "_mem_pool_xSPI2_Default", 0x70200000, 1, 0},
  {"AXISRAM3_AXISRAM4_AXISRAM5_AXISRAM6", "_mem_pool_AXISRAM3_AXISRAM4_AXISRAM5_AXISRAM6_Default", 0x34200000, 1, 0},
  {"_user_io_input_0", "_mem_pool__user_io_input_0_Default", 0x1, 0, 1},
  {"_user_io_output_0", "_mem_pool__user_io_output_0_Default", 0x3, 0, 1},
  {NULL, NULL, 0, 0, 0}
};


static const uint64_t __ec_params_memory_pool_base[79] = {
0xF81C92C1F7BB8CE8ULL,0xF8371B9DF7D1C33DULL,0xF7F71FC1F7FDE840ULL,0xF80294ABF80493C9ULL,0xF801E927F8079BB1ULL,0xF7811C02F815D779ULL,0xF7DC7D57F821CE9BULL,0xF7FDCD5DF83A4BEBULL,
0xF7EA74D6F8161B7CULL,0xF7EFD62EF7FFF235ULL,0xF809E37EF807BB46ULL,0xF7FF884FF7E700D6ULL,0xF820B4D8F800FEF7ULL,0xF82E261EF811A40FULL,0xF8247C57F7B18E6FULL,0xF7EAA282F8081461ULL,
0xF82AC0E8F80FB035ULL,0xF81A5699F7ED935BULL,0xF808AD38F80751C5ULL,0xF7B56788F7F407E4ULL,0xF819F109F7FC7455ULL,0xF7C25878F7F80DD5ULL,0xF7D18170F828BE08ULL,0xF802B5FAF7F01416ULL,
0xF803B726F80D2D4BULL,0xF7EA2A97F80D1891ULL,0xF7DE11F5F8052239ULL,0xF7F10823F7B5ECB8ULL,0xF7E076DBF7EE06E3ULL,0xF7E6B6EFF80D0EFFULL,0xF7F954E2F825CF61ULL,0xF7E65796F8065A56ULL,
0x36B517D9186E173EULL,0x154C14E317EE35AEULL,0x11C822521FD02A3AULL,0x28E614F12D6B2745ULL,0x253C3320294E3452ULL,0x1FB223E4383219D6ULL,0x1E353ABE24C72D03ULL,0x3442136A179615DDULL,
0x2149190D175F1E63ULL,0x132C1B922CC112A0ULL,0x3A1B3A023AE62D79ULL,0x184517C231A31F35ULL,0x2231296E1D244179ULL,0x206F18441A972212ULL,0x3877118112911BB1ULL,0x1B1E1A7212B930D2ULL,
0xF7F6B8ADF7E33396ULL,0xF80B3826F84A7027ULL,0xF87057A0F814A9EAULL,0xF7F64801F7CD49FAULL,0xF820E4C0F86571A4ULL,0xF8142BCEF7BE79AEULL,0xF81B04C7F7FAE708ULL,0xF82CBE70F8006D95ULL,
0xF7F9298CF7C6A1A0ULL,0xF811FEAFF810BF06ULL,0xF809235CF7D25F71ULL,0xF8005A19F81BCF13ULL,0xF8169607F7EF8419ULL,0xF7EF0135F809914FULL,0xF7EFA877F8088D69ULL,0xF831DFE0F7FF6C5BULL,
0x0F402CEC0F7D1AA5ULL,0x13C51D4752291A72ULL,0x10232027225E434DULL,0x18FD0E9C3ADC1573ULL,0x26A718090F1C159FULL,0x12050DB611A70F0AULL,0x14003BFA131D16CFULL,0x16F8101B0F2A15B6ULL,
0xFE742F52FE85122CULL,0x00000000FE73FFFAULL,0x00007DF66EFA6D73ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
};

void trace_ec__ec_blob_Default_2(void) {
  ec_trace_start_blob("_ec_blob_Default_2");
  ec_trace_start_epoch(2);
  {
  }
  {
  }
  ec_trace_end_epoch(2);
  ec_trace_start_epoch(3);
  {
    /* Unit= 10 [CONV_ACC_V2 0] */
    /* kind=Conv node=Gemm_3_conv_4 */
    static const LL_Convacc_InitTypeDef Gemm_3_conv_4_init3 = {
      .simd = 2,
      .fsub = -19,
      .accumulate = 1,
      .accumulate_gen_first = 1,
      .accumulate_first = 1,
      .afilt_mode = AFILT_MODE_FRAMEZERO,
      .afilt_tot = 151,
      .afilt_first = 1,
      .afilt_last = 150,
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
      .shift_a = 5,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 2,
      .shift_o = 5,
      .raw_o = 0,
      .fWidth = 1,
      .fHeight = 1,
      .kernelWidth = 1,
      .kernelHeight = 1,
      .nKernels = 16,
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
    LL_Convacc_Init(0, &Gemm_3_conv_4_init3);


    /* Unit= 18 [ARITH_ACC_V2 0] */
    /* kind=Mul node=Gemm_3_conv_4_mul_scale_22 */
    static const LL_Arithacc_InitTypeDef Gemm_3_conv_4_mul_scale_22_init3 = {
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
      .combinebc = 0,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 2,
      .shift_o = 11,
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
      .A_scalar = 1,
      .B_scalar = 0,
      .C_scalar = 0,
      .A_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 256))) /* Equivalent hex offset = 0x100 */},
      .B_vector = {0},
      .C_vector = {0},
      .vec_precision = {16, 16, 16},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(0, &Gemm_3_conv_4_mul_scale_22_init3);


    /* Unit= 19 [ARITH_ACC_V2 1] */
    /* kind=Add node=Gemm_3_conv_4_off_bias_24 */
    static const LL_Arithacc_InitTypeDef Gemm_3_conv_4_off_bias_24_init3 = {
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
      .fChannels = 64,
      .batchDepth = 16,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 17678,
      .B_scalar = 0,
      .C_scalar = (short)0,
      .A_vector = {0},
      .B_vector = {0},
      .C_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 0))) /* Equivalent hex offset = 0x0 */},
      .vec_precision = {16, 16, 32},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(1, &Gemm_3_conv_4_off_bias_24_init3);


    /* Unit= 20 [ARITH_ACC_V2 2] */
    /* kind=Sub node=Gemm_5_conv_10_suboff_27 */
    static const LL_Arithacc_InitTypeDef Gemm_5_conv_10_suboff_27_init3 = {
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
      .bcast = ARITH_BCAST_HEIGHT,
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
    LL_Arithacc_Init(2, &Gemm_5_conv_10_suboff_27_init3);


    /* Dma inputs units to cycle: */
    /* Unit= 5 [STREAM_ENG_V2 5] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_3_conv_4 input ports=0 range=8[0,302] */

    static const LL_Streng_TensorInitTypeDef Gemm_3_conv_4_dma_init_in_0_3 = {
      .dir = 0,
      .noblk = 0,
      .align_right = 1,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x34200000UL) /* Equivalent hex address = 0x34200000UL */}, /* Gemm_3_conv_4_zero_off_out_19 */
      .offset_start = 0,
      .offset_limit = 368,
      .frame_count = 0,
      .fwidth = 1,
      .fheight = 1,
      .batch_depth = 2,
      .batch_offset = 302,
      .frame_offset = 2,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 151,
      .frame_tot_cnt = 604,
      .nbits_in = 8,
      .nbits_out = 16,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(5, &Gemm_3_conv_4_dma_init_in_0_3, 1);

    /* Unit= 1 [STREAM_ENG_V2 1] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_3_conv_4 input ports=1 range=5[0,19328] */

    static const LL_Streng_TensorInitTypeDef Gemm_3_conv_4_dma_init_in_1_3 = {
      /* 64x1x1x302(8 bits) */
      .dir = 0,
      .raw = 1,
      .continuous = 1,
      .noblk = 0,
      .align_right = 1,
      .nbits_unsigned = 0,
      .cacheable = 1,
      .cache_allocate = 0,
      .addr_base = {(unsigned char *)(0x70200000UL) /* Equivalent hex address = 0x70200000UL */}, /* Gemm_3_weights_transposed_3 */
      .offset_start = 0,
      .offset_end = 19328,
      .offset_limit = 19392,
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
    LL_Streng_TensorInit(1, &Gemm_3_conv_4_dma_init_in_1_3, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM3 -> 302 */
    /* octoFlash -> 19328 */

    /* Dma output units from cycle: */
    /* Unit= 0 [STREAM_ENG_V2 0] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_5_conv_10_suboff_27 output ports=0 range=8[304,368] */

    static const LL_Streng_TensorInitTypeDef Gemm_5_conv_10_suboff_27_dma_init_out_0_3 = {
      /* to memory with batch=16 */
      .dir = 1,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 1,
      .addr_base = {(unsigned char *)(0x34200000UL) /* Equivalent hex address = 0x34200000UL */}, /* Gemm_5_conv_10_zero_off_out_28 */
      .offset_start = 304,
      .offset_end = 320,
      .offset_limit = 432,
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
    LL_Streng_TensorInit(0, &Gemm_5_conv_10_suboff_27_dma_init_out_0_3, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM3 <- 64 */

    static const LL_Switch_InitTypeDef switch_init_in_3[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 5, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_3_conv_4 IN: in unit=CONV_ACC_V2 0 in port=0 out unit=STREAM_ENG_V2 5 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_3_conv_4 IN: in unit=CONV_ACC_V2 0 in port=1 out unit=STREAM_ENG_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 2), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_3_conv_4 IN: in unit=CONV_ACC_V2 0 in port=2 out unit=CONV_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Context(0) = 0, LL_Switch_Init_Frames(0) = 150, LL_Switch_Init_Source(1) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Context(1) = 1, LL_Switch_Init_Frames(1) = 1, }, /* Gemm_3_conv_4_mul_scale_22 IN: in unit=ARITH_ACC_V2 0 in port=0 out unit=CONV_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_3_conv_4_off_bias_24 IN: in unit=ARITH_ACC_V2 1 in port=0 out unit=ARITH_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_5_conv_10_suboff_27 IN: in unit=ARITH_ACC_V2 2 in port=0 out unit=ARITH_ACC_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_5_conv_10_suboff_27 OUT: in unit=STREAM_ENG_V2 0 in port=0 out unit=ARITH_ACC_V2 2 out port=0 */
    };


    /* epoch=3 */
    LL_Switch_Init(switch_init_in_3, 7);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_3_all_units[] = {
      { {STRENG, 0} }, /* STREAM_ENG_V2 */
      { {CONVACC, 0} }, /* CONV_ACC_V2 */
      { {ARITH, 0} }, /* ARITH_ACC_V2 */
      { {ARITH, 1} }, /* ARITH_ACC_V2 */
      { {ARITH, 2} }, /* ARITH_ACC_V2 */
      { {STRENG, 1} }, /* STREAM_ENG_V2 */
      { {STRENG, 5} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_3_all_units, 7);

  }

  ec_trace_wait_epoch_end(0x1);

  {
    static const LL_Switch_DeinitTypeDef switch_deinit_in_3[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 5, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_3_conv_4 IN: in unit=CONV_ACC_V2 0 in port=0 out unit=STREAM_ENG_V2 5 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_3_conv_4 IN: in unit=CONV_ACC_V2 0 in port=1 out unit=STREAM_ENG_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 0, 2), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_3_conv_4 IN: in unit=CONV_ACC_V2 0 in port=2 out unit=CONV_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Context(0) = 0, LL_Switch_Init_Frames(0) = 150, LL_Switch_Init_Source(1) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 0, 0), LL_Switch_Init_Context(1) = 1, LL_Switch_Init_Frames(1) = 1, }, /* Gemm_3_conv_4_mul_scale_22 IN: in unit=ARITH_ACC_V2 0 in port=0 out unit=CONV_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_3_conv_4_off_bias_24 IN: in unit=ARITH_ACC_V2 1 in port=0 out unit=ARITH_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_5_conv_10_suboff_27 IN: in unit=ARITH_ACC_V2 2 in port=0 out unit=ARITH_ACC_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_5_conv_10_suboff_27 OUT: in unit=STREAM_ENG_V2 0 in port=0 out unit=ARITH_ACC_V2 2 out port=0 */
    };


    /* epoch=3 */
    LL_Switch_Deinit(switch_deinit_in_3, 7);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_3_all_units[] = {
      { {STRENG, 0} }, /* STREAM_ENG_V2 */
      { {CONVACC, 0} }, /* CONV_ACC_V2 */
      { {ARITH, 0} }, /* ARITH_ACC_V2 */
      { {ARITH, 1} }, /* ARITH_ACC_V2 */
      { {ARITH, 2} }, /* ARITH_ACC_V2 */
      { {STRENG, 1} }, /* STREAM_ENG_V2 */
      { {STRENG, 5} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_3_all_units, 7);

  }
  ec_trace_end_epoch(3);
  ec_trace_start_epoch(4);
  {
    /* Unit= 11 [CONV_ACC_V2 1] */
    /* kind=Conv node=Gemm_5_conv_10 */
    static const LL_Convacc_InitTypeDef Gemm_5_conv_10_init4 = {
      .simd = 1,
      .fsub = 0,
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
    LL_Convacc_Init(1, &Gemm_5_conv_10_init4);


    /* Unit= 21 [ARITH_ACC_V2 3] */
    /* kind=Mul node=Gemm_5_conv_10_mul_scale_31 */
    static const LL_Arithacc_InitTypeDef Gemm_5_conv_10_mul_scale_31_init4 = {
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
      .combinebc = 0,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 2,
      .shift_o = 11,
      .scalar = 0,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_CHAN,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 1,
      .fHeight = 1,
      .fChannels = 32,
      .batchDepth = 16,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 1,
      .B_scalar = 0,
      .C_scalar = 0,
      .A_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 512))) /* Equivalent hex offset = 0x200 */},
      .B_vector = {0},
      .C_vector = {0},
      .vec_precision = {16, 16, 16},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(3, &Gemm_5_conv_10_mul_scale_31_init4);


    /* Unit= 18 [ARITH_ACC_V2 0] */
    /* kind=Add node=Gemm_5_conv_10_off_bias_33 */
    static const LL_Arithacc_InitTypeDef Gemm_5_conv_10_off_bias_33_init4 = {
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
      .fChannels = 32,
      .batchDepth = 16,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 27310,
      .B_scalar = 0,
      .C_scalar = (short)0,
      .A_vector = {0},
      .B_vector = {0},
      .C_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 384))) /* Equivalent hex offset = 0x180 */},
      .vec_precision = {16, 16, 32},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(0, &Gemm_5_conv_10_off_bias_33_init4);


    /* Unit= 19 [ARITH_ACC_V2 1] */
    /* kind=Sub node=Gemm_7_conv_16_suboff_36 */
    static const LL_Arithacc_InitTypeDef Gemm_7_conv_16_suboff_36_init4 = {
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
      .bcast = ARITH_BCAST_HEIGHT,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 1,
      .fHeight = 1,
      .fChannels = 32,
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
    LL_Arithacc_Init(1, &Gemm_7_conv_16_suboff_36_init4);


    /* Dma inputs units to cycle: */
    /* Unit= 3 [STREAM_ENG_V2 3] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_5_conv_10 input ports=0 range=8[304,368] */

    static const LL_Streng_TensorInitTypeDef Gemm_5_conv_10_dma_init_in_0_4 = {
      /* 1x1x64(8 bits) */
      .dir = 0,
      .raw = 1,
      .raw_out = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 1,
      .addr_base = {(unsigned char *)(0x34200000UL) /* Equivalent hex address = 0x34200000UL */}, /* Gemm_5_conv_10_zero_off_out_28 */
      .offset_start = 304,
      .offset_end = 370,
      .offset_limit = 432,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 0,
      .line_offset = 0,
      .loop_offset = 64,
      .frame_loop_cnt = 2,
      .frame_tot_cnt = 2,
      .nbits_in = 24,
      .nbits_out = 24,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(3, &Gemm_5_conv_10_dma_init_in_0_4, 1);

    /* Unit= 7 [STREAM_ENG_V2 7] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_5_conv_10 input ports=1 range=5[19328,21380] */

    static const LL_Streng_TensorInitTypeDef Gemm_5_conv_10_dma_init_in_1_4 = {
      /* 32x1x1x64(8 bits) */
      .dir = 0,
      .raw = 1,
      .continuous = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .cacheable = 1,
      .cache_allocate = 0,
      .addr_base = {(unsigned char *)(0x70200000UL) /* Equivalent hex address = 0x70200000UL */}, /* Gemm_5_weights_transposed_9 */
      .offset_start = 19328,
      .offset_end = 21380,
      .offset_limit = 21448,
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
    LL_Streng_TensorInit(7, &Gemm_5_conv_10_dma_init_in_1_4, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM3 -> 128 */
    /* octoFlash -> 2048 */

    /* Dma output units from cycle: */
    /* Unit= 8 [STREAM_ENG_V2 8] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_7_conv_16_suboff_36 output ports=0 range=8[0,32] */

    static const LL_Streng_TensorInitTypeDef Gemm_7_conv_16_suboff_36_dma_init_out_0_4 = {
      /* to memory with batch=16 */
      .dir = 1,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 1,
      .addr_base = {(unsigned char *)(0x34200000UL) /* Equivalent hex address = 0x34200000UL */}, /* Gemm_7_conv_16_zero_off_out_37 */
      .offset_start = 0,
      .offset_end = 16,
      .offset_limit = 96,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 16,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 2,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(8, &Gemm_7_conv_16_suboff_36_dma_init_out_0_4, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM3 <- 32 */

    static const LL_Switch_InitTypeDef switch_init_in_4[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_5_conv_10 IN: in unit=CONV_ACC_V2 1 in port=0 out unit=STREAM_ENG_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 1, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 7, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_5_conv_10 IN: in unit=CONV_ACC_V2 1 in port=1 out unit=STREAM_ENG_V2 7 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 3, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_5_conv_10_mul_scale_31 IN: in unit=ARITH_ACC_V2 3 in port=0 out unit=CONV_ACC_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_5_conv_10_off_bias_33 IN: in unit=ARITH_ACC_V2 0 in port=0 out unit=ARITH_ACC_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16_suboff_36 IN: in unit=ARITH_ACC_V2 1 in port=0 out unit=ARITH_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 8, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16_suboff_36 OUT: in unit=STREAM_ENG_V2 8 in port=0 out unit=ARITH_ACC_V2 1 out port=0 */
    };


    /* epoch=4 */
    LL_Switch_Init(switch_init_in_4, 6);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_4_all_units[] = {
      { {STRENG, 8} }, /* STREAM_ENG_V2 */
      { {CONVACC, 1} }, /* CONV_ACC_V2 */
      { {ARITH, 0} }, /* ARITH_ACC_V2 */
      { {ARITH, 1} }, /* ARITH_ACC_V2 */
      { {ARITH, 3} }, /* ARITH_ACC_V2 */
      { {STRENG, 3} }, /* STREAM_ENG_V2 */
      { {STRENG, 7} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_4_all_units, 7);

  }

  ec_trace_wait_epoch_end(0x100);

  {
    static const LL_Switch_DeinitTypeDef switch_deinit_in_4[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_5_conv_10 IN: in unit=CONV_ACC_V2 1 in port=0 out unit=STREAM_ENG_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 1, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 7, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_5_conv_10 IN: in unit=CONV_ACC_V2 1 in port=1 out unit=STREAM_ENG_V2 7 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 3, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_5_conv_10_mul_scale_31 IN: in unit=ARITH_ACC_V2 3 in port=0 out unit=CONV_ACC_V2 1 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_5_conv_10_off_bias_33 IN: in unit=ARITH_ACC_V2 0 in port=0 out unit=ARITH_ACC_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16_suboff_36 IN: in unit=ARITH_ACC_V2 1 in port=0 out unit=ARITH_ACC_V2 0 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 8, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 1, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16_suboff_36 OUT: in unit=STREAM_ENG_V2 8 in port=0 out unit=ARITH_ACC_V2 1 out port=0 */
    };


    /* epoch=4 */
    LL_Switch_Deinit(switch_deinit_in_4, 6);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_4_all_units[] = {
      { {STRENG, 8} }, /* STREAM_ENG_V2 */
      { {CONVACC, 1} }, /* CONV_ACC_V2 */
      { {ARITH, 0} }, /* ARITH_ACC_V2 */
      { {ARITH, 1} }, /* ARITH_ACC_V2 */
      { {ARITH, 3} }, /* ARITH_ACC_V2 */
      { {STRENG, 3} }, /* STREAM_ENG_V2 */
      { {STRENG, 7} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_4_all_units, 7);

  }
  ec_trace_end_epoch(4);
  ec_trace_start_epoch(5);
  {
    /* Unit= 12 [CONV_ACC_V2 2] */
    /* kind=Conv node=Gemm_7_conv_16 */
    static const LL_Convacc_InitTypeDef Gemm_7_conv_16_init5 = {
      .simd = 1,
      .fsub = 0,
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
      .shift_a = 2,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 2,
      .shift_o = 2,
      .raw_o = 0,
      .fWidth = 1,
      .fHeight = 1,
      .kernelWidth = 1,
      .kernelHeight = 1,
      .nKernels = 3,
      .batchDepth = 32,
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
    LL_Convacc_Init(2, &Gemm_7_conv_16_init5);


    /* Unit= 20 [ARITH_ACC_V2 2] */
    /* kind=Mul node=Gemm_7_conv_16_mul_scale_40 */
    static const LL_Arithacc_InitTypeDef Gemm_7_conv_16_mul_scale_40_init5 = {
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
      .combinebc = 0,
      .clipout = 0,
      .shift_y = 0,
      .rounding_o = 1,
      .saturation_o = 1,
      .round_mode_o = 1,
      .relu_mode_o = 0,
      .outbytes_o = 2,
      .shift_o = 13,
      .scalar = 0,
      .dualinput = 0,
      .operation = ARITH_AFFINE,
      .bcast = ARITH_BCAST_CHAN,
      .Ax_shift = 0,
      .By_shift = 0,
      .C_shift = 0,
      .fWidth = 1,
      .fHeight = 1,
      .fChannels = 3,
      .batchDepth = 3,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 1,
      .B_scalar = 0,
      .C_scalar = 0,
      .A_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 592))) /* Equivalent hex offset = 0x250 */},
      .B_vector = {0},
      .C_vector = {0},
      .vec_precision = {16, 16, 16},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(2, &Gemm_7_conv_16_mul_scale_40_init5);


    /* Unit= 18 [ARITH_ACC_V2 0] */
    /* kind=Add node=Gemm_7_conv_16_off_bias_42 */
    static const LL_Arithacc_InitTypeDef Gemm_7_conv_16_off_bias_42_init5 = {
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
      .fChannels = 3,
      .batchDepth = 3,
      .clipmin = 0,
      .clipmax = 0,
      .A_scalar = 16649,
      .B_scalar = 0,
      .C_scalar = (short)0,
      .A_vector = {0},
      .B_vector = {0},
      .C_vector = {((unsigned char *)((((uintptr_t)(__ec_params_memory_pool_base)) + 576))) /* Equivalent hex offset = 0x240 */},
      .vec_precision = {16, 16, 32},
    };

    /* Unit=ARITH_ACC_V2 */
    LL_Arithacc_Init(0, &Gemm_7_conv_16_off_bias_42_init5);


    /* Dma inputs units to cycle: */
    /* Unit= 8 [STREAM_ENG_V2 8] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_7_conv_16 input ports=0 range=8[0,32] */

    static const LL_Streng_TensorInitTypeDef Gemm_7_conv_16_dma_init_in_0_5 = {
      /* 1x1x32(8 bits) */
      .dir = 0,
      .raw = 1,
      .raw_out = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 1,
      .addr_base = {(unsigned char *)(0x34200000UL) /* Equivalent hex address = 0x34200000UL */}, /* Gemm_7_conv_16_zero_off_out_37 */
      .offset_start = 0,
      .offset_end = 33,
      .offset_limit = 96,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 0,
      .line_offset = 0,
      .loop_offset = 32,
      .frame_loop_cnt = 1,
      .frame_tot_cnt = 1,
      .nbits_in = 24,
      .nbits_out = 24,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(8, &Gemm_7_conv_16_dma_init_in_0_5, 1);

    /* Unit= 3 [STREAM_ENG_V2 3] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_7_conv_16 input ports=1 range=5[21392,21488] */

    static const LL_Streng_TensorInitTypeDef Gemm_7_conv_16_dma_init_in_1_5 = {
      /* 3x1x1x32(8 bits) */
      .dir = 0,
      .raw = 1,
      .continuous = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .cacheable = 1,
      .cache_allocate = 0,
      .addr_base = {(unsigned char *)(0x70200000UL) /* Equivalent hex address = 0x70200000UL */}, /* Gemm_7_weights_transposed_15 */
      .offset_start = 21392,
      .offset_end = 21488,
      .offset_limit = 21552,
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
    LL_Streng_TensorInit(3, &Gemm_7_conv_16_dma_init_in_1_5, 1);


    /* Dma input bandwidth from memory pools: */
    /* npuRAM3 -> 32 */
    /* octoFlash -> 96 */

    /* Dma output units from cycle: */
    /* Unit= 6 [STREAM_ENG_V2 6] */
    /* Emit conf for STREAM_ENG_V2 node=Gemm_7_conv_16_off_bias_42 output ports=0 range=8[32,35] */

    static const LL_Streng_TensorInitTypeDef Gemm_7_conv_16_off_bias_42_dma_init_out_0_5 = {
      /* to memory with batch=3 */
      .dir = 1,
      .raw = 1,
      .noblk = 0,
      .align_right = 0,
      .nbits_unsigned = 0,
      .addr_base = {(unsigned char *)(0x34200000UL) /* Equivalent hex address = 0x34200000UL */}, /* Gemm_7_conv_16_off_bias_out_43 */
      .offset_start = 32,
      .offset_end = 35,
      .offset_limit = 104,
      .frame_count = 0,
      .fwidth = 0,
      .fheight = 0,
      .batch_depth = 0,
      .batch_offset = 0,
      .frame_offset = 3,
      .line_offset = 0,
      .loop_offset = 0,
      .frame_loop_cnt = 0,
      .frame_tot_cnt = 1,
      .nbits_in = 8,
      .nbits_out = 8,
    };

    /* Unit=STREAM_ENG_V2 */
    LL_Streng_TensorInit(6, &Gemm_7_conv_16_off_bias_42_dma_init_out_0_5, 1);


    /* Dma output bandwidth to memory pools: */
    /* npuRAM3 <- 3 */

    static const LL_Switch_InitTypeDef switch_init_in_5[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 8, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16 IN: in unit=CONV_ACC_V2 2 in port=0 out unit=STREAM_ENG_V2 8 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 2, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16 IN: in unit=CONV_ACC_V2 2 in port=1 out unit=STREAM_ENG_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16_mul_scale_40 IN: in unit=ARITH_ACC_V2 2 in port=0 out unit=CONV_ACC_V2 2 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16_off_bias_42 IN: in unit=ARITH_ACC_V2 0 in port=0 out unit=ARITH_ACC_V2 2 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 6, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16_off_bias_42 OUT: in unit=STREAM_ENG_V2 6 in port=0 out unit=ARITH_ACC_V2 0 out port=0 */
    };


    /* epoch=5 */
    LL_Switch_Init(switch_init_in_5, 5);

    static const LL_ATON_EnableUnits_InitTypeDef Enable_epoch_5_all_units[] = {
      { {STRENG, 6} }, /* STREAM_ENG_V2 */
      { {CONVACC, 2} }, /* CONV_ACC_V2 */
      { {ARITH, 0} }, /* ARITH_ACC_V2 */
      { {ARITH, 2} }, /* ARITH_ACC_V2 */
      { {STRENG, 3} }, /* STREAM_ENG_V2 */
      { {STRENG, 8} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_EnableUnits_Init(Enable_epoch_5_all_units, 6);

  }

  ec_trace_wait_epoch_end(0x40);

  {
    static const LL_Switch_DeinitTypeDef switch_deinit_in_5[] = {
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 8, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16 IN: in unit=CONV_ACC_V2 2 in port=0 out unit=STREAM_ENG_V2 8 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, CONVACC, 2, 1), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, STRENG, 3, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16 IN: in unit=CONV_ACC_V2 2 in port=1 out unit=STREAM_ENG_V2 3 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, CONVACC, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16_mul_scale_40 IN: in unit=ARITH_ACC_V2 2 in port=0 out unit=CONV_ACC_V2 2 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 2, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16_off_bias_42 IN: in unit=ARITH_ACC_V2 0 in port=0 out unit=ARITH_ACC_V2 2 out port=0 */
      { LL_Switch_Init_Dest() = ATONN_DSTPORT(STRSWITCH, 0, STRENG, 6, 0), LL_Switch_Init_Source(0) = ATONN_SRCPORT(STRSWITCH, 0, ARITH, 0, 0), LL_Switch_Init_Context(0) = 1, LL_Switch_Init_Frames(0) = 0, }, /* Gemm_7_conv_16_off_bias_42 OUT: in unit=STREAM_ENG_V2 6 in port=0 out unit=ARITH_ACC_V2 0 out port=0 */
    };


    /* epoch=5 */
    LL_Switch_Deinit(switch_deinit_in_5, 5);

    static const LL_ATON_DisableUnits_InitTypeDef Disable_epoch_5_all_units[] = {
      { {STRENG, 6} }, /* STREAM_ENG_V2 */
      { {CONVACC, 2} }, /* CONV_ACC_V2 */
      { {ARITH, 0} }, /* ARITH_ACC_V2 */
      { {ARITH, 2} }, /* ARITH_ACC_V2 */
      { {STRENG, 3} }, /* STREAM_ENG_V2 */
      { {STRENG, 8} }, /* STREAM_ENG_V2 */
    };


    LL_ATON_DisableUnits_Init(Disable_epoch_5_all_units, 6);

  }
  ec_trace_end_epoch(5);
  ec_trace_end_blob("_ec_blob_Default_2");
}


int main () {
  ec_trace_init("network_ecblobs.h", "Default", false);
  trace_ec__ec_blob_Default_2();
  ec_trace_all_blobs_done();
}
