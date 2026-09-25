/**
  ******************************************************************************
  * @file    dl4c.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-09-23T02:05:14+0800
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#include "ai_lite_inspect.h"
#include "ai_platform_interface.h"
#include "layers.h"
#include "core_convert.h"
#include "dl4c.h"
#include "dl4c_details.h"
#include "dl4c_data.h"
#include "stai_events.h"

#include "ai_lite_inspect.h"

#include "lite_operators.h"
/*****************************************************************************/
#define STAI_INTERNAL_API_MAJOR               (1)
#define STAI_INTERNAL_API_MINOR               (0)
#define STAI_INTERNAL_API_MICRO               (0)

#define STAI_MAGIC                            (0xB1C00100)

/*****************************************************************************/
#define _STAI_CONCAT_ARG(a, b)     a ## b
#define STAI_CONCAT(a, b)         _STAI_CONCAT_ARG(a, b)

/*!  STAI_CAST SECTION                       *********************************/
#define STAI_CAST(type, expr) \
  ((type)(expr))


/*****************************************************************************/
#define STAI_SIZE(_size) \
  ((stai_size)(_size))

/*****************************************************************************/
#define STAI_INIT_BUFFER(_flags, _size, _address) \
  { \
    .size = (_size), \
    .address = (uintptr_t)(_address), \
    .flags = (_flags), \
  }

#define STAI_INIT_TENSOR(_name, _flags, _fmt, _size_bytes, _shape, _scale, _zeropoint) \
  { \
    .size_bytes = (_size_bytes), \
    .flags = (_flags), \
    .format = (stai_format)(_fmt), \
    .shape = STAI_PACK(_shape), \
    .scale = STAI_PACK(_scale), \
    .zeropoint = STAI_PACK(_zeropoint), \
    .name = (_name) \
  }

#define STAI_INIT_ARRAY(_size, _ptr) \
  { .size = STAI_SIZE(_size), .data = STAI_PACK(_ptr) }


#define STAI_CAST_ARRAY(_type, _size, _ptr) \
  { .size = STAI_SIZE(_size), .data = (_type)STAI_PACK(_ptr) }


#define STAI_DECLARE_ARRAY(_type, _size, ...) \
  { .size = STAI_SIZE(_size), .data = (_type[_size]) { STAI_PACK(__VA_ARGS__) } }


#define STAI_EMPTY_ARRAY() \
  { .size = 0, .data = NULL }


#define STAI_INIT_VERSION(_major, _minor, _micro) \
  { .major = (_major), .minor = (_minor), .micro = (_micro), .reserved = 0x0 }

/*****************************************************************************/
/**  Getters and setters  **/

#define STAI_GET_ARRAY_SIZE(nd_array) \
  (nd_array.size)


#define STAI_GET_ARRAY_ELEM(nd_array, pos) \
  (nd_array.data[(pos)])

#define _STAI_SET_ERROR(net_ctx, cond, value, exit) { \
  if (!(net_ctx)) { return STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE; } \
  if (((uintptr_t)net_ctx) & (_STAI_CONTEXT_ALIGNMENT-1)) { return STAI_ERROR_NETWORK_INVALID_CONTEXT_ALIGNMENT; } \
  if (((value) >= STAI_ERROR_GENERIC) && (cond)) { \
    if ((net_ctx)->_return_code == STAI_SUCCESS) { \
      (net_ctx)->_return_code = (value); \
    } \
    return (exit); \
  } \
}

/*****************************************************************************/
/* TODO REMOVE THESE TWO MACROS */
#define STAI_EVENT_NODE_START_CB
#define STAI_EVENT_NODE_STOP_CB

#ifdef STAI_EVENT_NODE_START_CB
#ifndef _STAI_DL4C_EVENT_NODE_START_CB
  #define _STAI_DL4C_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
  if (net_ctx->_callback) { \
    const stai_event_node_start_stop _start_event = { \
      .node_id=(_node_id), \
      .buffers={ \
        .size=(_buffers_size), \
        .data=(stai_ptr const*)(const stai_ptr[_buffers_size])STAI_PACK(__VA_ARGS__) \
      } \
    }; \
    net_ctx->_callback(net_ctx->_callback_cookie, STAI_EVENT_NODE_START, (const void*)&_start_event); \
  }
#endif
#else
  #define _STAI_DL4C_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_DL4C_EVENT_NODE_START_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_START_CB */

#ifdef STAI_EVENT_NODE_STOP_CB
#ifndef _STAI_DL4C_EVENT_NODE_STOP_CB
  #define _STAI_DL4C_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
  if (net_ctx->_callback) { \
    const stai_event_node_start_stop _stop_event = { \
      .node_id=(_node_id), \
      .buffers={ \
        .size=(_buffers_size), \
        .data=(stai_ptr const*)(stai_ptr[_buffers_size])STAI_PACK(__VA_ARGS__) \
      } \
    }; \
    net_ctx->_callback(net_ctx->_callback_cookie, STAI_EVENT_NODE_STOP, (const void*)&_stop_event); \
  }
#endif
#else
  #define _STAI_DL4C_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_DL4C_EVENT_NODE_STOP_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_STOP_CB */


/*****************************************************************************/
#define _STAI_DL4C_MODEL_SIGNATURE     "0xdc3d1d5a0df0b42069b612e510a6b44c"
#define _STAI_DL4C_DATETIME            "2026-09-23T02:05:14+0800"
#define _STAI_DL4C_COMPILE_DATETIME    __DATE__ " " __TIME__

#define _STAI_CONTEXT_ALIGNMENT        STAI_DL4C_CONTEXT_ALIGNMENT

/*****************************************************************************/
#define g_dl4c_activations_1     (NULL)




#if defined(HAVE_DL4C_INFO)
/*****************************************************************************/
static const stai_network_info g_dl4c_info = {
  .model_signature = _STAI_DL4C_MODEL_SIGNATURE,
  .c_compile_datetime = _STAI_DL4C_COMPILE_DATETIME,
  .c_model_name = STAI_DL4C_MODEL_NAME,
  .c_model_datetime = _STAI_DL4C_DATETIME,
  .c_model_signature = 0x0,
  .runtime_version = STAI_INIT_VERSION(12, 0, 1),
  .tool_version = STAI_INIT_VERSION(4, 0, 1),
  .api_version = STAI_INIT_VERSION(1, 0, 0),
  .n_macc = STAI_DL4C_MACC_NUM,
  .n_nodes = STAI_DL4C_NODES_NUM,
  .flags = STAI_DL4C_FLAGS,
  .n_inputs = STAI_DL4C_IN_NUM,
  .n_outputs = STAI_DL4C_OUT_NUM,
  .n_activations = STAI_DL4C_ACTIVATIONS_NUM,
  .n_weights = STAI_DL4C_WEIGHTS_NUM,
  .n_states = STAI_DL4C_STATES_NUM,
  .inputs = (stai_tensor[STAI_DL4C_IN_NUM]) {
    STAI_INIT_TENSOR(
      STAI_DL4C_IN_1_NAME,
      STAI_DL4C_IN_1_FLAGS,
      STAI_DL4C_IN_1_FORMAT,
      STAI_DL4C_IN_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 30, 128),
      STAI_DECLARE_ARRAY(float, 1, 0.5137559175491333f),
      STAI_DECLARE_ARRAY(int16_t, 1, -9)),
    STAI_INIT_TENSOR(
      STAI_DL4C_IN_2_NAME,
      STAI_DL4C_IN_2_FLAGS,
      STAI_DL4C_IN_2_FORMAT,
      STAI_DL4C_IN_2_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 30, 96),
      STAI_DECLARE_ARRAY(float, 1, 0.09916097670793533f),
      STAI_DECLARE_ARRAY(int16_t, 1, -100)),
    },
    .outputs = (stai_tensor[STAI_DL4C_OUT_NUM]) {
    STAI_INIT_TENSOR(
      STAI_DL4C_OUT_1_NAME,
      STAI_DL4C_OUT_1_FLAGS,
      STAI_DL4C_OUT_1_FORMAT,
      STAI_DL4C_OUT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 30, 3),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .activations = (stai_tensor[STAI_DL4C_ACTIVATIONS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_DL4C_ACTIVATION_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_DL4C_ACTIVATION_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 22092),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_DL4C_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_DL4C_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_DL4C_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 46696),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },

  .states = NULL
};
#endif

#define _STAI_CONTEXT_ACQUIRE(_net_ctx, _net_handle) \
  _stai_dl4c_context* _net_ctx = (_stai_dl4c_context*)(_net_handle); \
  STAI_ASSERT(_net_ctx != NULL) \
  _STAI_SET_ERROR(_net_ctx, _net_ctx->_magic != STAI_MAGIC, \
                  STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE, _net_ctx->_return_code)


