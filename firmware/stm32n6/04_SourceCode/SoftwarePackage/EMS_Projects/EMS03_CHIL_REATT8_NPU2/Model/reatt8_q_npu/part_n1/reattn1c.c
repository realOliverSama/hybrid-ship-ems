/**
  ******************************************************************************
  * @file    reattn1c.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-09-23T00:24:54+0800
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
#include "reattn1c.h"
#include "reattn1c_details.h"
#include "reattn1c_data.h"
#include "stai_events.h"

#include "lite_operators.h"

#include "ai_lite_inspect.h"
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
#ifndef _STAI_REATTN1C_EVENT_NODE_START_CB
  #define _STAI_REATTN1C_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
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
  #define _STAI_REATTN1C_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_REATTN1C_EVENT_NODE_START_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_START_CB */

#ifdef STAI_EVENT_NODE_STOP_CB
#ifndef _STAI_REATTN1C_EVENT_NODE_STOP_CB
  #define _STAI_REATTN1C_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
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
  #define _STAI_REATTN1C_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_REATTN1C_EVENT_NODE_STOP_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_STOP_CB */


/*****************************************************************************/
#define _STAI_REATTN1C_MODEL_SIGNATURE     "0xc932652f7f4563460d7f564d6478e539"
#define _STAI_REATTN1C_DATETIME            "2026-09-23T00:24:54+0800"
#define _STAI_REATTN1C_COMPILE_DATETIME    __DATE__ " " __TIME__

#define _STAI_CONTEXT_ALIGNMENT        STAI_REATTN1C_CONTEXT_ALIGNMENT

/*****************************************************************************/
#define g_reattn1c_activations_1     (NULL)




#if defined(HAVE_REATTN1C_INFO)
/*****************************************************************************/
static const stai_network_info g_reattn1c_info = {
  .model_signature = _STAI_REATTN1C_MODEL_SIGNATURE,
  .c_compile_datetime = _STAI_REATTN1C_COMPILE_DATETIME,
  .c_model_name = STAI_REATTN1C_MODEL_NAME,
  .c_model_datetime = _STAI_REATTN1C_DATETIME,
  .c_model_signature = 0x0,
  .runtime_version = STAI_INIT_VERSION(12, 0, 1),
  .tool_version = STAI_INIT_VERSION(4, 0, 1),
  .api_version = STAI_INIT_VERSION(1, 0, 0),
  .n_macc = STAI_REATTN1C_MACC_NUM,
  .n_nodes = STAI_REATTN1C_NODES_NUM,
  .flags = STAI_REATTN1C_FLAGS,
  .n_inputs = STAI_REATTN1C_IN_NUM,
  .n_outputs = STAI_REATTN1C_OUT_NUM,
  .n_activations = STAI_REATTN1C_ACTIVATIONS_NUM,
  .n_weights = STAI_REATTN1C_WEIGHTS_NUM,
  .n_states = STAI_REATTN1C_STATES_NUM,
  .inputs = (stai_tensor[STAI_REATTN1C_IN_NUM]) {
    STAI_INIT_TENSOR(
      STAI_REATTN1C_IN_1_NAME,
      STAI_REATTN1C_IN_1_FLAGS,
      STAI_REATTN1C_IN_1_FORMAT,
      STAI_REATTN1C_IN_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 300, 11),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
    .outputs = (stai_tensor[STAI_REATTN1C_OUT_NUM]) {
    STAI_INIT_TENSOR(
      STAI_REATTN1C_OUT_1_NAME,
      STAI_REATTN1C_OUT_1_FLAGS,
      STAI_REATTN1C_OUT_1_FORMAT,
      STAI_REATTN1C_OUT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 1, 96),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .activations = (stai_tensor[STAI_REATTN1C_ACTIVATIONS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_REATTN1C_ACTIVATION_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_REATTN1C_ACTIVATION_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 283776),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_REATTN1C_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_REATTN1C_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_REATTN1C_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 584392),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },

  .states = NULL
};
#endif

#define _STAI_CONTEXT_ACQUIRE(_net_ctx, _net_handle) \
  _stai_reattn1c_context* _net_ctx = (_stai_reattn1c_context*)(_net_handle); \
  STAI_ASSERT(_net_ctx != NULL) \
  _STAI_SET_ERROR(_net_ctx, _net_ctx->_magic != STAI_MAGIC, \
                  STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE, _net_ctx->_return_code)


/*****************************************************************************/
static
void _stai_reattn1c_check(_stai_reattn1c_context* net_ctx)
{
  stai_size idx;

// Check activations status
  for (idx=0; idx<STAI_REATTN1C_ACTIVATIONS_NUM; idx++) {
    if (net_ctx->_activations[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_REATTN1C_ACTIVATIONS_NUM) ? STAI_FLAG_ACTIVATIONS : STAI_FLAG_NONE;
// Check inputs status
  for (idx=0; idx<STAI_REATTN1C_IN_NUM; idx++) {
    if (net_ctx->_inputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_REATTN1C_IN_NUM) ? STAI_FLAG_INPUTS : STAI_FLAG_NONE;

  // Check outputs status
  for (idx=0; idx<STAI_REATTN1C_OUT_NUM; idx++) {
    if (net_ctx->_outputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_REATTN1C_OUT_NUM) ? STAI_FLAG_OUTPUTS : STAI_FLAG_NONE;

// Check weights status
  for (idx=0; idx<STAI_REATTN1C_WEIGHTS_NUM; idx++) {
    if (net_ctx->_weights[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_REATTN1C_WEIGHTS_NUM) ? STAI_FLAG_WEIGHTS : STAI_FLAG_NONE;
STAI_PRINT("  [_stai_network_check] flags: 0x%08x\n", net_ctx->_flags)
}


/*****************************************************************************/
STAI_API_ENTRY
stai_return_code stai_reattn1c_init(
  stai_network* network)
{
  /* Memory where to store internal context is provided by applications as a raw byte buffer */
  _stai_reattn1c_context* net_ctx = (_stai_reattn1c_context*)(network);
  net_ctx->_return_code = STAI_SUCCESS;
  STAI_PRINT("[Entering Network Init] network(%p) context_size(%d)\n", net_ctx, (int32_t)sizeof(_stai_reattn1c_context))

  _STAI_SET_ERROR(net_ctx, STAI_REATTN1C_CONTEXT_SIZE != sizeof(_stai_reattn1c_context),
                 STAI_ERROR_NETWORK_INVALID_CONTEXT_SIZE, net_ctx->_return_code)

  {
    const _stai_reattn1c_context _reattn1c_context = {
      ._magic = STAI_MAGIC,
      ._signature = STAI_REATTN1C_MODEL_SIGNATURE,
      ._flags = STAI_REATTN1C_FLAGS,
      ._return_code = STAI_SUCCESS,
      ._callback = NULL,
      ._callback_cookie = NULL,
      ._activations = {
      (stai_ptr)g_reattn1c_activations_1
      },
      ._weights = {
      (stai_ptr)g_reattn1c_weights_array
      },
      ._inputs = {
    NULL},
      ._outputs = {
    NULL},
    };

    // Deep copy of internal context to opaque buffer provided by app
    *net_ctx = _reattn1c_context;

    _stai_reattn1c_check(net_ctx);
  }

  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reattn1c_deinit(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /*  Reset flags to initial state  */
  net_ctx->_flags = STAI_REATTN1C_FLAGS;
  return net_ctx->_return_code;
}

/*****************************************************************************/



/* Int quant #0 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_proj_1_convolution0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.026346741244196892f),
    AI_PACK_INTQ_ZP(20)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_proj_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.026002241298556328f),
    AI_PACK_INTQ_ZP(18)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(const_fold_opt__266_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0010774622205644846f),
    AI_PACK_INTQ_ZP(8)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__890_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.026002241298556328f),
    AI_PACK_INTQ_ZP(18)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad0_1_strided_slice0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.026002241298556328f),
    AI_PACK_INTQ_ZP(18)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad0_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.0f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.0f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ccat0_1_concat0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.026002241298556328f),
    AI_PACK_INTQ_ZP(18)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil0_1_convolution0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.18365514278411865f),
    AI_PACK_INTQ_ZP(-7)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil0_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.18389278650283813f),
    AI_PACK_INTQ_ZP(-7)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(const_fold_opt__267_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0013363308971747756f),
    AI_PACK_INTQ_ZP(-11)))

/* Int quant #11 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.18280307948589325f),
    AI_PACK_INTQ_ZP(-4)))

/* Int quant #12 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(2.265094041824341f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #13 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.3046870231628418f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #14 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.3046909272670746f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #15 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(const_fold_opt__281_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(3.921568804798881e-06f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #16 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007880622521042824f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #17 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009198823012411594f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #18 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_Reshape0_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.004577533807605505f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #19 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__1910_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009198823012411594f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #20 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.004083497449755669f),
    AI_PACK_INTQ_ZP(23)))

/* Int quant #21 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0012888251803815365f),
    AI_PACK_INTQ_ZP(12)))

/* Int quant #22 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0021643226500600576f),
    AI_PACK_INTQ_ZP(29)))

/* Int quant #23 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_Reshape_10_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.000937646022066474f),
    AI_PACK_INTQ_ZP(58)))

/* Int quant #24 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__1040_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.18389278650283813f),
    AI_PACK_INTQ_ZP(-7)))

/* Int quant #25 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03830874711275101f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #26 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln0_1_add_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03867676854133606f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #27 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.9405356645584106f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #28 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu0_1_Gelu_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04205605015158653f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #29 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00017535293591208756f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #30 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu0_1_Gelu_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0807328149676323f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #31 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu0_1_Gelu_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06441546976566315f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #32 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003128959098830819f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #33 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007843135856091976f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #34 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu0_1_Gelu_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00784313678741455f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #35 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921568859368563f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #36 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu0_1_Gelu_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01933838427066803f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #37 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0019607844296842813f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #38 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu0_1_Gelu_mul_30_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02004055865108967f),
    AI_PACK_INTQ_ZP(-120)))

/* Int quant #39 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_add0_1_Add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.028742823749780655f),
    AI_PACK_INTQ_ZP(-35)))

/* Int quant #40 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad1_1_strided_slice0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.028742823749780655f),
    AI_PACK_INTQ_ZP(-35)))

/* Int quant #41 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad1_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.0f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #42 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ccat1_1_concat0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.028742823749780655f),
    AI_PACK_INTQ_ZP(-35)))

/* Int quant #43 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.028742823749780655f),
    AI_PACK_INTQ_ZP(-35)))

/* Int quant #44 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.16143950819969177f),
    AI_PACK_INTQ_ZP(-31)))

/* Int quant #45 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil1_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.16178633272647858f),
    AI_PACK_INTQ_ZP(-31)))

/* Int quant #46 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(const_fold_opt__275_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0005957507528364658f),
    AI_PACK_INTQ_ZP(-43)))

/* Int quant #47 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.16178633272647858f),
    AI_PACK_INTQ_ZP(-30)))

/* Int quant #48 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(2.519399642944336f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #49 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.17308975756168365f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #50 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.17309367656707764f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #51 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.005884362850338221f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #52 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007729546166956425f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #53 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_Reshape0_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.005151270888745785f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #54 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__1970_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007729546166956425f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #55 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0048790681175887585f),
    AI_PACK_INTQ_ZP(8)))

/* Int quant #56 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.002128375694155693f),
    AI_PACK_INTQ_ZP(14)))

/* Int quant #57 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0030063919257372618f),
    AI_PACK_INTQ_ZP(23)))

/* Int quant #58 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_Reshape_10_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.001305680489167571f),
    AI_PACK_INTQ_ZP(50)))

/* Int quant #59 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__1150_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.16178633272647858f),
    AI_PACK_INTQ_ZP(-31)))

/* Int quant #60 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03923540934920311f),
    AI_PACK_INTQ_ZP(-3)))

/* Int quant #61 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln1_1_add_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03905080631375313f),
    AI_PACK_INTQ_ZP(-4)))

/* Int quant #62 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.9702898859977722f),
    AI_PACK_INTQ_ZP(-11)))

/* Int quant #63 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu1_1_Gelu_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.043386511504650116f),
    AI_PACK_INTQ_ZP(-11)))

/* Int quant #64 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu1_1_Gelu_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08243732154369354f),
    AI_PACK_INTQ_ZP(-8)))

/* Int quant #65 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu1_1_Gelu_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06577546149492264f),
    AI_PACK_INTQ_ZP(-8)))

/* Int quant #66 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007843135856091976f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #67 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu1_1_Gelu_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007843137718737125f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #68 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu1_1_Gelu_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.019525403156876564f),
    AI_PACK_INTQ_ZP(-4)))

/* Int quant #69 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu1_1_Gelu_mul_30_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.020731069147586823f),
    AI_PACK_INTQ_ZP(-120)))

/* Int quant #70 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_add1_1_Add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04640093073248863f),
    AI_PACK_INTQ_ZP(-69)))

/* Int quant #71 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad2_1_strided_slice0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04640093073248863f),
    AI_PACK_INTQ_ZP(-69)))

/* Int quant #72 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad2_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.0f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #73 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ccat2_1_concat0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04640093073248863f),
    AI_PACK_INTQ_ZP(-69)))

/* Int quant #74 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04640093073248863f),
    AI_PACK_INTQ_ZP(-69)))

/* Int quant #75 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.19326896965503693f),
    AI_PACK_INTQ_ZP(13)))

/* Int quant #76 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil2_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.19345253705978394f),
    AI_PACK_INTQ_ZP(13)))

/* Int quant #77 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(const_fold_opt__280_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0004355458659119904f),
    AI_PACK_INTQ_ZP(-43)))

/* Int quant #78 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.19694237411022186f),
    AI_PACK_INTQ_ZP(12)))

/* Int quant #79 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(2.9768118858337402f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #80 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.2259257435798645f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #81 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.22592966258525848f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #82 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.004253217484802008f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #83 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.005824788939207792f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #84 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_Reshape0_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.005370595492422581f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #85 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__2030_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.005824788939207792f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #86 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007604237645864487f),
    AI_PACK_INTQ_ZP(-21)))

/* Int quant #87 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0023035635240375996f),
    AI_PACK_INTQ_ZP(29)))

/* Int quant #88 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0035796561278402805f),
    AI_PACK_INTQ_ZP(36)))

/* Int quant #89 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_Reshape_10_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.001605612807907164f),
    AI_PACK_INTQ_ZP(41)))

/* Int quant #90 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__1260_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.19345253705978394f),
    AI_PACK_INTQ_ZP(13)))

/* Int quant #91 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04388583451509476f),
    AI_PACK_INTQ_ZP(-6)))

/* Int quant #92 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln2_1_add_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04316610470414162f),
    AI_PACK_INTQ_ZP(-3)))

/* Int quant #93 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.3092845678329468f),
    AI_PACK_INTQ_ZP(-9)))

/* Int quant #94 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu2_1_Gelu_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05854466184973717f),
    AI_PACK_INTQ_ZP(-9)))

/* Int quant #95 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu2_1_Gelu_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10171076655387878f),
    AI_PACK_INTQ_ZP(-6)))

/* Int quant #96 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu2_1_Gelu_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08115345239639282f),
    AI_PACK_INTQ_ZP(-6)))

/* Int quant #97 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0078431386500597f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #98 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu2_1_Gelu_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007843137718737125f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #99 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu2_1_Gelu_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02158305235207081f),
    AI_PACK_INTQ_ZP(-3)))

/* Int quant #100 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu2_1_Gelu_mul_30_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.022707391530275345f),
    AI_PACK_INTQ_ZP(-121)))

/* Int quant #101 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_add2_1_Add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0630774274468422f),
    AI_PACK_INTQ_ZP(-84)))

/* Int quant #102 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad3_1_strided_slice0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0630774274468422f),
    AI_PACK_INTQ_ZP(-84)))

/* Int quant #103 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad3_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.0f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #104 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ccat3_1_concat0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0630774274468422f),
    AI_PACK_INTQ_ZP(-84)))

/* Int quant #105 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0630774274468422f),
    AI_PACK_INTQ_ZP(-84)))

/* Int quant #106 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.2866164743900299f),
    AI_PACK_INTQ_ZP(-11)))

/* Int quant #107 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil3_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.28711163997650146f),
    AI_PACK_INTQ_ZP(-11)))

/* Int quant #108 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(const_fold_opt__282_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0006219926872290671f),
    AI_PACK_INTQ_ZP(-20)))

/* Int quant #109 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.2859433889389038f),
    AI_PACK_INTQ_ZP(-10)))

/* Int quant #110 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(6.038126468658447f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #111 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.45151495933532715f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #112 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.45151886343955994f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #113 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0022355897817760706f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #114 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0028593987226486206f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #115 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_Reshape0_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0050158253870904446f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #116 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__2090_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0028593987226486206f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #117 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00488232122734189f),
    AI_PACK_INTQ_ZP(76)))

/* Int quant #118 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0016424665227532387f),
    AI_PACK_INTQ_ZP(99)))

/* Int quant #119 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0030619786120951176f),
    AI_PACK_INTQ_ZP(62)))

/* Int quant #120 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_Reshape_10_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0015928666107356548f),
    AI_PACK_INTQ_ZP(29)))

/* Int quant #121 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__1370_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.28711163997650146f),
    AI_PACK_INTQ_ZP(-11)))

/* Int quant #122 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04555603489279747f),
    AI_PACK_INTQ_ZP(-6)))

/* Int quant #123 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln3_1_add_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04476398229598999f),
    AI_PACK_INTQ_ZP(-5)))

/* Int quant #124 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.4625608921051025f),
    AI_PACK_INTQ_ZP(-13)))

/* Int quant #125 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu3_1_Gelu_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06539840996265411f),
    AI_PACK_INTQ_ZP(-13)))

/* Int quant #126 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu3_1_Gelu_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.1101623922586441f),
    AI_PACK_INTQ_ZP(-9)))

/* Int quant #127 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu3_1_Gelu_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08789687603712082f),
    AI_PACK_INTQ_ZP(-9)))

/* Int quant #128 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0078431386500597f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #129 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu3_1_Gelu_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007843137718737125f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #130 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu3_1_Gelu_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.022381991147994995f),
    AI_PACK_INTQ_ZP(-5)))

/* Int quant #131 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu3_1_Gelu_mul_30_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.023758362978696823f),
    AI_PACK_INTQ_ZP(-121)))

/* Int quant #132 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_add3_1_Add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06920965015888214f),
    AI_PACK_INTQ_ZP(-85)))

/* Int quant #133 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad4_1_strided_slice0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06920965015888214f),
    AI_PACK_INTQ_ZP(-85)))

/* Int quant #134 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad4_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.0f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #135 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ccat4_1_concat0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06920965015888214f),
    AI_PACK_INTQ_ZP(-85)))

/* Int quant #136 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06920965015888214f),
    AI_PACK_INTQ_ZP(-85)))

/* Int quant #137 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.2449152171611786f),
    AI_PACK_INTQ_ZP(2)))

/* Int quant #138 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil4_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.2451942414045334f),
    AI_PACK_INTQ_ZP(2)))

/* Int quant #139 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(const_fold_opt__254_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0004801938484888524f),
    AI_PACK_INTQ_ZP(-33)))

/* Int quant #140 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.24388989806175232f),
    AI_PACK_INTQ_ZP(3)))

/* Int quant #141 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(4.016427516937256f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #142 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.30485841631889343f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #143 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.3048623502254486f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #144 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0017484892159700394f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #145 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.002620737999677658f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #146 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_Reshape0_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.005877876654267311f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #147 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__2150_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.002620737999677658f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #148 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.009281291626393795f),
    AI_PACK_INTQ_ZP(33)))

/* Int quant #149 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0019361469894647598f),
    AI_PACK_INTQ_ZP(30)))

/* Int quant #150 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0033915387466549873f),
    AI_PACK_INTQ_ZP(30)))

/* Int quant #151 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_Reshape_10_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0017848231364041567f),
    AI_PACK_INTQ_ZP(35)))

/* Int quant #152 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__1480_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.2451942414045334f),
    AI_PACK_INTQ_ZP(2)))

/* Int quant #153 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04549819976091385f),
    AI_PACK_INTQ_ZP(5)))

/* Int quant #154 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln4_1_add_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04702066630125046f),
    AI_PACK_INTQ_ZP(6)))

/* Int quant #155 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.7044577598571777f),
    AI_PACK_INTQ_ZP(20)))

/* Int quant #156 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu4_1_Gelu_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.07621482759714127f),
    AI_PACK_INTQ_ZP(20)))

/* Int quant #157 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu4_1_Gelu_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.12323548644781113f),
    AI_PACK_INTQ_ZP(15)))

/* Int quant #158 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu4_1_Gelu_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09832769632339478f),
    AI_PACK_INTQ_ZP(15)))

/* Int quant #159 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0078431386500597f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #160 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu4_1_Gelu_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007843137718737125f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #161 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu4_1_Gelu_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02351033315062523f),
    AI_PACK_INTQ_ZP(6)))

/* Int quant #162 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu4_1_Gelu_mul_30_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.022921688854694366f),
    AI_PACK_INTQ_ZP(-121)))

/* Int quant #163 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_add4_1_Add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08496036380529404f),
    AI_PACK_INTQ_ZP(-91)))

/* Int quant #164 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad5_1_strided_slice0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08496036380529404f),
    AI_PACK_INTQ_ZP(-91)))

/* Int quant #165 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_zpad5_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.0f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #166 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ccat5_1_concat0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08496036380529404f),
    AI_PACK_INTQ_ZP(-91)))

/* Int quant #167 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08496036380529404f),
    AI_PACK_INTQ_ZP(-91)))

/* Int quant #168 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.2834649085998535f),
    AI_PACK_INTQ_ZP(-12)))

/* Int quant #169 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_dil5_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.284013032913208f),
    AI_PACK_INTQ_ZP(-12)))

/* Int quant #170 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(const_fold_opt__283_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0005880004027858377f),
    AI_PACK_INTQ_ZP(-38)))

/* Int quant #171 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.2814463675022125f),
    AI_PACK_INTQ_ZP(-9)))

/* Int quant #172 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(5.758757591247559f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #173 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.6730678677558899f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #174 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.6730718016624451f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #175 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0014387157279998064f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #176 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0023337649181485176f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #177 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_Reshape0_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.006361242383718491f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #178 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__2210_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0023337649181485176f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #179 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010783279314637184f),
    AI_PACK_INTQ_ZP(82)))

/* Int quant #180 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0028621854726225138f),
    AI_PACK_INTQ_ZP(80)))

/* Int quant #181 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003991216421127319f),
    AI_PACK_INTQ_ZP(51)))

/* Int quant #182 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_Reshape_10_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0018837854731827974f),
    AI_PACK_INTQ_ZP(23)))

/* Int quant #183 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(Transpose__1570_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.284013032913208f),
    AI_PACK_INTQ_ZP(-12)))

/* Int quant #184 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04383905977010727f),
    AI_PACK_INTQ_ZP(-24)))

/* Int quant #185 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_ln5_1_add_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04370545968413353f),
    AI_PACK_INTQ_ZP(-20)))

/* Int quant #186 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.4511957168579102f),
    AI_PACK_INTQ_ZP(-55)))

/* Int quant #187 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu5_1_Gelu_mul_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06489021331071854f),
    AI_PACK_INTQ_ZP(-55)))

/* Int quant #188 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu5_1_Gelu_add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10859566926956177f),
    AI_PACK_INTQ_ZP(-41)))

/* Int quant #189 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu5_1_Gelu_mul_20_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08664681017398834f),
    AI_PACK_INTQ_ZP(-41)))

/* Int quant #190 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007843135856091976f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #191 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu5_1_Gelu_add_10_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0078431386500597f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #192 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu5_1_Gelu_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.021852729842066765f),
    AI_PACK_INTQ_ZP(-20)))

/* Int quant #193 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_gelu5_1_Gelu_mul_30_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.025840818881988525f),
    AI_PACK_INTQ_ZP(-121)))

/* Int quant #194 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_add5_1_Add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10051041841506958f),
    AI_PACK_INTQ_ZP(-96)))

/* Int quant #195 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10051041841506958f),
    AI_PACK_INTQ_ZP(-96)))

/* Int quant #196 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.4802679419517517f),
    AI_PACK_INTQ_ZP(6)))

/* Int quant #197 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007381719537079334f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #198 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921568859368563f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #199 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_pool_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06137390807271004f),
    AI_PACK_INTQ_ZP(-99)))



/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_proj_1_convolution0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_proj_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  const_fold_opt__266_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__890_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad0_1_strided_slice0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 192, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad0_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 192, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ccat0_1_concat0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28992, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil0_1_convolution0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil0_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  const_fold_opt__267_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#12 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_mean0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#13 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_mean0_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#14 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_SquaredDifference0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#15 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_variance0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  const_fold_opt__281_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#22 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#23 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#24 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_Reshape0_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#25 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__1910_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#26 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#27 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#28 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#29 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_Reshape_10_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#30 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__1040_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#31 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#32 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#33 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#34 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_Pow0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#35 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#36 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#37 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#38 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#39 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#40 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#41 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#42 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#43 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#44 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#45 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#46 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#47 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul_30_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#48 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_add0_1_Add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#49 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad1_1_strided_slice0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 384, AI_STATIC)

/* Array#50 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad1_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 384, AI_STATIC)

/* Array#51 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ccat1_1_concat0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 29184, AI_STATIC)

/* Array#52 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 29184, AI_STATIC)

/* Array#53 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#54 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil1_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#55 */
AI_ARRAY_OBJ_DECLARE(
  const_fold_opt__275_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#56 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#57 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_mean0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#58 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_mean0_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#59 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_SquaredDifference0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#60 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#61 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#62 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#63 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_variance0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#64 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#65 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#66 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#67 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#68 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_Reshape0_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#69 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__1970_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#70 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#71 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#72 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#73 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_Reshape_10_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#74 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__1150_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#75 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#76 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#77 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#78 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_Pow0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#79 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#80 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#81 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#82 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#83 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#84 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#85 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#86 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul_30_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#87 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_add1_1_Add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#88 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad2_1_strided_slice0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 768, AI_STATIC)

/* Array#89 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad2_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 768, AI_STATIC)

/* Array#90 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ccat2_1_concat0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 29568, AI_STATIC)

/* Array#91 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 29568, AI_STATIC)

/* Array#92 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#93 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil2_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#94 */
AI_ARRAY_OBJ_DECLARE(
  const_fold_opt__280_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#95 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#96 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_mean0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#97 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_mean0_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#98 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_SquaredDifference0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#99 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#100 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#101 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#102 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_variance0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#103 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#104 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#105 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#106 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#107 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_Reshape0_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#108 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__2030_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#109 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#110 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#111 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#112 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_Reshape_10_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#113 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__1260_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#114 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#115 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#116 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#117 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_Pow0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#118 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#119 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#120 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#121 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#122 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#123 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#124 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#125 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_30_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#126 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_add2_1_Add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#127 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad3_1_strided_slice0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1536, AI_STATIC)

/* Array#128 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad3_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1536, AI_STATIC)

/* Array#129 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ccat3_1_concat0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 30336, AI_STATIC)

/* Array#130 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 30336, AI_STATIC)

/* Array#131 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#132 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil3_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#133 */
AI_ARRAY_OBJ_DECLARE(
  const_fold_opt__282_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#134 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#135 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_mean0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#136 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_mean0_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#137 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_SquaredDifference0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#138 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#139 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#140 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#141 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_variance0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#142 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#143 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#144 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#145 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#146 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_Reshape0_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#147 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__2090_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#148 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#149 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#150 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#151 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_Reshape_10_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#152 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__1370_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#153 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#154 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#155 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#156 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_Pow0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#157 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#158 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#159 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#160 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#161 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#162 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#163 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#164 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_30_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#165 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_add3_1_Add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#166 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad4_1_strided_slice0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3072, AI_STATIC)

/* Array#167 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad4_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3072, AI_STATIC)

/* Array#168 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ccat4_1_concat0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 31872, AI_STATIC)

/* Array#169 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 31872, AI_STATIC)

/* Array#170 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#171 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil4_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#172 */
AI_ARRAY_OBJ_DECLARE(
  const_fold_opt__254_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#173 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#174 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_mean0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#175 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_mean0_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#176 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_SquaredDifference0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#177 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#178 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#179 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#180 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_variance0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#181 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#182 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#183 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#184 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#185 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_Reshape0_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#186 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__2150_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#187 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#188 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#189 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#190 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_Reshape_10_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#191 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__1480_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#192 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#193 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#194 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#195 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Pow0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#196 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#197 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#198 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#199 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#200 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#201 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#202 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#203 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul_30_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#204 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_add4_1_Add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#205 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad5_1_strided_slice0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6144, AI_STATIC)

/* Array#206 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_zpad5_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6144, AI_STATIC)

/* Array#207 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ccat5_1_concat0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 34944, AI_STATIC)

/* Array#208 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 34944, AI_STATIC)

/* Array#209 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#210 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil5_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#211 */
AI_ARRAY_OBJ_DECLARE(
  const_fold_opt__283_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#212 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#213 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_mean0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#214 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_mean0_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#215 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_SquaredDifference0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#216 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#217 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#218 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#219 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_variance0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#220 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#221 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#222 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#223 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#224 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_Reshape0_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#225 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__2210_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#226 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#227 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#228 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#229 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_Reshape_10_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#230 */
AI_ARRAY_OBJ_DECLARE(
  Transpose__1570_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#231 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#232 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#233 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#234 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_Pow0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#235 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#236 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#237 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#238 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul_20_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#239 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#240 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_add_10_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#241 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#242 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul_30_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#243 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_add5_1_Add0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#244 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#245 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#246 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#247 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#248 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 384, AI_STATIC)

/* Array#249 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#250 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#251 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#252 */
AI_ARRAY_OBJ_DECLARE(
  past_pool_output_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 96, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  const_fold_opt__266_DequantizeLinear_Output_const, AI_STATIC,
  14, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &const_fold_opt__266_DequantizeLinear_Output_const_array, &const_fold_opt__266_DequantizeLinear_Output_const_array_intq)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_proj_1_BiasAdd0_output, AI_STATIC,
  312, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_proj_1_BiasAdd0_output_array, &reatt_n1_1_past_proj_1_BiasAdd0_output_array_intq)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_proj_1_convolution0_output0, AI_STATIC,
  315, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_proj_1_convolution0_output_array, &reatt_n1_1_past_proj_1_convolution0_output_array_intq)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__890_output, AI_STATIC,
  12, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__890_output_array, &Transpose__890_output_array_intq)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad0_1_strided_slice0_output, AI_STATIC,
  319, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 2), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_zpad0_1_strided_slice0_output_array, &reatt_n1_1_past_zpad0_1_strided_slice0_output_array_intq)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad0_1_mul0_output, AI_STATIC,
  318, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 2), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_zpad0_1_mul0_output_array, &reatt_n1_1_past_zpad0_1_mul0_output_array_intq)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D, AI_STATIC,
  327, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array, &reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ccat0_1_concat0_output, AI_STATIC,
  31, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 302), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ccat0_1_concat0_output_array, &reatt_n1_1_past_ccat0_1_concat0_output_array_intq)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  const_fold_opt__267_DequantizeLinear_Output_const, AI_STATIC,
  15, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &const_fold_opt__267_DequantizeLinear_Output_const_array, &const_fold_opt__267_DequantizeLinear_Output_const_array_intq)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil0_1_BiasAdd0_output, AI_STATIC,
  39, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil0_1_BiasAdd0_output_array, &reatt_n1_1_past_dil0_1_BiasAdd0_output_array_intq)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil0_1_convolution0_output0, AI_STATIC,
  41, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil0_1_convolution0_output_array, &reatt_n1_1_past_dil0_1_convolution0_output_array_intq)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_output, AI_STATIC,
  38, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_output_array, NULL)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_mean0_output, AI_STATIC,
  169, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln0_1_moments_mean0_output_array, NULL)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_SquaredDifference0_output, AI_STATIC,
  162, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_ln0_1_moments_SquaredDifference0_output_array, NULL)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_mean0_Mul_output, AI_STATIC,
  166, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln0_1_moments_mean0_Mul_output_array, NULL)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_output, AI_STATIC,
  161, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_output_array, &reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_output_array_intq)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_output, AI_STATIC,
  164, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_output_array, &reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_output_array_intq)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_output, AI_STATIC,
  163, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_output_array, NULL)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_variance0_output, AI_STATIC,
  172, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln0_1_moments_variance0_output_array, NULL)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  const_fold_opt__281_DequantizeLinear_Output_const, AI_STATIC,
  18, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &const_fold_opt__281_DequantizeLinear_Output_const_array, &const_fold_opt__281_DequantizeLinear_Output_const_array_intq)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add0_output, AI_STATIC,
  157, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln0_1_add0_output_array, &reatt_n1_1_past_ln0_1_add0_output_array_intq)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_output, AI_STATIC,
  170, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_output_array, &reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_output_array_intq)

/* Tensor #22 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_Reshape0_DequantizeLinear_Output_const, AI_STATIC,
  151, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln0_1_Reshape0_DequantizeLinear_Output_const_array, &reatt_n1_1_past_ln0_1_Reshape0_DequantizeLinear_Output_const_array_intq)

/* Tensor #23 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_output, AI_STATIC,
  154, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_output_array, &reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_output_array_intq)

/* Tensor #24 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_mul0_output, AI_STATIC,
  173, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln0_1_mul0_output_array, &reatt_n1_1_past_ln0_1_mul0_output_array_intq)

/* Tensor #25 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__1910_output, AI_STATIC,
  6, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__1910_output_array, &Transpose__1910_output_array_intq)

/* Tensor #26 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_output, AI_STATIC,
  149, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 1), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_output_array, &reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_output_array_intq)

/* Tensor #27 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_mul_10_output, AI_STATIC,
  174, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln0_1_mul_10_output_array, &reatt_n1_1_past_ln0_1_mul_10_output_array_intq)

/* Tensor #28 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_Reshape_10_DequantizeLinear_Output_const, AI_STATIC,
  152, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 96), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln0_1_Reshape_10_DequantizeLinear_Output_const_array, &reatt_n1_1_past_ln0_1_Reshape_10_DequantizeLinear_Output_const_array_intq)

/* Tensor #29 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add_10_output, AI_STATIC,
  158, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln0_1_add_10_output_array, &reatt_n1_1_past_ln0_1_add_10_output_array_intq)

/* Tensor #30 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__1040_output, AI_STATIC,
  0, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__1040_output_array, &Transpose__1040_output_array_intq)

/* Tensor #31 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_mul_20_output, AI_STATIC,
  175, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln0_1_mul_20_output_array, &reatt_n1_1_past_ln0_1_mul_20_output_array_intq)

/* Tensor #32 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add_20_output, AI_STATIC,
  160, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln0_1_add_20_output_array, &reatt_n1_1_past_ln0_1_add_20_output_array_intq)

/* Tensor #33 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_Pow0_output, AI_STATIC,
  91, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_gelu0_1_Gelu_Pow0_output_array, NULL)

/* Tensor #34 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D, AI_STATIC,
  132, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array, NULL)

/* Tensor #35 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_output, AI_STATIC,
  159, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_output_array, NULL)

/* Tensor #36 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_output, AI_STATIC,
  90, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_output_array, &reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_output_array_intq)

/* Tensor #37 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul_10_output, AI_STATIC,
  96, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu0_1_Gelu_mul_10_output_array, &reatt_n1_1_past_gelu0_1_Gelu_mul_10_output_array_intq)

/* Tensor #38 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D, AI_STATIC,
  129, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array, &reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #39 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_add0_output, AI_STATIC,
  93, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu0_1_Gelu_add0_output_array, &reatt_n1_1_past_gelu0_1_Gelu_add0_output_array_intq)

/* Tensor #40 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul_20_output, AI_STATIC,
  97, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu0_1_Gelu_mul_20_output_array, &reatt_n1_1_past_gelu0_1_Gelu_mul_20_output_array_intq)

/* Tensor #41 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D, AI_STATIC,
  127, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array, &reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #42 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output, AI_STATIC,
  92, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output_array, &reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output_array_intq)

/* Tensor #43 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_add_10_output, AI_STATIC,
  94, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu0_1_Gelu_add_10_output_array, &reatt_n1_1_past_gelu0_1_Gelu_add_10_output_array_intq)

/* Tensor #44 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D, AI_STATIC,
  104, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array, &reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #45 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul0_output, AI_STATIC,
  95, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu0_1_Gelu_mul0_output_array, &reatt_n1_1_past_gelu0_1_Gelu_mul0_output_array_intq)

/* Tensor #46 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D, AI_STATIC,
  118, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array, &reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #47 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul_30_output, AI_STATIC,
  98, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu0_1_Gelu_mul_30_output_array, &reatt_n1_1_past_gelu0_1_Gelu_mul_30_output_array_intq)

/* Tensor #48 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_add0_1_Add0_output, AI_STATIC,
  25, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_add0_1_Add0_output_array, &reatt_n1_1_past_add0_1_Add0_output_array_intq)

/* Tensor #49 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad1_1_strided_slice0_output, AI_STATIC,
  321, 0x1,
  AI_SHAPE_INIT(4, 1, 4, 1, 96), AI_STRIDE_INIT(4, 1, 1, 4, 4),
  1, &reatt_n1_1_past_zpad1_1_strided_slice0_output_array, &reatt_n1_1_past_zpad1_1_strided_slice0_output_array_intq)

/* Tensor #50 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad1_1_mul0_output, AI_STATIC,
  320, 0x1,
  AI_SHAPE_INIT(4, 1, 4, 1, 96), AI_STRIDE_INIT(4, 1, 1, 4, 4),
  1, &reatt_n1_1_past_zpad1_1_mul0_output_array, &reatt_n1_1_past_zpad1_1_mul0_output_array_intq)

/* Tensor #51 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ccat1_1_concat0_output, AI_STATIC,
  33, 0x1,
  AI_SHAPE_INIT(4, 1, 304, 1, 96), AI_STRIDE_INIT(4, 1, 1, 304, 304),
  1, &reatt_n1_1_past_ccat1_1_concat0_output_array, &reatt_n1_1_past_ccat1_1_concat0_output_array_intq)

/* Tensor #52 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_output, AI_STATIC,
  53, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 304), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_output_array, &reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_output_array_intq)

/* Tensor #53 */
AI_TENSOR_OBJ_DECLARE(
  const_fold_opt__275_DequantizeLinear_Output_const, AI_STATIC,
  16, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &const_fold_opt__275_DequantizeLinear_Output_const_array, &const_fold_opt__275_DequantizeLinear_Output_const_array_intq)

/* Tensor #54 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil1_1_BiasAdd0_output, AI_STATIC,
  45, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil1_1_BiasAdd0_output_array, &reatt_n1_1_past_dil1_1_BiasAdd0_output_array_intq)

/* Tensor #55 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_output0, AI_STATIC,
  47, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_output_array, &reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_output_array_intq)

/* Tensor #56 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_output, AI_STATIC,
  44, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_output_array, NULL)

/* Tensor #57 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_mean0_output, AI_STATIC,
  194, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln1_1_moments_mean0_output_array, NULL)

/* Tensor #58 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_SquaredDifference0_output, AI_STATIC,
  189, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_ln1_1_moments_SquaredDifference0_output_array, NULL)

/* Tensor #59 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_mean0_Mul_output, AI_STATIC,
  192, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln1_1_moments_mean0_Mul_output_array, NULL)

/* Tensor #60 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_output, AI_STATIC,
  188, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_output_array, &reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_output_array_intq)

/* Tensor #61 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_output, AI_STATIC,
  191, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_output_array, &reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_output_array_intq)

/* Tensor #62 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_output, AI_STATIC,
  190, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_output_array, NULL)

/* Tensor #63 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_variance0_output, AI_STATIC,
  197, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln1_1_moments_variance0_output_array, NULL)

/* Tensor #64 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add0_output, AI_STATIC,
  184, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln1_1_add0_output_array, &reatt_n1_1_past_ln1_1_add0_output_array_intq)

/* Tensor #65 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_output, AI_STATIC,
  195, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_output_array, &reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_output_array_intq)

/* Tensor #66 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_Reshape0_DequantizeLinear_Output_const, AI_STATIC,
  178, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln1_1_Reshape0_DequantizeLinear_Output_const_array, &reatt_n1_1_past_ln1_1_Reshape0_DequantizeLinear_Output_const_array_intq)

/* Tensor #67 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_output, AI_STATIC,
  181, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_output_array, &reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_output_array_intq)

/* Tensor #68 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_mul0_output, AI_STATIC,
  198, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln1_1_mul0_output_array, &reatt_n1_1_past_ln1_1_mul0_output_array_intq)

/* Tensor #69 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__1970_output, AI_STATIC,
  7, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__1970_output_array, &Transpose__1970_output_array_intq)

/* Tensor #70 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_output, AI_STATIC,
  176, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 1), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_output_array, &reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_output_array_intq)

/* Tensor #71 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_mul_10_output, AI_STATIC,
  199, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln1_1_mul_10_output_array, &reatt_n1_1_past_ln1_1_mul_10_output_array_intq)

/* Tensor #72 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_Reshape_10_DequantizeLinear_Output_const, AI_STATIC,
  179, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 96), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln1_1_Reshape_10_DequantizeLinear_Output_const_array, &reatt_n1_1_past_ln1_1_Reshape_10_DequantizeLinear_Output_const_array_intq)

/* Tensor #73 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add_10_output, AI_STATIC,
  185, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln1_1_add_10_output_array, &reatt_n1_1_past_ln1_1_add_10_output_array_intq)

/* Tensor #74 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__1150_output, AI_STATIC,
  1, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__1150_output_array, &Transpose__1150_output_array_intq)

/* Tensor #75 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_mul_20_output, AI_STATIC,
  200, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln1_1_mul_20_output_array, &reatt_n1_1_past_ln1_1_mul_20_output_array_intq)

/* Tensor #76 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add_20_output, AI_STATIC,
  187, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln1_1_add_20_output_array, &reatt_n1_1_past_ln1_1_add_20_output_array_intq)

/* Tensor #77 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_Pow0_output, AI_STATIC,
  100, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_gelu1_1_Gelu_Pow0_output_array, NULL)

/* Tensor #78 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_output, AI_STATIC,
  186, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_output_array, NULL)

/* Tensor #79 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_output, AI_STATIC,
  99, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_output_array, &reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_output_array_intq)

/* Tensor #80 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul_10_output, AI_STATIC,
  106, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu1_1_Gelu_mul_10_output_array, &reatt_n1_1_past_gelu1_1_Gelu_mul_10_output_array_intq)

/* Tensor #81 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_add0_output, AI_STATIC,
  102, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu1_1_Gelu_add0_output_array, &reatt_n1_1_past_gelu1_1_Gelu_add0_output_array_intq)

/* Tensor #82 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul_20_output, AI_STATIC,
  107, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu1_1_Gelu_mul_20_output_array, &reatt_n1_1_past_gelu1_1_Gelu_mul_20_output_array_intq)

/* Tensor #83 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output, AI_STATIC,
  101, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output_array, &reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output_array_intq)

/* Tensor #84 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_add_10_output, AI_STATIC,
  103, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu1_1_Gelu_add_10_output_array, &reatt_n1_1_past_gelu1_1_Gelu_add_10_output_array_intq)

/* Tensor #85 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul0_output, AI_STATIC,
  105, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu1_1_Gelu_mul0_output_array, &reatt_n1_1_past_gelu1_1_Gelu_mul0_output_array_intq)

/* Tensor #86 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul_30_output, AI_STATIC,
  108, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu1_1_Gelu_mul_30_output_array, &reatt_n1_1_past_gelu1_1_Gelu_mul_30_output_array_intq)

/* Tensor #87 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_add1_1_Add0_output, AI_STATIC,
  26, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_add1_1_Add0_output_array, &reatt_n1_1_past_add1_1_Add0_output_array_intq)

/* Tensor #88 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad2_1_strided_slice0_output, AI_STATIC,
  323, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 1, 96), AI_STRIDE_INIT(4, 1, 1, 8, 8),
  1, &reatt_n1_1_past_zpad2_1_strided_slice0_output_array, &reatt_n1_1_past_zpad2_1_strided_slice0_output_array_intq)

/* Tensor #89 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad2_1_mul0_output, AI_STATIC,
  322, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 1, 96), AI_STRIDE_INIT(4, 1, 1, 8, 8),
  1, &reatt_n1_1_past_zpad2_1_mul0_output_array, &reatt_n1_1_past_zpad2_1_mul0_output_array_intq)

/* Tensor #90 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ccat2_1_concat0_output, AI_STATIC,
  34, 0x1,
  AI_SHAPE_INIT(4, 1, 308, 1, 96), AI_STRIDE_INIT(4, 1, 1, 308, 308),
  1, &reatt_n1_1_past_ccat2_1_concat0_output_array, &reatt_n1_1_past_ccat2_1_concat0_output_array_intq)

/* Tensor #91 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_output, AI_STATIC,
  62, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 308), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_output_array, &reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_output_array_intq)

/* Tensor #92 */
AI_TENSOR_OBJ_DECLARE(
  const_fold_opt__280_DequantizeLinear_Output_const, AI_STATIC,
  17, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &const_fold_opt__280_DequantizeLinear_Output_const_array, &const_fold_opt__280_DequantizeLinear_Output_const_array_intq)

/* Tensor #93 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil2_1_BiasAdd0_output, AI_STATIC,
  55, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil2_1_BiasAdd0_output_array, &reatt_n1_1_past_dil2_1_BiasAdd0_output_array_intq)

/* Tensor #94 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_output0, AI_STATIC,
  57, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_output_array, &reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_output_array_intq)

/* Tensor #95 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_output, AI_STATIC,
  54, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_output_array, NULL)

/* Tensor #96 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_mean0_output, AI_STATIC,
  219, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln2_1_moments_mean0_output_array, NULL)

/* Tensor #97 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_SquaredDifference0_output, AI_STATIC,
  214, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_ln2_1_moments_SquaredDifference0_output_array, NULL)

/* Tensor #98 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_mean0_Mul_output, AI_STATIC,
  217, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln2_1_moments_mean0_Mul_output_array, NULL)

/* Tensor #99 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_output, AI_STATIC,
  213, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_output_array, &reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_output_array_intq)

/* Tensor #100 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_output, AI_STATIC,
  216, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_output_array, &reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_output_array_intq)

/* Tensor #101 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_output, AI_STATIC,
  215, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_output_array, NULL)

/* Tensor #102 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_variance0_output, AI_STATIC,
  222, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln2_1_moments_variance0_output_array, NULL)

/* Tensor #103 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add0_output, AI_STATIC,
  209, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln2_1_add0_output_array, &reatt_n1_1_past_ln2_1_add0_output_array_intq)

/* Tensor #104 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_output, AI_STATIC,
  220, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_output_array, &reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_output_array_intq)

/* Tensor #105 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_Reshape0_DequantizeLinear_Output_const, AI_STATIC,
  203, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln2_1_Reshape0_DequantizeLinear_Output_const_array, &reatt_n1_1_past_ln2_1_Reshape0_DequantizeLinear_Output_const_array_intq)

/* Tensor #106 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_output, AI_STATIC,
  206, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_output_array, &reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_output_array_intq)

/* Tensor #107 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_mul0_output, AI_STATIC,
  223, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln2_1_mul0_output_array, &reatt_n1_1_past_ln2_1_mul0_output_array_intq)

/* Tensor #108 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__2030_output, AI_STATIC,
  8, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__2030_output_array, &Transpose__2030_output_array_intq)

/* Tensor #109 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_output, AI_STATIC,
  201, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 1), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_output_array, &reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_output_array_intq)

/* Tensor #110 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_mul_10_output, AI_STATIC,
  224, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln2_1_mul_10_output_array, &reatt_n1_1_past_ln2_1_mul_10_output_array_intq)

/* Tensor #111 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_Reshape_10_DequantizeLinear_Output_const, AI_STATIC,
  204, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 96), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln2_1_Reshape_10_DequantizeLinear_Output_const_array, &reatt_n1_1_past_ln2_1_Reshape_10_DequantizeLinear_Output_const_array_intq)

/* Tensor #112 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add_10_output, AI_STATIC,
  210, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln2_1_add_10_output_array, &reatt_n1_1_past_ln2_1_add_10_output_array_intq)

/* Tensor #113 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__1260_output, AI_STATIC,
  2, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__1260_output_array, &Transpose__1260_output_array_intq)

/* Tensor #114 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_mul_20_output, AI_STATIC,
  225, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln2_1_mul_20_output_array, &reatt_n1_1_past_ln2_1_mul_20_output_array_intq)

/* Tensor #115 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add_20_output, AI_STATIC,
  212, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln2_1_add_20_output_array, &reatt_n1_1_past_ln2_1_add_20_output_array_intq)

/* Tensor #116 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_Pow0_output, AI_STATIC,
  110, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_gelu2_1_Gelu_Pow0_output_array, NULL)

/* Tensor #117 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_output, AI_STATIC,
  211, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_output_array, NULL)

/* Tensor #118 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_output, AI_STATIC,
  109, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_output_array, &reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_output_array_intq)

/* Tensor #119 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_10_output, AI_STATIC,
  115, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu2_1_Gelu_mul_10_output_array, &reatt_n1_1_past_gelu2_1_Gelu_mul_10_output_array_intq)

/* Tensor #120 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_add0_output, AI_STATIC,
  112, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu2_1_Gelu_add0_output_array, &reatt_n1_1_past_gelu2_1_Gelu_add0_output_array_intq)

/* Tensor #121 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_20_output, AI_STATIC,
  116, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu2_1_Gelu_mul_20_output_array, &reatt_n1_1_past_gelu2_1_Gelu_mul_20_output_array_intq)

/* Tensor #122 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output, AI_STATIC,
  111, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output_array, &reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output_array_intq)

/* Tensor #123 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_add_10_output, AI_STATIC,
  113, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu2_1_Gelu_add_10_output_array, &reatt_n1_1_past_gelu2_1_Gelu_add_10_output_array_intq)

/* Tensor #124 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul0_output, AI_STATIC,
  114, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu2_1_Gelu_mul0_output_array, &reatt_n1_1_past_gelu2_1_Gelu_mul0_output_array_intq)

/* Tensor #125 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_30_output, AI_STATIC,
  117, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu2_1_Gelu_mul_30_output_array, &reatt_n1_1_past_gelu2_1_Gelu_mul_30_output_array_intq)

/* Tensor #126 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_add2_1_Add0_output, AI_STATIC,
  27, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_add2_1_Add0_output_array, &reatt_n1_1_past_add2_1_Add0_output_array_intq)

/* Tensor #127 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad3_1_strided_slice0_output, AI_STATIC,
  325, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 1, 96), AI_STRIDE_INIT(4, 1, 1, 16, 16),
  1, &reatt_n1_1_past_zpad3_1_strided_slice0_output_array, &reatt_n1_1_past_zpad3_1_strided_slice0_output_array_intq)

/* Tensor #128 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad3_1_mul0_output, AI_STATIC,
  324, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 1, 96), AI_STRIDE_INIT(4, 1, 1, 16, 16),
  1, &reatt_n1_1_past_zpad3_1_mul0_output_array, &reatt_n1_1_past_zpad3_1_mul0_output_array_intq)

/* Tensor #129 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ccat3_1_concat0_output, AI_STATIC,
  35, 0x1,
  AI_SHAPE_INIT(4, 1, 316, 1, 96), AI_STRIDE_INIT(4, 1, 1, 316, 316),
  1, &reatt_n1_1_past_ccat3_1_concat0_output_array, &reatt_n1_1_past_ccat3_1_concat0_output_array_intq)

/* Tensor #130 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_output, AI_STATIC,
  71, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 316), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_output_array, &reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_output_array_intq)

/* Tensor #131 */
AI_TENSOR_OBJ_DECLARE(
  const_fold_opt__282_DequantizeLinear_Output_const, AI_STATIC,
  19, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &const_fold_opt__282_DequantizeLinear_Output_const_array, &const_fold_opt__282_DequantizeLinear_Output_const_array_intq)

/* Tensor #132 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil3_1_BiasAdd0_output, AI_STATIC,
  64, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil3_1_BiasAdd0_output_array, &reatt_n1_1_past_dil3_1_BiasAdd0_output_array_intq)

/* Tensor #133 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_output0, AI_STATIC,
  66, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_output_array, &reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_output_array_intq)

/* Tensor #134 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_output, AI_STATIC,
  63, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_output_array, NULL)

/* Tensor #135 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_mean0_output, AI_STATIC,
  244, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln3_1_moments_mean0_output_array, NULL)

/* Tensor #136 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_SquaredDifference0_output, AI_STATIC,
  239, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_ln3_1_moments_SquaredDifference0_output_array, NULL)

/* Tensor #137 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_mean0_Mul_output, AI_STATIC,
  242, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln3_1_moments_mean0_Mul_output_array, NULL)

/* Tensor #138 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_output, AI_STATIC,
  238, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_output_array, &reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_output_array_intq)

/* Tensor #139 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_output, AI_STATIC,
  241, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_output_array, &reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_output_array_intq)

/* Tensor #140 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_output, AI_STATIC,
  240, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_output_array, NULL)

/* Tensor #141 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_variance0_output, AI_STATIC,
  247, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln3_1_moments_variance0_output_array, NULL)

/* Tensor #142 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add0_output, AI_STATIC,
  234, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln3_1_add0_output_array, &reatt_n1_1_past_ln3_1_add0_output_array_intq)

/* Tensor #143 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_output, AI_STATIC,
  245, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_output_array, &reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_output_array_intq)

/* Tensor #144 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_Reshape0_DequantizeLinear_Output_const, AI_STATIC,
  228, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln3_1_Reshape0_DequantizeLinear_Output_const_array, &reatt_n1_1_past_ln3_1_Reshape0_DequantizeLinear_Output_const_array_intq)

/* Tensor #145 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_output, AI_STATIC,
  231, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_output_array, &reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_output_array_intq)

/* Tensor #146 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_mul0_output, AI_STATIC,
  248, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln3_1_mul0_output_array, &reatt_n1_1_past_ln3_1_mul0_output_array_intq)

/* Tensor #147 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__2090_output, AI_STATIC,
  9, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__2090_output_array, &Transpose__2090_output_array_intq)

/* Tensor #148 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_output, AI_STATIC,
  226, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 1), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_output_array, &reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_output_array_intq)

/* Tensor #149 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_mul_10_output, AI_STATIC,
  249, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln3_1_mul_10_output_array, &reatt_n1_1_past_ln3_1_mul_10_output_array_intq)

/* Tensor #150 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_Reshape_10_DequantizeLinear_Output_const, AI_STATIC,
  229, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 96), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln3_1_Reshape_10_DequantizeLinear_Output_const_array, &reatt_n1_1_past_ln3_1_Reshape_10_DequantizeLinear_Output_const_array_intq)

/* Tensor #151 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add_10_output, AI_STATIC,
  235, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln3_1_add_10_output_array, &reatt_n1_1_past_ln3_1_add_10_output_array_intq)

/* Tensor #152 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__1370_output, AI_STATIC,
  3, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__1370_output_array, &Transpose__1370_output_array_intq)

/* Tensor #153 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_mul_20_output, AI_STATIC,
  250, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln3_1_mul_20_output_array, &reatt_n1_1_past_ln3_1_mul_20_output_array_intq)

/* Tensor #154 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add_20_output, AI_STATIC,
  237, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln3_1_add_20_output_array, &reatt_n1_1_past_ln3_1_add_20_output_array_intq)

/* Tensor #155 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_Pow0_output, AI_STATIC,
  120, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_gelu3_1_Gelu_Pow0_output_array, NULL)

/* Tensor #156 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_output, AI_STATIC,
  236, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_output_array, NULL)

/* Tensor #157 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_output, AI_STATIC,
  119, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_output_array, &reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_output_array_intq)

/* Tensor #158 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_10_output, AI_STATIC,
  125, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu3_1_Gelu_mul_10_output_array, &reatt_n1_1_past_gelu3_1_Gelu_mul_10_output_array_intq)

/* Tensor #159 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_add0_output, AI_STATIC,
  122, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu3_1_Gelu_add0_output_array, &reatt_n1_1_past_gelu3_1_Gelu_add0_output_array_intq)

/* Tensor #160 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_20_output, AI_STATIC,
  126, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu3_1_Gelu_mul_20_output_array, &reatt_n1_1_past_gelu3_1_Gelu_mul_20_output_array_intq)

/* Tensor #161 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output, AI_STATIC,
  121, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output_array, &reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output_array_intq)

/* Tensor #162 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_add_10_output, AI_STATIC,
  123, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu3_1_Gelu_add_10_output_array, &reatt_n1_1_past_gelu3_1_Gelu_add_10_output_array_intq)

/* Tensor #163 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul0_output, AI_STATIC,
  124, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu3_1_Gelu_mul0_output_array, &reatt_n1_1_past_gelu3_1_Gelu_mul0_output_array_intq)

/* Tensor #164 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_30_output, AI_STATIC,
  128, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu3_1_Gelu_mul_30_output_array, &reatt_n1_1_past_gelu3_1_Gelu_mul_30_output_array_intq)

/* Tensor #165 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_add3_1_Add0_output, AI_STATIC,
  28, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_add3_1_Add0_output_array, &reatt_n1_1_past_add3_1_Add0_output_array_intq)

/* Tensor #166 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad4_1_strided_slice0_output, AI_STATIC,
  328, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 1, 96), AI_STRIDE_INIT(4, 1, 1, 32, 32),
  1, &reatt_n1_1_past_zpad4_1_strided_slice0_output_array, &reatt_n1_1_past_zpad4_1_strided_slice0_output_array_intq)

/* Tensor #167 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad4_1_mul0_output, AI_STATIC,
  326, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 1, 96), AI_STRIDE_INIT(4, 1, 1, 32, 32),
  1, &reatt_n1_1_past_zpad4_1_mul0_output_array, &reatt_n1_1_past_zpad4_1_mul0_output_array_intq)

/* Tensor #168 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ccat4_1_concat0_output, AI_STATIC,
  36, 0x1,
  AI_SHAPE_INIT(4, 1, 332, 1, 96), AI_STRIDE_INIT(4, 1, 1, 332, 332),
  1, &reatt_n1_1_past_ccat4_1_concat0_output_array, &reatt_n1_1_past_ccat4_1_concat0_output_array_intq)

/* Tensor #169 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_output, AI_STATIC,
  80, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 332), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_output_array, &reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_output_array_intq)

/* Tensor #170 */
AI_TENSOR_OBJ_DECLARE(
  const_fold_opt__254_DequantizeLinear_Output_const, AI_STATIC,
  13, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &const_fold_opt__254_DequantizeLinear_Output_const_array, &const_fold_opt__254_DequantizeLinear_Output_const_array_intq)

/* Tensor #171 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil4_1_BiasAdd0_output, AI_STATIC,
  73, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil4_1_BiasAdd0_output_array, &reatt_n1_1_past_dil4_1_BiasAdd0_output_array_intq)

/* Tensor #172 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_output0, AI_STATIC,
  75, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_output_array, &reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_output_array_intq)

/* Tensor #173 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_output, AI_STATIC,
  72, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_output_array, NULL)

/* Tensor #174 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_mean0_output, AI_STATIC,
  269, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln4_1_moments_mean0_output_array, NULL)

/* Tensor #175 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_SquaredDifference0_output, AI_STATIC,
  264, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_ln4_1_moments_SquaredDifference0_output_array, NULL)

/* Tensor #176 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_mean0_Mul_output, AI_STATIC,
  267, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln4_1_moments_mean0_Mul_output_array, NULL)

/* Tensor #177 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_output, AI_STATIC,
  263, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_output_array, &reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_output_array_intq)

/* Tensor #178 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_output, AI_STATIC,
  266, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_output_array, &reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_output_array_intq)

/* Tensor #179 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_output, AI_STATIC,
  265, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_output_array, NULL)

/* Tensor #180 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_variance0_output, AI_STATIC,
  272, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln4_1_moments_variance0_output_array, NULL)

/* Tensor #181 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add0_output, AI_STATIC,
  259, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln4_1_add0_output_array, &reatt_n1_1_past_ln4_1_add0_output_array_intq)

/* Tensor #182 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_output, AI_STATIC,
  270, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_output_array, &reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_output_array_intq)

/* Tensor #183 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_Reshape0_DequantizeLinear_Output_const, AI_STATIC,
  253, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln4_1_Reshape0_DequantizeLinear_Output_const_array, &reatt_n1_1_past_ln4_1_Reshape0_DequantizeLinear_Output_const_array_intq)

/* Tensor #184 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_output, AI_STATIC,
  256, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_output_array, &reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_output_array_intq)

/* Tensor #185 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_mul0_output, AI_STATIC,
  273, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln4_1_mul0_output_array, &reatt_n1_1_past_ln4_1_mul0_output_array_intq)

/* Tensor #186 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__2150_output, AI_STATIC,
  10, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__2150_output_array, &Transpose__2150_output_array_intq)

/* Tensor #187 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_output, AI_STATIC,
  251, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 1), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_output_array, &reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_output_array_intq)

/* Tensor #188 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_mul_10_output, AI_STATIC,
  274, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln4_1_mul_10_output_array, &reatt_n1_1_past_ln4_1_mul_10_output_array_intq)

/* Tensor #189 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_Reshape_10_DequantizeLinear_Output_const, AI_STATIC,
  254, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 96), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln4_1_Reshape_10_DequantizeLinear_Output_const_array, &reatt_n1_1_past_ln4_1_Reshape_10_DequantizeLinear_Output_const_array_intq)

/* Tensor #190 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add_10_output, AI_STATIC,
  260, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln4_1_add_10_output_array, &reatt_n1_1_past_ln4_1_add_10_output_array_intq)

/* Tensor #191 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__1480_output, AI_STATIC,
  4, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__1480_output_array, &Transpose__1480_output_array_intq)

/* Tensor #192 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_mul_20_output, AI_STATIC,
  275, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln4_1_mul_20_output_array, &reatt_n1_1_past_ln4_1_mul_20_output_array_intq)

/* Tensor #193 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add_20_output, AI_STATIC,
  262, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln4_1_add_20_output_array, &reatt_n1_1_past_ln4_1_add_20_output_array_intq)

/* Tensor #194 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Pow0_output, AI_STATIC,
  131, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_gelu4_1_Gelu_Pow0_output_array, NULL)

/* Tensor #195 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_output, AI_STATIC,
  261, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_output_array, NULL)

/* Tensor #196 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_output, AI_STATIC,
  130, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_output_array, &reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_output_array_intq)

/* Tensor #197 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul_10_output, AI_STATIC,
  137, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu4_1_Gelu_mul_10_output_array, &reatt_n1_1_past_gelu4_1_Gelu_mul_10_output_array_intq)

/* Tensor #198 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_add0_output, AI_STATIC,
  134, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu4_1_Gelu_add0_output_array, &reatt_n1_1_past_gelu4_1_Gelu_add0_output_array_intq)

/* Tensor #199 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul_20_output, AI_STATIC,
  138, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu4_1_Gelu_mul_20_output_array, &reatt_n1_1_past_gelu4_1_Gelu_mul_20_output_array_intq)

/* Tensor #200 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output, AI_STATIC,
  133, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output_array, &reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output_array_intq)

/* Tensor #201 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_add_10_output, AI_STATIC,
  135, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu4_1_Gelu_add_10_output_array, &reatt_n1_1_past_gelu4_1_Gelu_add_10_output_array_intq)

/* Tensor #202 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul0_output, AI_STATIC,
  136, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu4_1_Gelu_mul0_output_array, &reatt_n1_1_past_gelu4_1_Gelu_mul0_output_array_intq)

/* Tensor #203 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul_30_output, AI_STATIC,
  139, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu4_1_Gelu_mul_30_output_array, &reatt_n1_1_past_gelu4_1_Gelu_mul_30_output_array_intq)

/* Tensor #204 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_add4_1_Add0_output, AI_STATIC,
  29, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_add4_1_Add0_output_array, &reatt_n1_1_past_add4_1_Add0_output_array_intq)

/* Tensor #205 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad5_1_strided_slice0_output, AI_STATIC,
  330, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 96), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &reatt_n1_1_past_zpad5_1_strided_slice0_output_array, &reatt_n1_1_past_zpad5_1_strided_slice0_output_array_intq)

/* Tensor #206 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_zpad5_1_mul0_output, AI_STATIC,
  329, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 96), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &reatt_n1_1_past_zpad5_1_mul0_output_array, &reatt_n1_1_past_zpad5_1_mul0_output_array_intq)

/* Tensor #207 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ccat5_1_concat0_output, AI_STATIC,
  37, 0x1,
  AI_SHAPE_INIT(4, 1, 364, 1, 96), AI_STRIDE_INIT(4, 1, 1, 364, 364),
  1, &reatt_n1_1_past_ccat5_1_concat0_output_array, &reatt_n1_1_past_ccat5_1_concat0_output_array_intq)

/* Tensor #208 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_output, AI_STATIC,
  89, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 364), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_output_array, &reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_output_array_intq)

/* Tensor #209 */
AI_TENSOR_OBJ_DECLARE(
  const_fold_opt__283_DequantizeLinear_Output_const, AI_STATIC,
  20, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &const_fold_opt__283_DequantizeLinear_Output_const_array, &const_fold_opt__283_DequantizeLinear_Output_const_array_intq)

/* Tensor #210 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil5_1_BiasAdd0_output, AI_STATIC,
  82, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil5_1_BiasAdd0_output_array, &reatt_n1_1_past_dil5_1_BiasAdd0_output_array_intq)

/* Tensor #211 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_output0, AI_STATIC,
  84, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_output_array, &reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_output_array_intq)

/* Tensor #212 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_output, AI_STATIC,
  81, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_output_array, NULL)

/* Tensor #213 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_mean0_output, AI_STATIC,
  294, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln5_1_moments_mean0_output_array, NULL)

/* Tensor #214 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_SquaredDifference0_output, AI_STATIC,
  289, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_ln5_1_moments_SquaredDifference0_output_array, NULL)

/* Tensor #215 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_mean0_Mul_output, AI_STATIC,
  292, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln5_1_moments_mean0_Mul_output_array, NULL)

/* Tensor #216 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_output, AI_STATIC,
  288, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_output_array, &reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_output_array_intq)

/* Tensor #217 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_output, AI_STATIC,
  291, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_output_array, &reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_output_array_intq)

/* Tensor #218 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_output, AI_STATIC,
  290, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_output_array, NULL)

/* Tensor #219 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_variance0_output, AI_STATIC,
  297, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_ln5_1_moments_variance0_output_array, NULL)

/* Tensor #220 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add0_output, AI_STATIC,
  284, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln5_1_add0_output_array, &reatt_n1_1_past_ln5_1_add0_output_array_intq)

/* Tensor #221 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_output, AI_STATIC,
  295, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_output_array, &reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_output_array_intq)

/* Tensor #222 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_Reshape0_DequantizeLinear_Output_const, AI_STATIC,
  278, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln5_1_Reshape0_DequantizeLinear_Output_const_array, &reatt_n1_1_past_ln5_1_Reshape0_DequantizeLinear_Output_const_array_intq)

/* Tensor #223 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_output, AI_STATIC,
  281, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_output_array, &reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_output_array_intq)

/* Tensor #224 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_mul0_output, AI_STATIC,
  298, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_ln5_1_mul0_output_array, &reatt_n1_1_past_ln5_1_mul0_output_array_intq)

/* Tensor #225 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__2210_output, AI_STATIC,
  11, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__2210_output_array, &Transpose__2210_output_array_intq)

/* Tensor #226 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_output, AI_STATIC,
  276, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 1), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_output_array, &reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_output_array_intq)

/* Tensor #227 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_mul_10_output, AI_STATIC,
  299, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln5_1_mul_10_output_array, &reatt_n1_1_past_ln5_1_mul_10_output_array_intq)

/* Tensor #228 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_Reshape_10_DequantizeLinear_Output_const, AI_STATIC,
  279, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 96), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_ln5_1_Reshape_10_DequantizeLinear_Output_const_array, &reatt_n1_1_past_ln5_1_Reshape_10_DequantizeLinear_Output_const_array_intq)

/* Tensor #229 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add_10_output, AI_STATIC,
  285, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln5_1_add_10_output_array, &reatt_n1_1_past_ln5_1_add_10_output_array_intq)

/* Tensor #230 */
AI_TENSOR_OBJ_DECLARE(
  Transpose__1570_output, AI_STATIC,
  5, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &Transpose__1570_output_array, &Transpose__1570_output_array_intq)