/*****************************************************************************/
static
void _stai_dl4c_check(_stai_dl4c_context* net_ctx)
{
  stai_size idx;

// Check activations status
  for (idx=0; idx<STAI_DL4C_ACTIVATIONS_NUM; idx++) {
    if (net_ctx->_activations[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_DL4C_ACTIVATIONS_NUM) ? STAI_FLAG_ACTIVATIONS : STAI_FLAG_NONE;
// Check inputs status
  for (idx=0; idx<STAI_DL4C_IN_NUM; idx++) {
    if (net_ctx->_inputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_DL4C_IN_NUM) ? STAI_FLAG_INPUTS : STAI_FLAG_NONE;

  // Check outputs status
  for (idx=0; idx<STAI_DL4C_OUT_NUM; idx++) {
    if (net_ctx->_outputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_DL4C_OUT_NUM) ? STAI_FLAG_OUTPUTS : STAI_FLAG_NONE;

// Check weights status
  for (idx=0; idx<STAI_DL4C_WEIGHTS_NUM; idx++) {
    if (net_ctx->_weights[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_DL4C_WEIGHTS_NUM) ? STAI_FLAG_WEIGHTS : STAI_FLAG_NONE;
STAI_PRINT("  [_stai_network_check] flags: 0x%08x\n", net_ctx->_flags)
}


/*****************************************************************************/
STAI_API_ENTRY
stai_return_code stai_dl4c_init(
  stai_network* network)
{
  /* Memory where to store internal context is provided by applications as a raw byte buffer */
  _stai_dl4c_context* net_ctx = (_stai_dl4c_context*)(network);
  net_ctx->_return_code = STAI_SUCCESS;
  STAI_PRINT("[Entering Network Init] network(%p) context_size(%d)\n", net_ctx, (int32_t)sizeof(_stai_dl4c_context))

  _STAI_SET_ERROR(net_ctx, STAI_DL4C_CONTEXT_SIZE != sizeof(_stai_dl4c_context),
                 STAI_ERROR_NETWORK_INVALID_CONTEXT_SIZE, net_ctx->_return_code)

  {
    const _stai_dl4c_context _dl4c_context = {
      ._magic = STAI_MAGIC,
      ._signature = STAI_DL4C_MODEL_SIGNATURE,
      ._flags = STAI_DL4C_FLAGS,
      ._return_code = STAI_SUCCESS,
      ._callback = NULL,
      ._callback_cookie = NULL,
      ._activations = {
      (stai_ptr)g_dl4c_activations_1
      },
      ._weights = {
      (stai_ptr)g_dl4c_weights_array
      },
      ._inputs = {
    NULL,NULL},
      ._outputs = {
    NULL},
    };

    // Deep copy of internal context to opaque buffer provided by app
    *net_ctx = _dl4c_context;

    _stai_dl4c_check(net_ctx);
  }

  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl4c_deinit(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /*  Reset flags to initial state  */
  net_ctx->_flags = STAI_DL4C_FLAGS;
  return net_ctx->_return_code;
}

/*****************************************************************************/



/* Int quant #0 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_h_tile_1_Tile0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09916097670793533f),
    AI_PACK_INTQ_ZP(-100)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_h_tile_1_Tile0_Transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09916097670793533f),
    AI_PACK_INTQ_ZP(-100)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut3_add2_1_Add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.5137559175491333f),
    AI_PACK_INTQ_ZP(-9)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut3_add2_1_Add0_Transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.5137559175491333f),
    AI_PACK_INTQ_ZP(-9)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_cat_1_concat0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.5137559175491333f),
    AI_PACK_INTQ_ZP(-9)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_moments_mean0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02341940626502037f),
    AI_PACK_INTQ_ZP(-34)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.5154440402984619f),
    AI_PACK_INTQ_ZP(-4)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(17.839344024658203f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_moments_variance0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.7026539444923401f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.7026578783988953f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_ln1_1_add_y0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(3.921568804798881e-06f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #11 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0014657231513410807f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #12 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0018609189428389072f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #13 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_Reshape0_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0049789221957325935f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #14 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05677397921681404f),
    AI_PACK_INTQ_ZP(3)))

/* Int quant #15 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_Neg0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02341940626502037f),
    AI_PACK_INTQ_ZP(33)))

/* Int quant #16 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.004015170503407717f),
    AI_PACK_INTQ_ZP(29)))

/* Int quant #17 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.004654208198189735f),
    AI_PACK_INTQ_ZP(24)))

/* Int quant #18 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_Reshape_10_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0009471720550209284f),
    AI_PACK_INTQ_ZP(-16)))

/* Int quant #19 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_ln_1_add_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05802692100405693f),
    AI_PACK_INTQ_ZP(8)))

/* Int quant #20 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05802692100405693f),
    AI_PACK_INTQ_ZP(8)))

/* Int quant #21 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.18565353751182556f),
    AI_PACK_INTQ_ZP(-12)))

/* Int quant #22 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.18565353751182556f),
    AI_PACK_INTQ_ZP(-12)))

/* Int quant #23 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.18565353751182556f),
    AI_PACK_INTQ_ZP(-13)))

/* Int quant #24 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0007800564053468406f),
    AI_PACK_INTQ_ZP(-44)))

/* Int quant #25 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(106.96804809570312f),
    AI_PACK_INTQ_ZP(-37)))

/* Int quant #26 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Gelu_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(4.78307580947876f),
    AI_PACK_INTQ_ZP(-37)))

/* Int quant #27 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00017535293591208756f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #28 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Gelu_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(4.968729496002197f),
    AI_PACK_INTQ_ZP(-36)))

/* Int quant #29 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Gelu_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(3.964472770690918f),
    AI_PACK_INTQ_ZP(-36)))

/* Int quant #30 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003128959098830819f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #31 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007843139581382275f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #32 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Gelu_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007843139581382275f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #33 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921568859368563f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #34 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Gelu_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09282676875591278f),
    AI_PACK_INTQ_ZP(-13)))

/* Int quant #35 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0019607844296842813f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #36 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fusion_dense_1_Gelu_mul_30_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10251101106405258f),
    AI_PACK_INTQ_ZP(-126)))

/* Int quant #37 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10251101106405258f),
    AI_PACK_INTQ_ZP(-126)))

/* Int quant #38 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.8406862616539001f),
    AI_PACK_INTQ_ZP(-27)))

/* Int quant #39 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.8406862616539001f),
    AI_PACK_INTQ_ZP(-27)))

/* Int quant #40 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.8410041332244873f),
    AI_PACK_INTQ_ZP(-27)))

/* Int quant #41 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0009740733657963574f),
    AI_PACK_INTQ_ZP(-63)))

/* Int quant #42 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(10898.0751953125f),
    AI_PACK_INTQ_ZP(-71)))

/* Int quant #43 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_Gelu_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(487.3074035644531f),
    AI_PACK_INTQ_ZP(-71)))

/* Int quant #44 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_Gelu_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(488.1484069824219f),
    AI_PACK_INTQ_ZP(-71)))

/* Int quant #45 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_Gelu_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(389.486083984375f),
    AI_PACK_INTQ_ZP(-71)))

/* Int quant #46 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_Gelu_Tanh0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007843139581382275f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #47 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_Gelu_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0078431386500597f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #48 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_Gelu_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.42050206661224365f),
    AI_PACK_INTQ_ZP(-27)))

/* Int quant #49 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_dec_dense_1_Gelu_mul_30_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.5076972842216492f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #50 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.5076972842216492f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #51 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(4.306164264678955f),
    AI_PACK_INTQ_ZP(-87)))

/* Int quant #52 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(4.306164264678955f),
    AI_PACK_INTQ_ZP(-87)))



/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_h_tile_1_Tile0_output_array, AI_ARRAY_FORMAT_S8|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 2880, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_h_tile_1_Tile0_Transpose_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2880, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut3_add2_1_Add0_output_array, AI_ARRAY_FORMAT_S8|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 3840, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut3_add2_1_Add0_Transpose_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_cat_1_concat0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6720, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_mean0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 30, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6720, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6720, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_variance0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 30, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 30, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_ln1_1_add_y0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 30, AI_STATIC)

/* Array#12 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6720, AI_STATIC)

/* Array#13 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_Reshape0_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 224, AI_STATIC)

/* Array#14 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6720, AI_STATIC)

/* Array#15 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_Neg0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 30, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6720, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6720, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_Reshape_10_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 224, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_add_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6720, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6720, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#22 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#23 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#24 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 128, AI_STATIC)

/* Array#25 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#26 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_Pow0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#27 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut3_ffn1_1_Gelu_Pow_y0_3D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#28 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#29 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#30 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#31 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#32 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#33 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#34 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#35 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#36 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#37 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#38 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#39 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_30_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#40 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#41 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#42 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#43 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#44 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 128, AI_STATIC)

/* Array#45 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#46 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_Pow0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#47 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#48 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#49 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#50 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#51 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_Tanh0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#52 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#53 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#54 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul_30_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#55 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#56 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 90, AI_STATIC)

/* Array#57 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 90, AI_STATIC)

/* Array#58 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3, AI_STATIC)

/* Array#59 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 90, AI_STATIC)

/* Array#60 */
AI_ARRAY_OBJ_DECLARE(
  raw_proj_QuantizeLinear_Input_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 90, AI_STATIC)

/* Array#61 */
AI_ARRAY_OBJ_DECLARE(
  raw_proj_QuantizeLinear_Input_Transpose_0_output_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 90, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_h_tile_1_Tile0_Transpose_output, AI_STATIC,
  63, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 96), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_h_tile_1_Tile0_Transpose_output_array, &reatt_n2_1_h_tile_1_Tile0_Transpose_output_array_intq)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_h_tile_1_Tile0_output, AI_STATIC,
  64, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 30), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n2_1_h_tile_1_Tile0_output_array, &reatt_n2_1_h_tile_1_Tile0_output_array_intq)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut3_add2_1_Add0_Transpose_output, AI_STATIC,
  60, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fut3_add2_1_Add0_Transpose_output_array, &reatt_n2_1_fut3_add2_1_Add0_Transpose_output_array_intq)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut3_add2_1_Add0_output, AI_STATIC,
  61, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut3_add2_1_Add0_output_array, &reatt_n2_1_fut3_add2_1_Add0_output_array_intq)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_cat_1_concat0_output, AI_STATIC,
  19, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 224), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_cat_1_concat0_output_array, &reatt_n2_1_fusion_cat_1_concat0_output_array_intq)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_mean0_output, AI_STATIC,
  52, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 1), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_ln_1_moments_mean0_output_array, &reatt_n2_1_fusion_ln_1_moments_mean0_output_array_intq)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output, AI_STATIC,
  50, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 224), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output_array, &reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output_array_intq)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output, AI_STATIC,
  51, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 224), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output_array, &reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output_array_intq)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_variance0_output, AI_STATIC,
  53, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 1), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_ln_1_moments_variance0_output_array, &reatt_n2_1_fusion_ln_1_moments_variance0_output_array_intq)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_add0_output, AI_STATIC,
  47, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 1), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_ln_1_add0_output_array, &reatt_n2_1_fusion_ln_1_add0_output_array_intq)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_ln1_1_add_y0_DequantizeLinear_Output_const_3D, AI_STATIC,
  57, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n2_1_fut0_ln1_1_add_y0_DequantizeLinear_Output_const_3D_array, &reatt_n2_1_fut0_ln1_1_add_y0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_Reshape0_DequantizeLinear_Output_const, AI_STATIC,
  41, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 224), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n2_1_fusion_ln_1_Reshape0_DequantizeLinear_Output_const_array, &reatt_n2_1_fusion_ln_1_Reshape0_DequantizeLinear_Output_const_array_intq)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_output, AI_STATIC,
  44, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 1), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_output_array, &reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_output_array_intq)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_mul0_output, AI_STATIC,
  54, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 224), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_ln_1_mul0_output_array, &reatt_n2_1_fusion_ln_1_mul0_output_array_intq)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_mul_20_output, AI_STATIC,
  56, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 224), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_ln_1_mul_20_output_array, &reatt_n2_1_fusion_ln_1_mul_20_output_array_intq)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_Neg0_output, AI_STATIC,
  40, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 1), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_ln_1_Neg0_output_array, &reatt_n2_1_fusion_ln_1_Neg0_output_array_intq)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_mul_10_output, AI_STATIC,
  55, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 224), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_ln_1_mul_10_output_array, &reatt_n2_1_fusion_ln_1_mul_10_output_array_intq)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_Reshape_10_DequantizeLinear_Output_const, AI_STATIC,
  42, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 224), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n2_1_fusion_ln_1_Reshape_10_DequantizeLinear_Output_const_array, &reatt_n2_1_fusion_ln_1_Reshape_10_DequantizeLinear_Output_const_array_intq)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_add_10_output, AI_STATIC,
  48, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 224), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_ln_1_add_10_output_array, &reatt_n2_1_fusion_ln_1_add_10_output_array_intq)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_add_20_output, AI_STATIC,
  49, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 224), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_ln_1_add_20_output_array, &reatt_n2_1_fusion_ln_1_add_20_output_array_intq)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output, AI_STATIC,
  35, 0x1,
  AI_SHAPE_INIT(4, 1, 224, 1, 30), AI_STRIDE_INIT(4, 1, 1, 224, 224),
  1, &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array, &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array_intq)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output, AI_STATIC,
  36, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array, &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array_intq)