/* Tensor #231 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_mul_20_output, AI_STATIC,
  300, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln5_1_mul_20_output_array, &reatt_n1_1_past_ln5_1_mul_20_output_array_intq)

/* Tensor #232 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add_20_output, AI_STATIC,
  287, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_ln5_1_add_20_output_array, &reatt_n1_1_past_ln5_1_add_20_output_array_intq)

/* Tensor #233 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_Pow0_output, AI_STATIC,
  141, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_gelu5_1_Gelu_Pow0_output_array, NULL)

/* Tensor #234 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_output, AI_STATIC,
  286, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_output_array, NULL)

/* Tensor #235 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_output, AI_STATIC,
  140, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_output_array, &reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_output_array_intq)

/* Tensor #236 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul_10_output, AI_STATIC,
  146, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu5_1_Gelu_mul_10_output_array, &reatt_n1_1_past_gelu5_1_Gelu_mul_10_output_array_intq)

/* Tensor #237 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_add0_output, AI_STATIC,
  143, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu5_1_Gelu_add0_output_array, &reatt_n1_1_past_gelu5_1_Gelu_add0_output_array_intq)

/* Tensor #238 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul_20_output, AI_STATIC,
  147, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu5_1_Gelu_mul_20_output_array, &reatt_n1_1_past_gelu5_1_Gelu_mul_20_output_array_intq)

/* Tensor #239 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output, AI_STATIC,
  142, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output_array, &reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output_array_intq)

/* Tensor #240 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_add_10_output, AI_STATIC,
  144, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu5_1_Gelu_add_10_output_array, &reatt_n1_1_past_gelu5_1_Gelu_add_10_output_array_intq)

/* Tensor #241 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul0_output, AI_STATIC,
  145, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu5_1_Gelu_mul0_output_array, &reatt_n1_1_past_gelu5_1_Gelu_mul0_output_array_intq)

/* Tensor #242 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul_30_output, AI_STATIC,
  148, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_gelu5_1_Gelu_mul_30_output_array, &reatt_n1_1_past_gelu5_1_Gelu_mul_30_output_array_intq)

/* Tensor #243 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_add5_1_Add0_output, AI_STATIC,
  30, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_add5_1_Add0_output_array, &reatt_n1_1_past_add5_1_Add0_output_array_intq)

/* Tensor #244 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output, AI_STATIC,
  306, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array_intq)

/* Tensor #245 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_bias, AI_STATIC,
  305, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_bias_array, NULL)

/* Tensor #246 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_output, AI_STATIC,
  307, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_output_array, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_output_array_intq)

/* Tensor #247 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_scratch0, AI_STATIC,
  308, 0x0,
  AI_SHAPE_INIT(4, 1, 384, 1, 1), AI_STRIDE_INIT(4, 1, 1, 384, 384),
  1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_scratch0_array, NULL)

/* Tensor #248 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_weights, AI_STATIC,
  309, 0x1,
  AI_SHAPE_INIT(4, 96, 1, 1, 1), AI_STRIDE_INIT(4, 1, 96, 96, 96),
  1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_weights_array, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_weights_array_intq)

/* Tensor #249 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output, AI_STATIC,
  301, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 1), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output_array, &reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output_array_intq)

/* Tensor #250 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_mul0_output, AI_STATIC,
  311, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &reatt_n1_1_past_pool_1_mul0_output_array, &reatt_n1_1_past_pool_1_mul0_output_array_intq)

/* Tensor #251 */
AI_TENSOR_OBJ_DECLARE(
  past_pool_output, AI_STATIC,
  24, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 96), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &past_pool_output_array, NULL)