/* Tensor #22 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_output, AI_STATIC,
  37, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_output_array, &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_output_array_intq)

/* Tensor #23 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_BiasAdd0_output, AI_STATIC,
  21, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_dense_1_BiasAdd0_output_array, &reatt_n2_1_fusion_dense_1_BiasAdd0_output_array_intq)

/* Tensor #24 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D, AI_STATIC,
  22, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 128), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n2_1_fusion_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array, &reatt_n2_1_fusion_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #25 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_output, AI_STATIC,
  20, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 4, 4, 120, 120),
  1, &reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_output_array, NULL)

/* Tensor #26 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_Pow0_output, AI_STATIC,
  24, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 4, 4, 120, 120),
  1, &reatt_n2_1_fusion_dense_1_Gelu_Pow0_output_array, NULL)

/* Tensor #27 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut3_ffn1_1_Gelu_Pow_y0_3D, AI_STATIC,
  62, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n2_1_fut3_ffn1_1_Gelu_Pow_y0_3D_array, NULL)

/* Tensor #28 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_output, AI_STATIC,
  23, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_output_array, &reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_output_array_intq)

/* Tensor #29 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_10_output, AI_STATIC,
  29, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_dense_1_Gelu_mul_10_output_array, &reatt_n2_1_fusion_dense_1_Gelu_mul_10_output_array_intq)

/* Tensor #30 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D, AI_STATIC,
  58, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array, &reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #31 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_add0_output, AI_STATIC,
  26, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_dense_1_Gelu_add0_output_array, &reatt_n2_1_fusion_dense_1_Gelu_add0_output_array_intq)

/* Tensor #32 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_20_output, AI_STATIC,
  30, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_dense_1_Gelu_mul_20_output_array, &reatt_n2_1_fusion_dense_1_Gelu_mul_20_output_array_intq)

/* Tensor #33 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D, AI_STATIC,
  31, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array, &reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #34 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output, AI_STATIC,
  25, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output_array, &reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output_array_intq)

/* Tensor #35 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_add_10_output, AI_STATIC,
  27, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_dense_1_Gelu_add_10_output_array, &reatt_n2_1_fusion_dense_1_Gelu_add_10_output_array_intq)

/* Tensor #36 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D, AI_STATIC,
  59, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array, &reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #37 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul0_output, AI_STATIC,
  28, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_dense_1_Gelu_mul0_output_array, &reatt_n2_1_fusion_dense_1_Gelu_mul0_output_array_intq)

/* Tensor #38 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D, AI_STATIC,
  33, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array, &reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #39 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_30_output, AI_STATIC,
  32, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_fusion_dense_1_Gelu_mul_30_output_array, &reatt_n2_1_fusion_dense_1_Gelu_mul_30_output_array_intq)

/* Tensor #40 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output, AI_STATIC,
  14, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array, &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array_intq)

/* Tensor #41 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output, AI_STATIC,
  15, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array, &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array_intq)

/* Tensor #42 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_output, AI_STATIC,
  16, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_output_array, &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_output_array_intq)

/* Tensor #43 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_BiasAdd0_output, AI_STATIC,
  3, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_dec_dense_1_BiasAdd0_output_array, &reatt_n2_1_dec_dense_1_BiasAdd0_output_array_intq)

/* Tensor #44 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D, AI_STATIC,
  4, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 128), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n2_1_dec_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array, &reatt_n2_1_dec_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #45 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_output, AI_STATIC,
  2, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 4, 4, 120, 120),
  1, &reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_output_array, NULL)

/* Tensor #46 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_Pow0_output, AI_STATIC,
  6, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 4, 4, 120, 120),
  1, &reatt_n2_1_dec_dense_1_Gelu_Pow0_output_array, NULL)

/* Tensor #47 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_output, AI_STATIC,
  5, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_output_array, &reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_output_array_intq)

/* Tensor #48 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul_10_output, AI_STATIC,
  11, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_dec_dense_1_Gelu_mul_10_output_array, &reatt_n2_1_dec_dense_1_Gelu_mul_10_output_array_intq)

/* Tensor #49 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_add0_output, AI_STATIC,
  8, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_dec_dense_1_Gelu_add0_output_array, &reatt_n2_1_dec_dense_1_Gelu_add0_output_array_intq)

/* Tensor #50 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul_20_output, AI_STATIC,
  12, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_dec_dense_1_Gelu_mul_20_output_array, &reatt_n2_1_dec_dense_1_Gelu_mul_20_output_array_intq)

/* Tensor #51 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_Tanh0_output, AI_STATIC,
  7, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_dec_dense_1_Gelu_Tanh0_output_array, &reatt_n2_1_dec_dense_1_Gelu_Tanh0_output_array_intq)

/* Tensor #52 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_add_10_output, AI_STATIC,
  9, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_dec_dense_1_Gelu_add_10_output_array, &reatt_n2_1_dec_dense_1_Gelu_add_10_output_array_intq)

/* Tensor #53 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul0_output, AI_STATIC,
  10, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_dec_dense_1_Gelu_mul0_output_array, &reatt_n2_1_dec_dense_1_Gelu_mul0_output_array_intq)

/* Tensor #54 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul_30_output, AI_STATIC,
  13, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 128), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_dec_dense_1_Gelu_mul_30_output_array, &reatt_n2_1_dec_dense_1_Gelu_mul_30_output_array_intq)

/* Tensor #55 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output, AI_STATIC,
  68, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array_intq)

/* Tensor #56 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output, AI_STATIC,
  70, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 1, 3), AI_STRIDE_INIT(4, 1, 1, 30, 30),
  1, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array_intq)

/* Tensor #57 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_output, AI_STATIC,
  71, 0x1,
  AI_SHAPE_INIT(4, 1, 3, 1, 30), AI_STRIDE_INIT(4, 1, 1, 3, 3),
  1, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_output_array, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_output_array_intq)

/* Tensor #58 */
AI_TENSOR_OBJ_DECLARE(
  raw_proj_QuantizeLinear_Input_output, AI_STATIC,
  1, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 1, 3), AI_STRIDE_INIT(4, 4, 4, 120, 120),
  1, &raw_proj_QuantizeLinear_Input_output_array, NULL)

/* Tensor #59 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_output, AI_STATIC,
  66, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 3), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_output_array, NULL)

/* Tensor #60 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_output, AI_STATIC,
  69, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 1, 3), AI_STRIDE_INIT(4, 4, 4, 120, 120),
  1, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_output_array, NULL)

/* Tensor #61 */
AI_TENSOR_OBJ_DECLARE(
  raw_proj_QuantizeLinear_Input_Transpose_0_output, AI_STATIC,
  0, 0x0,
  AI_SHAPE_INIT(4, 1, 3, 1, 30), AI_STRIDE_INIT(4, 4, 4, 12, 12),
  1, &raw_proj_QuantizeLinear_Input_Transpose_0_output_array, NULL)


AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_h_tile_1_Tile0_Transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_h_tile_1_Tile0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_h_tile_1_Tile0_Transpose_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_h_tile_1_Tile0_Transpose_layer, 2,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n2_1_h_tile_1_Tile0_Transpose_chain,
  NULL, &reatt_n2_1_h_tile_1_Tile0_Transpose_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fut3_add2_1_Add0_Transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut3_add2_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut3_add2_1_Add0_Transpose_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fut3_add2_1_Add0_Transpose_layer, 2,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n2_1_fut3_add2_1_Add0_Transpose_chain,
  NULL, &reatt_n2_1_fut3_add2_1_Add0_Transpose_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_cat_1_concat0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fut3_add2_1_Add0_Transpose_output, &reatt_n2_1_h_tile_1_Tile0_Transpose_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_cat_1_concat0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_cat_1_concat0_layer, 18,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &reatt_n2_1_fusion_cat_1_concat0_chain,
  NULL, &reatt_n2_1_fusion_cat_1_concat0_layer, AI_STATIC, 
  .axis = AI_SHAPE_HEIGHT, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_mean0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_cat_1_concat0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_moments_mean0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_mean0_layer, 21,
  POOL_TYPE, 0x0, NULL,
  pool, forward_ap_integer_INT8,
  &reatt_n2_1_fusion_ln_1_moments_mean0_chain,
  NULL, &reatt_n2_1_fusion_ln_1_moments_mean0_layer, AI_STATIC, 
  .pool_size = AI_SHAPE_2D_INIT(1, 224), 
  .pool_stride = AI_SHAPE_2D_INIT(1, 224), 
  .pool_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_cat_1_concat0_output, &reatt_n2_1_fusion_ln_1_moments_mean0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_layer, 25,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_chain,
  NULL, &reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_layer, AI_STATIC, 
  .operation = ai_sub_f32, 
  .buffer_operation = ai_sub_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output, &reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_layer, 30,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_chain,
  NULL, &reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_variance0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_moments_variance0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_moments_variance0_layer, 33,
  POOL_TYPE, 0x0, NULL,
  pool, forward_ap_integer_INT8,
  &reatt_n2_1_fusion_ln_1_moments_variance0_chain,
  NULL, &reatt_n2_1_fusion_ln_1_moments_variance0_layer, AI_STATIC, 
  .pool_size = AI_SHAPE_2D_INIT(1, 224), 
  .pool_stride = AI_SHAPE_2D_INIT(1, 224), 
  .pool_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_ln_1_moments_variance0_output, &reatt_n2_1_fut0_ln1_1_add_y0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_add0_layer, 36,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_ln_1_add0_chain,
  NULL, &reatt_n2_1_fusion_ln_1_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_output, &reatt_n2_1_fusion_ln_1_Reshape0_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_mul0_layer, 43,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_ln_1_mul0_chain,
  NULL, &reatt_n2_1_fusion_ln_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_cat_1_concat0_output, &reatt_n2_1_fusion_ln_1_mul0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_mul_20_layer, 46,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_ln_1_mul_20_chain,
  NULL, &reatt_n2_1_fusion_ln_1_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 reatt_n2_1_fusion_ln_1_Neg0_nl_params_data[] = { 127, 126, 125, 124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16, -17, -18, -19, -20, -21, -22, -23, -24, -25, -26, -27, -28, -29, -30, -31, -32, -33, -34, -35, -36, -37, -38, -39, -40, -41, -42, -43, -44, -45, -46, -47, -48, -49, -50, -51, -52, -53, -54, -55, -56, -57, -58, -59, -60, -61, -62, -63, -64, -65, -66, -67, -68, -69, -70, -71, -72, -73, -74, -75, -76, -77, -78, -79, -80, -81, -82, -83, -84, -85, -86, -87, -88, -89, -90, -91, -92, -93, -94, -95, -96, -97, -98, -99, -100, -101, -102, -103, -104, -105, -106, -107, -108, -109, -110, -111, -112, -113, -114, -115, -116, -117, -118, -119, -120, -121, -122, -123, -124, -125, -126, -127, -128 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n2_1_fusion_ln_1_Neg0_nl_params, AI_ARRAY_FORMAT_S8,
    reatt_n2_1_fusion_ln_1_Neg0_nl_params_data, reatt_n2_1_fusion_ln_1_Neg0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_Neg0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_moments_mean0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_Neg0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_Neg0_layer, 24,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &reatt_n2_1_fusion_ln_1_Neg0_chain,
  NULL, &reatt_n2_1_fusion_ln_1_Neg0_layer, AI_STATIC, 
  .nl_params = &reatt_n2_1_fusion_ln_1_Neg0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_ln_1_Neg0_output, &reatt_n2_1_fusion_ln_1_mul0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_mul_10_layer, 47,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_ln_1_mul_10_chain,
  NULL, &reatt_n2_1_fusion_ln_1_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_ln_1_mul_10_output, &reatt_n2_1_fusion_ln_1_Reshape_10_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_add_10_layer, 52,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_ln_1_add_10_chain,
  NULL, &reatt_n2_1_fusion_ln_1_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_add_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_ln_1_mul_20_output, &reatt_n2_1_fusion_ln_1_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_add_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_ln_1_add_20_layer, 55,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_ln_1_add_20_chain,
  NULL, &reatt_n2_1_fusion_ln_1_add_20_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_ln_1_add_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_layer, 61,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_chain,
  NULL, &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_layer, 61,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_chain,
  NULL, &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output, &reatt_n2_1_fusion_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_BiasAdd0_layer, 67,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_dense_1_BiasAdd0_chain,
  NULL, &reatt_n2_1_fusion_dense_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_Pow0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_output, &reatt_n2_1_fut3_ffn1_1_Gelu_Pow_y0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Gelu_Pow0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_Pow0_layer, 71,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n2_1_fusion_dense_1_Gelu_Pow0_chain,
  NULL, &reatt_n2_1_fusion_dense_1_Gelu_Pow0_layer, AI_STATIC, 
  .operation = ai_pow, 
  .buffer_operation = ai_pow_buffer, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D, &reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_10_layer, 76,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_dense_1_Gelu_mul_10_chain,
  NULL, &reatt_n2_1_fusion_dense_1_Gelu_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_dense_1_BiasAdd0_output, &reatt_n2_1_fusion_dense_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_add0_layer, 79,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_dense_1_Gelu_add0_chain,
  NULL, &reatt_n2_1_fusion_dense_1_Gelu_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D, &reatt_n2_1_fusion_dense_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_20_layer, 82,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_dense_1_Gelu_mul_20_chain,
  NULL, &reatt_n2_1_fusion_dense_1_Gelu_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 reatt_n2_1_fusion_dense_1_Gelu_Tanh0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, 0, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n2_1_fusion_dense_1_Gelu_Tanh0_nl_params, AI_ARRAY_FORMAT_S8,
    reatt_n2_1_fusion_dense_1_Gelu_Tanh0_nl_params_data, reatt_n2_1_fusion_dense_1_Gelu_Tanh0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_Tanh0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_Tanh0_layer, 85,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &reatt_n2_1_fusion_dense_1_Gelu_Tanh0_chain,
  NULL, &reatt_n2_1_fusion_dense_1_Gelu_Tanh0_layer, AI_STATIC, 
  .nl_params = &reatt_n2_1_fusion_dense_1_Gelu_Tanh0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D, &reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_add_10_layer, 88,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_dense_1_Gelu_add_10_chain,
  NULL, &reatt_n2_1_fusion_dense_1_Gelu_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D, &reatt_n2_1_fusion_dense_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Gelu_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul0_layer, 70,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_dense_1_Gelu_mul0_chain,
  NULL, &reatt_n2_1_fusion_dense_1_Gelu_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_30_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_dense_1_Gelu_mul0_output, &reatt_n2_1_fusion_dense_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fusion_dense_1_Gelu_mul_30_layer, 91,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fusion_dense_1_Gelu_mul_30_chain,
  NULL, &reatt_n2_1_fusion_dense_1_Gelu_mul_30_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fusion_dense_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_layer, 97,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_chain,
  NULL, &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_layer, 97,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_chain,
  NULL, &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output, &reatt_n2_1_dec_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_BiasAdd0_layer, 103,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_dec_dense_1_BiasAdd0_chain,
  NULL, &reatt_n2_1_dec_dense_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_Pow0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_output, &reatt_n2_1_fut3_ffn1_1_Gelu_Pow_y0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Gelu_Pow0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_Pow0_layer, 107,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n2_1_dec_dense_1_Gelu_Pow0_chain,
  NULL, &reatt_n2_1_dec_dense_1_Gelu_Pow0_layer, AI_STATIC, 
  .operation = ai_pow, 
  .buffer_operation = ai_pow_buffer, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D, &reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul_10_layer, 112,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_dec_dense_1_Gelu_mul_10_chain,
  NULL, &reatt_n2_1_dec_dense_1_Gelu_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_dec_dense_1_BiasAdd0_output, &reatt_n2_1_dec_dense_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_add0_layer, 115,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_dec_dense_1_Gelu_add0_chain,
  NULL, &reatt_n2_1_dec_dense_1_Gelu_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D, &reatt_n2_1_dec_dense_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul_20_layer, 118,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_dec_dense_1_Gelu_mul_20_chain,
  NULL, &reatt_n2_1_dec_dense_1_Gelu_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 reatt_n2_1_dec_dense_1_Gelu_Tanh0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, 0, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n2_1_dec_dense_1_Gelu_Tanh0_nl_params, AI_ARRAY_FORMAT_S8,
    reatt_n2_1_dec_dense_1_Gelu_Tanh0_nl_params_data, reatt_n2_1_dec_dense_1_Gelu_Tanh0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_Tanh0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_Tanh0_layer, 121,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &reatt_n2_1_dec_dense_1_Gelu_Tanh0_chain,
  NULL, &reatt_n2_1_dec_dense_1_Gelu_Tanh0_layer, AI_STATIC, 
  .nl_params = &reatt_n2_1_dec_dense_1_Gelu_Tanh0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D, &reatt_n2_1_dec_dense_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_add_10_layer, 124,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_dec_dense_1_Gelu_add_10_chain,
  NULL, &reatt_n2_1_dec_dense_1_Gelu_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D, &reatt_n2_1_dec_dense_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Gelu_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul0_layer, 106,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_dec_dense_1_Gelu_mul0_chain,
  NULL, &reatt_n2_1_dec_dense_1_Gelu_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul_30_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_dec_dense_1_Gelu_mul0_output, &reatt_n2_1_dec_dense_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_dec_dense_1_Gelu_mul_30_layer, 127,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_dec_dense_1_Gelu_mul_30_chain,
  NULL, &reatt_n2_1_dec_dense_1_Gelu_mul_30_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_dec_dense_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_layer, 133,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_chain,
  NULL, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_layer, 133,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_chain,
  NULL, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  raw_proj_QuantizeLinear_Input_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_output, &reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &raw_proj_QuantizeLinear_Input_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  raw_proj_QuantizeLinear_Input_layer, 139,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &raw_proj_QuantizeLinear_Input_chain,
  NULL, &raw_proj_QuantizeLinear_Input_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  raw_proj_QuantizeLinear_Input_Transpose_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &raw_proj_QuantizeLinear_Input_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &raw_proj_QuantizeLinear_Input_Transpose_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  raw_proj_QuantizeLinear_Input_Transpose_0_layer, 1,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &raw_proj_QuantizeLinear_Input_Transpose_0_chain,
  NULL, &raw_proj_QuantizeLinear_Input_Transpose_0_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)