/* Tensor #252 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output, AI_STATIC,
  310, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 96), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output_array, NULL)


AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_proj_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_proj_1_convolution0_output0, &const_fold_opt__266_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_proj_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_proj_1_BiasAdd0_layer, 44,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_proj_1_BiasAdd0_chain,
  NULL, &reatt_n1_1_past_proj_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__890_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_proj_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__890_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__890_layer, 48,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__890_chain,
  NULL, &Transpose__890_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 reatt_n1_1_past_zpad0_1_strided_slice0_axes_data[] = { 0, 2 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad0_1_strided_slice0_axes, AI_ARRAY_FORMAT_U8,
    reatt_n1_1_past_zpad0_1_strided_slice0_axes_data, reatt_n1_1_past_zpad0_1_strided_slice0_axes_data, 2, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 reatt_n1_1_past_zpad0_1_strided_slice0_starts_data[] = { 0, 0 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad0_1_strided_slice0_starts, AI_ARRAY_FORMAT_S16,
    reatt_n1_1_past_zpad0_1_strided_slice0_starts_data, reatt_n1_1_past_zpad0_1_strided_slice0_starts_data, 2, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 reatt_n1_1_past_zpad0_1_strided_slice0_ends_data[] = { 2, 96 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad0_1_strided_slice0_ends, AI_ARRAY_FORMAT_S16,
    reatt_n1_1_past_zpad0_1_strided_slice0_ends_data, reatt_n1_1_past_zpad0_1_strided_slice0_ends_data, 2, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_zpad0_1_strided_slice0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_proj_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_zpad0_1_strided_slice0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_zpad0_1_strided_slice0_layer, 47,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &reatt_n1_1_past_zpad0_1_strided_slice0_chain,
  NULL, &reatt_n1_1_past_zpad0_1_strided_slice0_layer, AI_STATIC, 
  .axes = &reatt_n1_1_past_zpad0_1_strided_slice0_axes, 
  .starts = &reatt_n1_1_past_zpad0_1_strided_slice0_starts, 
  .ends = &reatt_n1_1_past_zpad0_1_strided_slice0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_zpad0_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_zpad0_1_strided_slice0_output, &reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_zpad0_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_zpad0_1_mul0_layer, 53,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_zpad0_1_mul0_chain,
  NULL, &reatt_n1_1_past_zpad0_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ccat0_1_concat0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_zpad0_1_mul0_output, &reatt_n1_1_past_proj_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ccat0_1_concat0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ccat0_1_concat0_layer, 56,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &reatt_n1_1_past_ccat0_1_concat0_chain,
  NULL, &reatt_n1_1_past_ccat0_1_concat0_layer, AI_STATIC, 
  .axis = AI_SHAPE_HEIGHT, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_dil0_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_dil0_1_convolution0_output0, &const_fold_opt__267_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil0_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_dil0_1_BiasAdd0_layer, 68,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_dil0_1_BiasAdd0_chain,
  NULL, &reatt_n1_1_past_dil0_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_float reatt_n1_1_past_ln0_1_moments_mean0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_ln0_1_moments_mean0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reatt_n1_1_past_ln0_1_moments_mean0_neutral_value_data, reatt_n1_1_past_ln0_1_moments_mean0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_mean0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln0_1_moments_mean0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_mean0_layer, 71,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reatt_n1_1_past_ln0_1_moments_mean0_chain,
  NULL, &reatt_n1_1_past_ln0_1_moments_mean0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reatt_n1_1_past_ln0_1_moments_mean0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_SquaredDifference0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_output, &reatt_n1_1_past_ln0_1_moments_mean0_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln0_1_moments_SquaredDifference0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_SquaredDifference0_layer, 79,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n1_1_past_ln0_1_moments_SquaredDifference0_chain,
  NULL, &reatt_n1_1_past_ln0_1_moments_SquaredDifference0_layer, AI_STATIC, 
  .operation = ai_sub_f32, 
  .buffer_operation = ai_sub_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_output, &reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_layer, 83,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_chain,
  NULL, &reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_float reatt_n1_1_past_ln0_1_moments_variance0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_ln0_1_moments_variance0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reatt_n1_1_past_ln0_1_moments_variance0_neutral_value_data, reatt_n1_1_past_ln0_1_moments_variance0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_variance0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln0_1_moments_variance0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_moments_variance0_layer, 86,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reatt_n1_1_past_ln0_1_moments_variance0_chain,
  NULL, &reatt_n1_1_past_ln0_1_moments_variance0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reatt_n1_1_past_ln0_1_moments_variance0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_output, &const_fold_opt__281_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln0_1_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add0_layer, 89,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln0_1_add0_chain,
  NULL, &reatt_n1_1_past_ln0_1_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_output, &reatt_n1_1_past_ln0_1_Reshape0_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln0_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_mul0_layer, 96,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln0_1_mul0_chain,
  NULL, &reatt_n1_1_past_ln0_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__1910_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln0_1_mul0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__1910_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__1910_layer, 99,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__1910_chain,
  NULL, &Transpose__1910_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_output, &Transpose__1910_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln0_1_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_mul_10_layer, 103,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln0_1_mul_10_chain,
  NULL, &reatt_n1_1_past_ln0_1_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln0_1_mul_10_output, &reatt_n1_1_past_ln0_1_Reshape_10_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln0_1_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add_10_layer, 108,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln0_1_add_10_chain,
  NULL, &reatt_n1_1_past_ln0_1_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__1040_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil0_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__1040_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__1040_layer, 72,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__1040_chain,
  NULL, &Transpose__1040_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &Transpose__1040_output, &Transpose__1910_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln0_1_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_mul_20_layer, 102,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln0_1_mul_20_chain,
  NULL, &reatt_n1_1_past_ln0_1_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln0_1_mul_20_output, &reatt_n1_1_past_ln0_1_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln0_1_add_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln0_1_add_20_layer, 111,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln0_1_add_20_chain,
  NULL, &reatt_n1_1_past_ln0_1_add_20_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_Pow0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_output, &reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu0_1_Gelu_Pow0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_Pow0_layer, 115,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n1_1_past_gelu0_1_Gelu_Pow0_chain,
  NULL, &reatt_n1_1_past_gelu0_1_Gelu_Pow0_layer, AI_STATIC, 
  .operation = ai_pow, 
  .buffer_operation = ai_pow_buffer, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu0_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul_10_layer, 120,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu0_1_Gelu_mul_10_chain,
  NULL, &reatt_n1_1_past_gelu0_1_Gelu_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln0_1_add_20_output, &reatt_n1_1_past_gelu0_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu0_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_add0_layer, 123,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu0_1_Gelu_add0_chain,
  NULL, &reatt_n1_1_past_gelu0_1_Gelu_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu0_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu0_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul_20_layer, 126,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu0_1_Gelu_mul_20_chain,
  NULL, &reatt_n1_1_past_gelu0_1_Gelu_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 reatt_n1_1_past_gelu0_1_Gelu_Tanh0_nl_params_data[] = { -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -125, -125, -125, -124, -124, -123, -123, -122, -122, -121, -120, -119, -118, -116, -115, -113, -112, -109, -107, -105, -102, -99, -95, -91, -87, -83, -78, -72, -67, -60, -54, -47, -40, -32, -24, -16, -8, 0, 8, 16, 24, 32, 40, 47, 54, 60, 67, 72, 78, 83, 87, 91, 95, 99, 102, 105, 107, 109, 112, 113, 115, 116, 118, 119, 120, 121, 122, 122, 123, 123, 124, 124, 125, 125, 125, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_gelu0_1_Gelu_Tanh0_nl_params, AI_ARRAY_FORMAT_S8,
    reatt_n1_1_past_gelu0_1_Gelu_Tanh0_nl_params_data, reatt_n1_1_past_gelu0_1_Gelu_Tanh0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_Tanh0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu0_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_Tanh0_layer, 129,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &reatt_n1_1_past_gelu0_1_Gelu_Tanh0_chain,
  NULL, &reatt_n1_1_past_gelu0_1_Gelu_Tanh0_layer, AI_STATIC, 
  .nl_params = &reatt_n1_1_past_gelu0_1_Gelu_Tanh0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu0_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_add_10_layer, 132,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu0_1_Gelu_add_10_chain,
  NULL, &reatt_n1_1_past_gelu0_1_Gelu_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_ln0_1_add_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu0_1_Gelu_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul0_layer, 114,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu0_1_Gelu_mul0_chain,
  NULL, &reatt_n1_1_past_gelu0_1_Gelu_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul_30_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu0_1_Gelu_mul0_output, &reatt_n1_1_past_gelu0_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu0_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu0_1_Gelu_mul_30_layer, 135,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu0_1_Gelu_mul_30_chain,
  NULL, &reatt_n1_1_past_gelu0_1_Gelu_mul_30_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_add0_1_Add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &Transpose__890_output, &reatt_n1_1_past_gelu0_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add0_1_Add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_add0_1_Add0_layer, 138,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_add0_1_Add0_chain,
  NULL, &reatt_n1_1_past_add0_1_Add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_u8 reatt_n1_1_past_zpad1_1_strided_slice0_axes_data[] = { 0, 2 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad1_1_strided_slice0_axes, AI_ARRAY_FORMAT_U8,
    reatt_n1_1_past_zpad1_1_strided_slice0_axes_data, reatt_n1_1_past_zpad1_1_strided_slice0_axes_data, 2, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 reatt_n1_1_past_zpad1_1_strided_slice0_starts_data[] = { 0, 0 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad1_1_strided_slice0_starts, AI_ARRAY_FORMAT_S16,
    reatt_n1_1_past_zpad1_1_strided_slice0_starts_data, reatt_n1_1_past_zpad1_1_strided_slice0_starts_data, 2, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 reatt_n1_1_past_zpad1_1_strided_slice0_ends_data[] = { 96, 4 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad1_1_strided_slice0_ends, AI_ARRAY_FORMAT_S16,
    reatt_n1_1_past_zpad1_1_strided_slice0_ends_data, reatt_n1_1_past_zpad1_1_strided_slice0_ends_data, 2, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_zpad1_1_strided_slice0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add0_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_zpad1_1_strided_slice0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_zpad1_1_strided_slice0_layer, 141,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &reatt_n1_1_past_zpad1_1_strided_slice0_chain,
  NULL, &reatt_n1_1_past_zpad1_1_strided_slice0_layer, AI_STATIC, 
  .axes = &reatt_n1_1_past_zpad1_1_strided_slice0_axes, 
  .starts = &reatt_n1_1_past_zpad1_1_strided_slice0_starts, 
  .ends = &reatt_n1_1_past_zpad1_1_strided_slice0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_zpad1_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_zpad1_1_strided_slice0_output, &reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_zpad1_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_zpad1_1_mul0_layer, 144,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_zpad1_1_mul0_chain,
  NULL, &reatt_n1_1_past_zpad1_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ccat1_1_concat0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_zpad1_1_mul0_output, &reatt_n1_1_past_add0_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ccat1_1_concat0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ccat1_1_concat0_layer, 147,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &reatt_n1_1_past_ccat1_1_concat0_chain,
  NULL, &reatt_n1_1_past_ccat1_1_concat0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ccat1_1_concat0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_layer, 150,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_chain,
  NULL, &reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_dil1_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_output0, &const_fold_opt__275_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil1_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_dil1_1_BiasAdd0_layer, 162,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_dil1_1_BiasAdd0_chain,
  NULL, &reatt_n1_1_past_dil1_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_float reatt_n1_1_past_ln1_1_moments_mean0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_ln1_1_moments_mean0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reatt_n1_1_past_ln1_1_moments_mean0_neutral_value_data, reatt_n1_1_past_ln1_1_moments_mean0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_mean0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln1_1_moments_mean0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_mean0_layer, 165,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reatt_n1_1_past_ln1_1_moments_mean0_chain,
  NULL, &reatt_n1_1_past_ln1_1_moments_mean0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reatt_n1_1_past_ln1_1_moments_mean0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_SquaredDifference0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_output, &reatt_n1_1_past_ln1_1_moments_mean0_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln1_1_moments_SquaredDifference0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_SquaredDifference0_layer, 173,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n1_1_past_ln1_1_moments_SquaredDifference0_chain,
  NULL, &reatt_n1_1_past_ln1_1_moments_SquaredDifference0_layer, AI_STATIC, 
  .operation = ai_sub_f32, 
  .buffer_operation = ai_sub_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_output, &reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_layer, 177,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_chain,
  NULL, &reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_float reatt_n1_1_past_ln1_1_moments_variance0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_ln1_1_moments_variance0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reatt_n1_1_past_ln1_1_moments_variance0_neutral_value_data, reatt_n1_1_past_ln1_1_moments_variance0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_variance0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln1_1_moments_variance0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_moments_variance0_layer, 180,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reatt_n1_1_past_ln1_1_moments_variance0_chain,
  NULL, &reatt_n1_1_past_ln1_1_moments_variance0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reatt_n1_1_past_ln1_1_moments_variance0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_output, &const_fold_opt__281_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln1_1_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add0_layer, 183,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln1_1_add0_chain,
  NULL, &reatt_n1_1_past_ln1_1_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_output, &reatt_n1_1_past_ln1_1_Reshape0_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln1_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_mul0_layer, 190,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln1_1_mul0_chain,
  NULL, &reatt_n1_1_past_ln1_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__1970_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln1_1_mul0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__1970_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__1970_layer, 193,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__1970_chain,
  NULL, &Transpose__1970_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_output, &Transpose__1970_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln1_1_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_mul_10_layer, 196,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln1_1_mul_10_chain,
  NULL, &reatt_n1_1_past_ln1_1_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln1_1_mul_10_output, &reatt_n1_1_past_ln1_1_Reshape_10_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln1_1_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add_10_layer, 202,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln1_1_add_10_chain,
  NULL, &reatt_n1_1_past_ln1_1_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__1150_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil1_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__1150_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__1150_layer, 166,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__1150_chain,
  NULL, &Transpose__1150_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &Transpose__1150_output, &Transpose__1970_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln1_1_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_mul_20_layer, 197,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln1_1_mul_20_chain,
  NULL, &reatt_n1_1_past_ln1_1_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln1_1_mul_20_output, &reatt_n1_1_past_ln1_1_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln1_1_add_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln1_1_add_20_layer, 205,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln1_1_add_20_chain,
  NULL, &reatt_n1_1_past_ln1_1_add_20_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_Pow0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_output, &reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu1_1_Gelu_Pow0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_Pow0_layer, 209,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n1_1_past_gelu1_1_Gelu_Pow0_chain,
  NULL, &reatt_n1_1_past_gelu1_1_Gelu_Pow0_layer, AI_STATIC, 
  .operation = ai_pow, 
  .buffer_operation = ai_pow_buffer, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu1_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul_10_layer, 214,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu1_1_Gelu_mul_10_chain,
  NULL, &reatt_n1_1_past_gelu1_1_Gelu_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln1_1_add_20_output, &reatt_n1_1_past_gelu1_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu1_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_add0_layer, 217,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu1_1_Gelu_add0_chain,
  NULL, &reatt_n1_1_past_gelu1_1_Gelu_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu1_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu1_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul_20_layer, 220,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu1_1_Gelu_mul_20_chain,
  NULL, &reatt_n1_1_past_gelu1_1_Gelu_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 reatt_n1_1_past_gelu1_1_Gelu_Tanh0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -124, -123, -122, -121, -120, -119, -118, -117, -115, -113, -111, -109, -107, -104, -101, -97, -94, -89, -85, -80, -75, -69, -63, -56, -49, -41, -34, -26, -18, -9, -1, 7, 16, 24, 32, 39, 47, 54, 61, 67, 73, 78, 83, 87, 92, 95, 99, 102, 105, 107, 109, 111, 113, 115, 116, 117, 118, 119, 120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_gelu1_1_Gelu_Tanh0_nl_params, AI_ARRAY_FORMAT_S8,
    reatt_n1_1_past_gelu1_1_Gelu_Tanh0_nl_params_data, reatt_n1_1_past_gelu1_1_Gelu_Tanh0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_Tanh0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu1_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_Tanh0_layer, 223,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &reatt_n1_1_past_gelu1_1_Gelu_Tanh0_chain,
  NULL, &reatt_n1_1_past_gelu1_1_Gelu_Tanh0_layer, AI_STATIC, 
  .nl_params = &reatt_n1_1_past_gelu1_1_Gelu_Tanh0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu1_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_add_10_layer, 226,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu1_1_Gelu_add_10_chain,
  NULL, &reatt_n1_1_past_gelu1_1_Gelu_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_ln1_1_add_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu1_1_Gelu_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul0_layer, 208,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu1_1_Gelu_mul0_chain,
  NULL, &reatt_n1_1_past_gelu1_1_Gelu_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul_30_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu1_1_Gelu_mul0_output, &reatt_n1_1_past_gelu1_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu1_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu1_1_Gelu_mul_30_layer, 229,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu1_1_Gelu_mul_30_chain,
  NULL, &reatt_n1_1_past_gelu1_1_Gelu_mul_30_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_add1_1_Add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_add0_1_Add0_output, &reatt_n1_1_past_gelu1_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add1_1_Add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_add1_1_Add0_layer, 232,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_add1_1_Add0_chain,
  NULL, &reatt_n1_1_past_add1_1_Add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_u8 reatt_n1_1_past_zpad2_1_strided_slice0_axes_data[] = { 0, 2 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad2_1_strided_slice0_axes, AI_ARRAY_FORMAT_U8,
    reatt_n1_1_past_zpad2_1_strided_slice0_axes_data, reatt_n1_1_past_zpad2_1_strided_slice0_axes_data, 2, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 reatt_n1_1_past_zpad2_1_strided_slice0_starts_data[] = { 0, 0 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad2_1_strided_slice0_starts, AI_ARRAY_FORMAT_S16,
    reatt_n1_1_past_zpad2_1_strided_slice0_starts_data, reatt_n1_1_past_zpad2_1_strided_slice0_starts_data, 2, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 reatt_n1_1_past_zpad2_1_strided_slice0_ends_data[] = { 96, 8 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad2_1_strided_slice0_ends, AI_ARRAY_FORMAT_S16,
    reatt_n1_1_past_zpad2_1_strided_slice0_ends_data, reatt_n1_1_past_zpad2_1_strided_slice0_ends_data, 2, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_zpad2_1_strided_slice0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add1_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_zpad2_1_strided_slice0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_zpad2_1_strided_slice0_layer, 235,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &reatt_n1_1_past_zpad2_1_strided_slice0_chain,
  NULL, &reatt_n1_1_past_zpad2_1_strided_slice0_layer, AI_STATIC, 
  .axes = &reatt_n1_1_past_zpad2_1_strided_slice0_axes, 
  .starts = &reatt_n1_1_past_zpad2_1_strided_slice0_starts, 
  .ends = &reatt_n1_1_past_zpad2_1_strided_slice0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_zpad2_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_zpad2_1_strided_slice0_output, &reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_zpad2_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_zpad2_1_mul0_layer, 238,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_zpad2_1_mul0_chain,
  NULL, &reatt_n1_1_past_zpad2_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ccat2_1_concat0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_zpad2_1_mul0_output, &reatt_n1_1_past_add1_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ccat2_1_concat0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ccat2_1_concat0_layer, 241,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &reatt_n1_1_past_ccat2_1_concat0_chain,
  NULL, &reatt_n1_1_past_ccat2_1_concat0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ccat2_1_concat0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_layer, 244,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_chain,
  NULL, &reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_dil2_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_output0, &const_fold_opt__280_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil2_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_dil2_1_BiasAdd0_layer, 256,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_dil2_1_BiasAdd0_chain,
  NULL, &reatt_n1_1_past_dil2_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_float reatt_n1_1_past_ln2_1_moments_mean0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_ln2_1_moments_mean0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reatt_n1_1_past_ln2_1_moments_mean0_neutral_value_data, reatt_n1_1_past_ln2_1_moments_mean0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_mean0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln2_1_moments_mean0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_mean0_layer, 259,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reatt_n1_1_past_ln2_1_moments_mean0_chain,
  NULL, &reatt_n1_1_past_ln2_1_moments_mean0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reatt_n1_1_past_ln2_1_moments_mean0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_SquaredDifference0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_output, &reatt_n1_1_past_ln2_1_moments_mean0_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln2_1_moments_SquaredDifference0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_SquaredDifference0_layer, 267,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n1_1_past_ln2_1_moments_SquaredDifference0_chain,
  NULL, &reatt_n1_1_past_ln2_1_moments_SquaredDifference0_layer, AI_STATIC, 
  .operation = ai_sub_f32, 
  .buffer_operation = ai_sub_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_output, &reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_layer, 271,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_chain,
  NULL, &reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_float reatt_n1_1_past_ln2_1_moments_variance0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_ln2_1_moments_variance0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reatt_n1_1_past_ln2_1_moments_variance0_neutral_value_data, reatt_n1_1_past_ln2_1_moments_variance0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_variance0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln2_1_moments_variance0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_moments_variance0_layer, 274,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reatt_n1_1_past_ln2_1_moments_variance0_chain,
  NULL, &reatt_n1_1_past_ln2_1_moments_variance0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reatt_n1_1_past_ln2_1_moments_variance0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_output, &const_fold_opt__281_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln2_1_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add0_layer, 277,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln2_1_add0_chain,
  NULL, &reatt_n1_1_past_ln2_1_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_output, &reatt_n1_1_past_ln2_1_Reshape0_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln2_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_mul0_layer, 284,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln2_1_mul0_chain,
  NULL, &reatt_n1_1_past_ln2_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__2030_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln2_1_mul0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__2030_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__2030_layer, 287,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__2030_chain,
  NULL, &Transpose__2030_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_output, &Transpose__2030_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln2_1_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_mul_10_layer, 290,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln2_1_mul_10_chain,
  NULL, &reatt_n1_1_past_ln2_1_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln2_1_mul_10_output, &reatt_n1_1_past_ln2_1_Reshape_10_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln2_1_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add_10_layer, 296,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln2_1_add_10_chain,
  NULL, &reatt_n1_1_past_ln2_1_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__1260_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil2_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__1260_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__1260_layer, 260,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__1260_chain,
  NULL, &Transpose__1260_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &Transpose__1260_output, &Transpose__2030_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln2_1_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_mul_20_layer, 291,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln2_1_mul_20_chain,
  NULL, &reatt_n1_1_past_ln2_1_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln2_1_mul_20_output, &reatt_n1_1_past_ln2_1_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln2_1_add_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln2_1_add_20_layer, 299,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln2_1_add_20_chain,
  NULL, &reatt_n1_1_past_ln2_1_add_20_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_Pow0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_output, &reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu2_1_Gelu_Pow0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_Pow0_layer, 303,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n1_1_past_gelu2_1_Gelu_Pow0_chain,
  NULL, &reatt_n1_1_past_gelu2_1_Gelu_Pow0_layer, AI_STATIC, 
  .operation = ai_pow, 
  .buffer_operation = ai_pow_buffer, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu2_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_10_layer, 308,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu2_1_Gelu_mul_10_chain,
  NULL, &reatt_n1_1_past_gelu2_1_Gelu_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln2_1_add_20_output, &reatt_n1_1_past_gelu2_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu2_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_add0_layer, 311,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu2_1_Gelu_add0_chain,
  NULL, &reatt_n1_1_past_gelu2_1_Gelu_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu2_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu2_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_20_layer, 314,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu2_1_Gelu_mul_20_chain,
  NULL, &reatt_n1_1_past_gelu2_1_Gelu_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 reatt_n1_1_past_gelu2_1_Gelu_Tanh0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -125, -125, -124, -124, -123, -122, -122, -121, -119, -118, -116, -114, -112, -110, -107, -104, -100, -96, -91, -85, -79, -73, -66, -58, -49, -40, -30, -21, -10, 0, 10, 21, 30, 40, 49, 58, 66, 73, 79, 85, 91, 96, 100, 104, 107, 110, 112, 114, 116, 118, 119, 121, 122, 122, 123, 124, 124, 125, 125, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_gelu2_1_Gelu_Tanh0_nl_params, AI_ARRAY_FORMAT_S8,
    reatt_n1_1_past_gelu2_1_Gelu_Tanh0_nl_params_data, reatt_n1_1_past_gelu2_1_Gelu_Tanh0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_Tanh0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu2_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_Tanh0_layer, 317,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &reatt_n1_1_past_gelu2_1_Gelu_Tanh0_chain,
  NULL, &reatt_n1_1_past_gelu2_1_Gelu_Tanh0_layer, AI_STATIC, 
  .nl_params = &reatt_n1_1_past_gelu2_1_Gelu_Tanh0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu2_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_add_10_layer, 320,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu2_1_Gelu_add_10_chain,
  NULL, &reatt_n1_1_past_gelu2_1_Gelu_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_ln2_1_add_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu2_1_Gelu_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul0_layer, 302,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu2_1_Gelu_mul0_chain,
  NULL, &reatt_n1_1_past_gelu2_1_Gelu_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_30_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu2_1_Gelu_mul0_output, &reatt_n1_1_past_gelu2_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu2_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu2_1_Gelu_mul_30_layer, 323,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu2_1_Gelu_mul_30_chain,
  NULL, &reatt_n1_1_past_gelu2_1_Gelu_mul_30_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_add2_1_Add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_add1_1_Add0_output, &reatt_n1_1_past_gelu2_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add2_1_Add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_add2_1_Add0_layer, 326,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_add2_1_Add0_chain,
  NULL, &reatt_n1_1_past_add2_1_Add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_u8 reatt_n1_1_past_zpad3_1_strided_slice0_axes_data[] = { 0, 2 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad3_1_strided_slice0_axes, AI_ARRAY_FORMAT_U8,
    reatt_n1_1_past_zpad3_1_strided_slice0_axes_data, reatt_n1_1_past_zpad3_1_strided_slice0_axes_data, 2, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 reatt_n1_1_past_zpad3_1_strided_slice0_starts_data[] = { 0, 0 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad3_1_strided_slice0_starts, AI_ARRAY_FORMAT_S16,
    reatt_n1_1_past_zpad3_1_strided_slice0_starts_data, reatt_n1_1_past_zpad3_1_strided_slice0_starts_data, 2, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 reatt_n1_1_past_zpad3_1_strided_slice0_ends_data[] = { 96, 16 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad3_1_strided_slice0_ends, AI_ARRAY_FORMAT_S16,
    reatt_n1_1_past_zpad3_1_strided_slice0_ends_data, reatt_n1_1_past_zpad3_1_strided_slice0_ends_data, 2, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_zpad3_1_strided_slice0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add2_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_zpad3_1_strided_slice0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_zpad3_1_strided_slice0_layer, 329,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &reatt_n1_1_past_zpad3_1_strided_slice0_chain,
  NULL, &reatt_n1_1_past_zpad3_1_strided_slice0_layer, AI_STATIC, 
  .axes = &reatt_n1_1_past_zpad3_1_strided_slice0_axes, 
  .starts = &reatt_n1_1_past_zpad3_1_strided_slice0_starts, 
  .ends = &reatt_n1_1_past_zpad3_1_strided_slice0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_zpad3_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_zpad3_1_strided_slice0_output, &reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_zpad3_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_zpad3_1_mul0_layer, 332,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_zpad3_1_mul0_chain,
  NULL, &reatt_n1_1_past_zpad3_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ccat3_1_concat0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_zpad3_1_mul0_output, &reatt_n1_1_past_add2_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ccat3_1_concat0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ccat3_1_concat0_layer, 335,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &reatt_n1_1_past_ccat3_1_concat0_chain,
  NULL, &reatt_n1_1_past_ccat3_1_concat0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ccat3_1_concat0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_layer, 338,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_chain,
  NULL, &reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_dil3_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_output0, &const_fold_opt__282_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil3_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_dil3_1_BiasAdd0_layer, 350,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_dil3_1_BiasAdd0_chain,
  NULL, &reatt_n1_1_past_dil3_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_float reatt_n1_1_past_ln3_1_moments_mean0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_ln3_1_moments_mean0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reatt_n1_1_past_ln3_1_moments_mean0_neutral_value_data, reatt_n1_1_past_ln3_1_moments_mean0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_mean0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln3_1_moments_mean0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_mean0_layer, 353,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reatt_n1_1_past_ln3_1_moments_mean0_chain,
  NULL, &reatt_n1_1_past_ln3_1_moments_mean0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reatt_n1_1_past_ln3_1_moments_mean0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_SquaredDifference0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_output, &reatt_n1_1_past_ln3_1_moments_mean0_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln3_1_moments_SquaredDifference0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_SquaredDifference0_layer, 361,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n1_1_past_ln3_1_moments_SquaredDifference0_chain,
  NULL, &reatt_n1_1_past_ln3_1_moments_SquaredDifference0_layer, AI_STATIC, 
  .operation = ai_sub_f32, 
  .buffer_operation = ai_sub_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_output, &reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_layer, 365,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_chain,
  NULL, &reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_float reatt_n1_1_past_ln3_1_moments_variance0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_ln3_1_moments_variance0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reatt_n1_1_past_ln3_1_moments_variance0_neutral_value_data, reatt_n1_1_past_ln3_1_moments_variance0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_variance0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln3_1_moments_variance0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_moments_variance0_layer, 368,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reatt_n1_1_past_ln3_1_moments_variance0_chain,
  NULL, &reatt_n1_1_past_ln3_1_moments_variance0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reatt_n1_1_past_ln3_1_moments_variance0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_output, &const_fold_opt__281_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln3_1_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add0_layer, 371,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln3_1_add0_chain,
  NULL, &reatt_n1_1_past_ln3_1_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_output, &reatt_n1_1_past_ln3_1_Reshape0_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln3_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_mul0_layer, 378,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln3_1_mul0_chain,
  NULL, &reatt_n1_1_past_ln3_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__2090_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln3_1_mul0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__2090_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__2090_layer, 381,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__2090_chain,
  NULL, &Transpose__2090_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_output, &Transpose__2090_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln3_1_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_mul_10_layer, 384,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln3_1_mul_10_chain,
  NULL, &reatt_n1_1_past_ln3_1_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln3_1_mul_10_output, &reatt_n1_1_past_ln3_1_Reshape_10_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln3_1_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add_10_layer, 390,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln3_1_add_10_chain,
  NULL, &reatt_n1_1_past_ln3_1_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__1370_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil3_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__1370_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__1370_layer, 354,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__1370_chain,
  NULL, &Transpose__1370_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &Transpose__1370_output, &Transpose__2090_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln3_1_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_mul_20_layer, 385,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln3_1_mul_20_chain,
  NULL, &reatt_n1_1_past_ln3_1_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln3_1_mul_20_output, &reatt_n1_1_past_ln3_1_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln3_1_add_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln3_1_add_20_layer, 393,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln3_1_add_20_chain,
  NULL, &reatt_n1_1_past_ln3_1_add_20_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_Pow0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_output, &reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu3_1_Gelu_Pow0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_Pow0_layer, 397,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n1_1_past_gelu3_1_Gelu_Pow0_chain,
  NULL, &reatt_n1_1_past_gelu3_1_Gelu_Pow0_layer, AI_STATIC, 
  .operation = ai_pow, 
  .buffer_operation = ai_pow_buffer, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu3_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_10_layer, 402,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu3_1_Gelu_mul_10_chain,
  NULL, &reatt_n1_1_past_gelu3_1_Gelu_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln3_1_add_20_output, &reatt_n1_1_past_gelu3_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu3_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_add0_layer, 405,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu3_1_Gelu_add0_chain,
  NULL, &reatt_n1_1_past_gelu3_1_Gelu_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu3_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu3_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_20_layer, 408,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu3_1_Gelu_mul_20_chain,
  NULL, &reatt_n1_1_past_gelu3_1_Gelu_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 reatt_n1_1_past_gelu3_1_Gelu_Tanh0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -125, -125, -124, -124, -123, -122, -121, -120, -119, -117, -115, -113, -110, -107, -104, -100, -95, -90, -84, -77, -70, -62, -53, -43, -33, -22, -11, 0, 11, 22, 33, 43, 53, 62, 70, 77, 84, 90, 95, 100, 104, 107, 110, 113, 115, 117, 119, 120, 121, 122, 123, 124, 124, 125, 125, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_gelu3_1_Gelu_Tanh0_nl_params, AI_ARRAY_FORMAT_S8,
    reatt_n1_1_past_gelu3_1_Gelu_Tanh0_nl_params_data, reatt_n1_1_past_gelu3_1_Gelu_Tanh0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_Tanh0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu3_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_Tanh0_layer, 411,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &reatt_n1_1_past_gelu3_1_Gelu_Tanh0_chain,
  NULL, &reatt_n1_1_past_gelu3_1_Gelu_Tanh0_layer, AI_STATIC, 
  .nl_params = &reatt_n1_1_past_gelu3_1_Gelu_Tanh0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu3_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_add_10_layer, 414,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu3_1_Gelu_add_10_chain,
  NULL, &reatt_n1_1_past_gelu3_1_Gelu_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_ln3_1_add_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu3_1_Gelu_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul0_layer, 396,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu3_1_Gelu_mul0_chain,
  NULL, &reatt_n1_1_past_gelu3_1_Gelu_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_30_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu3_1_Gelu_mul0_output, &reatt_n1_1_past_gelu3_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu3_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu3_1_Gelu_mul_30_layer, 417,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu3_1_Gelu_mul_30_chain,
  NULL, &reatt_n1_1_past_gelu3_1_Gelu_mul_30_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_add3_1_Add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_add2_1_Add0_output, &reatt_n1_1_past_gelu3_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add3_1_Add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_add3_1_Add0_layer, 420,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_add3_1_Add0_chain,
  NULL, &reatt_n1_1_past_add3_1_Add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_u8 reatt_n1_1_past_zpad4_1_strided_slice0_axes_data[] = { 0, 2 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad4_1_strided_slice0_axes, AI_ARRAY_FORMAT_U8,
    reatt_n1_1_past_zpad4_1_strided_slice0_axes_data, reatt_n1_1_past_zpad4_1_strided_slice0_axes_data, 2, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 reatt_n1_1_past_zpad4_1_strided_slice0_starts_data[] = { 0, 0 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad4_1_strided_slice0_starts, AI_ARRAY_FORMAT_S16,
    reatt_n1_1_past_zpad4_1_strided_slice0_starts_data, reatt_n1_1_past_zpad4_1_strided_slice0_starts_data, 2, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 reatt_n1_1_past_zpad4_1_strided_slice0_ends_data[] = { 96, 32 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad4_1_strided_slice0_ends, AI_ARRAY_FORMAT_S16,
    reatt_n1_1_past_zpad4_1_strided_slice0_ends_data, reatt_n1_1_past_zpad4_1_strided_slice0_ends_data, 2, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_zpad4_1_strided_slice0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add3_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_zpad4_1_strided_slice0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_zpad4_1_strided_slice0_layer, 423,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &reatt_n1_1_past_zpad4_1_strided_slice0_chain,
  NULL, &reatt_n1_1_past_zpad4_1_strided_slice0_layer, AI_STATIC, 
  .axes = &reatt_n1_1_past_zpad4_1_strided_slice0_axes, 
  .starts = &reatt_n1_1_past_zpad4_1_strided_slice0_starts, 
  .ends = &reatt_n1_1_past_zpad4_1_strided_slice0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_zpad4_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_zpad4_1_strided_slice0_output, &reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_zpad4_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_zpad4_1_mul0_layer, 426,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_zpad4_1_mul0_chain,
  NULL, &reatt_n1_1_past_zpad4_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ccat4_1_concat0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_zpad4_1_mul0_output, &reatt_n1_1_past_add3_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ccat4_1_concat0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ccat4_1_concat0_layer, 429,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &reatt_n1_1_past_ccat4_1_concat0_chain,
  NULL, &reatt_n1_1_past_ccat4_1_concat0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ccat4_1_concat0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_layer, 432,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_chain,
  NULL, &reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_dil4_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_output0, &const_fold_opt__254_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil4_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_dil4_1_BiasAdd0_layer, 444,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_dil4_1_BiasAdd0_chain,
  NULL, &reatt_n1_1_past_dil4_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_float reatt_n1_1_past_ln4_1_moments_mean0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_ln4_1_moments_mean0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reatt_n1_1_past_ln4_1_moments_mean0_neutral_value_data, reatt_n1_1_past_ln4_1_moments_mean0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_mean0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln4_1_moments_mean0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_mean0_layer, 447,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reatt_n1_1_past_ln4_1_moments_mean0_chain,
  NULL, &reatt_n1_1_past_ln4_1_moments_mean0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reatt_n1_1_past_ln4_1_moments_mean0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_SquaredDifference0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_output, &reatt_n1_1_past_ln4_1_moments_mean0_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln4_1_moments_SquaredDifference0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_SquaredDifference0_layer, 455,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n1_1_past_ln4_1_moments_SquaredDifference0_chain,
  NULL, &reatt_n1_1_past_ln4_1_moments_SquaredDifference0_layer, AI_STATIC, 
  .operation = ai_sub_f32, 
  .buffer_operation = ai_sub_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_output, &reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_layer, 459,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_chain,
  NULL, &reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_float reatt_n1_1_past_ln4_1_moments_variance0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_ln4_1_moments_variance0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reatt_n1_1_past_ln4_1_moments_variance0_neutral_value_data, reatt_n1_1_past_ln4_1_moments_variance0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_variance0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln4_1_moments_variance0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_moments_variance0_layer, 462,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reatt_n1_1_past_ln4_1_moments_variance0_chain,
  NULL, &reatt_n1_1_past_ln4_1_moments_variance0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reatt_n1_1_past_ln4_1_moments_variance0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_output, &const_fold_opt__281_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln4_1_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add0_layer, 465,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln4_1_add0_chain,
  NULL, &reatt_n1_1_past_ln4_1_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_output, &reatt_n1_1_past_ln4_1_Reshape0_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln4_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_mul0_layer, 472,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln4_1_mul0_chain,
  NULL, &reatt_n1_1_past_ln4_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__2150_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln4_1_mul0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__2150_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__2150_layer, 475,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__2150_chain,
  NULL, &Transpose__2150_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_output, &Transpose__2150_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln4_1_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_mul_10_layer, 478,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln4_1_mul_10_chain,
  NULL, &reatt_n1_1_past_ln4_1_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln4_1_mul_10_output, &reatt_n1_1_past_ln4_1_Reshape_10_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln4_1_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add_10_layer, 484,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln4_1_add_10_chain,
  NULL, &reatt_n1_1_past_ln4_1_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__1480_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil4_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__1480_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__1480_layer, 448,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__1480_chain,
  NULL, &Transpose__1480_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &Transpose__1480_output, &Transpose__2150_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln4_1_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_mul_20_layer, 479,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln4_1_mul_20_chain,
  NULL, &reatt_n1_1_past_ln4_1_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln4_1_mul_20_output, &reatt_n1_1_past_ln4_1_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln4_1_add_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln4_1_add_20_layer, 487,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln4_1_add_20_chain,
  NULL, &reatt_n1_1_past_ln4_1_add_20_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Pow0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_output, &reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu4_1_Gelu_Pow0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Pow0_layer, 491,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n1_1_past_gelu4_1_Gelu_Pow0_chain,
  NULL, &reatt_n1_1_past_gelu4_1_Gelu_Pow0_layer, AI_STATIC, 
  .operation = ai_pow, 
  .buffer_operation = ai_pow_buffer, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu4_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul_10_layer, 496,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu4_1_Gelu_mul_10_chain,
  NULL, &reatt_n1_1_past_gelu4_1_Gelu_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln4_1_add_20_output, &reatt_n1_1_past_gelu4_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu4_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_add0_layer, 499,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu4_1_Gelu_add0_chain,
  NULL, &reatt_n1_1_past_gelu4_1_Gelu_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu4_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu4_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul_20_layer, 502,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu4_1_Gelu_mul_20_chain,
  NULL, &reatt_n1_1_past_gelu4_1_Gelu_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 reatt_n1_1_past_gelu4_1_Gelu_Tanh0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -125, -125, -124, -123, -123, -122, -120, -119, -117, -115, -112, -109, -105, -101, -96, -90, -84, -76, -68, -58, -48, -37, -25, -12, 0, 12, 25, 37, 48, 58, 68, 76, 84, 90, 96, 101, 105, 109, 112, 115, 117, 119, 120, 122, 123, 123, 124, 125, 125, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_gelu4_1_Gelu_Tanh0_nl_params, AI_ARRAY_FORMAT_S8,
    reatt_n1_1_past_gelu4_1_Gelu_Tanh0_nl_params_data, reatt_n1_1_past_gelu4_1_Gelu_Tanh0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Tanh0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu4_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_Tanh0_layer, 505,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &reatt_n1_1_past_gelu4_1_Gelu_Tanh0_chain,
  NULL, &reatt_n1_1_past_gelu4_1_Gelu_Tanh0_layer, AI_STATIC, 
  .nl_params = &reatt_n1_1_past_gelu4_1_Gelu_Tanh0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu4_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_add_10_layer, 508,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu4_1_Gelu_add_10_chain,
  NULL, &reatt_n1_1_past_gelu4_1_Gelu_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_ln4_1_add_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu4_1_Gelu_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul0_layer, 490,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu4_1_Gelu_mul0_chain,
  NULL, &reatt_n1_1_past_gelu4_1_Gelu_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul_30_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu4_1_Gelu_mul0_output, &reatt_n1_1_past_gelu4_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu4_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu4_1_Gelu_mul_30_layer, 511,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu4_1_Gelu_mul_30_chain,
  NULL, &reatt_n1_1_past_gelu4_1_Gelu_mul_30_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_add4_1_Add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_add3_1_Add0_output, &reatt_n1_1_past_gelu4_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add4_1_Add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_add4_1_Add0_layer, 514,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_add4_1_Add0_chain,
  NULL, &reatt_n1_1_past_add4_1_Add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_u8 reatt_n1_1_past_zpad5_1_strided_slice0_axes_data[] = { 0, 2 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad5_1_strided_slice0_axes, AI_ARRAY_FORMAT_U8,
    reatt_n1_1_past_zpad5_1_strided_slice0_axes_data, reatt_n1_1_past_zpad5_1_strided_slice0_axes_data, 2, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 reatt_n1_1_past_zpad5_1_strided_slice0_starts_data[] = { 0, 0 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad5_1_strided_slice0_starts, AI_ARRAY_FORMAT_S16,
    reatt_n1_1_past_zpad5_1_strided_slice0_starts_data, reatt_n1_1_past_zpad5_1_strided_slice0_starts_data, 2, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 reatt_n1_1_past_zpad5_1_strided_slice0_ends_data[] = { 96, 64 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_zpad5_1_strided_slice0_ends, AI_ARRAY_FORMAT_S16,
    reatt_n1_1_past_zpad5_1_strided_slice0_ends_data, reatt_n1_1_past_zpad5_1_strided_slice0_ends_data, 2, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_zpad5_1_strided_slice0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add4_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_zpad5_1_strided_slice0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_zpad5_1_strided_slice0_layer, 517,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &reatt_n1_1_past_zpad5_1_strided_slice0_chain,
  NULL, &reatt_n1_1_past_zpad5_1_strided_slice0_layer, AI_STATIC, 
  .axes = &reatt_n1_1_past_zpad5_1_strided_slice0_axes, 
  .starts = &reatt_n1_1_past_zpad5_1_strided_slice0_starts, 
  .ends = &reatt_n1_1_past_zpad5_1_strided_slice0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_zpad5_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_zpad5_1_strided_slice0_output, &reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_zpad5_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_zpad5_1_mul0_layer, 520,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_zpad5_1_mul0_chain,
  NULL, &reatt_n1_1_past_zpad5_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ccat5_1_concat0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_zpad5_1_mul0_output, &reatt_n1_1_past_add4_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ccat5_1_concat0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ccat5_1_concat0_layer, 523,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &reatt_n1_1_past_ccat5_1_concat0_chain,
  NULL, &reatt_n1_1_past_ccat5_1_concat0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ccat5_1_concat0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_layer, 526,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_chain,
  NULL, &reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_dil5_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_output0, &const_fold_opt__283_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil5_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_dil5_1_BiasAdd0_layer, 538,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_dil5_1_BiasAdd0_chain,
  NULL, &reatt_n1_1_past_dil5_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_float reatt_n1_1_past_ln5_1_moments_mean0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_ln5_1_moments_mean0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reatt_n1_1_past_ln5_1_moments_mean0_neutral_value_data, reatt_n1_1_past_ln5_1_moments_mean0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_mean0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln5_1_moments_mean0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_mean0_layer, 541,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reatt_n1_1_past_ln5_1_moments_mean0_chain,
  NULL, &reatt_n1_1_past_ln5_1_moments_mean0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reatt_n1_1_past_ln5_1_moments_mean0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_SquaredDifference0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_output, &reatt_n1_1_past_ln5_1_moments_mean0_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln5_1_moments_SquaredDifference0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_SquaredDifference0_layer, 549,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n1_1_past_ln5_1_moments_SquaredDifference0_chain,
  NULL, &reatt_n1_1_past_ln5_1_moments_SquaredDifference0_layer, AI_STATIC, 
  .operation = ai_sub_f32, 
  .buffer_operation = ai_sub_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_output, &reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_layer, 553,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_chain,
  NULL, &reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_float reatt_n1_1_past_ln5_1_moments_variance0_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_ln5_1_moments_variance0_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reatt_n1_1_past_ln5_1_moments_variance0_neutral_value_data, reatt_n1_1_past_ln5_1_moments_variance0_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_variance0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln5_1_moments_variance0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_moments_variance0_layer, 556,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reatt_n1_1_past_ln5_1_moments_variance0_chain,
  NULL, &reatt_n1_1_past_ln5_1_moments_variance0_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reatt_n1_1_past_ln5_1_moments_variance0_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_output, &const_fold_opt__281_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln5_1_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add0_layer, 559,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln5_1_add0_chain,
  NULL, &reatt_n1_1_past_ln5_1_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_output, &reatt_n1_1_past_ln5_1_Reshape0_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln5_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_mul0_layer, 566,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln5_1_mul0_chain,
  NULL, &reatt_n1_1_past_ln5_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__2210_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln5_1_mul0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__2210_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__2210_layer, 569,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__2210_chain,
  NULL, &Transpose__2210_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_output, &Transpose__2210_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln5_1_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_mul_10_layer, 572,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln5_1_mul_10_chain,
  NULL, &reatt_n1_1_past_ln5_1_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln5_1_mul_10_output, &reatt_n1_1_past_ln5_1_Reshape_10_DequantizeLinear_Output_const),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln5_1_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add_10_layer, 578,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln5_1_add_10_chain,
  NULL, &reatt_n1_1_past_ln5_1_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  Transpose__1570_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_dil5_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &Transpose__1570_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  Transpose__1570_layer, 542,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &Transpose__1570_chain,
  NULL, &Transpose__1570_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &Transpose__1570_output, &Transpose__2210_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln5_1_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_mul_20_layer, 573,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln5_1_mul_20_chain,
  NULL, &reatt_n1_1_past_ln5_1_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln5_1_mul_20_output, &reatt_n1_1_past_ln5_1_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_ln5_1_add_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_ln5_1_add_20_layer, 581,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_ln5_1_add_20_chain,
  NULL, &reatt_n1_1_past_ln5_1_add_20_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_Pow0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_output, &reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu5_1_Gelu_Pow0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_Pow0_layer, 585,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &reatt_n1_1_past_gelu5_1_Gelu_Pow0_chain,
  NULL, &reatt_n1_1_past_gelu5_1_Gelu_Pow0_layer, AI_STATIC, 
  .operation = ai_pow, 
  .buffer_operation = ai_pow_buffer, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu5_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul_10_layer, 590,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu5_1_Gelu_mul_10_chain,
  NULL, &reatt_n1_1_past_gelu5_1_Gelu_mul_10_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_ln5_1_add_20_output, &reatt_n1_1_past_gelu5_1_Gelu_mul_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu5_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_add0_layer, 593,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu5_1_Gelu_add0_chain,
  NULL, &reatt_n1_1_past_gelu5_1_Gelu_add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu5_1_Gelu_add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu5_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul_20_layer, 596,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu5_1_Gelu_mul_20_chain,
  NULL, &reatt_n1_1_past_gelu5_1_Gelu_mul_20_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 reatt_n1_1_past_gelu5_1_Gelu_Tanh0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -126, -126, -125, -125, -124, -123, -122, -121, -119, -118, -116, -114, -111, -108, -104, -100, -96, -90, -84, -78, -70, -62, -53, -44, -33, -23, -12, -1, 10, 21, 31, 42, 51, 60, 68, 76, 82, 88, 94, 98, 102, 106, 109, 112, 114, 116, 117, 119, 120, 121, 122, 123, 123, 124, 124, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    reatt_n1_1_past_gelu5_1_Gelu_Tanh0_nl_params, AI_ARRAY_FORMAT_S8,
    reatt_n1_1_past_gelu5_1_Gelu_Tanh0_nl_params_data, reatt_n1_1_past_gelu5_1_Gelu_Tanh0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_Tanh0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu5_1_Gelu_mul_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_Tanh0_layer, 599,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &reatt_n1_1_past_gelu5_1_Gelu_Tanh0_chain,
  NULL, &reatt_n1_1_past_gelu5_1_Gelu_Tanh0_layer, AI_STATIC, 
  .nl_params = &reatt_n1_1_past_gelu5_1_Gelu_Tanh0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_add_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu5_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_add_10_layer, 602,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu5_1_Gelu_add_10_chain,
  NULL, &reatt_n1_1_past_gelu5_1_Gelu_add_10_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D, &reatt_n1_1_past_ln5_1_add_20_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu5_1_Gelu_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul0_layer, 584,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu5_1_Gelu_mul0_chain,
  NULL, &reatt_n1_1_past_gelu5_1_Gelu_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul_30_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_gelu5_1_Gelu_mul0_output, &reatt_n1_1_past_gelu5_1_Gelu_add_10_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_gelu5_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_gelu5_1_Gelu_mul_30_layer, 605,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_gelu5_1_Gelu_mul_30_chain,
  NULL, &reatt_n1_1_past_gelu5_1_Gelu_mul_30_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_add5_1_Add0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_add4_1_Add0_output, &reatt_n1_1_past_gelu5_1_Gelu_mul_30_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add5_1_Add0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_add5_1_Add0_layer, 608,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_add5_1_Add0_chain,
  NULL, &reatt_n1_1_past_add5_1_Add0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add5_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_layer, 614,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_chain,
  NULL, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_weights, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_layer, 614,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSSA,
  &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_chain,
  NULL, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(1, 1), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n1_1_past_add5_1_Add0_output, &reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_pool_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_mul0_layer, 626,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n1_1_past_pool_1_mul0_chain,
  NULL, &reatt_n1_1_past_pool_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_float past_pool_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    past_pool_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    past_pool_neutral_value_data, past_pool_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  past_pool_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &past_pool_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  past_pool_layer, 629,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &past_pool_chain,
  NULL, &past_pool_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &past_pool_neutral_value, 
)
/**  Hybrid layers declarations section  *************************************/
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_proj_1_BiasAdd0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_proj_1_convolution0_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_proj_1_convolution0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  const_fold_opt__266_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 120);
  const_fold_opt__266_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 120);
  reatt_n1_1_past_proj_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_proj_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(44, 2, { reatt_n1_1_past_proj_1_convolution0_output0.data->data,const_fold_opt__266_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_proj_1_BiasAdd0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(44, 1, { reatt_n1_1_past_proj_1_BiasAdd0_output.data->data});
}
void forward_lite_transpose_Transpose__890(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_proj_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_proj_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__890_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  Transpose__890_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(48, 1, { reatt_n1_1_past_proj_1_BiasAdd0_output.data->data});
  forward_transpose(&Transpose__890_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(48, 1, { Transpose__890_output.data->data});
}
void forward_lite_slice_reatt_n1_1_past_zpad0_1_strided_slice0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_proj_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_proj_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_zpad0_1_strided_slice0_output_array.data = AI_PTR(net_ctx->_activations[0] + 57408);
  reatt_n1_1_past_zpad0_1_strided_slice0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57408);
  _STAI_REATTN1C_EVENT_NODE_START_CB(47, 1, { reatt_n1_1_past_proj_1_BiasAdd0_output.data->data});
  forward_slice(&reatt_n1_1_past_zpad0_1_strided_slice0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(47, 1, { reatt_n1_1_past_zpad0_1_strided_slice0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_zpad0_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_zpad0_1_strided_slice0_output_array.data = AI_PTR(net_ctx->_activations[0] + 57408);
  reatt_n1_1_past_zpad0_1_strided_slice0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57408);
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 4);
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 4);
  reatt_n1_1_past_zpad0_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 57216);
  reatt_n1_1_past_zpad0_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57216);
  _STAI_REATTN1C_EVENT_NODE_START_CB(53, 2, { reatt_n1_1_past_zpad0_1_strided_slice0_output.data->data,reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_zpad0_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(53, 1, { reatt_n1_1_past_zpad0_1_mul0_output.data->data});
}
void forward_lite_concat_reatt_n1_1_past_ccat0_1_concat0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_zpad0_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 57216);
  reatt_n1_1_past_zpad0_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57216);
  reatt_n1_1_past_proj_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_proj_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ccat0_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ccat0_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(56, 2, { reatt_n1_1_past_zpad0_1_mul0_output.data->data,reatt_n1_1_past_proj_1_BiasAdd0_output.data->data});
  forward_concat(&reatt_n1_1_past_ccat0_1_concat0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(56, 1, { reatt_n1_1_past_ccat0_1_concat0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_dil0_1_BiasAdd0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil0_1_convolution0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_dil0_1_convolution0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  const_fold_opt__267_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 216);
  const_fold_opt__267_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 216);
  reatt_n1_1_past_dil0_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_dil0_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(68, 2, { reatt_n1_1_past_dil0_1_convolution0_output0.data->data,const_fold_opt__267_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_dil0_1_BiasAdd0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(68, 1, { reatt_n1_1_past_dil0_1_BiasAdd0_output.data->data});
}
void forward_lite_reduce_reatt_n1_1_past_ln0_1_moments_mean0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln0_1_moments_mean0_output_array.data = AI_PTR(net_ctx->_activations[0] + 201600);
  reatt_n1_1_past_ln0_1_moments_mean0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 201600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(71, 1, { reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_output.data->data});
  forward_reduce(&reatt_n1_1_past_ln0_1_moments_mean0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(71, 1, { reatt_n1_1_past_ln0_1_moments_mean0_output.data->data});
}
void forward_lite_eltwise_reatt_n1_1_past_ln0_1_moments_SquaredDifference0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln0_1_moments_mean0_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 201600);
  reatt_n1_1_past_ln0_1_moments_mean0_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 201600);
  reatt_n1_1_past_ln0_1_moments_SquaredDifference0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln0_1_moments_SquaredDifference0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(79, 2, { reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_output.data->data,reatt_n1_1_past_ln0_1_moments_mean0_Mul_output.data->data});
  forward_eltwise(&reatt_n1_1_past_ln0_1_moments_SquaredDifference0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(79, 1, { reatt_n1_1_past_ln0_1_moments_SquaredDifference0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_output_array.data = AI_PTR(net_ctx->_activations[0] + 172800);
  reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 172800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(83, 2, { reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_output.data->data,reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(83, 1, { reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_output.data->data});
}
void forward_lite_reduce_reatt_n1_1_past_ln0_1_moments_variance0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln0_1_moments_variance0_output_array.data = AI_PTR(net_ctx->_activations[0] + 202800);
  reatt_n1_1_past_ln0_1_moments_variance0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 202800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(86, 1, { reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_output.data->data});
  forward_reduce(&reatt_n1_1_past_ln0_1_moments_variance0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(86, 1, { reatt_n1_1_past_ln0_1_moments_variance0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 87600);
  reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 87600);
  const_fold_opt__281_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 504);
  const_fold_opt__281_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 504);
  reatt_n1_1_past_ln0_1_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln0_1_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(89, 2, { reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_output.data->data,const_fold_opt__281_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln0_1_add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(89, 1, { reatt_n1_1_past_ln0_1_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 87600);
  reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 87600);
  reatt_n1_1_past_ln0_1_Reshape0_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 700);
  reatt_n1_1_past_ln0_1_Reshape0_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 700);
  reatt_n1_1_past_ln0_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 87900);
  reatt_n1_1_past_ln0_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 87900);
  _STAI_REATTN1C_EVENT_NODE_START_CB(96, 2, { reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_output.data->data,reatt_n1_1_past_ln0_1_Reshape0_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln0_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(96, 1, { reatt_n1_1_past_ln0_1_mul0_output.data->data});
}
void forward_lite_transpose_Transpose__1910(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln0_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 87900);
  reatt_n1_1_past_ln0_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 87900);
  Transpose__1910_output_array.data = AI_PTR(net_ctx->_activations[0] + 116700);
  Transpose__1910_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 116700);
  _STAI_REATTN1C_EVENT_NODE_START_CB(99, 1, { reatt_n1_1_past_ln0_1_mul0_output.data->data});
  forward_transpose(&Transpose__1910_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(99, 1, { Transpose__1910_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_mul_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 87600);
  reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 87600);
  Transpose__1910_output_array.data = AI_PTR(net_ctx->_activations[0] + 116700);
  Transpose__1910_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 116700);
  reatt_n1_1_past_ln0_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 87900);
  reatt_n1_1_past_ln0_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 87900);
  _STAI_REATTN1C_EVENT_NODE_START_CB(103, 2, { reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_output.data->data,Transpose__1910_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln0_1_mul_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(103, 1, { reatt_n1_1_past_ln0_1_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_add_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln0_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 87900);
  reatt_n1_1_past_ln0_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 87900);
  reatt_n1_1_past_ln0_1_Reshape_10_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 796);
  reatt_n1_1_past_ln0_1_Reshape_10_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 796);
  reatt_n1_1_past_ln0_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 145500);
  reatt_n1_1_past_ln0_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 145500);
  _STAI_REATTN1C_EVENT_NODE_START_CB(108, 2, { reatt_n1_1_past_ln0_1_mul_10_output.data->data,reatt_n1_1_past_ln0_1_Reshape_10_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln0_1_add_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(108, 1, { reatt_n1_1_past_ln0_1_add_10_output.data->data});
}
void forward_lite_transpose_Transpose__1040(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil0_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_dil0_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  Transpose__1040_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__1040_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(72, 1, { reatt_n1_1_past_dil0_1_BiasAdd0_output.data->data});
  forward_transpose(&Transpose__1040_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(72, 1, { Transpose__1040_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_mul_20(_stai_reattn1c_context* net_ctx)
{
  Transpose__1040_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__1040_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__1910_output_array.data = AI_PTR(net_ctx->_activations[0] + 116700);
  Transpose__1910_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 116700);
  reatt_n1_1_past_ln0_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln0_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(102, 2, { Transpose__1040_output.data->data,Transpose__1910_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln0_1_mul_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(102, 1, { reatt_n1_1_past_ln0_1_mul_20_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_add_20(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln0_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln0_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln0_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 145500);
  reatt_n1_1_past_ln0_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 145500);
  reatt_n1_1_past_ln0_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln0_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(111, 2, { reatt_n1_1_past_ln0_1_mul_20_output.data->data,reatt_n1_1_past_ln0_1_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln0_1_add_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(111, 1, { reatt_n1_1_past_ln0_1_add_20_output.data->data});
}
void forward_lite_eltwise_reatt_n1_1_past_gelu0_1_Gelu_Pow0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_gelu0_1_Gelu_Pow0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu0_1_Gelu_Pow0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(115, 2, { reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_output.data->data,reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D.data->data});
  forward_eltwise(&reatt_n1_1_past_gelu0_1_Gelu_Pow0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(115, 1, { reatt_n1_1_past_gelu0_1_Gelu_Pow0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu0_1_Gelu_mul_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu0_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu0_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(120, 2, { reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu0_1_Gelu_mul_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(120, 1, { reatt_n1_1_past_gelu0_1_Gelu_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu0_1_Gelu_add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln0_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln0_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu0_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu0_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu0_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu0_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(123, 2, { reatt_n1_1_past_ln0_1_add_20_output.data->data,reatt_n1_1_past_gelu0_1_Gelu_mul_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu0_1_Gelu_add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(123, 1, { reatt_n1_1_past_gelu0_1_Gelu_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu0_1_Gelu_mul_20(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n1_1_past_gelu0_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu0_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu0_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu0_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(126, 2, { reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu0_1_Gelu_add0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu0_1_Gelu_mul_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(126, 1, { reatt_n1_1_past_gelu0_1_Gelu_mul_20_output.data->data});
}
void forward_lite_nl_integer_reatt_n1_1_past_gelu0_1_Gelu_Tanh0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu0_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu0_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(129, 1, { reatt_n1_1_past_gelu0_1_Gelu_mul_20_output.data->data});
  forward_nl_integer(&reatt_n1_1_past_gelu0_1_Gelu_Tanh0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(129, 1, { reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu0_1_Gelu_add_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu0_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu0_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(132, 2, { reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu0_1_Gelu_Tanh0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu0_1_Gelu_add_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(132, 1, { reatt_n1_1_past_gelu0_1_Gelu_add_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu0_1_Gelu_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n1_1_past_ln0_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln0_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu0_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_gelu0_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  _STAI_REATTN1C_EVENT_NODE_START_CB(114, 2, { reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_ln0_1_add_20_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu0_1_Gelu_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(114, 1, { reatt_n1_1_past_gelu0_1_Gelu_mul0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu0_1_Gelu_mul_30(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu0_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_gelu0_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_gelu0_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu0_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu0_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu0_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(135, 2, { reatt_n1_1_past_gelu0_1_Gelu_mul0_output.data->data,reatt_n1_1_past_gelu0_1_Gelu_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu0_1_Gelu_mul_30_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(135, 1, { reatt_n1_1_past_gelu0_1_Gelu_mul_30_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_add0_1_Add0(_stai_reattn1c_context* net_ctx)
{
  Transpose__890_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  Transpose__890_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu0_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu0_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_add0_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_add0_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(138, 2, { Transpose__890_output.data->data,reatt_n1_1_past_gelu0_1_Gelu_mul_30_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_add0_1_Add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(138, 1, { reatt_n1_1_past_add0_1_Add0_output.data->data});
}
void forward_lite_slice_reatt_n1_1_past_zpad1_1_strided_slice0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_add0_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_add0_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_zpad1_1_strided_slice0_output_array.data = AI_PTR(net_ctx->_activations[0] + 203616);
  reatt_n1_1_past_zpad1_1_strided_slice0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 203616);
  _STAI_REATTN1C_EVENT_NODE_START_CB(141, 1, { reatt_n1_1_past_add0_1_Add0_output.data->data});
  forward_slice(&reatt_n1_1_past_zpad1_1_strided_slice0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(141, 1, { reatt_n1_1_past_zpad1_1_strided_slice0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_zpad1_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_zpad1_1_strided_slice0_output_array.data = AI_PTR(net_ctx->_activations[0] + 203616);
  reatt_n1_1_past_zpad1_1_strided_slice0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 203616);
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 4);
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 4);
  reatt_n1_1_past_zpad1_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 204000);
  reatt_n1_1_past_zpad1_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 204000);
  _STAI_REATTN1C_EVENT_NODE_START_CB(144, 2, { reatt_n1_1_past_zpad1_1_strided_slice0_output.data->data,reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_zpad1_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(144, 1, { reatt_n1_1_past_zpad1_1_mul0_output.data->data});
}
void forward_lite_concat_reatt_n1_1_past_ccat1_1_concat0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_zpad1_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 204000);
  reatt_n1_1_past_zpad1_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 204000);
  reatt_n1_1_past_add0_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_add0_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ccat1_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 174816);
  reatt_n1_1_past_ccat1_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 174816);
  _STAI_REATTN1C_EVENT_NODE_START_CB(147, 2, { reatt_n1_1_past_zpad1_1_mul0_output.data->data,reatt_n1_1_past_add0_1_Add0_output.data->data});
  forward_concat(&reatt_n1_1_past_ccat1_1_concat0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(147, 1, { reatt_n1_1_past_ccat1_1_concat0_output.data->data});
}
void forward_lite_transpose_reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ccat1_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 174816);
  reatt_n1_1_past_ccat1_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 174816);
  reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 86016);
  reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86016);
  _STAI_REATTN1C_EVENT_NODE_START_CB(150, 1, { reatt_n1_1_past_ccat1_1_concat0_output.data->data});
  forward_transpose(&reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(150, 1, { reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_dil1_1_BiasAdd0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  const_fold_opt__275_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 312);
  const_fold_opt__275_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 312);
  reatt_n1_1_past_dil1_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_dil1_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(162, 2, { reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_output0.data->data,const_fold_opt__275_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_dil1_1_BiasAdd0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(162, 1, { reatt_n1_1_past_dil1_1_BiasAdd0_output.data->data});
}
void forward_lite_reduce_reatt_n1_1_past_ln1_1_moments_mean0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln1_1_moments_mean0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln1_1_moments_mean0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(165, 1, { reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_output.data->data});
  forward_reduce(&reatt_n1_1_past_ln1_1_moments_mean0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(165, 1, { reatt_n1_1_past_ln1_1_moments_mean0_output.data->data});
}
void forward_lite_eltwise_reatt_n1_1_past_ln1_1_moments_SquaredDifference0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln1_1_moments_mean0_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln1_1_moments_mean0_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln1_1_moments_SquaredDifference0_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln1_1_moments_SquaredDifference0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  _STAI_REATTN1C_EVENT_NODE_START_CB(173, 2, { reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_output.data->data,reatt_n1_1_past_ln1_1_moments_mean0_Mul_output.data->data});
  forward_eltwise(&reatt_n1_1_past_ln1_1_moments_SquaredDifference0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(173, 1, { reatt_n1_1_past_ln1_1_moments_SquaredDifference0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 31200);
  reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31200);
  reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_output_array.data = AI_PTR(net_ctx->_activations[0] + 60000);
  reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 60000);
  _STAI_REATTN1C_EVENT_NODE_START_CB(177, 2, { reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_output.data->data,reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(177, 1, { reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_output.data->data});
}
void forward_lite_reduce_reatt_n1_1_past_ln1_1_moments_variance0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln1_1_moments_variance0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln1_1_moments_variance0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(180, 1, { reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_output.data->data});
  forward_reduce(&reatt_n1_1_past_ln1_1_moments_variance0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(180, 1, { reatt_n1_1_past_ln1_1_moments_variance0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  const_fold_opt__281_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 504);
  const_fold_opt__281_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 504);
  reatt_n1_1_past_ln1_1_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 29100);
  reatt_n1_1_past_ln1_1_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29100);
  _STAI_REATTN1C_EVENT_NODE_START_CB(183, 2, { reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_output.data->data,const_fold_opt__281_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln1_1_add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(183, 1, { reatt_n1_1_past_ln1_1_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln1_1_Reshape0_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 892);
  reatt_n1_1_past_ln1_1_Reshape0_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 892);
  reatt_n1_1_past_ln1_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31200);
  reatt_n1_1_past_ln1_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(190, 2, { reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_output.data->data,reatt_n1_1_past_ln1_1_Reshape0_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln1_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(190, 1, { reatt_n1_1_past_ln1_1_mul0_output.data->data});
}
void forward_lite_transpose_Transpose__1970(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln1_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31200);
  reatt_n1_1_past_ln1_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31200);
  Transpose__1970_output_array.data = AI_PTR(net_ctx->_activations[0] + 60000);
  Transpose__1970_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 60000);
  _STAI_REATTN1C_EVENT_NODE_START_CB(193, 1, { reatt_n1_1_past_ln1_1_mul0_output.data->data});
  forward_transpose(&Transpose__1970_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(193, 1, { Transpose__1970_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_mul_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30000);
  Transpose__1970_output_array.data = AI_PTR(net_ctx->_activations[0] + 60000);
  Transpose__1970_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 60000);
  reatt_n1_1_past_ln1_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 30300);
  reatt_n1_1_past_ln1_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30300);
  _STAI_REATTN1C_EVENT_NODE_START_CB(196, 2, { reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_output.data->data,Transpose__1970_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln1_1_mul_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(196, 1, { reatt_n1_1_past_ln1_1_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_add_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln1_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 30300);
  reatt_n1_1_past_ln1_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30300);
  reatt_n1_1_past_ln1_1_Reshape_10_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 988);
  reatt_n1_1_past_ln1_1_Reshape_10_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 988);
  reatt_n1_1_past_ln1_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln1_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  _STAI_REATTN1C_EVENT_NODE_START_CB(202, 2, { reatt_n1_1_past_ln1_1_mul_10_output.data->data,reatt_n1_1_past_ln1_1_Reshape_10_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln1_1_add_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(202, 1, { reatt_n1_1_past_ln1_1_add_10_output.data->data});
}
void forward_lite_transpose_Transpose__1150(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil1_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_dil1_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  Transpose__1150_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  Transpose__1150_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(166, 1, { reatt_n1_1_past_dil1_1_BiasAdd0_output.data->data});
  forward_transpose(&Transpose__1150_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(166, 1, { Transpose__1150_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_mul_20(_stai_reattn1c_context* net_ctx)
{
  Transpose__1150_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  Transpose__1150_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  Transpose__1970_output_array.data = AI_PTR(net_ctx->_activations[0] + 60000);
  Transpose__1970_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 60000);
  reatt_n1_1_past_ln1_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_ln1_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(197, 2, { Transpose__1150_output.data->data,Transpose__1970_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln1_1_mul_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(197, 1, { reatt_n1_1_past_ln1_1_mul_20_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_add_20(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln1_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_ln1_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_ln1_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln1_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln1_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln1_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(205, 2, { reatt_n1_1_past_ln1_1_mul_20_output.data->data,reatt_n1_1_past_ln1_1_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln1_1_add_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(205, 1, { reatt_n1_1_past_ln1_1_add_20_output.data->data});
}
void forward_lite_eltwise_reatt_n1_1_past_gelu1_1_Gelu_Pow0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_gelu1_1_Gelu_Pow0_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_gelu1_1_Gelu_Pow0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  _STAI_REATTN1C_EVENT_NODE_START_CB(209, 2, { reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_output.data->data,reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D.data->data});
  forward_eltwise(&reatt_n1_1_past_gelu1_1_Gelu_Pow0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(209, 1, { reatt_n1_1_past_gelu1_1_Gelu_Pow0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu1_1_Gelu_mul_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu1_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu1_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(214, 2, { reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu1_1_Gelu_mul_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(214, 1, { reatt_n1_1_past_gelu1_1_Gelu_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu1_1_Gelu_add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln1_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln1_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu1_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu1_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu1_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu1_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(217, 2, { reatt_n1_1_past_ln1_1_add_20_output.data->data,reatt_n1_1_past_gelu1_1_Gelu_mul_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu1_1_Gelu_add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(217, 1, { reatt_n1_1_past_gelu1_1_Gelu_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu1_1_Gelu_mul_20(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n1_1_past_gelu1_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu1_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu1_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu1_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(220, 2, { reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu1_1_Gelu_add0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu1_1_Gelu_mul_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(220, 1, { reatt_n1_1_past_gelu1_1_Gelu_mul_20_output.data->data});
}
void forward_lite_nl_integer_reatt_n1_1_past_gelu1_1_Gelu_Tanh0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu1_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu1_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(223, 1, { reatt_n1_1_past_gelu1_1_Gelu_mul_20_output.data->data});
  forward_nl_integer(&reatt_n1_1_past_gelu1_1_Gelu_Tanh0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(223, 1, { reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu1_1_Gelu_add_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu1_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu1_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(226, 2, { reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu1_1_Gelu_Tanh0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu1_1_Gelu_add_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(226, 1, { reatt_n1_1_past_gelu1_1_Gelu_add_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu1_1_Gelu_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n1_1_past_ln1_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln1_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu1_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu1_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(208, 2, { reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_ln1_1_add_20_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu1_1_Gelu_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(208, 1, { reatt_n1_1_past_gelu1_1_Gelu_mul0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu1_1_Gelu_mul_30(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu1_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu1_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu1_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu1_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu1_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu1_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(229, 2, { reatt_n1_1_past_gelu1_1_Gelu_mul0_output.data->data,reatt_n1_1_past_gelu1_1_Gelu_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu1_1_Gelu_mul_30_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(229, 1, { reatt_n1_1_past_gelu1_1_Gelu_mul_30_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_add1_1_Add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_add0_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_add0_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu1_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu1_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_add1_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_add1_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(232, 2, { reatt_n1_1_past_add0_1_Add0_output.data->data,reatt_n1_1_past_gelu1_1_Gelu_mul_30_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_add1_1_Add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(232, 1, { reatt_n1_1_past_add1_1_Add0_output.data->data});
}
void forward_lite_slice_reatt_n1_1_past_zpad2_1_strided_slice0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_add1_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_add1_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_zpad2_1_strided_slice0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_zpad2_1_strided_slice0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(235, 1, { reatt_n1_1_past_add1_1_Add0_output.data->data});
  forward_slice(&reatt_n1_1_past_zpad2_1_strided_slice0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(235, 1, { reatt_n1_1_past_zpad2_1_strided_slice0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_zpad2_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_zpad2_1_strided_slice0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_zpad2_1_strided_slice0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 4);
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 4);
  reatt_n1_1_past_zpad2_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 29568);
  reatt_n1_1_past_zpad2_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29568);
  _STAI_REATTN1C_EVENT_NODE_START_CB(238, 2, { reatt_n1_1_past_zpad2_1_strided_slice0_output.data->data,reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_zpad2_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(238, 1, { reatt_n1_1_past_zpad2_1_mul0_output.data->data});
}
void forward_lite_concat_reatt_n1_1_past_ccat2_1_concat0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_zpad2_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 29568);
  reatt_n1_1_past_zpad2_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29568);
  reatt_n1_1_past_add1_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_add1_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_ccat2_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 30336);
  reatt_n1_1_past_ccat2_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30336);
  _STAI_REATTN1C_EVENT_NODE_START_CB(241, 2, { reatt_n1_1_past_zpad2_1_mul0_output.data->data,reatt_n1_1_past_add1_1_Add0_output.data->data});
  forward_concat(&reatt_n1_1_past_ccat2_1_concat0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(241, 1, { reatt_n1_1_past_ccat2_1_concat0_output.data->data});
}
void forward_lite_transpose_reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ccat2_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 30336);
  reatt_n1_1_past_ccat2_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30336);
  reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 231168);
  reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 231168);
  _STAI_REATTN1C_EVENT_NODE_START_CB(244, 1, { reatt_n1_1_past_ccat2_1_concat0_output.data->data});
  forward_transpose(&reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(244, 1, { reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_dil2_1_BiasAdd0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  const_fold_opt__280_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 408);
  const_fold_opt__280_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 408);
  reatt_n1_1_past_dil2_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_dil2_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(256, 2, { reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_output0.data->data,const_fold_opt__280_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_dil2_1_BiasAdd0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(256, 1, { reatt_n1_1_past_dil2_1_BiasAdd0_output.data->data});
}
void forward_lite_reduce_reatt_n1_1_past_ln2_1_moments_mean0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln2_1_moments_mean0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln2_1_moments_mean0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(259, 1, { reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_output.data->data});
  forward_reduce(&reatt_n1_1_past_ln2_1_moments_mean0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(259, 1, { reatt_n1_1_past_ln2_1_moments_mean0_output.data->data});
}
void forward_lite_eltwise_reatt_n1_1_past_ln2_1_moments_SquaredDifference0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln2_1_moments_mean0_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln2_1_moments_mean0_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln2_1_moments_SquaredDifference0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln2_1_moments_SquaredDifference0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(267, 2, { reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_output.data->data,reatt_n1_1_past_ln2_1_moments_mean0_Mul_output.data->data});
  forward_eltwise(&reatt_n1_1_past_ln2_1_moments_SquaredDifference0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(267, 1, { reatt_n1_1_past_ln2_1_moments_SquaredDifference0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 201600);
  reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 201600);
  reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(271, 2, { reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_output.data->data,reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(271, 1, { reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_output.data->data});
}
void forward_lite_reduce_reatt_n1_1_past_ln2_1_moments_variance0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln2_1_moments_variance0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln2_1_moments_variance0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(274, 1, { reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_output.data->data});
  forward_reduce(&reatt_n1_1_past_ln2_1_moments_variance0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(274, 1, { reatt_n1_1_past_ln2_1_moments_variance0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  const_fold_opt__281_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 504);
  const_fold_opt__281_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 504);
  reatt_n1_1_past_ln2_1_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 29100);
  reatt_n1_1_past_ln2_1_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29100);
  _STAI_REATTN1C_EVENT_NODE_START_CB(277, 2, { reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_output.data->data,const_fold_opt__281_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln2_1_add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(277, 1, { reatt_n1_1_past_ln2_1_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln2_1_Reshape0_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 1084);
  reatt_n1_1_past_ln2_1_Reshape0_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 1084);
  reatt_n1_1_past_ln2_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln2_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(284, 2, { reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_output.data->data,reatt_n1_1_past_ln2_1_Reshape0_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln2_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(284, 1, { reatt_n1_1_past_ln2_1_mul0_output.data->data});
}
void forward_lite_transpose_Transpose__2030(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln2_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln2_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__2030_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  Transpose__2030_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(287, 1, { reatt_n1_1_past_ln2_1_mul0_output.data->data});
  forward_transpose(&Transpose__2030_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(287, 1, { Transpose__2030_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_mul_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30000);
  Transpose__2030_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  Transpose__2030_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln2_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln2_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(290, 2, { reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_output.data->data,Transpose__2030_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln2_1_mul_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(290, 1, { reatt_n1_1_past_ln2_1_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_add_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln2_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln2_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln2_1_Reshape_10_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 1180);
  reatt_n1_1_past_ln2_1_Reshape_10_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 1180);
  reatt_n1_1_past_ln2_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln2_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(296, 2, { reatt_n1_1_past_ln2_1_mul_10_output.data->data,reatt_n1_1_past_ln2_1_Reshape_10_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln2_1_add_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(296, 1, { reatt_n1_1_past_ln2_1_add_10_output.data->data});
}
void forward_lite_transpose_Transpose__1260(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil2_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_dil2_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  Transpose__1260_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__1260_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(260, 1, { reatt_n1_1_past_dil2_1_BiasAdd0_output.data->data});
  forward_transpose(&Transpose__1260_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(260, 1, { Transpose__1260_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_mul_20(_stai_reattn1c_context* net_ctx)
{
  Transpose__1260_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__1260_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__2030_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  Transpose__2030_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln2_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_ln2_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(291, 2, { Transpose__1260_output.data->data,Transpose__2030_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln2_1_mul_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(291, 1, { reatt_n1_1_past_ln2_1_mul_20_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_add_20(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln2_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_ln2_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_ln2_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln2_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln2_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln2_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(299, 2, { reatt_n1_1_past_ln2_1_mul_20_output.data->data,reatt_n1_1_past_ln2_1_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln2_1_add_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(299, 1, { reatt_n1_1_past_ln2_1_add_20_output.data->data});
}
void forward_lite_eltwise_reatt_n1_1_past_gelu2_1_Gelu_Pow0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_gelu2_1_Gelu_Pow0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu2_1_Gelu_Pow0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(303, 2, { reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_output.data->data,reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D.data->data});
  forward_eltwise(&reatt_n1_1_past_gelu2_1_Gelu_Pow0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(303, 1, { reatt_n1_1_past_gelu2_1_Gelu_Pow0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu2_1_Gelu_mul_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu2_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu2_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(308, 2, { reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu2_1_Gelu_mul_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(308, 1, { reatt_n1_1_past_gelu2_1_Gelu_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu2_1_Gelu_add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln2_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln2_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu2_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu2_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu2_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu2_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(311, 2, { reatt_n1_1_past_ln2_1_add_20_output.data->data,reatt_n1_1_past_gelu2_1_Gelu_mul_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu2_1_Gelu_add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(311, 1, { reatt_n1_1_past_gelu2_1_Gelu_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu2_1_Gelu_mul_20(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n1_1_past_gelu2_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu2_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu2_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu2_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(314, 2, { reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu2_1_Gelu_add0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu2_1_Gelu_mul_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(314, 1, { reatt_n1_1_past_gelu2_1_Gelu_mul_20_output.data->data});
}
void forward_lite_nl_integer_reatt_n1_1_past_gelu2_1_Gelu_Tanh0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu2_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu2_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(317, 1, { reatt_n1_1_past_gelu2_1_Gelu_mul_20_output.data->data});
  forward_nl_integer(&reatt_n1_1_past_gelu2_1_Gelu_Tanh0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(317, 1, { reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu2_1_Gelu_add_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu2_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu2_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(320, 2, { reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu2_1_Gelu_Tanh0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu2_1_Gelu_add_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(320, 1, { reatt_n1_1_past_gelu2_1_Gelu_add_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu2_1_Gelu_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n1_1_past_ln2_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln2_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu2_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu2_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(302, 2, { reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_ln2_1_add_20_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu2_1_Gelu_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(302, 1, { reatt_n1_1_past_gelu2_1_Gelu_mul0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu2_1_Gelu_mul_30(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu2_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu2_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu2_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu2_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu2_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu2_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(323, 2, { reatt_n1_1_past_gelu2_1_Gelu_mul0_output.data->data,reatt_n1_1_past_gelu2_1_Gelu_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu2_1_Gelu_mul_30_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(323, 1, { reatt_n1_1_past_gelu2_1_Gelu_mul_30_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_add2_1_Add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_add1_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_add1_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu2_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu2_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_add2_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_add2_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(326, 2, { reatt_n1_1_past_add1_1_Add0_output.data->data,reatt_n1_1_past_gelu2_1_Gelu_mul_30_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_add2_1_Add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(326, 1, { reatt_n1_1_past_add2_1_Add0_output.data->data});
}
void forward_lite_slice_reatt_n1_1_past_zpad3_1_strided_slice0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_add2_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_add2_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_zpad3_1_strided_slice0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_zpad3_1_strided_slice0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(329, 1, { reatt_n1_1_past_add2_1_Add0_output.data->data});
  forward_slice(&reatt_n1_1_past_zpad3_1_strided_slice0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(329, 1, { reatt_n1_1_past_zpad3_1_strided_slice0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_zpad3_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_zpad3_1_strided_slice0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_zpad3_1_strided_slice0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 4);
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 4);
  reatt_n1_1_past_zpad3_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 1536);
  reatt_n1_1_past_zpad3_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 1536);
  _STAI_REATTN1C_EVENT_NODE_START_CB(332, 2, { reatt_n1_1_past_zpad3_1_strided_slice0_output.data->data,reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_zpad3_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(332, 1, { reatt_n1_1_past_zpad3_1_mul0_output.data->data});
}
void forward_lite_concat_reatt_n1_1_past_ccat3_1_concat0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_zpad3_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 1536);
  reatt_n1_1_past_zpad3_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 1536);
  reatt_n1_1_past_add2_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_add2_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ccat3_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 3072);
  reatt_n1_1_past_ccat3_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 3072);
  _STAI_REATTN1C_EVENT_NODE_START_CB(335, 2, { reatt_n1_1_past_zpad3_1_mul0_output.data->data,reatt_n1_1_past_add2_1_Add0_output.data->data});
  forward_concat(&reatt_n1_1_past_ccat3_1_concat0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(335, 1, { reatt_n1_1_past_ccat3_1_concat0_output.data->data});
}
void forward_lite_transpose_reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ccat3_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 3072);
  reatt_n1_1_past_ccat3_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 3072);
  reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 84864);
  reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 84864);
  _STAI_REATTN1C_EVENT_NODE_START_CB(338, 1, { reatt_n1_1_past_ccat3_1_concat0_output.data->data});
  forward_transpose(&reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(338, 1, { reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_dil3_1_BiasAdd0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  const_fold_opt__282_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 508);
  const_fold_opt__282_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 508);
  reatt_n1_1_past_dil3_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_dil3_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(350, 2, { reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_output0.data->data,const_fold_opt__282_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_dil3_1_BiasAdd0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(350, 1, { reatt_n1_1_past_dil3_1_BiasAdd0_output.data->data});
}
void forward_lite_reduce_reatt_n1_1_past_ln3_1_moments_mean0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln3_1_moments_mean0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln3_1_moments_mean0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(353, 1, { reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_output.data->data});
  forward_reduce(&reatt_n1_1_past_ln3_1_moments_mean0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(353, 1, { reatt_n1_1_past_ln3_1_moments_mean0_output.data->data});
}
void forward_lite_eltwise_reatt_n1_1_past_ln3_1_moments_SquaredDifference0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln3_1_moments_mean0_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln3_1_moments_mean0_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln3_1_moments_SquaredDifference0_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln3_1_moments_SquaredDifference0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  _STAI_REATTN1C_EVENT_NODE_START_CB(361, 2, { reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_output.data->data,reatt_n1_1_past_ln3_1_moments_mean0_Mul_output.data->data});
  forward_eltwise(&reatt_n1_1_past_ln3_1_moments_SquaredDifference0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(361, 1, { reatt_n1_1_past_ln3_1_moments_SquaredDifference0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 31200);
  reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31200);
  reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_output_array.data = AI_PTR(net_ctx->_activations[0] + 60000);
  reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 60000);
  _STAI_REATTN1C_EVENT_NODE_START_CB(365, 2, { reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_output.data->data,reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(365, 1, { reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_output.data->data});
}
void forward_lite_reduce_reatt_n1_1_past_ln3_1_moments_variance0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln3_1_moments_variance0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln3_1_moments_variance0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(368, 1, { reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_output.data->data});
  forward_reduce(&reatt_n1_1_past_ln3_1_moments_variance0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(368, 1, { reatt_n1_1_past_ln3_1_moments_variance0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  const_fold_opt__281_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 504);
  const_fold_opt__281_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 504);
  reatt_n1_1_past_ln3_1_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 29100);
  reatt_n1_1_past_ln3_1_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29100);
  _STAI_REATTN1C_EVENT_NODE_START_CB(371, 2, { reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_output.data->data,const_fold_opt__281_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln3_1_add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(371, 1, { reatt_n1_1_past_ln3_1_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln3_1_Reshape0_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 1276);
  reatt_n1_1_past_ln3_1_Reshape0_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 1276);
  reatt_n1_1_past_ln3_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31200);
  reatt_n1_1_past_ln3_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(378, 2, { reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_output.data->data,reatt_n1_1_past_ln3_1_Reshape0_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln3_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(378, 1, { reatt_n1_1_past_ln3_1_mul0_output.data->data});
}
void forward_lite_transpose_Transpose__2090(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln3_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 31200);
  reatt_n1_1_past_ln3_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 31200);
  Transpose__2090_output_array.data = AI_PTR(net_ctx->_activations[0] + 60000);
  Transpose__2090_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 60000);
  _STAI_REATTN1C_EVENT_NODE_START_CB(381, 1, { reatt_n1_1_past_ln3_1_mul0_output.data->data});
  forward_transpose(&Transpose__2090_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(381, 1, { Transpose__2090_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_mul_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30000);
  Transpose__2090_output_array.data = AI_PTR(net_ctx->_activations[0] + 60000);
  Transpose__2090_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 60000);
  reatt_n1_1_past_ln3_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 30300);
  reatt_n1_1_past_ln3_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30300);
  _STAI_REATTN1C_EVENT_NODE_START_CB(384, 2, { reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_output.data->data,Transpose__2090_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln3_1_mul_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(384, 1, { reatt_n1_1_past_ln3_1_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_add_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln3_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 30300);
  reatt_n1_1_past_ln3_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30300);
  reatt_n1_1_past_ln3_1_Reshape_10_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 1372);
  reatt_n1_1_past_ln3_1_Reshape_10_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 1372);
  reatt_n1_1_past_ln3_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln3_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  _STAI_REATTN1C_EVENT_NODE_START_CB(390, 2, { reatt_n1_1_past_ln3_1_mul_10_output.data->data,reatt_n1_1_past_ln3_1_Reshape_10_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln3_1_add_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(390, 1, { reatt_n1_1_past_ln3_1_add_10_output.data->data});
}
void forward_lite_transpose_Transpose__1370(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil3_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_dil3_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  Transpose__1370_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  Transpose__1370_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(354, 1, { reatt_n1_1_past_dil3_1_BiasAdd0_output.data->data});
  forward_transpose(&Transpose__1370_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(354, 1, { Transpose__1370_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_mul_20(_stai_reattn1c_context* net_ctx)
{
  Transpose__1370_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  Transpose__1370_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  Transpose__2090_output_array.data = AI_PTR(net_ctx->_activations[0] + 60000);
  Transpose__2090_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 60000);
  reatt_n1_1_past_ln3_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_ln3_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(385, 2, { Transpose__1370_output.data->data,Transpose__2090_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln3_1_mul_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(385, 1, { reatt_n1_1_past_ln3_1_mul_20_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_add_20(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln3_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_ln3_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_ln3_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln3_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln3_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln3_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(393, 2, { reatt_n1_1_past_ln3_1_mul_20_output.data->data,reatt_n1_1_past_ln3_1_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln3_1_add_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(393, 1, { reatt_n1_1_past_ln3_1_add_20_output.data->data});
}
void forward_lite_eltwise_reatt_n1_1_past_gelu3_1_Gelu_Pow0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_gelu3_1_Gelu_Pow0_output_array.data = AI_PTR(net_ctx->_activations[0] + 144000);
  reatt_n1_1_past_gelu3_1_Gelu_Pow0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 144000);
  _STAI_REATTN1C_EVENT_NODE_START_CB(397, 2, { reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_output.data->data,reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D.data->data});
  forward_eltwise(&reatt_n1_1_past_gelu3_1_Gelu_Pow0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(397, 1, { reatt_n1_1_past_gelu3_1_Gelu_Pow0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu3_1_Gelu_mul_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu3_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu3_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(402, 2, { reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu3_1_Gelu_mul_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(402, 1, { reatt_n1_1_past_gelu3_1_Gelu_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu3_1_Gelu_add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln3_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln3_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu3_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu3_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu3_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu3_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(405, 2, { reatt_n1_1_past_ln3_1_add_20_output.data->data,reatt_n1_1_past_gelu3_1_Gelu_mul_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu3_1_Gelu_add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(405, 1, { reatt_n1_1_past_gelu3_1_Gelu_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu3_1_Gelu_mul_20(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n1_1_past_gelu3_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu3_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu3_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu3_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(408, 2, { reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu3_1_Gelu_add0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu3_1_Gelu_mul_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(408, 1, { reatt_n1_1_past_gelu3_1_Gelu_mul_20_output.data->data});
}
void forward_lite_nl_integer_reatt_n1_1_past_gelu3_1_Gelu_Tanh0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu3_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu3_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(411, 1, { reatt_n1_1_past_gelu3_1_Gelu_mul_20_output.data->data});
  forward_nl_integer(&reatt_n1_1_past_gelu3_1_Gelu_Tanh0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(411, 1, { reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu3_1_Gelu_add_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu3_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu3_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(414, 2, { reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu3_1_Gelu_Tanh0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu3_1_Gelu_add_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(414, 1, { reatt_n1_1_past_gelu3_1_Gelu_add_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu3_1_Gelu_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n1_1_past_ln3_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln3_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu3_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu3_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(396, 2, { reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_ln3_1_add_20_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu3_1_Gelu_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(396, 1, { reatt_n1_1_past_gelu3_1_Gelu_mul0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu3_1_Gelu_mul_30(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu3_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu3_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu3_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu3_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu3_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu3_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(417, 2, { reatt_n1_1_past_gelu3_1_Gelu_mul0_output.data->data,reatt_n1_1_past_gelu3_1_Gelu_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu3_1_Gelu_mul_30_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(417, 1, { reatt_n1_1_past_gelu3_1_Gelu_mul_30_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_add3_1_Add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_add2_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_add2_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu3_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu3_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_add3_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_add3_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(420, 2, { reatt_n1_1_past_add2_1_Add0_output.data->data,reatt_n1_1_past_gelu3_1_Gelu_mul_30_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_add3_1_Add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(420, 1, { reatt_n1_1_past_add3_1_Add0_output.data->data});
}
void forward_lite_slice_reatt_n1_1_past_zpad4_1_strided_slice0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_add3_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_add3_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_zpad4_1_strided_slice0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_zpad4_1_strided_slice0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(423, 1, { reatt_n1_1_past_add3_1_Add0_output.data->data});
  forward_slice(&reatt_n1_1_past_zpad4_1_strided_slice0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(423, 1, { reatt_n1_1_past_zpad4_1_strided_slice0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_zpad4_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_zpad4_1_strided_slice0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_zpad4_1_strided_slice0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 4);
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 4);
  reatt_n1_1_past_zpad4_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_zpad4_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(426, 2, { reatt_n1_1_past_zpad4_1_strided_slice0_output.data->data,reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_zpad4_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(426, 1, { reatt_n1_1_past_zpad4_1_mul0_output.data->data});
}
void forward_lite_concat_reatt_n1_1_past_ccat4_1_concat0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_zpad4_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_zpad4_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_add3_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_add3_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_ccat4_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 233472);
  reatt_n1_1_past_ccat4_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 233472);
  _STAI_REATTN1C_EVENT_NODE_START_CB(429, 2, { reatt_n1_1_past_zpad4_1_mul0_output.data->data,reatt_n1_1_past_add3_1_Add0_output.data->data});
  forward_concat(&reatt_n1_1_past_ccat4_1_concat0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(429, 1, { reatt_n1_1_past_ccat4_1_concat0_output.data->data});
}
void forward_lite_transpose_reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ccat4_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 233472);
  reatt_n1_1_past_ccat4_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 233472);
  reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 201600);
  reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 201600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(432, 1, { reatt_n1_1_past_ccat4_1_concat0_output.data->data});
  forward_transpose(&reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(432, 1, { reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_dil4_1_BiasAdd0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  const_fold_opt__254_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 24);
  const_fold_opt__254_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 24);
  reatt_n1_1_past_dil4_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_dil4_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(444, 2, { reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_output0.data->data,const_fold_opt__254_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_dil4_1_BiasAdd0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(444, 1, { reatt_n1_1_past_dil4_1_BiasAdd0_output.data->data});
}
void forward_lite_reduce_reatt_n1_1_past_ln4_1_moments_mean0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln4_1_moments_mean0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln4_1_moments_mean0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(447, 1, { reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_output.data->data});
  forward_reduce(&reatt_n1_1_past_ln4_1_moments_mean0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(447, 1, { reatt_n1_1_past_ln4_1_moments_mean0_output.data->data});
}
void forward_lite_eltwise_reatt_n1_1_past_ln4_1_moments_SquaredDifference0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln4_1_moments_mean0_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln4_1_moments_mean0_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln4_1_moments_SquaredDifference0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln4_1_moments_SquaredDifference0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(455, 2, { reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_output.data->data,reatt_n1_1_past_ln4_1_moments_mean0_Mul_output.data->data});
  forward_eltwise(&reatt_n1_1_past_ln4_1_moments_SquaredDifference0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(455, 1, { reatt_n1_1_past_ln4_1_moments_SquaredDifference0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 201600);
  reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 201600);
  reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(459, 2, { reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_output.data->data,reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(459, 1, { reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_output.data->data});
}
void forward_lite_reduce_reatt_n1_1_past_ln4_1_moments_variance0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln4_1_moments_variance0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln4_1_moments_variance0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(462, 1, { reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_output.data->data});
  forward_reduce(&reatt_n1_1_past_ln4_1_moments_variance0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(462, 1, { reatt_n1_1_past_ln4_1_moments_variance0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  const_fold_opt__281_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 504);
  const_fold_opt__281_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 504);
  reatt_n1_1_past_ln4_1_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 29100);
  reatt_n1_1_past_ln4_1_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29100);
  _STAI_REATTN1C_EVENT_NODE_START_CB(465, 2, { reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_output.data->data,const_fold_opt__281_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln4_1_add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(465, 1, { reatt_n1_1_past_ln4_1_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln4_1_Reshape0_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 1468);
  reatt_n1_1_past_ln4_1_Reshape0_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 1468);
  reatt_n1_1_past_ln4_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln4_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(472, 2, { reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_output.data->data,reatt_n1_1_past_ln4_1_Reshape0_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln4_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(472, 1, { reatt_n1_1_past_ln4_1_mul0_output.data->data});
}
void forward_lite_transpose_Transpose__2150(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln4_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln4_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__2150_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  Transpose__2150_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(475, 1, { reatt_n1_1_past_ln4_1_mul0_output.data->data});
  forward_transpose(&Transpose__2150_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(475, 1, { Transpose__2150_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_mul_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30000);
  Transpose__2150_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  Transpose__2150_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln4_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln4_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(478, 2, { reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_output.data->data,Transpose__2150_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln4_1_mul_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(478, 1, { reatt_n1_1_past_ln4_1_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_add_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln4_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln4_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln4_1_Reshape_10_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 1564);
  reatt_n1_1_past_ln4_1_Reshape_10_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 1564);
  reatt_n1_1_past_ln4_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln4_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(484, 2, { reatt_n1_1_past_ln4_1_mul_10_output.data->data,reatt_n1_1_past_ln4_1_Reshape_10_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln4_1_add_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(484, 1, { reatt_n1_1_past_ln4_1_add_10_output.data->data});
}
void forward_lite_transpose_Transpose__1480(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil4_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_dil4_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  Transpose__1480_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__1480_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(448, 1, { reatt_n1_1_past_dil4_1_BiasAdd0_output.data->data});
  forward_transpose(&Transpose__1480_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(448, 1, { Transpose__1480_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_mul_20(_stai_reattn1c_context* net_ctx)
{
  Transpose__1480_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__1480_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__2150_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  Transpose__2150_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln4_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_ln4_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(479, 2, { Transpose__1480_output.data->data,Transpose__2150_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln4_1_mul_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(479, 1, { reatt_n1_1_past_ln4_1_mul_20_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_add_20(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln4_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_ln4_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_ln4_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln4_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln4_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln4_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(487, 2, { reatt_n1_1_past_ln4_1_mul_20_output.data->data,reatt_n1_1_past_ln4_1_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln4_1_add_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(487, 1, { reatt_n1_1_past_ln4_1_add_20_output.data->data});
}
void forward_lite_eltwise_reatt_n1_1_past_gelu4_1_Gelu_Pow0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_gelu4_1_Gelu_Pow0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu4_1_Gelu_Pow0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(491, 2, { reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_output.data->data,reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D.data->data});
  forward_eltwise(&reatt_n1_1_past_gelu4_1_Gelu_Pow0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(491, 1, { reatt_n1_1_past_gelu4_1_Gelu_Pow0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu4_1_Gelu_mul_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu4_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu4_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(496, 2, { reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu4_1_Gelu_mul_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(496, 1, { reatt_n1_1_past_gelu4_1_Gelu_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu4_1_Gelu_add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln4_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln4_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu4_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu4_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu4_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu4_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(499, 2, { reatt_n1_1_past_ln4_1_add_20_output.data->data,reatt_n1_1_past_gelu4_1_Gelu_mul_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu4_1_Gelu_add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(499, 1, { reatt_n1_1_past_gelu4_1_Gelu_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu4_1_Gelu_mul_20(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n1_1_past_gelu4_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu4_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu4_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu4_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(502, 2, { reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu4_1_Gelu_add0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu4_1_Gelu_mul_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(502, 1, { reatt_n1_1_past_gelu4_1_Gelu_mul_20_output.data->data});
}
void forward_lite_nl_integer_reatt_n1_1_past_gelu4_1_Gelu_Tanh0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu4_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu4_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(505, 1, { reatt_n1_1_past_gelu4_1_Gelu_mul_20_output.data->data});
  forward_nl_integer(&reatt_n1_1_past_gelu4_1_Gelu_Tanh0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(505, 1, { reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu4_1_Gelu_add_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu4_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu4_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(508, 2, { reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu4_1_Gelu_Tanh0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu4_1_Gelu_add_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(508, 1, { reatt_n1_1_past_gelu4_1_Gelu_add_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu4_1_Gelu_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n1_1_past_ln4_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln4_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu4_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu4_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(490, 2, { reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_ln4_1_add_20_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu4_1_Gelu_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(490, 1, { reatt_n1_1_past_gelu4_1_Gelu_mul0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu4_1_Gelu_mul_30(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu4_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu4_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu4_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu4_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu4_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu4_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(511, 2, { reatt_n1_1_past_gelu4_1_Gelu_mul0_output.data->data,reatt_n1_1_past_gelu4_1_Gelu_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu4_1_Gelu_mul_30_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(511, 1, { reatt_n1_1_past_gelu4_1_Gelu_mul_30_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_add4_1_Add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_add3_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_add3_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu4_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu4_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_add4_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_add4_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(514, 2, { reatt_n1_1_past_add3_1_Add0_output.data->data,reatt_n1_1_past_gelu4_1_Gelu_mul_30_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_add4_1_Add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(514, 1, { reatt_n1_1_past_add4_1_Add0_output.data->data});
}
void forward_lite_slice_reatt_n1_1_past_zpad5_1_strided_slice0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_add4_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_add4_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_zpad5_1_strided_slice0_output_array.data = AI_PTR(net_ctx->_activations[0] + 265344);
  reatt_n1_1_past_zpad5_1_strided_slice0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 265344);
  _STAI_REATTN1C_EVENT_NODE_START_CB(517, 1, { reatt_n1_1_past_add4_1_Add0_output.data->data});
  forward_slice(&reatt_n1_1_past_zpad5_1_strided_slice0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(517, 1, { reatt_n1_1_past_zpad5_1_strided_slice0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_zpad5_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_zpad5_1_strided_slice0_output_array.data = AI_PTR(net_ctx->_activations[0] + 265344);
  reatt_n1_1_past_zpad5_1_strided_slice0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 265344);
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 4);
  reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 4);
  reatt_n1_1_past_zpad5_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 271488);
  reatt_n1_1_past_zpad5_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 271488);
  _STAI_REATTN1C_EVENT_NODE_START_CB(520, 2, { reatt_n1_1_past_zpad5_1_strided_slice0_output.data->data,reatt_n1_1_past_zpad4_1_mul_y0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_zpad5_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(520, 1, { reatt_n1_1_past_zpad5_1_mul0_output.data->data});
}
void forward_lite_concat_reatt_n1_1_past_ccat5_1_concat0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_zpad5_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 271488);
  reatt_n1_1_past_zpad5_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 271488);
  reatt_n1_1_past_add4_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_add4_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_ccat5_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 236544);
  reatt_n1_1_past_ccat5_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 236544);
  _STAI_REATTN1C_EVENT_NODE_START_CB(523, 2, { reatt_n1_1_past_zpad5_1_mul0_output.data->data,reatt_n1_1_past_add4_1_Add0_output.data->data});
  forward_concat(&reatt_n1_1_past_ccat5_1_concat0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(523, 1, { reatt_n1_1_past_ccat5_1_concat0_output.data->data});
}
void forward_lite_transpose_reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ccat5_1_concat0_output_array.data = AI_PTR(net_ctx->_activations[0] + 236544);
  reatt_n1_1_past_ccat5_1_concat0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 236544);
  reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 201600);
  reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 201600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(526, 1, { reatt_n1_1_past_ccat5_1_concat0_output.data->data});
  forward_transpose(&reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(526, 1, { reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_dil5_1_BiasAdd0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  const_fold_opt__283_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 604);
  const_fold_opt__283_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 604);
  reatt_n1_1_past_dil5_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_dil5_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(538, 2, { reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_output0.data->data,const_fold_opt__283_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_dil5_1_BiasAdd0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(538, 1, { reatt_n1_1_past_dil5_1_BiasAdd0_output.data->data});
}
void forward_lite_reduce_reatt_n1_1_past_ln5_1_moments_mean0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln5_1_moments_mean0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln5_1_moments_mean0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(541, 1, { reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_output.data->data});
  forward_reduce(&reatt_n1_1_past_ln5_1_moments_mean0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(541, 1, { reatt_n1_1_past_ln5_1_moments_mean0_output.data->data});
}
void forward_lite_eltwise_reatt_n1_1_past_ln5_1_moments_SquaredDifference0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln5_1_moments_mean0_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln5_1_moments_mean0_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln5_1_moments_SquaredDifference0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln5_1_moments_SquaredDifference0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(549, 2, { reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_output.data->data,reatt_n1_1_past_ln5_1_moments_mean0_Mul_output.data->data});
  forward_eltwise(&reatt_n1_1_past_ln5_1_moments_SquaredDifference0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(549, 1, { reatt_n1_1_past_ln5_1_moments_SquaredDifference0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 201600);
  reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 201600);
  reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(553, 2, { reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_output.data->data,reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(553, 1, { reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_output.data->data});
}
void forward_lite_reduce_reatt_n1_1_past_ln5_1_moments_variance0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln5_1_moments_variance0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln5_1_moments_variance0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(556, 1, { reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_output.data->data});
  forward_reduce(&reatt_n1_1_past_ln5_1_moments_variance0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(556, 1, { reatt_n1_1_past_ln5_1_moments_variance0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  const_fold_opt__281_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 504);
  const_fold_opt__281_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 504);
  reatt_n1_1_past_ln5_1_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 29100);
  reatt_n1_1_past_ln5_1_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29100);
  _STAI_REATTN1C_EVENT_NODE_START_CB(559, 2, { reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_output.data->data,const_fold_opt__281_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln5_1_add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(559, 1, { reatt_n1_1_past_ln5_1_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln5_1_Reshape0_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 1660);
  reatt_n1_1_past_ln5_1_Reshape0_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 1660);
  reatt_n1_1_past_ln5_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln5_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(566, 2, { reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_output.data->data,reatt_n1_1_past_ln5_1_Reshape0_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln5_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(566, 1, { reatt_n1_1_past_ln5_1_mul0_output.data->data});
}
void forward_lite_transpose_Transpose__2210(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln5_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln5_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__2210_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  Transpose__2210_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(569, 1, { reatt_n1_1_past_ln5_1_mul0_output.data->data});
  forward_transpose(&Transpose__2210_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(569, 1, { Transpose__2210_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_mul_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 30000);
  reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30000);
  Transpose__2210_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  Transpose__2210_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln5_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln5_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(572, 2, { reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_output.data->data,Transpose__2210_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln5_1_mul_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(572, 1, { reatt_n1_1_past_ln5_1_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_add_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln5_1_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln5_1_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln5_1_Reshape_10_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 1756);
  reatt_n1_1_past_ln5_1_Reshape_10_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 1756);
  reatt_n1_1_past_ln5_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln5_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(578, 2, { reatt_n1_1_past_ln5_1_mul_10_output.data->data,reatt_n1_1_past_ln5_1_Reshape_10_DequantizeLinear_Output_const.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln5_1_add_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(578, 1, { reatt_n1_1_past_ln5_1_add_10_output.data->data});
}
void forward_lite_transpose_Transpose__1570(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_dil5_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_dil5_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  Transpose__1570_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__1570_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(542, 1, { reatt_n1_1_past_dil5_1_BiasAdd0_output.data->data});
  forward_transpose(&Transpose__1570_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(542, 1, { Transpose__1570_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_mul_20(_stai_reattn1c_context* net_ctx)
{
  Transpose__1570_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__1570_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  Transpose__2210_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  Transpose__2210_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln5_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_ln5_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(573, 2, { Transpose__1570_output.data->data,Transpose__2210_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln5_1_mul_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(573, 1, { reatt_n1_1_past_ln5_1_mul_20_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_add_20(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln5_1_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_ln5_1_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_ln5_1_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln5_1_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_ln5_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln5_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(581, 2, { reatt_n1_1_past_ln5_1_mul_20_output.data->data,reatt_n1_1_past_ln5_1_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_ln5_1_add_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(581, 1, { reatt_n1_1_past_ln5_1_add_20_output.data->data});
}
void forward_lite_eltwise_reatt_n1_1_past_gelu5_1_Gelu_Pow0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_gelu5_1_Gelu_Pow0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115200);
  reatt_n1_1_past_gelu5_1_Gelu_Pow0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115200);
  _STAI_REATTN1C_EVENT_NODE_START_CB(585, 2, { reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_output.data->data,reatt_n1_1_past_gelu4_1_Gelu_Pow_y0_3D.data->data});
  forward_eltwise(&reatt_n1_1_past_gelu5_1_Gelu_Pow0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(585, 1, { reatt_n1_1_past_gelu5_1_Gelu_Pow0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu5_1_Gelu_mul_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu5_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu5_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(590, 2, { reatt_n1_1_past_gelu4_1_Gelu_Cast_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu5_1_Gelu_mul_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(590, 1, { reatt_n1_1_past_gelu5_1_Gelu_mul_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu5_1_Gelu_add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_ln5_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln5_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu5_1_Gelu_mul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu5_1_Gelu_mul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu5_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu5_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(593, 2, { reatt_n1_1_past_ln5_1_add_20_output.data->data,reatt_n1_1_past_gelu5_1_Gelu_mul_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu5_1_Gelu_add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(593, 1, { reatt_n1_1_past_gelu5_1_Gelu_add0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu5_1_Gelu_mul_20(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n1_1_past_gelu5_1_Gelu_add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu5_1_Gelu_add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu5_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu5_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(596, 2, { reatt_n1_1_past_gelu3_1_Gelu_mul_2_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu5_1_Gelu_add0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu5_1_Gelu_mul_20_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(596, 1, { reatt_n1_1_past_gelu5_1_Gelu_mul_20_output.data->data});
}
void forward_lite_nl_integer_reatt_n1_1_past_gelu5_1_Gelu_Tanh0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu5_1_Gelu_mul_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu5_1_Gelu_mul_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(599, 1, { reatt_n1_1_past_gelu5_1_Gelu_mul_20_output.data->data});
  forward_nl_integer(&reatt_n1_1_past_gelu5_1_Gelu_Tanh0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(599, 1, { reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu5_1_Gelu_add_10(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 20);
  reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu5_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu5_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(602, 2, { reatt_n1_1_past_gelu1_1_Gelu_add_1_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_gelu5_1_Gelu_Tanh0_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu5_1_Gelu_add_10_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(602, 1, { reatt_n1_1_past_gelu5_1_Gelu_add_10_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu5_1_Gelu_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 16);
  reatt_n1_1_past_ln5_1_add_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_ln5_1_add_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu5_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu5_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(584, 2, { reatt_n1_1_past_gelu2_1_Gelu_mul_x0_DequantizeLinear_Output_const_3D.data->data,reatt_n1_1_past_ln5_1_add_20_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu5_1_Gelu_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(584, 1, { reatt_n1_1_past_gelu5_1_Gelu_mul0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu5_1_Gelu_mul_30(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_gelu5_1_Gelu_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu5_1_Gelu_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_gelu5_1_Gelu_add_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu5_1_Gelu_add_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_gelu5_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu5_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_REATTN1C_EVENT_NODE_START_CB(605, 2, { reatt_n1_1_past_gelu5_1_Gelu_mul0_output.data->data,reatt_n1_1_past_gelu5_1_Gelu_add_10_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_gelu5_1_Gelu_mul_30_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(605, 1, { reatt_n1_1_past_gelu5_1_Gelu_mul_30_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_add5_1_Add0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_add4_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_add4_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_gelu5_1_Gelu_mul_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_gelu5_1_Gelu_mul_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_add5_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_add5_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  _STAI_REATTN1C_EVENT_NODE_START_CB(608, 2, { reatt_n1_1_past_add4_1_Add0_output.data->data,reatt_n1_1_past_gelu5_1_Gelu_mul_30_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_add5_1_Add0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(608, 1, { reatt_n1_1_past_add5_1_Add0_output.data->data});
}
void forward_lite_transpose_reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_add5_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_add5_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(614, 1, { reatt_n1_1_past_add5_1_Add0_output.data->data});
  forward_transpose(&reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(614, 1, { reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output.data->data});
}
void forward_lite_conv2d_integer_SSSA_reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_weights_array.data = AI_PTR(net_ctx->_weights[0] + 584292);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 584292);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_bias_array.data = AI_PTR(net_ctx->_weights[0] + 584388);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 584388);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_output_array.data = AI_PTR(net_ctx->_activations[0] + 57984);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57984);
  _STAI_REATTN1C_EVENT_NODE_START_CB(614, 1, { reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose_output.data->data});
  forward_conv2d_integer_SSSA(&reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(614, 1, { reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_pool_1_mul0(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_add5_1_Add0_output_array.data = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_add5_1_Add0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 28800);
  reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_pool_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 57600);
  reatt_n1_1_past_pool_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 57600);
  _STAI_REATTN1C_EVENT_NODE_START_CB(626, 2, { reatt_n1_1_past_add5_1_Add0_output.data->data,reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_pool_1_mul0_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(626, 1, { reatt_n1_1_past_pool_1_mul0_output.data->data});
}
void forward_lite_reduce_past_pool(_stai_reattn1c_context* net_ctx)
{
  reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  past_pool_output_array.data = AI_PTR(net_ctx->_outputs[0] + 0);
  past_pool_output_array.data_start = AI_PTR(net_ctx->_outputs[0] + 0);
  _STAI_REATTN1C_EVENT_NODE_START_CB(629, 1, { reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output.data->data});
  forward_reduce(&past_pool_layer);
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(629, 1, { past_pool_output.data->data});
}

/*****************************************************************************/


static const ai_u32 hist_input_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3300;
static const ai_float hist_input_0_conversion_t_out_0_fmt_scale_const_f32 = 0.05808529257774353f;
static const ai_i8 hist_input_0_conversion_t_out_0_fmt_zero_const_s8 = -35;

static const ai_u16 reatt_n1_1_past_proj_1_convolution0_t_in_0_shape_w_const_u16 = 300;
static const ai_u16 reatt_n1_1_past_proj_1_convolution0_t_in_0_shape_h_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_proj_1_convolution0_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_proj_1_convolution0_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_proj_1_convolution0_t_in_0_shape_ch_const_u16 = 11;
static const ai_u16 reatt_n1_1_past_proj_1_convolution0_t_out_0_shape_ch_const_u16 = 96;
static const ai_i8 reatt_n1_1_past_proj_1_convolution0_t_in_0_fmt_zero_const_s8 = -35;
static const ai_i8 reatt_n1_1_past_proj_1_convolution0_t_out_0_fmt_zero_const_s8 = 20;
static const ai_float reatt_n1_1_past_proj_1_convolution0_t_in_0_fmt_scale_const_f32 = 0.05808529257774353f;
static const ai_float reatt_n1_1_past_proj_1_convolution0_t_out_0_fmt_scale_const_f32 = 0.026346741244196892f;
static const ai_float reatt_n1_1_past_proj_1_convolution0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002573152305558324f, 0.001831245026551187f, 0.0023619369603693485f, 0.001438189297914505f, 0.0021014364901930094f, 0.002098512602970004f, 0.0028967345133423805f, 0.002028915099799633f, 0.0017924036365002394f, 0.0020934329368174076f, 0.0022525573149323463f, 0.0023134425282478333f, 0.002351561561226845f, 0.0025658803060650826f, 0.0036548126954585314f, 0.00211802264675498f, 0.002114620991051197f, 0.0017336738528683782f, 0.00208779308013618f, 0.002625563880428672f, 0.00221093837171793f, 0.0021604523062705994f, 0.0019027299713343382f, 0.001537762931548059f, 0.0020083002746105194f, 0.002411778550595045f, 0.001516424585133791f, 0.0016373730031773448f, 0.0018837809329852462f, 0.0018104711780324578f, 0.0019734457600861788f, 0.001501387683674693f, 0.0022037948947399855f, 0.0017139086266979575f, 0.001445080153644085f, 0.0023195622488856316f, 0.0020966765005141497f, 0.0015304730040952563f, 0.002471430692821741f, 0.00230040424503386f, 0.0018529612571001053f, 0.001994691789150238f, 0.0017827333649620414f, 0.0025679010432213545f, 0.0019034961005672812f, 0.0016375863924622536f, 0.003381185932084918f, 0.0021105012856423855f, 0.00282777426764369f, 0.002259559463709593f, 0.002578870626166463f, 0.002348381094634533f, 0.0016436039004474878f, 0.002868903335183859f, 0.002187540987506509f, 0.0023888698779046535f, 0.001679181237705052f, 0.002199763199314475f, 0.0012539552990347147f, 0.002022341825067997f, 0.0026990207843482494f, 0.0020114919170737267f, 0.0027447722386568785f, 0.0027551152743399143f, 0.0017711377004161477f, 0.002914584008976817f, 0.0022841773461550474f, 0.002070824848487973f, 0.0018484470201656222f, 0.0025759499985724688f, 0.00184347084723413f, 0.002021443098783493f, 0.0015813085483387113f, 0.0020072762854397297f, 0.0021208240650594234f, 0.0024445028975605965f, 0.002256720792502165f, 0.0020805583335459232f, 0.0024184638168662786f, 0.0022293191868811846f, 0.002514110878109932f, 0.00225131050683558f, 0.002880356041714549f, 0.0017946148291230202f, 0.0016811141977086663f, 0.0035220265854150057f, 0.0017366573447361588f, 0.001750490046106279f, 0.0020826158579438925f, 0.002137787640094757f, 0.002487792866304517f, 0.0021160501055419445f, 0.0015549364034086466f, 0.002247617579996586f, 0.001402427675202489f, 0.0025735110975801945f);
static const ai_layer_format_type reatt_n1_1_past_proj_1_convolution0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;






static const ai_u16 reatt_n1_1_past_dil0_1_convolution0_t_in_0_shape_w_const_u16 = 302;
static const ai_u16 reatt_n1_1_past_dil0_1_convolution0_t_in_0_shape_h_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil0_1_convolution0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 reatt_n1_1_past_dil0_1_convolution0_t_out_0_shape_ch_const_u16 = 96;
static const ai_u16 reatt_n1_1_past_dil0_1_convolution0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 reatt_n1_1_past_dil0_1_convolution0_t_weight_0_shape_h_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil0_1_convolution0_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil0_1_convolution0_l_stride_0_const_u16 = 1;
static const ai_i8 reatt_n1_1_past_dil0_1_convolution0_t_in_0_fmt_zero_const_s8 = 18;
static const ai_i8 reatt_n1_1_past_dil0_1_convolution0_t_out_0_fmt_zero_const_s8 = -7;
static const ai_float reatt_n1_1_past_dil0_1_convolution0_t_in_0_fmt_scale_const_f32 = 0.026002241298556328f;
static const ai_float reatt_n1_1_past_dil0_1_convolution0_t_out_0_fmt_scale_const_f32 = 0.18365514278411865f;
static const ai_float reatt_n1_1_past_dil0_1_convolution0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0023695980198681355f, 0.001138285850174725f, 0.0011178472777828574f, 0.001327855745330453f, 0.0015449487837031484f, 0.0020215536933392286f, 0.002004977548494935f, 0.0016141735250130296f, 0.0013454767176881433f, 0.0013692168286070228f, 0.00111877778545022f, 0.0013179086381569505f, 0.001263354322873056f, 0.0011402844684198499f, 0.002021732972934842f, 0.0011157990666106343f, 0.0022293287329375744f, 0.0016404928173869848f, 0.0021713695023208857f, 0.0021254182793200016f, 0.0013254678342491388f, 0.001844255719333887f, 0.0013868131209164858f, 0.0019101161742582917f, 0.0017861771630123258f, 0.0018880540737882257f, 0.0010441348422318697f, 0.0016951564466580749f, 0.0018999461317434907f, 0.001714745070785284f, 0.001891333726234734f, 0.0015468945493921638f, 0.001246777712367475f, 0.0016675711376592517f, 0.0015999986790120602f, 0.0028059000615030527f, 0.0018664209637790918f, 0.0014455490745604038f, 0.001666313037276268f, 0.0014570459024980664f, 0.002073865383863449f, 0.001752370735630393f, 0.00139252794906497f, 0.0016885094810277224f, 0.0017511392943561077f, 0.0017718190792948008f, 0.0015089825028553605f, 0.0019506416283547878f, 0.0016220585675910115f, 0.0017439298098906875f, 0.001854192465543747f, 0.001754100201651454f, 0.0016141640953719616f, 0.001113007077947259f, 0.0012647116091102362f, 0.001253509079106152f, 0.0014260733732953668f, 0.0021885584574192762f, 0.002438570139929652f, 0.001976157771423459f, 0.001527811517007649f, 0.0015349870081990957f, 0.0012756962096318603f, 0.0012878745328634977f, 0.0017452772008255124f, 0.0017075126525014639f, 0.0017520374385640025f, 0.0017457092180848122f, 0.0021468799095600843f, 0.0012838911497965455f, 0.001518797711469233f, 0.0016144471010193229f, 0.0021436060778796673f, 0.001613799948245287f, 0.001311848871409893f, 0.0019097501644864678f, 0.0017804496455937624f, 0.001165414578281343f, 0.001411635079421103f, 0.0015250261640176177f, 0.0021000488195568323f, 0.002071486786007881f, 0.0017057667719200253f, 0.0018471803050488234f, 0.00224545830860734f, 0.0016855326248332858f, 0.0012671579606831074f, 0.0015117647126317024f, 0.0014536682283505797f, 0.0015919841825962067f, 0.0015540637541562319f, 0.0019359817961230874f, 0.0013956979382783175f, 0.0019185257842764258f, 0.0018845511367544532f, 0.001524580642580986f);
static const ai_layer_format_type reatt_n1_1_past_dil0_1_convolution0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 reatt_n1_1_past_dil0_1_convolution0_t_out_0_shape_w_const_u16 = 300;
static const ai_u16 reatt_n1_1_past_dil0_1_convolution0_t_out_0_shape_h_const_u16 = 1;


static const ai_u32 reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_t_in_0_fmt_scale_const_f32 = 0.18389278650283813f;
static const ai_i8 reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_t_in_0_fmt_zero_const_s8 = -7;




static const ai_u32 reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_t_out_0_fmt_scale_const_f32 = 0.18280307948589325f;
static const ai_i8 reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_t_out_0_fmt_zero_const_s8 = -4;


static const ai_u32 reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_t_in_0_fmt_scale_const_f32 = 2.265094041824341f;
static const ai_i8 reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_t_in_0_fmt_zero_const_s8 = -128;



static const ai_u32 reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_t_out_0_fmt_scale_const_f32 = 0.3046870231628418f;
static const ai_i8 reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_u32 reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32 = 0.3046909272670746f;
static const ai_i8 reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_i32 reatt_n1_1_past_ln0_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_i32 reatt_n1_1_past_ln0_1_Rsqrt__220_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_t_out_0_fmt_scale_const_f32 = 0.007880622521042824f;
static const ai_i8 reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_t_out_0_fmt_zero_const_s8 = -128;



static const ai_i32 reatt_n1_1_past_ln0_1_Neg0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 0.004083497449755669f;
static const ai_i8 reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_t_out_0_fmt_zero_const_s8 = 23;






static const ai_u32 reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32 = 0.03867676854133606f;
static const ai_i8 reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8 = -1;


static const ai_u32 reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 0.9405356645584106f;
static const ai_i8 reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8 = -1;













static const ai_u32 reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 29184;
static const ai_float reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion_t_in_0_fmt_scale_const_f32 = 0.028742823749780655f;
static const ai_i8 reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion_t_in_0_fmt_zero_const_s8 = -35;

static const ai_u32 reatt_n1_1_past_dil1_1_convolution0_t_in_0_shape_ch_const_u32 = 96;
static const ai_u32 reatt_n1_1_past_dil1_1_convolution0_t_out_0_shape_ch_const_u32 = 96;
static const ai_u32 reatt_n1_1_past_dil1_1_convolution0_t_in_0_shape_w_const_u32 = 304;
static const ai_u32 reatt_n1_1_past_dil1_1_convolution0_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 reatt_n1_1_past_dil1_1_convolution0_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 reatt_n1_1_past_dil1_1_convolution0_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 reatt_n1_1_past_dil1_1_convolution0_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 reatt_n1_1_past_dil1_1_convolution0_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 reatt_n1_1_past_dil1_1_convolution0_l_pad_W_0_const_s32 = 0;
static const ai_i32 reatt_n1_1_past_dil1_1_convolution0_l_pad_H_0_const_s32 = 0;
static const ai_u16 reatt_n1_1_past_dil1_1_convolution0_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil1_1_convolution0_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil1_1_convolution0_l_dilation_W_const_u16 = 2;
static const ai_u16 reatt_n1_1_past_dil1_1_convolution0_l_dilation_H_const_u16 = 1;
static const ai_size reatt_n1_1_past_dil1_1_convolution0_v_n_groups_const_size = 1;

static const ai_u32 reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_t_out_0_fmt_scale_const_f32 = 0.16143950819969177f;
static const ai_i8 reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_t_out_0_fmt_zero_const_s8 = -31;


static const ai_u32 reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_t_in_0_fmt_scale_const_f32 = 0.16178633272647858f;
static const ai_i8 reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_t_in_0_fmt_zero_const_s8 = -31;




static const ai_u32 reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_t_out_0_fmt_scale_const_f32 = 0.16178633272647858f;
static const ai_i8 reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_t_out_0_fmt_zero_const_s8 = -30;


static const ai_u32 reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_t_in_0_fmt_scale_const_f32 = 2.519399642944336f;
static const ai_i8 reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_t_in_0_fmt_zero_const_s8 = -128;



static const ai_u32 reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_t_out_0_fmt_scale_const_f32 = 0.17308975756168365f;
static const ai_i8 reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_u32 reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32 = 0.17309367656707764f;
static const ai_i8 reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_i32 reatt_n1_1_past_ln1_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_i32 reatt_n1_1_past_ln1_1_Rsqrt__330_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_t_out_0_fmt_scale_const_f32 = 0.005884362850338221f;
static const ai_i8 reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_t_out_0_fmt_zero_const_s8 = -128;



static const ai_i32 reatt_n1_1_past_ln1_1_Neg0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 0.0048790681175887585f;
static const ai_i8 reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_t_out_0_fmt_zero_const_s8 = 8;






static const ai_u32 reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32 = 0.03905080631375313f;
static const ai_i8 reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8 = -4;


static const ai_u32 reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 0.9702898859977722f;
static const ai_i8 reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8 = -11;













static const ai_u32 reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 29568;
static const ai_float reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion_t_in_0_fmt_scale_const_f32 = 0.04640093073248863f;
static const ai_i8 reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion_t_in_0_fmt_zero_const_s8 = -69;

static const ai_u32 reatt_n1_1_past_dil2_1_convolution0_t_in_0_shape_ch_const_u32 = 96;
static const ai_u32 reatt_n1_1_past_dil2_1_convolution0_t_out_0_shape_ch_const_u32 = 96;
static const ai_u32 reatt_n1_1_past_dil2_1_convolution0_t_in_0_shape_w_const_u32 = 308;
static const ai_u32 reatt_n1_1_past_dil2_1_convolution0_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 reatt_n1_1_past_dil2_1_convolution0_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 reatt_n1_1_past_dil2_1_convolution0_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 reatt_n1_1_past_dil2_1_convolution0_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 reatt_n1_1_past_dil2_1_convolution0_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 reatt_n1_1_past_dil2_1_convolution0_l_pad_W_0_const_s32 = 0;
static const ai_i32 reatt_n1_1_past_dil2_1_convolution0_l_pad_H_0_const_s32 = 0;
static const ai_u16 reatt_n1_1_past_dil2_1_convolution0_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil2_1_convolution0_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil2_1_convolution0_l_dilation_W_const_u16 = 4;
static const ai_u16 reatt_n1_1_past_dil2_1_convolution0_l_dilation_H_const_u16 = 1;
static const ai_size reatt_n1_1_past_dil2_1_convolution0_v_n_groups_const_size = 1;

static const ai_u32 reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_t_out_0_fmt_scale_const_f32 = 0.19326896965503693f;
static const ai_i8 reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_t_out_0_fmt_zero_const_s8 = 13;


static const ai_u32 reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_t_in_0_fmt_scale_const_f32 = 0.19345253705978394f;
static const ai_i8 reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_t_in_0_fmt_zero_const_s8 = 13;




static const ai_u32 reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_t_out_0_fmt_scale_const_f32 = 0.19694237411022186f;
static const ai_i8 reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_t_out_0_fmt_zero_const_s8 = 12;


static const ai_u32 reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_t_in_0_fmt_scale_const_f32 = 2.9768118858337402f;
static const ai_i8 reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_t_in_0_fmt_zero_const_s8 = -128;



static const ai_u32 reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_t_out_0_fmt_scale_const_f32 = 0.2259257435798645f;
static const ai_i8 reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_u32 reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32 = 0.22592966258525848f;
static const ai_i8 reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_i32 reatt_n1_1_past_ln2_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_i32 reatt_n1_1_past_ln2_1_Rsqrt__440_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_t_out_0_fmt_scale_const_f32 = 0.004253217484802008f;
static const ai_i8 reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_t_out_0_fmt_zero_const_s8 = -128;



static const ai_i32 reatt_n1_1_past_ln2_1_Neg0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 0.007604237645864487f;
static const ai_i8 reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_t_out_0_fmt_zero_const_s8 = -21;






static const ai_u32 reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32 = 0.04316610470414162f;
static const ai_i8 reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8 = -3;


static const ai_u32 reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 1.3092845678329468f;
static const ai_i8 reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8 = -9;













static const ai_u32 reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 30336;
static const ai_float reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion_t_in_0_fmt_scale_const_f32 = 0.0630774274468422f;
static const ai_i8 reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion_t_in_0_fmt_zero_const_s8 = -84;

static const ai_u32 reatt_n1_1_past_dil3_1_convolution0_t_in_0_shape_ch_const_u32 = 96;
static const ai_u32 reatt_n1_1_past_dil3_1_convolution0_t_out_0_shape_ch_const_u32 = 96;
static const ai_u32 reatt_n1_1_past_dil3_1_convolution0_t_in_0_shape_w_const_u32 = 316;
static const ai_u32 reatt_n1_1_past_dil3_1_convolution0_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 reatt_n1_1_past_dil3_1_convolution0_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 reatt_n1_1_past_dil3_1_convolution0_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 reatt_n1_1_past_dil3_1_convolution0_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 reatt_n1_1_past_dil3_1_convolution0_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 reatt_n1_1_past_dil3_1_convolution0_l_pad_W_0_const_s32 = 0;
static const ai_i32 reatt_n1_1_past_dil3_1_convolution0_l_pad_H_0_const_s32 = 0;
static const ai_u16 reatt_n1_1_past_dil3_1_convolution0_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil3_1_convolution0_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil3_1_convolution0_l_dilation_W_const_u16 = 8;
static const ai_u16 reatt_n1_1_past_dil3_1_convolution0_l_dilation_H_const_u16 = 1;
static const ai_size reatt_n1_1_past_dil3_1_convolution0_v_n_groups_const_size = 1;

static const ai_u32 reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_t_out_0_fmt_scale_const_f32 = 0.2866164743900299f;
static const ai_i8 reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_t_out_0_fmt_zero_const_s8 = -11;


static const ai_u32 reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_t_in_0_fmt_scale_const_f32 = 0.28711163997650146f;
static const ai_i8 reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_t_in_0_fmt_zero_const_s8 = -11;




static const ai_u32 reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_t_out_0_fmt_scale_const_f32 = 0.2859433889389038f;
static const ai_i8 reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_t_out_0_fmt_zero_const_s8 = -10;


static const ai_u32 reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_t_in_0_fmt_scale_const_f32 = 6.038126468658447f;
static const ai_i8 reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_t_in_0_fmt_zero_const_s8 = -128;



static const ai_u32 reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_t_out_0_fmt_scale_const_f32 = 0.45151495933532715f;
static const ai_i8 reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_u32 reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32 = 0.45151886343955994f;
static const ai_i8 reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_i32 reatt_n1_1_past_ln3_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_i32 reatt_n1_1_past_ln3_1_Rsqrt__550_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_t_out_0_fmt_scale_const_f32 = 0.0022355897817760706f;
static const ai_i8 reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_t_out_0_fmt_zero_const_s8 = -128;



static const ai_i32 reatt_n1_1_past_ln3_1_Neg0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 0.00488232122734189f;
static const ai_i8 reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_t_out_0_fmt_zero_const_s8 = 76;






static const ai_u32 reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32 = 0.04476398229598999f;
static const ai_i8 reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8 = -5;


static const ai_u32 reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 1.4625608921051025f;
static const ai_i8 reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8 = -13;













static const ai_u32 reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 31872;
static const ai_float reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion_t_in_0_fmt_scale_const_f32 = 0.06920965015888214f;
static const ai_i8 reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion_t_in_0_fmt_zero_const_s8 = -85;

static const ai_u32 reatt_n1_1_past_dil4_1_convolution0_t_in_0_shape_ch_const_u32 = 96;
static const ai_u32 reatt_n1_1_past_dil4_1_convolution0_t_out_0_shape_ch_const_u32 = 96;
static const ai_u32 reatt_n1_1_past_dil4_1_convolution0_t_in_0_shape_w_const_u32 = 332;
static const ai_u32 reatt_n1_1_past_dil4_1_convolution0_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 reatt_n1_1_past_dil4_1_convolution0_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 reatt_n1_1_past_dil4_1_convolution0_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 reatt_n1_1_past_dil4_1_convolution0_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 reatt_n1_1_past_dil4_1_convolution0_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 reatt_n1_1_past_dil4_1_convolution0_l_pad_W_0_const_s32 = 0;
static const ai_i32 reatt_n1_1_past_dil4_1_convolution0_l_pad_H_0_const_s32 = 0;
static const ai_u16 reatt_n1_1_past_dil4_1_convolution0_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil4_1_convolution0_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil4_1_convolution0_l_dilation_W_const_u16 = 16;
static const ai_u16 reatt_n1_1_past_dil4_1_convolution0_l_dilation_H_const_u16 = 1;
static const ai_size reatt_n1_1_past_dil4_1_convolution0_v_n_groups_const_size = 1;

static const ai_u32 reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_t_out_0_fmt_scale_const_f32 = 0.2449152171611786f;
static const ai_i8 reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_t_out_0_fmt_zero_const_s8 = 2;


static const ai_u32 reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_t_in_0_fmt_scale_const_f32 = 0.2451942414045334f;
static const ai_i8 reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_t_in_0_fmt_zero_const_s8 = 2;




static const ai_u32 reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_t_out_0_fmt_scale_const_f32 = 0.24388989806175232f;
static const ai_i8 reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_t_out_0_fmt_zero_const_s8 = 3;


static const ai_u32 reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_t_in_0_fmt_scale_const_f32 = 4.016427516937256f;
static const ai_i8 reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_t_in_0_fmt_zero_const_s8 = -128;



static const ai_u32 reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_t_out_0_fmt_scale_const_f32 = 0.30485841631889343f;
static const ai_i8 reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_u32 reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32 = 0.3048623502254486f;
static const ai_i8 reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_i32 reatt_n1_1_past_ln4_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_i32 reatt_n1_1_past_ln4_1_Rsqrt__660_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_t_out_0_fmt_scale_const_f32 = 0.0017484892159700394f;
static const ai_i8 reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_t_out_0_fmt_zero_const_s8 = -128;



static const ai_i32 reatt_n1_1_past_ln4_1_Neg0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 0.009281291626393795f;
static const ai_i8 reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_t_out_0_fmt_zero_const_s8 = 33;






static const ai_u32 reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32 = 0.04702066630125046f;
static const ai_i8 reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8 = 6;


static const ai_u32 reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 1.7044577598571777f;
static const ai_i8 reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8 = 20;













static const ai_u32 reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 34944;
static const ai_float reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion_t_in_0_fmt_scale_const_f32 = 0.08496036380529404f;
static const ai_i8 reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion_t_in_0_fmt_zero_const_s8 = -91;

static const ai_u32 reatt_n1_1_past_dil5_1_convolution0_t_in_0_shape_ch_const_u32 = 96;
static const ai_u32 reatt_n1_1_past_dil5_1_convolution0_t_out_0_shape_ch_const_u32 = 96;
static const ai_u32 reatt_n1_1_past_dil5_1_convolution0_t_in_0_shape_w_const_u32 = 364;
static const ai_u32 reatt_n1_1_past_dil5_1_convolution0_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 reatt_n1_1_past_dil5_1_convolution0_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 reatt_n1_1_past_dil5_1_convolution0_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 reatt_n1_1_past_dil5_1_convolution0_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 reatt_n1_1_past_dil5_1_convolution0_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 reatt_n1_1_past_dil5_1_convolution0_l_pad_W_0_const_s32 = 0;
static const ai_i32 reatt_n1_1_past_dil5_1_convolution0_l_pad_H_0_const_s32 = 0;
static const ai_u16 reatt_n1_1_past_dil5_1_convolution0_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil5_1_convolution0_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n1_1_past_dil5_1_convolution0_l_dilation_W_const_u16 = 32;
static const ai_u16 reatt_n1_1_past_dil5_1_convolution0_l_dilation_H_const_u16 = 1;
static const ai_size reatt_n1_1_past_dil5_1_convolution0_v_n_groups_const_size = 1;

static const ai_u32 reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_t_out_0_fmt_scale_const_f32 = 0.2834649085998535f;
static const ai_i8 reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_t_out_0_fmt_zero_const_s8 = -12;


static const ai_u32 reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_t_in_0_fmt_scale_const_f32 = 0.284013032913208f;
static const ai_i8 reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_t_in_0_fmt_zero_const_s8 = -12;




static const ai_u32 reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_t_out_0_fmt_scale_const_f32 = 0.2814463675022125f;
static const ai_i8 reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_t_out_0_fmt_zero_const_s8 = -9;


static const ai_u32 reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_t_in_0_fmt_scale_const_f32 = 5.758757591247559f;
static const ai_i8 reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_t_in_0_fmt_zero_const_s8 = -128;



static const ai_u32 reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_t_out_0_fmt_scale_const_f32 = 0.6730678677558899f;
static const ai_i8 reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_u32 reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32 = 0.6730718016624451f;
static const ai_i8 reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_i32 reatt_n1_1_past_ln5_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_i32 reatt_n1_1_past_ln5_1_Rsqrt__770_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_t_out_0_fmt_scale_const_f32 = 0.0014387157279998064f;
static const ai_i8 reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_t_out_0_fmt_zero_const_s8 = -128;



static const ai_i32 reatt_n1_1_past_ln5_1_Neg0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 0.010783279314637184f;
static const ai_i8 reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_t_out_0_fmt_zero_const_s8 = 82;






static const ai_u32 reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32 = 0.04370545968413353f;
static const ai_i8 reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8 = -20;


static const ai_u32 reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32 = 1.4511957168579102f;
static const ai_i8 reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8 = -55;











static const ai_u32 reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_fmt_scale_const_f32 = 0.4802679419517517f;
static const ai_i8 reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_fmt_zero_const_s8 = 6;

static const ai_i32 reatt_n1_1_past_pool_1_Softmax0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_fmt_scale_const_f32 = 0.003921568859368563f;
static const ai_i8 reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_u32 reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_fmt_scale_const_f32 = 0.06137390807271004f;
static const ai_i8 reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_fmt_zero_const_s8 = -99;

STAI_API_ENTRY
stai_return_code stai_reattn1c_run(
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


  /* LITE_KERNEL_SECTION BEGIN hist_input_0_conversion */
  {
      const ai_float* hist_input_0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_inputs[0] + 0);
    ai_i8* hist_input_0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(0, 1, {(stai_ptr) hist_input_0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(hist_input_0_conversion_t_in_0_ptr_const_f32, hist_input_0_conversion_t_out_0_ptr_s8, hist_input_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, hist_input_0_conversion_t_out_0_fmt_scale_const_f32, hist_input_0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(0, 1, {(stai_ptr) hist_input_0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END hist_input_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_proj_1_convolution0 */
  {
      const ai_i8* reatt_n1_1_past_proj_1_convolution0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
    const ai_i8* reatt_n1_1_past_proj_1_convolution0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 1852);
    const ai_i32* reatt_n1_1_past_proj_1_convolution0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 2908);
    ai_i8* reatt_n1_1_past_proj_1_convolution0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 57600);
    ai_i16* reatt_n1_1_past_proj_1_convolution0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 89700);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(38, 1, {(stai_ptr) reatt_n1_1_past_proj_1_convolution0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(reatt_n1_1_past_proj_1_convolution0_t_in_0_ptr_const_s8, reatt_n1_1_past_proj_1_convolution0_t_in_0_shape_w_const_u16, reatt_n1_1_past_proj_1_convolution0_t_in_0_shape_h_const_u16, reatt_n1_1_past_proj_1_convolution0_l_stride_1_const_u16, reatt_n1_1_past_proj_1_convolution0_l_stride_0_const_u16, reatt_n1_1_past_proj_1_convolution0_t_in_0_shape_ch_const_u16, reatt_n1_1_past_proj_1_convolution0_t_weight_0_ptr_const_s8, reatt_n1_1_past_proj_1_convolution0_t_out_0_shape_ch_const_u16, reatt_n1_1_past_proj_1_convolution0_t_weight_1_ptr_const_s32, reatt_n1_1_past_proj_1_convolution0_t_in_0_fmt_zero_const_s8, reatt_n1_1_past_proj_1_convolution0_t_out_0_fmt_zero_const_s8, reatt_n1_1_past_proj_1_convolution0_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_proj_1_convolution0_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_proj_1_convolution0_t_weight_0_fmt_scale_const_f32, reatt_n1_1_past_proj_1_convolution0_l_out_ch_format_const_layer_format_type, reatt_n1_1_past_proj_1_convolution0_t_out_0_ptr_s8, 1, 768, reatt_n1_1_past_proj_1_convolution0_t_scratch_0_ptr_s16);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(38, 1, {(stai_ptr) reatt_n1_1_past_proj_1_convolution0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_proj_1_convolution0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_proj_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_proj_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_proj_1_BiasAdd0 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__890 */
  {
    
  forward_lite_transpose_Transpose__890(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__890 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_zpad0_1_strided_slice0 */
  {
    
  forward_lite_slice_reatt_n1_1_past_zpad0_1_strided_slice0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_zpad0_1_strided_slice0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_zpad0_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_zpad0_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_zpad0_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ccat0_1_concat0 */
  {
    
  forward_lite_concat_reatt_n1_1_past_ccat0_1_concat0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ccat0_1_concat0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil0_1_convolution0 */
  {
      const ai_i8* reatt_n1_1_past_dil0_1_convolution0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 115200);
    const ai_i8* reatt_n1_1_past_dil0_1_convolution0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 3292);
    const ai_i32* reatt_n1_1_past_dil0_1_convolution0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 2908);
    ai_i8* reatt_n1_1_past_dil0_1_convolution0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
    ai_i16* reatt_n1_1_past_dil0_1_convolution0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 55680);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(62, 1, {(stai_ptr) reatt_n1_1_past_dil0_1_convolution0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_sssa8_ch(reatt_n1_1_past_dil0_1_convolution0_t_in_0_ptr_const_s8, reatt_n1_1_past_dil0_1_convolution0_t_in_0_shape_w_const_u16, reatt_n1_1_past_dil0_1_convolution0_t_in_0_shape_h_const_u16, reatt_n1_1_past_dil0_1_convolution0_t_in_0_shape_ch_const_u16, reatt_n1_1_past_dil0_1_convolution0_t_weight_0_ptr_const_s8, reatt_n1_1_past_dil0_1_convolution0_t_out_0_shape_ch_const_u16, reatt_n1_1_past_dil0_1_convolution0_t_weight_0_shape_w_const_u16, reatt_n1_1_past_dil0_1_convolution0_t_weight_0_shape_h_const_u16, reatt_n1_1_past_dil0_1_convolution0_l_stride_1_const_u16, reatt_n1_1_past_dil0_1_convolution0_l_stride_0_const_u16, reatt_n1_1_past_dil0_1_convolution0_t_weight_1_ptr_const_s32, reatt_n1_1_past_dil0_1_convolution0_t_in_0_fmt_zero_const_s8, reatt_n1_1_past_dil0_1_convolution0_t_out_0_fmt_zero_const_s8, reatt_n1_1_past_dil0_1_convolution0_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_dil0_1_convolution0_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_dil0_1_convolution0_t_weight_0_fmt_scale_const_f32, reatt_n1_1_past_dil0_1_convolution0_l_out_ch_format_const_layer_format_type, reatt_n1_1_past_dil0_1_convolution0_t_out_0_ptr_s8, reatt_n1_1_past_dil0_1_convolution0_t_out_0_shape_w_const_u16, reatt_n1_1_past_dil0_1_convolution0_t_out_0_shape_h_const_u16, 1, 1, 1920, reatt_n1_1_past_dil0_1_convolution0_t_scratch_0_ptr_s16);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(62, 1, {(stai_ptr) reatt_n1_1_past_dil0_1_convolution0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil0_1_convolution0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil0_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_dil0_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil0_1_BiasAdd0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion */
  {
      const ai_i8* reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
    ai_float* reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(68, 1, {(stai_ptr) reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(68, 1, {(stai_ptr) reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil0_1_BiasAdd0_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_moments_mean0 */
  {
    
  forward_lite_reduce_reatt_n1_1_past_ln0_1_moments_mean0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_moments_mean0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_moments_mean0_Mul */
  {
      ai_float* reatt_n1_1_past_ln0_1_moments_mean0_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 201600);
    const ai_float* reatt_n1_1_past_ln0_1_moments_mean0_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 201600);
    const ai_float* reatt_n1_1_past_ln0_1_moments_mean0_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30940);
    const ai_float* reatt_n1_1_past_ln0_1_moments_mean0_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30944);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(71, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_moments_mean0_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reatt_n1_1_past_ln0_1_moments_mean0_Mul_t_out_0_ptr_f32, reatt_n1_1_past_ln0_1_moments_mean0_Mul_t_in_0_ptr_const_f32, reatt_n1_1_past_ln0_1_moments_mean0_Mul_t_weight_0_ptr_const_f32, reatt_n1_1_past_ln0_1_moments_mean0_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(71, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_moments_mean0_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_moments_mean0_Mul */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_moments_SquaredDifference0 */
  {
    
  forward_lite_eltwise_reatt_n1_1_past_ln0_1_moments_SquaredDifference0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_moments_SquaredDifference0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion */
  {
      const ai_float* reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
    ai_i8* reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(79, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(79, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_moments_SquaredDifference__210 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_moments_SquaredDifference__210(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_moments_SquaredDifference__210 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 172800);
    ai_float* reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(83, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(83, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_moments_SquaredDifference__210_0_0_reatt_n1_1_past_ln0_1_moments_variance0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_moments_variance0 */
  {
    
  forward_lite_reduce_reatt_n1_1_past_ln0_1_moments_variance0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_moments_variance0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_moments_variance0_Mul */
  {
      ai_float* reatt_n1_1_past_ln0_1_moments_variance0_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
    const ai_float* reatt_n1_1_past_ln0_1_moments_variance0_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 202800);
    const ai_float* reatt_n1_1_past_ln0_1_moments_variance0_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30940);
    const ai_float* reatt_n1_1_past_ln0_1_moments_variance0_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30944);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(86, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_moments_variance0_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reatt_n1_1_past_ln0_1_moments_variance0_Mul_t_out_0_ptr_f32, reatt_n1_1_past_ln0_1_moments_variance0_Mul_t_in_0_ptr_const_f32, reatt_n1_1_past_ln0_1_moments_variance0_Mul_t_weight_0_ptr_const_f32, reatt_n1_1_past_ln0_1_moments_variance0_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(86, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_moments_variance0_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_moments_variance0_Mul */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion */
  {
      const ai_float* reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
    ai_i8* reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 87600);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(86, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(86, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln0_1_add0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
    ai_float* reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 86700);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(89, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(89, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_add0_0_0_reatt_n1_1_past_ln0_1_Rsqrt0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_Rsqrt0 */
  {
      ai_handle reatt_n1_1_past_ln0_1_Rsqrt0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 87900);
    const ai_handle reatt_n1_1_past_ln0_1_Rsqrt0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 86700);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(92, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_Rsqrt0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_sqrt_if32of32(reatt_n1_1_past_ln0_1_Rsqrt0_t_out_0_ptr_handle, reatt_n1_1_past_ln0_1_Rsqrt0_t_in_0_ptr_const_handle, reatt_n1_1_past_ln0_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(92, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_Rsqrt0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_Rsqrt0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_Rsqrt__220 */
  {
      ai_handle reatt_n1_1_past_ln0_1_Rsqrt__220_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 86400);
    const ai_handle reatt_n1_1_past_ln0_1_Rsqrt__220_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 87900);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(93, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_Rsqrt__220_t_in_0_ptr_const_handle});
    
  forward_lite_nl_reciprocal_if32of32(reatt_n1_1_past_ln0_1_Rsqrt__220_t_out_0_ptr_handle, reatt_n1_1_past_ln0_1_Rsqrt__220_t_in_0_ptr_const_handle, reatt_n1_1_past_ln0_1_Rsqrt__220_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(93, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_Rsqrt__220_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_Rsqrt__220 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion */
  {
      const ai_float* reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
    ai_i8* reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 87600);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(93, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(93, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_Rsqrt__220_0_0_reatt_n1_1_past_ln0_1_mul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__1910 */
  {
    
  forward_lite_transpose_Transpose__1910(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__1910 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_Neg0 */
  {
      ai_handle reatt_n1_1_past_ln0_1_Neg0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 86400);
    const ai_handle reatt_n1_1_past_ln0_1_Neg0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 201600);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(75, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_Neg0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(reatt_n1_1_past_ln0_1_Neg0_t_out_0_ptr_handle, reatt_n1_1_past_ln0_1_Neg0_t_in_0_ptr_const_handle, reatt_n1_1_past_ln0_1_Neg0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(75, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_Neg0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_Neg0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion */
  {
      const ai_float* reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
    ai_i8* reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 87600);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(75, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(75, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_Neg0_0_0_reatt_n1_1_past_ln0_1_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_add_10 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__1040 */
  {
    
  forward_lite_transpose_Transpose__1040(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__1040 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_add_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln0_1_add_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_add_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
    ai_float* reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(111, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(111, 1, {(stai_ptr) reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln0_1_add_20_0_0_reatt_n1_1_past_gelu0_1_Gelu_Pow0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu0_1_Gelu_Pow0 */
  {
    
  forward_lite_eltwise_reatt_n1_1_past_gelu0_1_Gelu_Pow0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu0_1_Gelu_Pow0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion */
  {
      const ai_float* reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
    ai_i8* reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(115, 1, {(stai_ptr) reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_t_out_0_ptr_s8, reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(115, 1, {(stai_ptr) reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu0_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu0_1_Gelu_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu0_1_Gelu_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu0_1_Gelu_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu0_1_Gelu_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu0_1_Gelu_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu0_1_Gelu_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu0_1_Gelu_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu0_1_Gelu_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu0_1_Gelu_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu0_1_Gelu_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu0_1_Gelu_Tanh0 */
  {
    
  forward_lite_nl_integer_reatt_n1_1_past_gelu0_1_Gelu_Tanh0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu0_1_Gelu_Tanh0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu0_1_Gelu_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu0_1_Gelu_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu0_1_Gelu_add_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu0_1_Gelu_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu0_1_Gelu_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu0_1_Gelu_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu0_1_Gelu_mul_30 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu0_1_Gelu_mul_30(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu0_1_Gelu_mul_30 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_add0_1_Add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_add0_1_Add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_add0_1_Add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_zpad1_1_strided_slice0 */
  {
    
  forward_lite_slice_reatt_n1_1_past_zpad1_1_strided_slice0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_zpad1_1_strided_slice0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_zpad1_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_zpad1_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_zpad1_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ccat1_1_concat0 */
  {
    
  forward_lite_concat_reatt_n1_1_past_ccat1_1_concat0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ccat1_1_concat0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast */
  {
    
  forward_lite_transpose_reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion */
  {
      const ai_i8* reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 86016);
    ai_float* reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(150, 1, {(stai_ptr) reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(150, 1, {(stai_ptr) reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil1_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil1_1_convolution0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil1_1_convolution0 */
  {
      const ai_float* reatt_n1_1_past_dil1_1_convolution0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
    ai_float* reatt_n1_1_past_dil1_1_convolution0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_u8* reatt_n1_1_past_dil1_1_convolution0_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 30948);
    const ai_u8* reatt_n1_1_past_dil1_1_convolution0_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 141540);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(156, 1, {(stai_ptr) reatt_n1_1_past_dil1_1_convolution0_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(reatt_n1_1_past_dil1_1_convolution0_t_in_0_ptr_const_f32, reatt_n1_1_past_dil1_1_convolution0_t_out_0_ptr_f32, reatt_n1_1_past_dil1_1_convolution0_t_weight_0_ptr_const_u8, reatt_n1_1_past_dil1_1_convolution0_t_weight_1_ptr_const_u8, reatt_n1_1_past_dil1_1_convolution0_t_in_0_shape_ch_const_u32, reatt_n1_1_past_dil1_1_convolution0_t_out_0_shape_ch_const_u32, reatt_n1_1_past_dil1_1_convolution0_t_in_0_shape_w_const_u32, reatt_n1_1_past_dil1_1_convolution0_t_in_0_shape_h_const_u32, reatt_n1_1_past_dil1_1_convolution0_t_out_0_shape_w_const_u32, reatt_n1_1_past_dil1_1_convolution0_t_out_0_shape_h_const_u32, reatt_n1_1_past_dil1_1_convolution0_t_weight_0_shape_w_const_u32, reatt_n1_1_past_dil1_1_convolution0_t_weight_0_shape_h_const_u32, reatt_n1_1_past_dil1_1_convolution0_l_pad_W_0_const_s32, reatt_n1_1_past_dil1_1_convolution0_l_pad_H_0_const_s32, reatt_n1_1_past_dil1_1_convolution0_l_stride_1_const_u16, reatt_n1_1_past_dil1_1_convolution0_l_stride_0_const_u16, 1, 5, reatt_n1_1_past_dil1_1_convolution0_l_dilation_W_const_u16, reatt_n1_1_past_dil1_1_convolution0_l_dilation_H_const_u16, reatt_n1_1_past_dil1_1_convolution0_v_n_groups_const_size);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(156, 1, {(stai_ptr) reatt_n1_1_past_dil1_1_convolution0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil1_1_convolution0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion */
  {
      const ai_float* reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_i8* reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 144000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(156, 1, {(stai_ptr) reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(156, 1, {(stai_ptr) reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil1_1_convolution0_0_0_reatt_n1_1_past_dil1_1_BiasAdd0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil1_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_dil1_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil1_1_BiasAdd0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion */
  {
      const ai_i8* reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_float* reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(162, 1, {(stai_ptr) reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(162, 1, {(stai_ptr) reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil1_1_BiasAdd0_0_0_reatt_n1_1_past_ln1_1_moments_mean0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_moments_mean0 */
  {
    
  forward_lite_reduce_reatt_n1_1_past_ln1_1_moments_mean0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_moments_mean0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_moments_mean0_Mul */
  {
      ai_float* reatt_n1_1_past_ln1_1_moments_mean0_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30000);
    const ai_float* reatt_n1_1_past_ln1_1_moments_mean0_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    const ai_float* reatt_n1_1_past_ln1_1_moments_mean0_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30940);
    const ai_float* reatt_n1_1_past_ln1_1_moments_mean0_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30944);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(165, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_moments_mean0_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reatt_n1_1_past_ln1_1_moments_mean0_Mul_t_out_0_ptr_f32, reatt_n1_1_past_ln1_1_moments_mean0_Mul_t_in_0_ptr_const_f32, reatt_n1_1_past_ln1_1_moments_mean0_Mul_t_weight_0_ptr_const_f32, reatt_n1_1_past_ln1_1_moments_mean0_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(165, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_moments_mean0_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_moments_mean0_Mul */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_moments_SquaredDifference0 */
  {
    
  forward_lite_eltwise_reatt_n1_1_past_ln1_1_moments_SquaredDifference0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_moments_SquaredDifference0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion */
  {
      const ai_float* reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
    ai_i8* reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 31200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(173, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(173, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_moments_SquaredDifference__320 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_moments_SquaredDifference__320(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_moments_SquaredDifference__320 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 60000);
    ai_float* reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(177, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(177, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_moments_SquaredDifference__320_0_0_reatt_n1_1_past_ln1_1_moments_variance0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_moments_variance0 */
  {
    
  forward_lite_reduce_reatt_n1_1_past_ln1_1_moments_variance0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_moments_variance0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_moments_variance0_Mul */
  {
      ai_float* reatt_n1_1_past_ln1_1_moments_variance0_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    const ai_float* reatt_n1_1_past_ln1_1_moments_variance0_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    const ai_float* reatt_n1_1_past_ln1_1_moments_variance0_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30940);
    const ai_float* reatt_n1_1_past_ln1_1_moments_variance0_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30944);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(180, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_moments_variance0_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reatt_n1_1_past_ln1_1_moments_variance0_Mul_t_out_0_ptr_f32, reatt_n1_1_past_ln1_1_moments_variance0_Mul_t_in_0_ptr_const_f32, reatt_n1_1_past_ln1_1_moments_variance0_Mul_t_weight_0_ptr_const_f32, reatt_n1_1_past_ln1_1_moments_variance0_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(180, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_moments_variance0_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_moments_variance0_Mul */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion */
  {
      const ai_float* reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    ai_i8* reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(180, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(180, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln1_1_add0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 29100);
    ai_float* reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(183, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(183, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_add0_0_0_reatt_n1_1_past_ln1_1_Rsqrt0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_Rsqrt0 */
  {
      ai_handle reatt_n1_1_past_ln1_1_Rsqrt0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
    const ai_handle reatt_n1_1_past_ln1_1_Rsqrt0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 31200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(186, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_Rsqrt0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_sqrt_if32of32(reatt_n1_1_past_ln1_1_Rsqrt0_t_out_0_ptr_handle, reatt_n1_1_past_ln1_1_Rsqrt0_t_in_0_ptr_const_handle, reatt_n1_1_past_ln1_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(186, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_Rsqrt0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_Rsqrt0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_Rsqrt__330 */
  {
      ai_handle reatt_n1_1_past_ln1_1_Rsqrt__330_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 31200);
    const ai_handle reatt_n1_1_past_ln1_1_Rsqrt__330_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(187, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_Rsqrt__330_t_in_0_ptr_const_handle});
    
  forward_lite_nl_reciprocal_if32of32(reatt_n1_1_past_ln1_1_Rsqrt__330_t_out_0_ptr_handle, reatt_n1_1_past_ln1_1_Rsqrt__330_t_in_0_ptr_const_handle, reatt_n1_1_past_ln1_1_Rsqrt__330_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(187, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_Rsqrt__330_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_Rsqrt__330 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion */
  {
      const ai_float* reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    ai_i8* reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(187, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(187, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_Rsqrt__330_0_0_reatt_n1_1_past_ln1_1_mul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__1970 */
  {
    
  forward_lite_transpose_Transpose__1970(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__1970 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_Neg0 */
  {
      ai_handle reatt_n1_1_past_ln1_1_Neg0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
    const ai_handle reatt_n1_1_past_ln1_1_Neg0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(169, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_Neg0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(reatt_n1_1_past_ln1_1_Neg0_t_out_0_ptr_handle, reatt_n1_1_past_ln1_1_Neg0_t_in_0_ptr_const_handle, reatt_n1_1_past_ln1_1_Neg0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(169, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_Neg0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_Neg0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion */
  {
      const ai_float* reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    ai_i8* reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 30000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(169, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(169, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_Neg0_0_0_reatt_n1_1_past_ln1_1_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_add_10 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__1150 */
  {
    
  forward_lite_transpose_Transpose__1150(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__1150 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_add_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln1_1_add_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_add_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
    ai_float* reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(205, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(205, 1, {(stai_ptr) reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln1_1_add_20_0_0_reatt_n1_1_past_gelu1_1_Gelu_Pow0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu1_1_Gelu_Pow0 */
  {
    
  forward_lite_eltwise_reatt_n1_1_past_gelu1_1_Gelu_Pow0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu1_1_Gelu_Pow0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion */
  {
      const ai_float* reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
    ai_i8* reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(209, 1, {(stai_ptr) reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_t_out_0_ptr_s8, reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(209, 1, {(stai_ptr) reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu1_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu1_1_Gelu_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu1_1_Gelu_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu1_1_Gelu_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu1_1_Gelu_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu1_1_Gelu_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu1_1_Gelu_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu1_1_Gelu_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu1_1_Gelu_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu1_1_Gelu_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu1_1_Gelu_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu1_1_Gelu_Tanh0 */
  {
    
  forward_lite_nl_integer_reatt_n1_1_past_gelu1_1_Gelu_Tanh0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu1_1_Gelu_Tanh0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu1_1_Gelu_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu1_1_Gelu_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu1_1_Gelu_add_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu1_1_Gelu_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu1_1_Gelu_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu1_1_Gelu_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu1_1_Gelu_mul_30 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu1_1_Gelu_mul_30(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu1_1_Gelu_mul_30 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_add1_1_Add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_add1_1_Add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_add1_1_Add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_zpad2_1_strided_slice0 */
  {
    
  forward_lite_slice_reatt_n1_1_past_zpad2_1_strided_slice0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_zpad2_1_strided_slice0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_zpad2_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_zpad2_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_zpad2_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ccat2_1_concat0 */
  {
    
  forward_lite_concat_reatt_n1_1_past_ccat2_1_concat0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ccat2_1_concat0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast */
  {
    
  forward_lite_transpose_reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion */
  {
      const ai_i8* reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 231168);
    ai_float* reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(244, 1, {(stai_ptr) reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(244, 1, {(stai_ptr) reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil2_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil2_1_convolution0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil2_1_convolution0 */
  {
      const ai_float* reatt_n1_1_past_dil2_1_convolution0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    ai_float* reatt_n1_1_past_dil2_1_convolution0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 147072);
    const ai_u8* reatt_n1_1_past_dil2_1_convolution0_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 141924);
    const ai_u8* reatt_n1_1_past_dil2_1_convolution0_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 141540);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(250, 1, {(stai_ptr) reatt_n1_1_past_dil2_1_convolution0_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(reatt_n1_1_past_dil2_1_convolution0_t_in_0_ptr_const_f32, reatt_n1_1_past_dil2_1_convolution0_t_out_0_ptr_f32, reatt_n1_1_past_dil2_1_convolution0_t_weight_0_ptr_const_u8, reatt_n1_1_past_dil2_1_convolution0_t_weight_1_ptr_const_u8, reatt_n1_1_past_dil2_1_convolution0_t_in_0_shape_ch_const_u32, reatt_n1_1_past_dil2_1_convolution0_t_out_0_shape_ch_const_u32, reatt_n1_1_past_dil2_1_convolution0_t_in_0_shape_w_const_u32, reatt_n1_1_past_dil2_1_convolution0_t_in_0_shape_h_const_u32, reatt_n1_1_past_dil2_1_convolution0_t_out_0_shape_w_const_u32, reatt_n1_1_past_dil2_1_convolution0_t_out_0_shape_h_const_u32, reatt_n1_1_past_dil2_1_convolution0_t_weight_0_shape_w_const_u32, reatt_n1_1_past_dil2_1_convolution0_t_weight_0_shape_h_const_u32, reatt_n1_1_past_dil2_1_convolution0_l_pad_W_0_const_s32, reatt_n1_1_past_dil2_1_convolution0_l_pad_H_0_const_s32, reatt_n1_1_past_dil2_1_convolution0_l_stride_1_const_u16, reatt_n1_1_past_dil2_1_convolution0_l_stride_0_const_u16, 1, 9, reatt_n1_1_past_dil2_1_convolution0_l_dilation_W_const_u16, reatt_n1_1_past_dil2_1_convolution0_l_dilation_H_const_u16, reatt_n1_1_past_dil2_1_convolution0_v_n_groups_const_size);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(250, 1, {(stai_ptr) reatt_n1_1_past_dil2_1_convolution0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil2_1_convolution0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion */
  {
      const ai_float* reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 147072);
    ai_i8* reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(250, 1, {(stai_ptr) reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(250, 1, {(stai_ptr) reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil2_1_convolution0_0_0_reatt_n1_1_past_dil2_1_BiasAdd0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil2_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_dil2_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil2_1_BiasAdd0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion */
  {
      const ai_i8* reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 57600);
    ai_float* reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(256, 1, {(stai_ptr) reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(256, 1, {(stai_ptr) reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil2_1_BiasAdd0_0_0_reatt_n1_1_past_ln2_1_moments_mean0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_moments_mean0 */
  {
    
  forward_lite_reduce_reatt_n1_1_past_ln2_1_moments_mean0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_moments_mean0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_moments_mean0_Mul */
  {
      ai_float* reatt_n1_1_past_ln2_1_moments_mean0_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30000);
    const ai_float* reatt_n1_1_past_ln2_1_moments_mean0_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    const ai_float* reatt_n1_1_past_ln2_1_moments_mean0_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30940);
    const ai_float* reatt_n1_1_past_ln2_1_moments_mean0_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30944);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(259, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_moments_mean0_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reatt_n1_1_past_ln2_1_moments_mean0_Mul_t_out_0_ptr_f32, reatt_n1_1_past_ln2_1_moments_mean0_Mul_t_in_0_ptr_const_f32, reatt_n1_1_past_ln2_1_moments_mean0_Mul_t_weight_0_ptr_const_f32, reatt_n1_1_past_ln2_1_moments_mean0_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(259, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_moments_mean0_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_moments_mean0_Mul */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_moments_SquaredDifference0 */
  {
    
  forward_lite_eltwise_reatt_n1_1_past_ln2_1_moments_SquaredDifference0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_moments_SquaredDifference0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion */
  {
      const ai_float* reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
    ai_i8* reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 201600);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(267, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(267, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_moments_SquaredDifference__430 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_moments_SquaredDifference__430(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_moments_SquaredDifference__430 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
    ai_float* reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 115200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(271, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(271, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_moments_SquaredDifference__430_0_0_reatt_n1_1_past_ln2_1_moments_variance0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_moments_variance0 */
  {
    
  forward_lite_reduce_reatt_n1_1_past_ln2_1_moments_variance0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_moments_variance0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_moments_variance0_Mul */
  {
      ai_float* reatt_n1_1_past_ln2_1_moments_variance0_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    const ai_float* reatt_n1_1_past_ln2_1_moments_variance0_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    const ai_float* reatt_n1_1_past_ln2_1_moments_variance0_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30940);
    const ai_float* reatt_n1_1_past_ln2_1_moments_variance0_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30944);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(274, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_moments_variance0_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reatt_n1_1_past_ln2_1_moments_variance0_Mul_t_out_0_ptr_f32, reatt_n1_1_past_ln2_1_moments_variance0_Mul_t_in_0_ptr_const_f32, reatt_n1_1_past_ln2_1_moments_variance0_Mul_t_weight_0_ptr_const_f32, reatt_n1_1_past_ln2_1_moments_variance0_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(274, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_moments_variance0_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_moments_variance0_Mul */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion */
  {
      const ai_float* reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    ai_i8* reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(274, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(274, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln2_1_add0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 29100);
    ai_float* reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(277, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(277, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_add0_0_0_reatt_n1_1_past_ln2_1_Rsqrt0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_Rsqrt0 */
  {
      ai_handle reatt_n1_1_past_ln2_1_Rsqrt0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
    const ai_handle reatt_n1_1_past_ln2_1_Rsqrt0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 31200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(280, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_Rsqrt0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_sqrt_if32of32(reatt_n1_1_past_ln2_1_Rsqrt0_t_out_0_ptr_handle, reatt_n1_1_past_ln2_1_Rsqrt0_t_in_0_ptr_const_handle, reatt_n1_1_past_ln2_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(280, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_Rsqrt0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_Rsqrt0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_Rsqrt__440 */
  {
      ai_handle reatt_n1_1_past_ln2_1_Rsqrt__440_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 31200);
    const ai_handle reatt_n1_1_past_ln2_1_Rsqrt__440_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(281, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_Rsqrt__440_t_in_0_ptr_const_handle});
    
  forward_lite_nl_reciprocal_if32of32(reatt_n1_1_past_ln2_1_Rsqrt__440_t_out_0_ptr_handle, reatt_n1_1_past_ln2_1_Rsqrt__440_t_in_0_ptr_const_handle, reatt_n1_1_past_ln2_1_Rsqrt__440_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(281, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_Rsqrt__440_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_Rsqrt__440 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion */
  {
      const ai_float* reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    ai_i8* reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(281, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(281, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_Rsqrt__440_0_0_reatt_n1_1_past_ln2_1_mul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__2030 */
  {
    
  forward_lite_transpose_Transpose__2030(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__2030 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_Neg0 */
  {
      ai_handle reatt_n1_1_past_ln2_1_Neg0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
    const ai_handle reatt_n1_1_past_ln2_1_Neg0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(263, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_Neg0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(reatt_n1_1_past_ln2_1_Neg0_t_out_0_ptr_handle, reatt_n1_1_past_ln2_1_Neg0_t_in_0_ptr_const_handle, reatt_n1_1_past_ln2_1_Neg0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(263, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_Neg0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_Neg0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion */
  {
      const ai_float* reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    ai_i8* reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 30000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(263, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(263, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_Neg0_0_0_reatt_n1_1_past_ln2_1_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_add_10 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__1260 */
  {
    
  forward_lite_transpose_Transpose__1260(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__1260 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_add_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln2_1_add_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_add_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
    ai_float* reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 115200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(299, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(299, 1, {(stai_ptr) reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln2_1_add_20_0_0_reatt_n1_1_past_gelu2_1_Gelu_Pow0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu2_1_Gelu_Pow0 */
  {
    
  forward_lite_eltwise_reatt_n1_1_past_gelu2_1_Gelu_Pow0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu2_1_Gelu_Pow0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion */
  {
      const ai_float* reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 115200);
    ai_i8* reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(303, 1, {(stai_ptr) reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_t_out_0_ptr_s8, reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(303, 1, {(stai_ptr) reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu2_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu2_1_Gelu_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu2_1_Gelu_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu2_1_Gelu_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu2_1_Gelu_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu2_1_Gelu_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu2_1_Gelu_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu2_1_Gelu_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu2_1_Gelu_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu2_1_Gelu_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu2_1_Gelu_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu2_1_Gelu_Tanh0 */
  {
    
  forward_lite_nl_integer_reatt_n1_1_past_gelu2_1_Gelu_Tanh0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu2_1_Gelu_Tanh0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu2_1_Gelu_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu2_1_Gelu_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu2_1_Gelu_add_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu2_1_Gelu_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu2_1_Gelu_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu2_1_Gelu_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu2_1_Gelu_mul_30 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu2_1_Gelu_mul_30(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu2_1_Gelu_mul_30 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_add2_1_Add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_add2_1_Add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_add2_1_Add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_zpad3_1_strided_slice0 */
  {
    
  forward_lite_slice_reatt_n1_1_past_zpad3_1_strided_slice0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_zpad3_1_strided_slice0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_zpad3_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_zpad3_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_zpad3_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ccat3_1_concat0 */
  {
    
  forward_lite_concat_reatt_n1_1_past_ccat3_1_concat0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ccat3_1_concat0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast */
  {
    
  forward_lite_transpose_reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion */
  {
      const ai_i8* reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 84864);
    ai_float* reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(338, 1, {(stai_ptr) reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(338, 1, {(stai_ptr) reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil3_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil3_1_convolution0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil3_1_convolution0 */
  {
      const ai_float* reatt_n1_1_past_dil3_1_convolution0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
    ai_float* reatt_n1_1_past_dil3_1_convolution0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_u8* reatt_n1_1_past_dil3_1_convolution0_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 252516);
    const ai_u8* reatt_n1_1_past_dil3_1_convolution0_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 141540);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(344, 1, {(stai_ptr) reatt_n1_1_past_dil3_1_convolution0_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(reatt_n1_1_past_dil3_1_convolution0_t_in_0_ptr_const_f32, reatt_n1_1_past_dil3_1_convolution0_t_out_0_ptr_f32, reatt_n1_1_past_dil3_1_convolution0_t_weight_0_ptr_const_u8, reatt_n1_1_past_dil3_1_convolution0_t_weight_1_ptr_const_u8, reatt_n1_1_past_dil3_1_convolution0_t_in_0_shape_ch_const_u32, reatt_n1_1_past_dil3_1_convolution0_t_out_0_shape_ch_const_u32, reatt_n1_1_past_dil3_1_convolution0_t_in_0_shape_w_const_u32, reatt_n1_1_past_dil3_1_convolution0_t_in_0_shape_h_const_u32, reatt_n1_1_past_dil3_1_convolution0_t_out_0_shape_w_const_u32, reatt_n1_1_past_dil3_1_convolution0_t_out_0_shape_h_const_u32, reatt_n1_1_past_dil3_1_convolution0_t_weight_0_shape_w_const_u32, reatt_n1_1_past_dil3_1_convolution0_t_weight_0_shape_h_const_u32, reatt_n1_1_past_dil3_1_convolution0_l_pad_W_0_const_s32, reatt_n1_1_past_dil3_1_convolution0_l_pad_H_0_const_s32, reatt_n1_1_past_dil3_1_convolution0_l_stride_1_const_u16, reatt_n1_1_past_dil3_1_convolution0_l_stride_0_const_u16, 1, 17, reatt_n1_1_past_dil3_1_convolution0_l_dilation_W_const_u16, reatt_n1_1_past_dil3_1_convolution0_l_dilation_H_const_u16, reatt_n1_1_past_dil3_1_convolution0_v_n_groups_const_size);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(344, 1, {(stai_ptr) reatt_n1_1_past_dil3_1_convolution0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil3_1_convolution0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion */
  {
      const ai_float* reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_i8* reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 144000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(344, 1, {(stai_ptr) reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(344, 1, {(stai_ptr) reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil3_1_convolution0_0_0_reatt_n1_1_past_dil3_1_BiasAdd0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil3_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_dil3_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil3_1_BiasAdd0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion */
  {
      const ai_i8* reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_float* reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(350, 1, {(stai_ptr) reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(350, 1, {(stai_ptr) reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil3_1_BiasAdd0_0_0_reatt_n1_1_past_ln3_1_moments_mean0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_moments_mean0 */
  {
    
  forward_lite_reduce_reatt_n1_1_past_ln3_1_moments_mean0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_moments_mean0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_moments_mean0_Mul */
  {
      ai_float* reatt_n1_1_past_ln3_1_moments_mean0_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30000);
    const ai_float* reatt_n1_1_past_ln3_1_moments_mean0_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    const ai_float* reatt_n1_1_past_ln3_1_moments_mean0_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30940);
    const ai_float* reatt_n1_1_past_ln3_1_moments_mean0_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30944);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(353, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_moments_mean0_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reatt_n1_1_past_ln3_1_moments_mean0_Mul_t_out_0_ptr_f32, reatt_n1_1_past_ln3_1_moments_mean0_Mul_t_in_0_ptr_const_f32, reatt_n1_1_past_ln3_1_moments_mean0_Mul_t_weight_0_ptr_const_f32, reatt_n1_1_past_ln3_1_moments_mean0_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(353, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_moments_mean0_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_moments_mean0_Mul */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_moments_SquaredDifference0 */
  {
    
  forward_lite_eltwise_reatt_n1_1_past_ln3_1_moments_SquaredDifference0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_moments_SquaredDifference0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion */
  {
      const ai_float* reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
    ai_i8* reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 31200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(361, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(361, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_moments_SquaredDifference__540 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_moments_SquaredDifference__540(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_moments_SquaredDifference__540 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 60000);
    ai_float* reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(365, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(365, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_moments_SquaredDifference__540_0_0_reatt_n1_1_past_ln3_1_moments_variance0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_moments_variance0 */
  {
    
  forward_lite_reduce_reatt_n1_1_past_ln3_1_moments_variance0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_moments_variance0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_moments_variance0_Mul */
  {
      ai_float* reatt_n1_1_past_ln3_1_moments_variance0_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    const ai_float* reatt_n1_1_past_ln3_1_moments_variance0_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    const ai_float* reatt_n1_1_past_ln3_1_moments_variance0_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30940);
    const ai_float* reatt_n1_1_past_ln3_1_moments_variance0_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30944);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(368, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_moments_variance0_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reatt_n1_1_past_ln3_1_moments_variance0_Mul_t_out_0_ptr_f32, reatt_n1_1_past_ln3_1_moments_variance0_Mul_t_in_0_ptr_const_f32, reatt_n1_1_past_ln3_1_moments_variance0_Mul_t_weight_0_ptr_const_f32, reatt_n1_1_past_ln3_1_moments_variance0_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(368, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_moments_variance0_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_moments_variance0_Mul */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion */
  {
      const ai_float* reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    ai_i8* reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(368, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(368, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln3_1_add0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 29100);
    ai_float* reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(371, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(371, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_add0_0_0_reatt_n1_1_past_ln3_1_Rsqrt0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_Rsqrt0 */
  {
      ai_handle reatt_n1_1_past_ln3_1_Rsqrt0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
    const ai_handle reatt_n1_1_past_ln3_1_Rsqrt0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 31200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(374, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_Rsqrt0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_sqrt_if32of32(reatt_n1_1_past_ln3_1_Rsqrt0_t_out_0_ptr_handle, reatt_n1_1_past_ln3_1_Rsqrt0_t_in_0_ptr_const_handle, reatt_n1_1_past_ln3_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(374, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_Rsqrt0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_Rsqrt0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_Rsqrt__550 */
  {
      ai_handle reatt_n1_1_past_ln3_1_Rsqrt__550_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 31200);
    const ai_handle reatt_n1_1_past_ln3_1_Rsqrt__550_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(375, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_Rsqrt__550_t_in_0_ptr_const_handle});
    
  forward_lite_nl_reciprocal_if32of32(reatt_n1_1_past_ln3_1_Rsqrt__550_t_out_0_ptr_handle, reatt_n1_1_past_ln3_1_Rsqrt__550_t_in_0_ptr_const_handle, reatt_n1_1_past_ln3_1_Rsqrt__550_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(375, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_Rsqrt__550_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_Rsqrt__550 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion */
  {
      const ai_float* reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    ai_i8* reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(375, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(375, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_Rsqrt__550_0_0_reatt_n1_1_past_ln3_1_mul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__2090 */
  {
    
  forward_lite_transpose_Transpose__2090(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__2090 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_Neg0 */
  {
      ai_handle reatt_n1_1_past_ln3_1_Neg0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
    const ai_handle reatt_n1_1_past_ln3_1_Neg0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(357, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_Neg0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(reatt_n1_1_past_ln3_1_Neg0_t_out_0_ptr_handle, reatt_n1_1_past_ln3_1_Neg0_t_in_0_ptr_const_handle, reatt_n1_1_past_ln3_1_Neg0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(357, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_Neg0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_Neg0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion */
  {
      const ai_float* reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    ai_i8* reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 30000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(357, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(357, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_Neg0_0_0_reatt_n1_1_past_ln3_1_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_add_10 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__1370 */
  {
    
  forward_lite_transpose_Transpose__1370(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__1370 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_add_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln3_1_add_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_add_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
    ai_float* reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(393, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(393, 1, {(stai_ptr) reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln3_1_add_20_0_0_reatt_n1_1_past_gelu3_1_Gelu_Pow0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu3_1_Gelu_Pow0 */
  {
    
  forward_lite_eltwise_reatt_n1_1_past_gelu3_1_Gelu_Pow0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu3_1_Gelu_Pow0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion */
  {
      const ai_float* reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 144000);
    ai_i8* reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(397, 1, {(stai_ptr) reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_t_out_0_ptr_s8, reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(397, 1, {(stai_ptr) reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu3_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu3_1_Gelu_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu3_1_Gelu_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu3_1_Gelu_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu3_1_Gelu_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu3_1_Gelu_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu3_1_Gelu_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu3_1_Gelu_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu3_1_Gelu_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu3_1_Gelu_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu3_1_Gelu_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu3_1_Gelu_Tanh0 */
  {
    
  forward_lite_nl_integer_reatt_n1_1_past_gelu3_1_Gelu_Tanh0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu3_1_Gelu_Tanh0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu3_1_Gelu_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu3_1_Gelu_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu3_1_Gelu_add_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu3_1_Gelu_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu3_1_Gelu_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu3_1_Gelu_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu3_1_Gelu_mul_30 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu3_1_Gelu_mul_30(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu3_1_Gelu_mul_30 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_add3_1_Add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_add3_1_Add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_add3_1_Add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_zpad4_1_strided_slice0 */
  {
    
  forward_lite_slice_reatt_n1_1_past_zpad4_1_strided_slice0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_zpad4_1_strided_slice0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_zpad4_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_zpad4_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_zpad4_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ccat4_1_concat0 */
  {
    
  forward_lite_concat_reatt_n1_1_past_ccat4_1_concat0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ccat4_1_concat0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast */
  {
    
  forward_lite_transpose_reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion */
  {
      const ai_i8* reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 201600);
    ai_float* reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(432, 1, {(stai_ptr) reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(432, 1, {(stai_ptr) reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil4_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil4_1_convolution0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil4_1_convolution0 */
  {
      const ai_float* reatt_n1_1_past_dil4_1_convolution0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    ai_float* reatt_n1_1_past_dil4_1_convolution0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 156288);
    const ai_u8* reatt_n1_1_past_dil4_1_convolution0_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 363108);
    const ai_u8* reatt_n1_1_past_dil4_1_convolution0_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 141540);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(438, 1, {(stai_ptr) reatt_n1_1_past_dil4_1_convolution0_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(reatt_n1_1_past_dil4_1_convolution0_t_in_0_ptr_const_f32, reatt_n1_1_past_dil4_1_convolution0_t_out_0_ptr_f32, reatt_n1_1_past_dil4_1_convolution0_t_weight_0_ptr_const_u8, reatt_n1_1_past_dil4_1_convolution0_t_weight_1_ptr_const_u8, reatt_n1_1_past_dil4_1_convolution0_t_in_0_shape_ch_const_u32, reatt_n1_1_past_dil4_1_convolution0_t_out_0_shape_ch_const_u32, reatt_n1_1_past_dil4_1_convolution0_t_in_0_shape_w_const_u32, reatt_n1_1_past_dil4_1_convolution0_t_in_0_shape_h_const_u32, reatt_n1_1_past_dil4_1_convolution0_t_out_0_shape_w_const_u32, reatt_n1_1_past_dil4_1_convolution0_t_out_0_shape_h_const_u32, reatt_n1_1_past_dil4_1_convolution0_t_weight_0_shape_w_const_u32, reatt_n1_1_past_dil4_1_convolution0_t_weight_0_shape_h_const_u32, reatt_n1_1_past_dil4_1_convolution0_l_pad_W_0_const_s32, reatt_n1_1_past_dil4_1_convolution0_l_pad_H_0_const_s32, reatt_n1_1_past_dil4_1_convolution0_l_stride_1_const_u16, reatt_n1_1_past_dil4_1_convolution0_l_stride_0_const_u16, 1, 33, reatt_n1_1_past_dil4_1_convolution0_l_dilation_W_const_u16, reatt_n1_1_past_dil4_1_convolution0_l_dilation_H_const_u16, reatt_n1_1_past_dil4_1_convolution0_v_n_groups_const_size);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(438, 1, {(stai_ptr) reatt_n1_1_past_dil4_1_convolution0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil4_1_convolution0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion */
  {
      const ai_float* reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 156288);
    ai_i8* reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(438, 1, {(stai_ptr) reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(438, 1, {(stai_ptr) reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil4_1_convolution0_0_0_reatt_n1_1_past_dil4_1_BiasAdd0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil4_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_dil4_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil4_1_BiasAdd0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion */
  {
      const ai_i8* reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 57600);
    ai_float* reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(444, 1, {(stai_ptr) reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(444, 1, {(stai_ptr) reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil4_1_BiasAdd0_0_0_reatt_n1_1_past_ln4_1_moments_mean0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_moments_mean0 */
  {
    
  forward_lite_reduce_reatt_n1_1_past_ln4_1_moments_mean0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_moments_mean0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_moments_mean0_Mul */
  {
      ai_float* reatt_n1_1_past_ln4_1_moments_mean0_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30000);
    const ai_float* reatt_n1_1_past_ln4_1_moments_mean0_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    const ai_float* reatt_n1_1_past_ln4_1_moments_mean0_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30940);
    const ai_float* reatt_n1_1_past_ln4_1_moments_mean0_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30944);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(447, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_moments_mean0_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reatt_n1_1_past_ln4_1_moments_mean0_Mul_t_out_0_ptr_f32, reatt_n1_1_past_ln4_1_moments_mean0_Mul_t_in_0_ptr_const_f32, reatt_n1_1_past_ln4_1_moments_mean0_Mul_t_weight_0_ptr_const_f32, reatt_n1_1_past_ln4_1_moments_mean0_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(447, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_moments_mean0_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_moments_mean0_Mul */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_moments_SquaredDifference0 */
  {
    
  forward_lite_eltwise_reatt_n1_1_past_ln4_1_moments_SquaredDifference0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_moments_SquaredDifference0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion */
  {
      const ai_float* reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
    ai_i8* reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 201600);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(455, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(455, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_moments_SquaredDifference__650 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_moments_SquaredDifference__650(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_moments_SquaredDifference__650 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
    ai_float* reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 115200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(459, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(459, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_moments_SquaredDifference__650_0_0_reatt_n1_1_past_ln4_1_moments_variance0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_moments_variance0 */
  {
    
  forward_lite_reduce_reatt_n1_1_past_ln4_1_moments_variance0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_moments_variance0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_moments_variance0_Mul */
  {
      ai_float* reatt_n1_1_past_ln4_1_moments_variance0_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    const ai_float* reatt_n1_1_past_ln4_1_moments_variance0_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    const ai_float* reatt_n1_1_past_ln4_1_moments_variance0_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30940);
    const ai_float* reatt_n1_1_past_ln4_1_moments_variance0_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30944);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(462, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_moments_variance0_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reatt_n1_1_past_ln4_1_moments_variance0_Mul_t_out_0_ptr_f32, reatt_n1_1_past_ln4_1_moments_variance0_Mul_t_in_0_ptr_const_f32, reatt_n1_1_past_ln4_1_moments_variance0_Mul_t_weight_0_ptr_const_f32, reatt_n1_1_past_ln4_1_moments_variance0_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(462, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_moments_variance0_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_moments_variance0_Mul */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion */
  {
      const ai_float* reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    ai_i8* reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(462, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(462, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln4_1_add0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 29100);
    ai_float* reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(465, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(465, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_add0_0_0_reatt_n1_1_past_ln4_1_Rsqrt0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_Rsqrt0 */
  {
      ai_handle reatt_n1_1_past_ln4_1_Rsqrt0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
    const ai_handle reatt_n1_1_past_ln4_1_Rsqrt0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 31200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(468, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_Rsqrt0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_sqrt_if32of32(reatt_n1_1_past_ln4_1_Rsqrt0_t_out_0_ptr_handle, reatt_n1_1_past_ln4_1_Rsqrt0_t_in_0_ptr_const_handle, reatt_n1_1_past_ln4_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(468, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_Rsqrt0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_Rsqrt0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_Rsqrt__660 */
  {
      ai_handle reatt_n1_1_past_ln4_1_Rsqrt__660_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 31200);
    const ai_handle reatt_n1_1_past_ln4_1_Rsqrt__660_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(469, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_Rsqrt__660_t_in_0_ptr_const_handle});
    
  forward_lite_nl_reciprocal_if32of32(reatt_n1_1_past_ln4_1_Rsqrt__660_t_out_0_ptr_handle, reatt_n1_1_past_ln4_1_Rsqrt__660_t_in_0_ptr_const_handle, reatt_n1_1_past_ln4_1_Rsqrt__660_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(469, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_Rsqrt__660_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_Rsqrt__660 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion */
  {
      const ai_float* reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    ai_i8* reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(469, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(469, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_Rsqrt__660_0_0_reatt_n1_1_past_ln4_1_mul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__2150 */
  {
    
  forward_lite_transpose_Transpose__2150(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__2150 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_Neg0 */
  {
      ai_handle reatt_n1_1_past_ln4_1_Neg0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
    const ai_handle reatt_n1_1_past_ln4_1_Neg0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(451, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_Neg0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(reatt_n1_1_past_ln4_1_Neg0_t_out_0_ptr_handle, reatt_n1_1_past_ln4_1_Neg0_t_in_0_ptr_const_handle, reatt_n1_1_past_ln4_1_Neg0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(451, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_Neg0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_Neg0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion */
  {
      const ai_float* reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    ai_i8* reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 30000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(451, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(451, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_Neg0_0_0_reatt_n1_1_past_ln4_1_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_add_10 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__1480 */
  {
    
  forward_lite_transpose_Transpose__1480(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__1480 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_add_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln4_1_add_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_add_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
    ai_float* reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 115200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(487, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(487, 1, {(stai_ptr) reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln4_1_add_20_0_0_reatt_n1_1_past_gelu4_1_Gelu_Pow0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu4_1_Gelu_Pow0 */
  {
    
  forward_lite_eltwise_reatt_n1_1_past_gelu4_1_Gelu_Pow0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu4_1_Gelu_Pow0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion */
  {
      const ai_float* reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 115200);
    ai_i8* reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(491, 1, {(stai_ptr) reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_t_out_0_ptr_s8, reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(491, 1, {(stai_ptr) reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu4_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu4_1_Gelu_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu4_1_Gelu_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu4_1_Gelu_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu4_1_Gelu_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu4_1_Gelu_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu4_1_Gelu_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu4_1_Gelu_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu4_1_Gelu_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu4_1_Gelu_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu4_1_Gelu_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu4_1_Gelu_Tanh0 */
  {
    
  forward_lite_nl_integer_reatt_n1_1_past_gelu4_1_Gelu_Tanh0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu4_1_Gelu_Tanh0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu4_1_Gelu_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu4_1_Gelu_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu4_1_Gelu_add_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu4_1_Gelu_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu4_1_Gelu_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu4_1_Gelu_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu4_1_Gelu_mul_30 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu4_1_Gelu_mul_30(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu4_1_Gelu_mul_30 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_add4_1_Add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_add4_1_Add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_add4_1_Add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_zpad5_1_strided_slice0 */
  {
    
  forward_lite_slice_reatt_n1_1_past_zpad5_1_strided_slice0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_zpad5_1_strided_slice0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_zpad5_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_zpad5_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_zpad5_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ccat5_1_concat0 */
  {
    
  forward_lite_concat_reatt_n1_1_past_ccat5_1_concat0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ccat5_1_concat0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast */
  {
    
  forward_lite_transpose_reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion */
  {
      const ai_i8* reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 201600);
    ai_float* reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(526, 1, {(stai_ptr) reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(526, 1, {(stai_ptr) reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil5_1_convolution_ExpandDims0_to_chlast_0_0_reatt_n1_1_past_dil5_1_convolution0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil5_1_convolution0 */
  {
      const ai_float* reatt_n1_1_past_dil5_1_convolution0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    ai_float* reatt_n1_1_past_dil5_1_convolution0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 168576);
    const ai_u8* reatt_n1_1_past_dil5_1_convolution0_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 473700);
    const ai_u8* reatt_n1_1_past_dil5_1_convolution0_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 141540);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(532, 1, {(stai_ptr) reatt_n1_1_past_dil5_1_convolution0_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(reatt_n1_1_past_dil5_1_convolution0_t_in_0_ptr_const_f32, reatt_n1_1_past_dil5_1_convolution0_t_out_0_ptr_f32, reatt_n1_1_past_dil5_1_convolution0_t_weight_0_ptr_const_u8, reatt_n1_1_past_dil5_1_convolution0_t_weight_1_ptr_const_u8, reatt_n1_1_past_dil5_1_convolution0_t_in_0_shape_ch_const_u32, reatt_n1_1_past_dil5_1_convolution0_t_out_0_shape_ch_const_u32, reatt_n1_1_past_dil5_1_convolution0_t_in_0_shape_w_const_u32, reatt_n1_1_past_dil5_1_convolution0_t_in_0_shape_h_const_u32, reatt_n1_1_past_dil5_1_convolution0_t_out_0_shape_w_const_u32, reatt_n1_1_past_dil5_1_convolution0_t_out_0_shape_h_const_u32, reatt_n1_1_past_dil5_1_convolution0_t_weight_0_shape_w_const_u32, reatt_n1_1_past_dil5_1_convolution0_t_weight_0_shape_h_const_u32, reatt_n1_1_past_dil5_1_convolution0_l_pad_W_0_const_s32, reatt_n1_1_past_dil5_1_convolution0_l_pad_H_0_const_s32, reatt_n1_1_past_dil5_1_convolution0_l_stride_1_const_u16, reatt_n1_1_past_dil5_1_convolution0_l_stride_0_const_u16, 1, 65, reatt_n1_1_past_dil5_1_convolution0_l_dilation_W_const_u16, reatt_n1_1_past_dil5_1_convolution0_l_dilation_H_const_u16, reatt_n1_1_past_dil5_1_convolution0_v_n_groups_const_size);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(532, 1, {(stai_ptr) reatt_n1_1_past_dil5_1_convolution0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil5_1_convolution0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion */
  {
      const ai_float* reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 168576);
    ai_i8* reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(532, 1, {(stai_ptr) reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(532, 1, {(stai_ptr) reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil5_1_convolution0_0_0_reatt_n1_1_past_dil5_1_BiasAdd0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil5_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_dil5_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil5_1_BiasAdd0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion */
  {
      const ai_i8* reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 57600);
    ai_float* reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(538, 1, {(stai_ptr) reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(538, 1, {(stai_ptr) reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_dil5_1_BiasAdd0_0_0_reatt_n1_1_past_ln5_1_moments_mean0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_moments_mean0 */
  {
    
  forward_lite_reduce_reatt_n1_1_past_ln5_1_moments_mean0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_moments_mean0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_moments_mean0_Mul */
  {
      ai_float* reatt_n1_1_past_ln5_1_moments_mean0_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30000);
    const ai_float* reatt_n1_1_past_ln5_1_moments_mean0_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    const ai_float* reatt_n1_1_past_ln5_1_moments_mean0_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30940);
    const ai_float* reatt_n1_1_past_ln5_1_moments_mean0_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30944);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(541, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_moments_mean0_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reatt_n1_1_past_ln5_1_moments_mean0_Mul_t_out_0_ptr_f32, reatt_n1_1_past_ln5_1_moments_mean0_Mul_t_in_0_ptr_const_f32, reatt_n1_1_past_ln5_1_moments_mean0_Mul_t_weight_0_ptr_const_f32, reatt_n1_1_past_ln5_1_moments_mean0_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(541, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_moments_mean0_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_moments_mean0_Mul */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_moments_SquaredDifference0 */
  {
    
  forward_lite_eltwise_reatt_n1_1_past_ln5_1_moments_SquaredDifference0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_moments_SquaredDifference0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion */
  {
      const ai_float* reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
    ai_i8* reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 201600);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(549, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(549, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_moments_SquaredDifference0_0_0_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_moments_SquaredDifference__760 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_moments_SquaredDifference__760(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_moments_SquaredDifference__760 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
    ai_float* reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 115200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(553, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(553, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_moments_SquaredDifference__760_0_0_reatt_n1_1_past_ln5_1_moments_variance0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_moments_variance0 */
  {
    
  forward_lite_reduce_reatt_n1_1_past_ln5_1_moments_variance0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_moments_variance0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_moments_variance0_Mul */
  {
      ai_float* reatt_n1_1_past_ln5_1_moments_variance0_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    const ai_float* reatt_n1_1_past_ln5_1_moments_variance0_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    const ai_float* reatt_n1_1_past_ln5_1_moments_variance0_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30940);
    const ai_float* reatt_n1_1_past_ln5_1_moments_variance0_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30944);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(556, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_moments_variance0_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reatt_n1_1_past_ln5_1_moments_variance0_Mul_t_out_0_ptr_f32, reatt_n1_1_past_ln5_1_moments_variance0_Mul_t_in_0_ptr_const_f32, reatt_n1_1_past_ln5_1_moments_variance0_Mul_t_weight_0_ptr_const_f32, reatt_n1_1_past_ln5_1_moments_variance0_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(556, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_moments_variance0_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_moments_variance0_Mul */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion */
  {
      const ai_float* reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    ai_i8* reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(556, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(556, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_moments_variance0_Mul_0_0_reatt_n1_1_past_ln5_1_add0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 29100);
    ai_float* reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(559, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(559, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_add0_0_0_reatt_n1_1_past_ln5_1_Rsqrt0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_Rsqrt0 */
  {
      ai_handle reatt_n1_1_past_ln5_1_Rsqrt0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
    const ai_handle reatt_n1_1_past_ln5_1_Rsqrt0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 31200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(562, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_Rsqrt0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_sqrt_if32of32(reatt_n1_1_past_ln5_1_Rsqrt0_t_out_0_ptr_handle, reatt_n1_1_past_ln5_1_Rsqrt0_t_in_0_ptr_const_handle, reatt_n1_1_past_ln5_1_Rsqrt0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(562, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_Rsqrt0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_Rsqrt0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_Rsqrt__770 */
  {
      ai_handle reatt_n1_1_past_ln5_1_Rsqrt__770_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 31200);
    const ai_handle reatt_n1_1_past_ln5_1_Rsqrt__770_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(563, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_Rsqrt__770_t_in_0_ptr_const_handle});
    
  forward_lite_nl_reciprocal_if32of32(reatt_n1_1_past_ln5_1_Rsqrt__770_t_out_0_ptr_handle, reatt_n1_1_past_ln5_1_Rsqrt__770_t_in_0_ptr_const_handle, reatt_n1_1_past_ln5_1_Rsqrt__770_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(563, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_Rsqrt__770_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_Rsqrt__770 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion */
  {
      const ai_float* reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 31200);
    ai_i8* reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(563, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(563, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_Rsqrt__770_0_0_reatt_n1_1_past_ln5_1_mul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__2210 */
  {
    
  forward_lite_transpose_Transpose__2210(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__2210 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_Neg0 */
  {
      ai_handle reatt_n1_1_past_ln5_1_Neg0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 28800);
    const ai_handle reatt_n1_1_past_ln5_1_Neg0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(545, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_Neg0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(reatt_n1_1_past_ln5_1_Neg0_t_out_0_ptr_handle, reatt_n1_1_past_ln5_1_Neg0_t_in_0_ptr_const_handle, reatt_n1_1_past_ln5_1_Neg0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(545, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_Neg0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_Neg0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion */
  {
      const ai_float* reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 28800);
    ai_i8* reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 30000);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(545, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_t_out_0_ptr_s8, reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(545, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_Neg0_0_0_reatt_n1_1_past_ln5_1_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_add_10 */
  /* LITE_KERNEL_SECTION BEGIN Transpose__1570 */
  {
    
  forward_lite_transpose_Transpose__1570(net_ctx);
  }
  /* LITE_KERNEL_SECTION END Transpose__1570 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_add_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_ln5_1_add_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_add_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion */
  {
      const ai_i8* reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
    ai_float* reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 115200);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(581, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(581, 1, {(stai_ptr) reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_ln5_1_add_20_0_0_reatt_n1_1_past_gelu5_1_Gelu_Pow0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu5_1_Gelu_Pow0 */
  {
    
  forward_lite_eltwise_reatt_n1_1_past_gelu5_1_Gelu_Pow0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu5_1_Gelu_Pow0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion */
  {
      const ai_float* reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 115200);
    ai_i8* reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 28800);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(585, 1, {(stai_ptr) reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_t_out_0_ptr_s8, reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(585, 1, {(stai_ptr) reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu5_1_Gelu_Pow0_0_1_reatt_n1_1_past_gelu5_1_Gelu_mul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu5_1_Gelu_mul_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu5_1_Gelu_mul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu5_1_Gelu_mul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu5_1_Gelu_add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu5_1_Gelu_add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu5_1_Gelu_add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu5_1_Gelu_mul_20 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu5_1_Gelu_mul_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu5_1_Gelu_mul_20 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu5_1_Gelu_Tanh0 */
  {
    
  forward_lite_nl_integer_reatt_n1_1_past_gelu5_1_Gelu_Tanh0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu5_1_Gelu_Tanh0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu5_1_Gelu_add_10 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu5_1_Gelu_add_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu5_1_Gelu_add_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu5_1_Gelu_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu5_1_Gelu_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu5_1_Gelu_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_gelu5_1_Gelu_mul_30 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_gelu5_1_Gelu_mul_30(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_gelu5_1_Gelu_mul_30 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_add5_1_Add0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_add5_1_Add0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_add5_1_Add0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose */
  {
    
  forward_lite_transpose_reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_in_transpose */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense */
  {
    
  forward_lite_conv2d_integer_SSSA_reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion */
  {
      const ai_i8* reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 57984);
    ai_float* reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(614, 1, {(stai_ptr) reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(614, 1, {(stai_ptr) reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_gemm_to_dense_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_Softmax0 */
  {
      ai_handle reatt_n1_1_past_pool_1_Softmax0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 1200);
    const ai_handle reatt_n1_1_past_pool_1_Softmax0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 0);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(620, 1, {(stai_ptr) reatt_n1_1_past_pool_1_Softmax0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_softmax_if32of32(reatt_n1_1_past_pool_1_Softmax0_t_out_0_ptr_handle, reatt_n1_1_past_pool_1_Softmax0_t_in_0_ptr_const_handle, reatt_n1_1_past_pool_1_Softmax0_t_in_0_shape_ch_h_prod_const_s32, 1, 300);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(620, 1, {(stai_ptr) reatt_n1_1_past_pool_1_Softmax0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_pool_1_Softmax0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion */
  {
      const ai_float* reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 1200);
    ai_i8* reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(620, 1, {(stai_ptr) reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(620, 1, {(stai_ptr) reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_pool_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_pool_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion */
  {
      const ai_i8* reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 57600);
    ai_float* reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 86400);
  
  _STAI_REATTN1C_EVENT_NODE_START_CB(626, 1, {(stai_ptr) reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_out_0_ptr_f32, reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_REATTN1C_EVENT_NODE_STOP_CB(626, 1, {(stai_ptr) reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion */
  /* LITE_KERNEL_SECTION BEGIN past_pool */
  {
    
  forward_lite_reduce_past_pool(net_ctx);
  }
  /* LITE_KERNEL_SECTION END past_pool */
  return net_ctx->_return_code;
}

/*****************************************************************************/
/*  Getters APIs Section  */
STAI_API_ENTRY
stai_size stai_reattn1c_get_context_size()
{
  return (stai_size)STAI_REATTN1C_CONTEXT_SIZE;
}

#if defined(HAVE_REATTN1C_INFO)
STAI_API_ENTRY
stai_return_code stai_reattn1c_get_info(
  stai_network* network,
  stai_network_info* info)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, info==NULL, STAI_ERROR_NETWORK_INVALID_INFO, net_ctx->_return_code)

  // Copy of network info struct
  *info = g_reattn1c_info;

  return STAI_SUCCESS;
}
#endif


STAI_API_ENTRY
stai_return_code stai_reattn1c_get_activations(
  stai_network* network, stai_ptr* activations, stai_size* n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, !n_activations, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_activations = STAI_REATTN1C_ACTIVATIONS_NUM;
for (stai_size idx=0; activations && (idx<STAI_REATTN1C_ACTIVATIONS_NUM); idx++) {
    // get address of the activations buffers
    activations[idx] = net_ctx->_activations[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reattn1c_get_weights(
  stai_network* network, stai_ptr* weights, stai_size* n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_weights, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_weights = STAI_REATTN1C_WEIGHTS_NUM;
for (stai_size idx=0; weights && (idx<STAI_REATTN1C_WEIGHTS_NUM); idx++) {
    // get address of the weights buffers
    weights[idx] = net_ctx->_weights[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reattn1c_get_inputs(
  stai_network* network, stai_ptr* inputs, stai_size* n_inputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_inputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_inputs = STAI_REATTN1C_IN_NUM;
  for (stai_size idx=0; inputs && (idx<STAI_REATTN1C_IN_NUM); idx++) {
    inputs[idx] = net_ctx->_inputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reattn1c_get_outputs(
  stai_network* network, stai_ptr* outputs, stai_size* n_outputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_outputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_outputs = STAI_REATTN1C_OUT_NUM;
  for (stai_size idx=0; outputs && (idx<STAI_REATTN1C_OUT_NUM); idx++) {
    outputs[idx] = net_ctx->_outputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reattn1c_get_error(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /* return 1st generated error or STAI_SUCCESS if no errors so far */
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reattn1c_get_states(
  stai_network* network, stai_ptr* states, stai_size* n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_states, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  /* get the number of internals states (supporting multi-heap also for internal states) */
  *n_states = STAI_REATTN1C_STATES_NUM;

  STAI_UNUSED(states)
return net_ctx->_return_code;
}


/*****************************************************************************/
/*  Setters APIs Section  */

STAI_API_ENTRY
stai_return_code stai_reattn1c_set_activations(
  stai_network* network,
  const stai_ptr* activations,
  const stai_size n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _activations_alignment[] = STAI_REATTN1C_ACTIVATIONS_ALIGNMENTS;
  STAI_PRINT("  [stai_reattn1c_set_activations] network(%p) activations[%d]: %p\n\n", net_ctx, n_activations, activations)
  _STAI_SET_ERROR(net_ctx, !activations,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_activations!=STAI_REATTN1C_ACTIVATIONS_NUM,
                  STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_NUM, net_ctx->_return_code)

  for (stai_size idx=0; activations && idx<STAI_REATTN1C_ACTIVATIONS_NUM; idx++) {
    STAI_PRINT("  activation[%d]: %p\n", idx, activations[idx])
    _STAI_SET_ERROR(net_ctx, activations[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)activations[idx]) & (_activations_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_activations[idx] = activations[idx];
  }_stai_reattn1c_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reattn1c_set_weights(
  stai_network* network,
  const stai_ptr* weights,
  const stai_size n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _weights_alignment[] = STAI_REATTN1C_WEIGHTS_ALIGNMENTS;
  _STAI_SET_ERROR(net_ctx, !weights,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_weights!=STAI_REATTN1C_WEIGHTS_NUM,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_NUM, net_ctx->_return_code)
  for (stai_size idx=0; weights && idx<STAI_REATTN1C_WEIGHTS_NUM; idx++) {
    STAI_PRINT("  weight[%d]: %p\n", idx, weights[idx])
    _STAI_SET_ERROR(net_ctx, weights[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)weights[idx]) & (_weights_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_weights[idx] = weights[idx];
  }_stai_reattn1c_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reattn1c_set_inputs(
  stai_network* network,
  const stai_ptr* inputs,
  const stai_size n_inputs)
{
  const uintptr_t _inputs_alignment[] = STAI_REATTN1C_IN_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !inputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_inputs!=STAI_REATTN1C_IN_NUM,
                  STAI_ERROR_NETWORK_INVALID_IN_NUM, net_ctx->_return_code)

  for (stai_size idx=0; inputs && idx<STAI_REATTN1C_IN_NUM; idx++) {
    STAI_PRINT("  input[%d]: %p\n", idx, inputs[idx])
    _STAI_SET_ERROR(net_ctx, inputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)inputs[idx]) & (_inputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_inputs[idx] = inputs[idx];
  }

  _stai_reattn1c_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reattn1c_set_outputs(
  stai_network* network,
  const stai_ptr* outputs,
  const stai_size n_outputs)
{
  const uintptr_t _outputs_alignment[] = STAI_REATTN1C_OUT_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !outputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_outputs!=STAI_REATTN1C_OUT_NUM,
                  STAI_ERROR_NETWORK_INVALID_OUT_NUM, net_ctx->_return_code)

  for (stai_size idx=0; outputs && idx<n_outputs; idx++) {
    STAI_PRINT("  output[%d]: %p\n", idx, outputs[idx])
    _STAI_SET_ERROR(net_ctx, outputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)outputs[idx]) & (_outputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_outputs[idx] = outputs[idx];
  }

  _stai_reattn1c_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reattn1c_set_states(
  stai_network* network,
  const stai_ptr* states,
  const stai_size n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  STAI_UNUSED(states)
  STAI_UNUSED(n_states)
_stai_reattn1c_check(net_ctx);
  return net_ctx->_return_code;
}

STAI_API_ENTRY
stai_return_code stai_reattn1c_set_callback(
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
#undef _STAI_REATTN1C_EVENT_NODE_START_CB
#undef _STAI_REATTN1C_EVENT_NODE_STOP_CB
#undef _STAI_REATTN1C_MODEL_SIGNATURE
#undef _STAI_REATTN1C_DATETIME
#undef _STAI_REATTN1C_COMPILE_DATETIME