/**  Hybrid layers declarations section  *************************************/
void forward_lite_transpose_reatt_n2_1_h_tile_1_Tile0_Transpose(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_h_tile_1_Tile0_output_array.data = AI_PTR(net_ctx->_inputs[1] + 0);
  reatt_n2_1_h_tile_1_Tile0_output_array.data_start = AI_PTR(net_ctx->_inputs[1] + 0);
  reatt_n2_1_h_tile_1_Tile0_Transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_h_tile_1_Tile0_Transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(2, 1, { reatt_n2_1_h_tile_1_Tile0_output.data->data});
  forward_transpose(&reatt_n2_1_h_tile_1_Tile0_Transpose_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(2, 1, { reatt_n2_1_h_tile_1_Tile0_Transpose_output.data->data});
}
void forward_lite_transpose_reatt_n2_1_fut3_add2_1_Add0_Transpose(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fut3_add2_1_Add0_output_array.data = AI_PTR(net_ctx->_inputs[0] + 0);
  reatt_n2_1_fut3_add2_1_Add0_output_array.data_start = AI_PTR(net_ctx->_inputs[0] + 0);
  reatt_n2_1_fut3_add2_1_Add0_Transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 9612);
  reatt_n2_1_fut3_add2_1_Add0_Transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9612);
  _STAI_DL4C_EVENT_NODE_START_CB(2, 1, { reatt_n2_1_fut3_add2_1_Add0_output.data->data});
  forward_transpose(&reatt_n2_1_fut3_add2_1_Add0_Transpose_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(2, 1, { reatt_n2_1_fut3_add2_1_Add0_Transpose_output.data->data});
}
void forward_lite_concat_reatt_n2_1_fusion_cat_1_concat0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fut3_add2_1_Add0_Transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 9612);
  reatt_n2_1_fut3_add2_1_Add0_Transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9612);
  reatt_n2_1_h_tile_1_Tile0_Transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_h_tile_1_Tile0_Transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_cat_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 13452);
  reatt_n2_1_fusion_cat_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 13452);
  _STAI_DL4C_EVENT_NODE_START_CB(18, 2, { reatt_n2_1_fut3_add2_1_Add0_Transpose_output.data->data,reatt_n2_1_h_tile_1_Tile0_Transpose_output.data->data});
  forward_concat(&reatt_n2_1_fusion_cat_1_concat0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(18, 1, { reatt_n2_1_fusion_cat_1_concat0_output.data->data});
}
void forward_lite_ap_integer_INT8_reatt_n2_1_fusion_ln_1_moments_mean0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_cat_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 13452);
  reatt_n2_1_fusion_cat_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 13452);
  reatt_n2_1_fusion_ln_1_moments_mean0_output_array.data = AI_PTR(net_ctx->_activations[0] + 20172);
  reatt_n2_1_fusion_ln_1_moments_mean0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 20172);
  _STAI_DL4C_EVENT_NODE_START_CB(21, 1, { reatt_n2_1_fusion_cat_1_concat0_output.data->data});
  forward_ap_integer_INT8(&reatt_n2_1_fusion_ln_1_moments_mean0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(21, 1, { reatt_n2_1_fusion_ln_1_moments_mean0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_moments_SquaredDifference0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_cat_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 13452);
  reatt_n2_1_fusion_cat_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 13452);
  reatt_n2_1_fusion_ln_1_moments_mean0_output_array.data = AI_PTR(net_ctx->_activations[0] + 20172);
  reatt_n2_1_fusion_ln_1_moments_mean0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 20172);
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(25, 2, { reatt_n2_1_fusion_cat_1_concat0_output.data->data,reatt_n2_1_fusion_ln_1_moments_mean0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(25, 1, { reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_DL4C_EVENT_NODE_START_CB(30, 2, { reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output.data->data,reatt_n2_1_fusion_ln_1_moments_SquaredDifference0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(30, 1, { reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output.data->data});
}
void forward_lite_ap_integer_INT8_reatt_n2_1_fusion_ln_1_moments_variance0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fusion_ln_1_moments_variance0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_moments_variance0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(33, 1, { reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850_output.data->data});
  forward_ap_integer_INT8(&reatt_n2_1_fusion_ln_1_moments_variance0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(33, 1, { reatt_n2_1_fusion_ln_1_moments_variance0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_add0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_ln_1_moments_variance0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_moments_variance0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fut0_ln1_1_add_y0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n2_1_fut0_ln1_1_add_y0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n2_1_fusion_ln_1_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fusion_ln_1_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_DL4C_EVENT_NODE_START_CB(36, 2, { reatt_n2_1_fusion_ln_1_moments_variance0_output.data->data,reatt_n2_1_fut0_ln1_1_add_y0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_ln_1_add0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(36, 1, { reatt_n2_1_fusion_ln_1_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_mul0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 120);
  reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 120);
  reatt_n2_1_fusion_ln_1_Reshape0_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 284);
  reatt_n2_1_fusion_ln_1_Reshape0_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 284);
  reatt_n2_1_fusion_ln_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(43, 2, { reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_output.data->data,reatt_n2_1_fusion_ln_1_Reshape0_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_ln_1_mul0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(43, 1, { reatt_n2_1_fusion_ln_1_mul0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_mul_20(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_cat_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 13452);
  reatt_n2_1_fusion_cat_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 13452);
  reatt_n2_1_fusion_ln_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fusion_ln_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_DL4C_EVENT_NODE_START_CB(46, 2, { reatt_n2_1_fusion_cat_1_concat0_output.data->data,reatt_n2_1_fusion_ln_1_mul0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_ln_1_mul_20_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(46, 1, { reatt_n2_1_fusion_ln_1_mul_20_output.data->data});
}
void forward_lite_nl_integer_reatt_n2_1_fusion_ln_1_Neg0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_ln_1_moments_mean0_output_array.data = AI_PTR(net_ctx->_activations[0] + 20172);
  reatt_n2_1_fusion_ln_1_moments_mean0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 20172);
  reatt_n2_1_fusion_ln_1_Neg0_output_array.data = AI_PTR(net_ctx->_activations[0] + 20140);
  reatt_n2_1_fusion_ln_1_Neg0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 20140);
  _STAI_DL4C_EVENT_NODE_START_CB(24, 1, { reatt_n2_1_fusion_ln_1_moments_mean0_output.data->data});
  forward_nl_integer(&reatt_n2_1_fusion_ln_1_Neg0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(24, 1, { reatt_n2_1_fusion_ln_1_Neg0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_mul_10(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_ln_1_Neg0_output_array.data = AI_PTR(net_ctx->_activations[0] + 20140);
  reatt_n2_1_fusion_ln_1_Neg0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 20140);
  reatt_n2_1_fusion_ln_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(47, 2, { reatt_n2_1_fusion_ln_1_Neg0_output.data->data,reatt_n2_1_fusion_ln_1_mul0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_ln_1_mul_10_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(47, 1, { reatt_n2_1_fusion_ln_1_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_add_10(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_ln_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_Reshape_10_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 508);
  reatt_n2_1_fusion_ln_1_Reshape_10_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 508);
  reatt_n2_1_fusion_ln_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(52, 2, { reatt_n2_1_fusion_ln_1_mul_10_output.data->data,reatt_n2_1_fusion_ln_1_Reshape_10_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_ln_1_add_10_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(52, 1, { reatt_n2_1_fusion_ln_1_add_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_add_20(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_ln_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fusion_ln_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fusion_ln_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(55, 2, { reatt_n2_1_fusion_ln_1_mul_20_output.data->data,reatt_n2_1_fusion_ln_1_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_ln_1_add_20_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(55, 1, { reatt_n2_1_fusion_ln_1_add_20_output.data->data});
}
void forward_lite_transpose_reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_ln_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_ln_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 13480);
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 13480);
  _STAI_DL4C_EVENT_NODE_START_CB(61, 1, { reatt_n2_1_fusion_ln_1_add_20_output.data->data});
  forward_transpose(&reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(61, 1, { reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output.data->data});
}
void forward_lite_transpose_reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 16360);
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 16360);
  _STAI_DL4C_EVENT_NODE_START_CB(61, 1, { reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_output.data->data});
  forward_transpose(&reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(61, 1, { reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_BiasAdd0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 16360);
  reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 16360);
  reatt_n2_1_fusion_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 28);
  reatt_n2_1_fusion_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 28);
  reatt_n2_1_fusion_dense_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 2880);
  reatt_n2_1_fusion_dense_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 2880);
  _STAI_DL4C_EVENT_NODE_START_CB(67, 2, { reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output.data->data,reatt_n2_1_fusion_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_dense_1_BiasAdd0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(67, 1, { reatt_n2_1_fusion_dense_1_BiasAdd0_output.data->data});
}
void forward_lite_eltwise_reatt_n2_1_fusion_dense_1_Gelu_Pow0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fut3_ffn1_1_Gelu_Pow_y0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n2_1_fut3_ffn1_1_Gelu_Pow_y0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n2_1_fusion_dense_1_Gelu_Pow0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Gelu_Pow0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(71, 2, { reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_output.data->data,reatt_n2_1_fut3_ffn1_1_Gelu_Pow_y0_3D.data->data});
  forward_eltwise(&reatt_n2_1_fusion_dense_1_Gelu_Pow0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(71, 1, { reatt_n2_1_fusion_dense_1_Gelu_Pow0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_Gelu_mul_10(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_fusion_dense_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10572);
  _STAI_DL4C_EVENT_NODE_START_CB(76, 2, { reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_dense_1_Gelu_mul_10_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(76, 1, { reatt_n2_1_fusion_dense_1_Gelu_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_Gelu_add0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_dense_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 2880);
  reatt_n2_1_fusion_dense_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 2880);
  reatt_n2_1_fusion_dense_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_fusion_dense_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_fusion_dense_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(79, 2, { reatt_n2_1_fusion_dense_1_BiasAdd0_output.data->data,reatt_n2_1_fusion_dense_1_Gelu_mul_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_dense_1_Gelu_add0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(79, 1, { reatt_n2_1_fusion_dense_1_Gelu_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_Gelu_mul_20(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n2_1_fusion_dense_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_fusion_dense_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10572);
  _STAI_DL4C_EVENT_NODE_START_CB(82, 2, { reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n2_1_fusion_dense_1_Gelu_add0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_dense_1_Gelu_mul_20_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(82, 1, { reatt_n2_1_fusion_dense_1_Gelu_mul_20_output.data->data});
}
void forward_lite_nl_integer_reatt_n2_1_fusion_dense_1_Gelu_Tanh0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_dense_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_fusion_dense_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(85, 1, { reatt_n2_1_fusion_dense_1_Gelu_mul_20_output.data->data});
  forward_nl_integer(&reatt_n2_1_fusion_dense_1_Gelu_Tanh0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(85, 1, { reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_Gelu_add_10(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_fusion_dense_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10572);
  _STAI_DL4C_EVENT_NODE_START_CB(88, 2, { reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n2_1_fusion_dense_1_Gelu_Tanh0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_dense_1_Gelu_add_10_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(88, 1, { reatt_n2_1_fusion_dense_1_Gelu_add_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_Gelu_mul0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 24);
  reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 24);
  reatt_n2_1_fusion_dense_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 2880);
  reatt_n2_1_fusion_dense_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 2880);
  reatt_n2_1_fusion_dense_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(70, 2, { reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n2_1_fusion_dense_1_BiasAdd0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_dense_1_Gelu_mul0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(70, 1, { reatt_n2_1_fusion_dense_1_Gelu_mul0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_Gelu_mul_30(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_dense_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fusion_dense_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_fusion_dense_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_fusion_dense_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fusion_dense_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_DL4C_EVENT_NODE_START_CB(91, 2, { reatt_n2_1_fusion_dense_1_Gelu_mul0_output.data->data,reatt_n2_1_fusion_dense_1_Gelu_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fusion_dense_1_Gelu_mul_30_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(91, 1, { reatt_n2_1_fusion_dense_1_Gelu_mul_30_output.data->data});
}
void forward_lite_transpose_reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_dense_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fusion_dense_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(97, 1, { reatt_n2_1_fusion_dense_1_Gelu_mul_30_output.data->data});
  forward_transpose(&reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(97, 1, { reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output.data->data});
}
void forward_lite_transpose_reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_output_array.data = AI_PTR(net_ctx->_activations[0] + 1024);
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 1024);
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(97, 1, { reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_output.data->data});
  forward_transpose(&reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(97, 1, { reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_BiasAdd0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 156);
  reatt_n2_1_dec_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 156);
  reatt_n2_1_dec_dense_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_dec_dense_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_DL4C_EVENT_NODE_START_CB(103, 2, { reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output.data->data,reatt_n2_1_dec_dense_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_dec_dense_1_BiasAdd0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(103, 1, { reatt_n2_1_dec_dense_1_BiasAdd0_output.data->data});
}
void forward_lite_eltwise_reatt_n2_1_dec_dense_1_Gelu_Pow0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_fut3_ffn1_1_Gelu_Pow_y0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n2_1_fut3_ffn1_1_Gelu_Pow_y0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n2_1_dec_dense_1_Gelu_Pow0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Gelu_Pow0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(107, 2, { reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_output.data->data,reatt_n2_1_fut3_ffn1_1_Gelu_Pow_y0_3D.data->data});
  forward_eltwise(&reatt_n2_1_dec_dense_1_Gelu_Pow0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(107, 1, { reatt_n2_1_dec_dense_1_Gelu_Pow0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_Gelu_mul_10(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_dec_dense_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10572);
  _STAI_DL4C_EVENT_NODE_START_CB(112, 2, { reatt_n2_1_fut1_ffn1_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_dec_dense_1_Gelu_mul_10_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(112, 1, { reatt_n2_1_dec_dense_1_Gelu_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_Gelu_add0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_dec_dense_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_dec_dense_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_dec_dense_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_dec_dense_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_dec_dense_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(115, 2, { reatt_n2_1_dec_dense_1_BiasAdd0_output.data->data,reatt_n2_1_dec_dense_1_Gelu_mul_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_dec_dense_1_Gelu_add0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(115, 1, { reatt_n2_1_dec_dense_1_Gelu_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_Gelu_mul_20(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n2_1_dec_dense_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_dec_dense_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10572);
  _STAI_DL4C_EVENT_NODE_START_CB(118, 2, { reatt_n2_1_fusion_dense_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n2_1_dec_dense_1_Gelu_add0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_dec_dense_1_Gelu_mul_20_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(118, 1, { reatt_n2_1_dec_dense_1_Gelu_mul_20_output.data->data});
}
void forward_lite_nl_integer_reatt_n2_1_dec_dense_1_Gelu_Tanh0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_dec_dense_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_dec_dense_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_dec_dense_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(121, 1, { reatt_n2_1_dec_dense_1_Gelu_mul_20_output.data->data});
  forward_nl_integer(&reatt_n2_1_dec_dense_1_Gelu_Tanh0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(121, 1, { reatt_n2_1_dec_dense_1_Gelu_Tanh0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_Gelu_add_10(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n2_1_dec_dense_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_dec_dense_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10572);
  _STAI_DL4C_EVENT_NODE_START_CB(124, 2, { reatt_n2_1_fut2_ffn1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n2_1_dec_dense_1_Gelu_Tanh0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_dec_dense_1_Gelu_add_10_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(124, 1, { reatt_n2_1_dec_dense_1_Gelu_add_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_Gelu_mul0(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 24);
  reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 24);
  reatt_n2_1_dec_dense_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_dec_dense_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_dec_dense_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(106, 2, { reatt_n2_1_fusion_dense_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n2_1_dec_dense_1_BiasAdd0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_dec_dense_1_Gelu_mul0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(106, 1, { reatt_n2_1_dec_dense_1_Gelu_mul0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_Gelu_mul_30(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_dec_dense_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_dec_dense_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_dec_dense_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10572);
  reatt_n2_1_dec_dense_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_dec_dense_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_DL4C_EVENT_NODE_START_CB(127, 2, { reatt_n2_1_dec_dense_1_Gelu_mul0_output.data->data,reatt_n2_1_dec_dense_1_Gelu_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_dec_dense_1_Gelu_mul_30_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(127, 1, { reatt_n2_1_dec_dense_1_Gelu_mul_30_output.data->data});
}
void forward_lite_transpose_reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_dec_dense_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_dec_dense_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 6732);
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6732);
  _STAI_DL4C_EVENT_NODE_START_CB(133, 1, { reatt_n2_1_dec_dense_1_Gelu_mul_30_output.data->data});
  forward_transpose(&reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(133, 1, { reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output.data->data});
}
void forward_lite_transpose_reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_output_array.data = AI_PTR(net_ctx->_activations[0] + 24);
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 24);
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 116);
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 116);
  _STAI_DL4C_EVENT_NODE_START_CB(133, 1, { reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_output.data->data});
  forward_transpose(&reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(133, 1, { reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_output.data->data});
}
void forward_lite_eltwise_raw_proj_QuantizeLinear_Input(_stai_dl4c_context* net_ctx)
{
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 208);
  reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 208);
  reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 6720);
  reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6720);
  raw_proj_QuantizeLinear_Input_output_array.data = AI_PTR(net_ctx->_activations[0] + 568);
  raw_proj_QuantizeLinear_Input_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 568);
  _STAI_DL4C_EVENT_NODE_START_CB(139, 2, { reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_output.data->data,reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_output.data->data});
  forward_eltwise(&raw_proj_QuantizeLinear_Input_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(139, 1, { raw_proj_QuantizeLinear_Input_output.data->data});
}
void forward_lite_transpose_raw_proj_QuantizeLinear_Input_Transpose_0(_stai_dl4c_context* net_ctx)
{
  raw_proj_QuantizeLinear_Input_output_array.data = AI_PTR(net_ctx->_activations[0] + 568);
  raw_proj_QuantizeLinear_Input_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 568);
  raw_proj_QuantizeLinear_Input_Transpose_0_output_array.data = AI_PTR(net_ctx->_outputs[0] + 0);
  raw_proj_QuantizeLinear_Input_Transpose_0_output_array.data_start = AI_PTR(net_ctx->_outputs[0] + 0);
  _STAI_DL4C_EVENT_NODE_START_CB(1, 1, { raw_proj_QuantizeLinear_Input_output.data->data});
  forward_transpose(&raw_proj_QuantizeLinear_Input_Transpose_0_layer);
  _STAI_DL4C_EVENT_NODE_STOP_CB(1, 1, { raw_proj_QuantizeLinear_Input_Transpose_0_output.data->data});
}

/*****************************************************************************/


static const ai_u32 reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3;
static const ai_float reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_t_in_0_fmt_scale_const_f32 = 0.7036324739456177f;
static const ai_i8 reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_t_in_0_fmt_zero_const_s8 = -128;









static const ai_u32 reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 30;
static const ai_float reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32 = 0.7026578783988953f;
static const ai_i8 reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_i32 reatt_n2_1_fusion_ln_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32 = 30;

static const ai_i32 reatt_n2_1_fusion_ln_1_Rsqrt__3860_t_in_0_shape_ch_h_prod_const_s32 = 30;

static const ai_u32 reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 30;
static const ai_float reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_t_out_0_fmt_scale_const_f32 = 0.0014657231513410807f;
static const ai_i8 reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_t_out_0_fmt_zero_const_s8 = -128;








static const ai_u16 reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_h_const_u16 = 30;
static const ai_u16 reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_ch_const_u16 = 224;
static const ai_u16 reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_shape_ch_const_u16 = 128;
static const ai_i8 reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_fmt_zero_const_s8 = 8;
static const ai_i8 reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_fmt_zero_const_s8 = -12;
static const ai_float reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_fmt_scale_const_f32 = 0.05802692100405693f;
static const ai_float reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_fmt_scale_const_f32 = 0.18565353751182556f;
static const ai_float reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0021195157896727324f, 0.00545008759945631f, 0.00576843973249197f, 0.003463748376816511f, 0.0016567067941650748f, 0.0020247376523911953f, 0.0017788299592211843f, 0.002255919622257352f, 0.003466664580628276f, 0.004491705447435379f, 0.004557378590106964f, 0.002127742860466242f, 0.0020754809956997633f, 0.005360215436667204f, 0.004820977803319693f, 0.003941635601222515f, 0.00181601254735142f, 0.0023059819359332323f, 0.0018610208062455058f, 0.002121889963746071f, 0.002045833505690098f, 0.0020947004668414593f, 0.0020975908264517784f, 0.0018698022468015552f, 0.0020049980375915766f, 0.0019924312364310026f, 0.004797027446329594f, 0.0024621698539704084f, 0.0018341195536777377f, 0.0021852757781744003f, 0.0018660725327208638f, 0.002001246204599738f, 0.0018978104926645756f, 0.0015906284097582102f, 0.0019701567944139242f, 0.0019934773445129395f, 0.002411070978268981f, 0.0021140624303370714f, 0.002770929131656885f, 0.001993234036490321f, 0.005540070123970509f, 0.002525847405195236f, 0.001968204043805599f, 0.002162862801924348f, 0.0047769867815077305f, 0.004248622339218855f, 0.0027294508181512356f, 0.0025666833389550447f, 0.00209584878757596f, 0.004695652984082699f, 0.002013767370954156f, 0.0023714739363640547f, 0.002674469258636236f, 0.0029561275150626898f, 0.001797368167899549f, 0.0018641249043866992f, 0.0019829466473311186f, 0.002065019216388464f, 0.0019950049463659525f, 0.0022608316503465176f, 0.0025785458274185658f, 0.007538905832916498f, 0.003436377737671137f, 0.0028621600940823555f, 0.0019276599632576108f, 0.003750309580937028f, 0.0021261058282107115f, 0.0043198661878705025f, 0.0025304178707301617f, 0.0022548085544258356f, 0.002513573970645666f, 0.0025227265432476997f, 0.0022110675927251577f, 0.0049524204805493355f, 0.0027192747220396996f, 0.0022393586114048958f, 0.0018986479844897985f, 0.0022149907890707254f, 0.001886541722342372f, 0.0034582281950861216f, 0.0018681737128645182f, 0.0018833338981494308f, 0.0018685631221160293f, 0.0021665242966264486f, 0.0019950135610997677f, 0.0022739414125680923f, 0.002077516634017229f, 0.0024686132092028856f, 0.0033621718175709248f, 0.0023092690389603376f, 0.0022202525287866592f, 0.0029403241351246834f, 0.0025321177672594786f, 0.0051094600930809975f, 0.006032149773091078f, 0.0032939906232059f, 0.003056129440665245f, 0.002151902299374342f, 0.0023280573077499866f, 0.0018292663153260946f, 0.0021342120599001646f, 0.002204734366387129f, 0.0023260044399648905f, 0.003920774906873703f, 0.007776307407766581f, 0.0021194859873503447f, 0.002214728156104684f, 0.002279693726450205f, 0.0020020247902721167f, 0.0021429709158837795f, 0.007674191612750292f, 0.0023975037038326263f, 0.002795914886519313f, 0.0021362968254834414f, 0.0017757401801645756f, 0.0017999026458710432f, 0.0027492789085954428f, 0.0021332846954464912f, 0.002508921781554818f, 0.0018983213230967522f, 0.002099926583468914f, 0.002035663928836584f, 0.006619009654968977f, 0.0016461588675156236f, 0.0022063020151108503f, 0.002357501769438386f, 0.0023008857388049364f, 0.0018431524513289332f);
static const ai_layer_format_type reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_u32 reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3840;
static const ai_float reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32 = 0.18565353751182556f;
static const ai_i8 reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8 = -13;


static const ai_u32 reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3840;
static const ai_float reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 106.96804809570312f;
static const ai_i8 reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8 = -37;









static const ai_u16 reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_h_const_u16 = 30;
static const ai_u16 reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_ch_const_u16 = 128;
static const ai_u16 reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_shape_ch_const_u16 = 128;
static const ai_i8 reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_fmt_zero_const_s8 = -126;
static const ai_i8 reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_fmt_zero_const_s8 = -27;
static const ai_float reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_fmt_scale_const_f32 = 0.10251101106405258f;
static const ai_float reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_fmt_scale_const_f32 = 0.8406862616539001f;
static const ai_float reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.001454762415960431f, 0.001643190742470324f, 0.0026192432269454002f, 0.0030453845392912626f, 0.010995418764650822f, 0.011972332373261452f, 0.009869764558970928f, 0.009594323113560677f, 0.00907712522894144f, 0.013845575973391533f, 0.010212334804236889f, 0.011012464761734009f, 0.009466477669775486f, 0.0069089168682694435f, 0.001363225164823234f, 0.00783050898462534f, 0.007349297404289246f, 0.009315733797848225f, 0.007732434663921595f, 0.010004128329455853f, 0.0017130535561591387f, 0.007909799925982952f, 0.006670517381280661f, 0.0013934631133452058f, 0.001352160470560193f, 0.008378301747143269f, 0.01132531650364399f, 0.010166683234274387f, 0.006943047046661377f, 0.006243591662496328f, 0.009637466631829739f, 0.008242502808570862f, 0.006225604563951492f, 0.001522923936136067f, 0.0013903361978009343f, 0.0015425861347466707f, 0.0014425736153498292f, 0.004077124409377575f, 0.0017130590276792645f, 0.010995172895491123f, 0.011445876210927963f, 0.009421556256711483f, 0.001300302566960454f, 0.0014045105781406164f, 0.0013616438955068588f, 0.009789224714040756f, 0.00864716898649931f, 0.00914116483181715f, 0.0064563509076833725f, 0.012446724809706211f, 0.005265198647975922f, 0.009842603467404842f, 0.009060333482921124f, 0.009554686956107616f, 0.001665232703089714f, 0.006462481338530779f, 0.0013558578211814165f, 0.004650261718779802f, 0.00874772947281599f, 0.009875952266156673f, 0.00953382346779108f, 0.008943150751292706f, 0.00137939618434757f, 0.006722932681441307f, 0.00887682568281889f, 0.008710834197700024f, 0.0015560610918328166f, 0.001555454800836742f, 0.001423900481313467f, 0.0014012794708833098f, 0.010331625118851662f, 0.0014784291852265596f, 0.007772330194711685f, 0.006272891070693731f, 0.0012786848237738013f, 0.001513250288553536f, 0.001281140954233706f, 0.012200655415654182f, 0.0013187471777200699f, 0.009830464608967304f, 0.009692416526377201f, 0.008271561935544014f, 0.01070581004023552f, 0.008522924967110157f, 0.0019645518623292446f, 0.011371438391506672f, 0.0017289107199758291f, 0.0013072304427623749f, 0.01060139574110508f, 0.001234990544617176f, 0.011728353798389435f, 0.009620122611522675f, 0.010376593098044395f, 0.001293396344408393f, 0.009136236272752285f, 0.008381091058254242f, 0.0015625065425410867f, 0.001320685027167201f, 0.0013583647087216377f, 0.0014191825175657868f, 0.0013467941898852587f, 0.007915993221104145f, 0.010412280447781086f, 0.011330503970384598f, 0.009250497445464134f, 0.0014050876488909125f, 0.008388764224946499f, 0.012628277763724327f, 0.00977529026567936f, 0.0095734354108572f, 0.008906534872949123f, 0.0012865159660577774f, 0.0012553759152069688f, 0.001354619744233787f, 0.009570763446390629f, 0.008707013912498951f, 0.008297438733279705f, 0.009458749555051327f, 0.0015617316821590066f, 0.0017291181720793247f, 0.001529680215753615f, 0.010051438584923744f, 0.006789105944335461f, 0.010633553378283978f, 0.0014737735036760569f, 0.0014383642701432109f, 0.0097347442060709f, 0.010531268082559109f);
static const ai_layer_format_type reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_u32 reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3840;
static const ai_float reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32 = 0.8410041332244873f;
static const ai_i8 reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8 = -27;


static const ai_u32 reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3840;
static const ai_float reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 10898.0751953125f;
static const ai_i8 reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8 = -71;









static const ai_u16 reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_h_const_u16 = 30;
static const ai_u16 reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_ch_const_u16 = 128;
static const ai_u16 reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_shape_ch_const_u16 = 3;
static const ai_i8 reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_fmt_zero_const_s8 = -87;
static const ai_float reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_fmt_scale_const_f32 = 0.5076972842216492f;
static const ai_float reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_fmt_scale_const_f32 = 4.306164264678955f;
static const ai_float reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0036261677742004395f, 0.0070250495336949825f, 0.003534780815243721f);
static const ai_layer_format_type reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;


static const ai_u32 reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 90;
static const ai_float reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_t_in_0_fmt_scale_const_f32 = 4.306164264678955f;
static const ai_i8 reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_t_in_0_fmt_zero_const_s8 = -87;


STAI_API_ENTRY
stai_return_code stai_dl4c_run(
  stai_network* network,
  const stai_run_mode mode)
{
   STAI_UNUSED(mode)
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_ACTIVATIONS) != STAI_FLAG_ACTIVATIONS,
        STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_INPUTS) != STAI_FLAG_INPUTS,
                  STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_OUTPUTS) != STAI_FLAG_OUTPUTS,
                  STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_WEIGHTS) != STAI_FLAG_WEIGHTS,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)


  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion */
  {
      const ai_i8* reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 4);
    ai_float* reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 6720);
  
  _STAI_DL4C_EVENT_NODE_START_CB(12, 1, {(stai_ptr) reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_t_in_0_ptr_const_s8, reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_t_out_0_ptr_f32, reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_t_in_0_fmt_scale_const_f32, reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(12, 1, {(stai_ptr) reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_raw_proj_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_h_tile_1_Tile0_Transpose */
  {
    
  forward_lite_transpose_reatt_n2_1_h_tile_1_Tile0_Transpose(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_h_tile_1_Tile0_Transpose */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut3_add2_1_Add0_Transpose */
  {
    
  forward_lite_transpose_reatt_n2_1_fut3_add2_1_Add0_Transpose(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut3_add2_1_Add0_Transpose */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_cat_1_concat0 */
  {
    
  forward_lite_concat_reatt_n2_1_fusion_cat_1_concat0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_cat_1_concat0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_moments_mean0 */
  {
    
  forward_lite_ap_integer_INT8_reatt_n2_1_fusion_ln_1_moments_mean0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_moments_mean0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_moments_SquaredDifference0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_moments_SquaredDifference0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_moments_SquaredDifference0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_moments_SquaredDifference__3850 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_moments_variance0 */
  {
    
  forward_lite_ap_integer_INT8_reatt_n2_1_fusion_ln_1_moments_variance0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_moments_variance0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion */
  {
      const ai_i8* reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_float* reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 32);
  
  _STAI_DL4C_EVENT_NODE_START_CB(36, 1, {(stai_ptr) reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion_t_in_0_ptr_const_s8, reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion_t_out_0_ptr_f32, reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32, reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(36, 1, {(stai_ptr) reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_add0_0_0_reatt_n2_1_fusion_ln_1_Rsqrt0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_Rsqrt0 */
  {
      ai_handle reatt_n2_1_fusion_ln_1_Rsqrt0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 152);
    const ai_handle reatt_n2_1_fusion_ln_1_Rsqrt0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 32);
  
  _STAI_DL4C_EVENT_NODE_START_CB(39, 1, {(stai_ptr) reatt_n2_1_fusion_ln_1_Rsqrt0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_sqrt_if32of32(reatt_n2_1_fusion_ln_1_Rsqrt0_t_out_0_ptr_handle, reatt_n2_1_fusion_ln_1_Rsqrt0_t_in_0_ptr_const_handle, reatt_n2_1_fusion_ln_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(39, 1, {(stai_ptr) reatt_n2_1_fusion_ln_1_Rsqrt0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_Rsqrt0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_Rsqrt__3860 */
  {
      ai_handle reatt_n2_1_fusion_ln_1_Rsqrt__3860_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 0);
    const ai_handle reatt_n2_1_fusion_ln_1_Rsqrt__3860_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 152);
  
  _STAI_DL4C_EVENT_NODE_START_CB(40, 1, {(stai_ptr) reatt_n2_1_fusion_ln_1_Rsqrt__3860_t_in_0_ptr_const_handle});
    
  forward_lite_nl_reciprocal_if32of32(reatt_n2_1_fusion_ln_1_Rsqrt__3860_t_out_0_ptr_handle, reatt_n2_1_fusion_ln_1_Rsqrt__3860_t_in_0_ptr_const_handle, reatt_n2_1_fusion_ln_1_Rsqrt__3860_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(40, 1, {(stai_ptr) reatt_n2_1_fusion_ln_1_Rsqrt__3860_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_Rsqrt__3860 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion */
  {
      const ai_float* reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_i8* reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 120);
  
  _STAI_DL4C_EVENT_NODE_START_CB(40, 1, {(stai_ptr) reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_t_in_0_ptr_const_f32, reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_t_out_0_ptr_s8, reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_t_out_0_fmt_scale_const_f32, reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(40, 1, {(stai_ptr) reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_Rsqrt__3860_0_0_reatt_n2_1_fusion_ln_1_mul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_Neg0 */
  {
    
  forward_lite_nl_integer_reatt_n2_1_fusion_ln_1_Neg0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_Neg0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_add_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_ln_1_add_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_ln_1_add_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_ln_1_add_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose */
  {
    
  forward_lite_transpose_reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense */
  {
      const ai_i8* reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 13480);
    const ai_i8* reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 732);
    const ai_i32* reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 29404);
    ai_i8* reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 6732);
    ai_i16* reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 5696);
  
  _STAI_DL4C_EVENT_NODE_START_CB(61, 1, {(stai_ptr) reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_ptr_const_s8, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_w_const_u16, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_h_const_u16, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_l_stride_1_const_u16, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_l_stride_0_const_u16, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_ch_const_u16, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_weight_0_ptr_const_s8, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_shape_ch_const_u16, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_weight_1_ptr_const_s32, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_fmt_zero_const_s8, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_fmt_zero_const_s8, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_fmt_scale_const_f32, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_fmt_scale_const_f32, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_weight_0_fmt_scale_const_f32, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_l_out_ch_format_const_layer_format_type, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_ptr_s8, 1, 1024, reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_scratch_0_ptr_s16);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(61, 1, {(stai_ptr) reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose */
  {
    
  forward_lite_transpose_reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_BiasAdd0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion */
  {
      const ai_i8* reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 2880);
    ai_float* reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 6732);
  
  _STAI_DL4C_EVENT_NODE_START_CB(67, 1, {(stai_ptr) reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8, reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_t_out_0_ptr_f32, reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32, reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(67, 1, {(stai_ptr) reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_BiasAdd0_0_0_reatt_n2_1_fusion_dense_1_Gelu_Pow0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_Gelu_Pow0 */
  {
    
  forward_lite_eltwise_reatt_n2_1_fusion_dense_1_Gelu_Pow0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_Gelu_Pow0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion */
  {
      const ai_float* reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 6732);
    ai_i8* reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 6732);
  
  _STAI_DL4C_EVENT_NODE_START_CB(71, 1, {(stai_ptr) reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_t_out_0_ptr_s8, reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(71, 1, {(stai_ptr) reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_Gelu_Pow0_0_1_reatt_n2_1_fusion_dense_1_Gelu_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_Gelu_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_Gelu_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_Gelu_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_Gelu_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_Gelu_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_Gelu_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_Gelu_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_Gelu_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_Gelu_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_Gelu_Tanh0 */
  {
    
  forward_lite_nl_integer_reatt_n2_1_fusion_dense_1_Gelu_Tanh0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_Gelu_Tanh0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_Gelu_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_Gelu_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_Gelu_add_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_Gelu_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_Gelu_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_Gelu_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fusion_dense_1_Gelu_mul_30 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fusion_dense_1_Gelu_mul_30(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fusion_dense_1_Gelu_mul_30 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose */
  {
    
  forward_lite_transpose_reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_in_transpose */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense */
  {
      const ai_i8* reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 6732);
    const ai_i8* reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 29916);
    const ai_i32* reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 29404);
    ai_i8* reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 1024);
    ai_i16* reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 0);
  
  _STAI_DL4C_EVENT_NODE_START_CB(97, 1, {(stai_ptr) reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_ptr_const_s8, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_w_const_u16, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_h_const_u16, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_l_stride_1_const_u16, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_l_stride_0_const_u16, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_ch_const_u16, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_weight_0_ptr_const_s8, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_shape_ch_const_u16, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_weight_1_ptr_const_s32, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_fmt_zero_const_s8, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_fmt_zero_const_s8, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_fmt_scale_const_f32, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_fmt_scale_const_f32, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_weight_0_fmt_scale_const_f32, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_l_out_ch_format_const_layer_format_type, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_ptr_s8, 1, 1024, reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_scratch_0_ptr_s16);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(97, 1, {(stai_ptr) reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose */
  {
    
  forward_lite_transpose_reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_Tensordot_MatMul0_gemm_to_dense_out_transpose */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_BiasAdd0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion */
  {
      const ai_i8* reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_float* reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 6732);
  
  _STAI_DL4C_EVENT_NODE_START_CB(103, 1, {(stai_ptr) reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8, reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_t_out_0_ptr_f32, reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32, reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(103, 1, {(stai_ptr) reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_BiasAdd0_0_0_reatt_n2_1_dec_dense_1_Gelu_Pow0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_Gelu_Pow0 */
  {
    
  forward_lite_eltwise_reatt_n2_1_dec_dense_1_Gelu_Pow0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_Gelu_Pow0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion */
  {
      const ai_float* reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 6732);
    ai_i8* reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 6732);
  
  _STAI_DL4C_EVENT_NODE_START_CB(107, 1, {(stai_ptr) reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_t_out_0_ptr_s8, reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(107, 1, {(stai_ptr) reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_Gelu_Pow0_0_1_reatt_n2_1_dec_dense_1_Gelu_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_Gelu_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_Gelu_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_Gelu_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_Gelu_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_Gelu_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_Gelu_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_Gelu_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_Gelu_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_Gelu_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_Gelu_Tanh0 */
  {
    
  forward_lite_nl_integer_reatt_n2_1_dec_dense_1_Gelu_Tanh0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_Gelu_Tanh0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_Gelu_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_Gelu_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_Gelu_add_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_Gelu_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_Gelu_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_Gelu_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_dec_dense_1_Gelu_mul_30 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_dec_dense_1_Gelu_mul_30(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_dec_dense_1_Gelu_mul_30 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose */
  {
    
  forward_lite_transpose_reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_in_transpose */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense */
  {
      const ai_i8* reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 6732);
    const ai_i8* reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 46300);
    const ai_i32* reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 46684);
    ai_i8* reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 24);
    ai_i16* reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 0);
  
  _STAI_DL4C_EVENT_NODE_START_CB(133, 1, {(stai_ptr) reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_ptr_const_s8, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_w_const_u16, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_h_const_u16, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_l_stride_1_const_u16, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_l_stride_0_const_u16, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_shape_ch_const_u16, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_weight_0_ptr_const_s8, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_shape_ch_const_u16, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_weight_1_ptr_const_s32, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_fmt_zero_const_s8, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_fmt_zero_const_s8, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_in_0_fmt_scale_const_f32, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_fmt_scale_const_f32, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_weight_0_fmt_scale_const_f32, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_l_out_ch_format_const_layer_format_type, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_ptr_s8, 1, 24, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_scratch_0_ptr_s16);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(133, 1, {(stai_ptr) reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose */
  {
    
  forward_lite_transpose_reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion */
  {
      const ai_i8* reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 116);
    ai_float* reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 208);
  
  _STAI_DL4C_EVENT_NODE_START_CB(133, 1, {(stai_ptr) reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_t_in_0_ptr_const_s8, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_t_out_0_ptr_f32, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_t_in_0_fmt_scale_const_f32, reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_DL4C_EVENT_NODE_STOP_CB(133, 1, {(stai_ptr) reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_raw_proj_1_Tensordot_MatMul0_gemm_to_dense_out_transpose_0_0_raw_proj_QuantizeLinear_Input_conversion */
  /* LITE_KERNEL_SECTION BEGIN raw_proj_QuantizeLinear_Input */
  {
    
  forward_lite_eltwise_raw_proj_QuantizeLinear_Input(net_ctx);
  }
  /* LITE_KERNEL_SECTION END raw_proj_QuantizeLinear_Input */
  /* LITE_KERNEL_SECTION BEGIN raw_proj_QuantizeLinear_Input_Transpose_0 */
  {
    
  forward_lite_transpose_raw_proj_QuantizeLinear_Input_Transpose_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END raw_proj_QuantizeLinear_Input_Transpose_0 */
  return net_ctx->_return_code;
}

/*****************************************************************************/
/*  Getters APIs Section  */
STAI_API_ENTRY
stai_size stai_dl4c_get_context_size()
{
  return (stai_size)STAI_DL4C_CONTEXT_SIZE;
}

#if defined(HAVE_DL4C_INFO)
STAI_API_ENTRY
stai_return_code stai_dl4c_get_info(
  stai_network* network,
  stai_network_info* info)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, info==NULL, STAI_ERROR_NETWORK_INVALID_INFO, net_ctx->_return_code)

  // Copy of network info struct
  *info = g_dl4c_info;

  return STAI_SUCCESS;
}
#endif


STAI_API_ENTRY
stai_return_code stai_dl4c_get_activations(
  stai_network* network, stai_ptr* activations, stai_size* n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, !n_activations, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_activations = STAI_DL4C_ACTIVATIONS_NUM;
for (stai_size idx=0; activations && (idx<STAI_DL4C_ACTIVATIONS_NUM); idx++) {
    // get address of the activations buffers
    activations[idx] = net_ctx->_activations[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl4c_get_weights(
  stai_network* network, stai_ptr* weights, stai_size* n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_weights, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_weights = STAI_DL4C_WEIGHTS_NUM;
for (stai_size idx=0; weights && (idx<STAI_DL4C_WEIGHTS_NUM); idx++) {
    // get address of the weights buffers
    weights[idx] = net_ctx->_weights[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl4c_get_inputs(
  stai_network* network, stai_ptr* inputs, stai_size* n_inputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_inputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_inputs = STAI_DL4C_IN_NUM;
  for (stai_size idx=0; inputs && (idx<STAI_DL4C_IN_NUM); idx++) {
    inputs[idx] = net_ctx->_inputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl4c_get_outputs(
  stai_network* network, stai_ptr* outputs, stai_size* n_outputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_outputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_outputs = STAI_DL4C_OUT_NUM;
  for (stai_size idx=0; outputs && (idx<STAI_DL4C_OUT_NUM); idx++) {
    outputs[idx] = net_ctx->_outputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl4c_get_error(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /* return 1st generated error or STAI_SUCCESS if no errors so far */
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl4c_get_states(
  stai_network* network, stai_ptr* states, stai_size* n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_states, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  /* get the number of internals states (supporting multi-heap also for internal states) */
  *n_states = STAI_DL4C_STATES_NUM;

  STAI_UNUSED(states)
return net_ctx->_return_code;
}


/*****************************************************************************/
/*  Setters APIs Section  */

STAI_API_ENTRY
stai_return_code stai_dl4c_set_activations(
  stai_network* network,
  const stai_ptr* activations,
  const stai_size n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _activations_alignment[] = STAI_DL4C_ACTIVATIONS_ALIGNMENTS;
  STAI_PRINT("  [stai_dl4c_set_activations] network(%p) activations[%d]: %p\n\n", net_ctx, n_activations, activations)
  _STAI_SET_ERROR(net_ctx, !activations,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_activations!=STAI_DL4C_ACTIVATIONS_NUM,
                  STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_NUM, net_ctx->_return_code)

  for (stai_size idx=0; activations && idx<STAI_DL4C_ACTIVATIONS_NUM; idx++) {
    STAI_PRINT("  activation[%d]: %p\n", idx, activations[idx])
    _STAI_SET_ERROR(net_ctx, activations[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)activations[idx]) & (_activations_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_activations[idx] = activations[idx];
  }_stai_dl4c_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl4c_set_weights(
  stai_network* network,
  const stai_ptr* weights,
  const stai_size n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _weights_alignment[] = STAI_DL4C_WEIGHTS_ALIGNMENTS;
  _STAI_SET_ERROR(net_ctx, !weights,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_weights!=STAI_DL4C_WEIGHTS_NUM,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_NUM, net_ctx->_return_code)
  for (stai_size idx=0; weights && idx<STAI_DL4C_WEIGHTS_NUM; idx++) {
    STAI_PRINT("  weight[%d]: %p\n", idx, weights[idx])
    _STAI_SET_ERROR(net_ctx, weights[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)weights[idx]) & (_weights_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_weights[idx] = weights[idx];
  }_stai_dl4c_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl4c_set_inputs(
  stai_network* network,
  const stai_ptr* inputs,
  const stai_size n_inputs)
{
  const uintptr_t _inputs_alignment[] = STAI_DL4C_IN_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !inputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_inputs!=STAI_DL4C_IN_NUM,
                  STAI_ERROR_NETWORK_INVALID_IN_NUM, net_ctx->_return_code)

  for (stai_size idx=0; inputs && idx<STAI_DL4C_IN_NUM; idx++) {
    STAI_PRINT("  input[%d]: %p\n", idx, inputs[idx])
    _STAI_SET_ERROR(net_ctx, inputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)inputs[idx]) & (_inputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_inputs[idx] = inputs[idx];
  }

  _stai_dl4c_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl4c_set_outputs(
  stai_network* network,
  const stai_ptr* outputs,
  const stai_size n_outputs)
{
  const uintptr_t _outputs_alignment[] = STAI_DL4C_OUT_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !outputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_outputs!=STAI_DL4C_OUT_NUM,
                  STAI_ERROR_NETWORK_INVALID_OUT_NUM, net_ctx->_return_code)

  for (stai_size idx=0; outputs && idx<n_outputs; idx++) {
    STAI_PRINT("  output[%d]: %p\n", idx, outputs[idx])
    _STAI_SET_ERROR(net_ctx, outputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)outputs[idx]) & (_outputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_outputs[idx] = outputs[idx];
  }

  _stai_dl4c_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl4c_set_states(
  stai_network* network,
  const stai_ptr* states,
  const stai_size n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  STAI_UNUSED(states)
  STAI_UNUSED(n_states)
_stai_dl4c_check(net_ctx);
  return net_ctx->_return_code;
}

STAI_API_ENTRY
stai_return_code stai_dl4c_set_callback(
  stai_network* network, const stai_event_cb cb, void* cb_cookie)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  STAI_PRINT("  set_callback %p cb %p cookie %p\n", net_ctx, cb, cb_cookie)
  // _STAI_SET_ERROR(net_ctx, cb==NULL, STAI_ERROR_NETWORK_INVALID_CALLBACK, net_ctx->_return_code)
  net_ctx->_callback = cb;
  net_ctx->_callback_cookie = cb_cookie;
  return net_ctx->_return_code;
}

#undef _STAI_SET_ERROR
#undef _STAI_CONTEXT_ALIGNMENT
#undef _STAI_CONTEXT_ACQUIRE
#undef _STAI_DL4C_EVENT_NODE_START_CB
#undef _STAI_DL4C_EVENT_NODE_STOP_CB
#undef _STAI_DL4C_MODEL_SIGNATURE
#undef _STAI_DL4C_DATETIME
#undef _STAI_DL4C_COMPILE_DATETIME

