/**
  ******************************************************************************
  * @file    dl2c.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-09-23T02:04:52+0800
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
#include "dl2c.h"
#include "dl2c_details.h"
#include "dl2c_data.h"
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
#ifndef _STAI_DL2C_EVENT_NODE_START_CB
  #define _STAI_DL2C_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
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
  #define _STAI_DL2C_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_DL2C_EVENT_NODE_START_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_START_CB */

#ifdef STAI_EVENT_NODE_STOP_CB
#ifndef _STAI_DL2C_EVENT_NODE_STOP_CB
  #define _STAI_DL2C_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
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
  #define _STAI_DL2C_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_DL2C_EVENT_NODE_STOP_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_STOP_CB */


/*****************************************************************************/
#define _STAI_DL2C_MODEL_SIGNATURE     "0x5471fd9a5b979f3b2cf7a7a5da1d0b31"
#define _STAI_DL2C_DATETIME            "2026-09-23T02:04:52+0800"
#define _STAI_DL2C_COMPILE_DATETIME    __DATE__ " " __TIME__

#define _STAI_CONTEXT_ALIGNMENT        STAI_DL2C_CONTEXT_ALIGNMENT

/*****************************************************************************/
#define g_dl2c_activations_1     (NULL)




#if defined(HAVE_DL2C_INFO)
/*****************************************************************************/
static const stai_network_info g_dl2c_info = {
  .model_signature = _STAI_DL2C_MODEL_SIGNATURE,
  .c_compile_datetime = _STAI_DL2C_COMPILE_DATETIME,
  .c_model_name = STAI_DL2C_MODEL_NAME,
  .c_model_datetime = _STAI_DL2C_DATETIME,
  .c_model_signature = 0x0,
  .runtime_version = STAI_INIT_VERSION(12, 0, 1),
  .tool_version = STAI_INIT_VERSION(4, 0, 1),
  .api_version = STAI_INIT_VERSION(1, 0, 0),
  .n_macc = STAI_DL2C_MACC_NUM,
  .n_nodes = STAI_DL2C_NODES_NUM,
  .flags = STAI_DL2C_FLAGS,
  .n_inputs = STAI_DL2C_IN_NUM,
  .n_outputs = STAI_DL2C_OUT_NUM,
  .n_activations = STAI_DL2C_ACTIVATIONS_NUM,
  .n_weights = STAI_DL2C_WEIGHTS_NUM,
  .n_states = STAI_DL2C_STATES_NUM,
  .inputs = (stai_tensor[STAI_DL2C_IN_NUM]) {
    STAI_INIT_TENSOR(
      STAI_DL2C_IN_1_NAME,
      STAI_DL2C_IN_1_FLAGS,
      STAI_DL2C_IN_1_FORMAT,
      STAI_DL2C_IN_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 30, 128),
      STAI_DECLARE_ARRAY(float, 1, 0.028821656480431557f),
      STAI_DECLARE_ARRAY(int16_t, 1, 5)),
    },
    .outputs = (stai_tensor[STAI_DL2C_OUT_NUM]) {
    STAI_INIT_TENSOR(
      STAI_DL2C_OUT_1_NAME,
      STAI_DL2C_OUT_1_FLAGS,
      STAI_DL2C_OUT_1_FORMAT,
      STAI_DL2C_OUT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 30, 128),
      STAI_DECLARE_ARRAY(float, 1, 0.16095532476902008f),
      STAI_DECLARE_ARRAY(int16_t, 1, -14)),
    },
  .activations = (stai_tensor[STAI_DL2C_ACTIVATIONS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_DL2C_ACTIVATION_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_DL2C_ACTIVATION_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 45128),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_DL2C_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_DL2C_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_DL2C_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 66572),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },

  .states = NULL
};
#endif

#define _STAI_CONTEXT_ACQUIRE(_net_ctx, _net_handle) \
  _stai_dl2c_context* _net_ctx = (_stai_dl2c_context*)(_net_handle); \
  STAI_ASSERT(_net_ctx != NULL) \
  _STAI_SET_ERROR(_net_ctx, _net_ctx->_magic != STAI_MAGIC, \
                  STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE, _net_ctx->_return_code)


/*****************************************************************************/
static
void _stai_dl2c_check(_stai_dl2c_context* net_ctx)
{
  stai_size idx;

// Check activations status
  for (idx=0; idx<STAI_DL2C_ACTIVATIONS_NUM; idx++) {
    if (net_ctx->_activations[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_DL2C_ACTIVATIONS_NUM) ? STAI_FLAG_ACTIVATIONS : STAI_FLAG_NONE;
// Check inputs status
  for (idx=0; idx<STAI_DL2C_IN_NUM; idx++) {
    if (net_ctx->_inputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_DL2C_IN_NUM) ? STAI_FLAG_INPUTS : STAI_FLAG_NONE;

  // Check outputs status
  for (idx=0; idx<STAI_DL2C_OUT_NUM; idx++) {
    if (net_ctx->_outputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_DL2C_OUT_NUM) ? STAI_FLAG_OUTPUTS : STAI_FLAG_NONE;

// Check weights status
  for (idx=0; idx<STAI_DL2C_WEIGHTS_NUM; idx++) {
    if (net_ctx->_weights[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_DL2C_WEIGHTS_NUM) ? STAI_FLAG_WEIGHTS : STAI_FLAG_NONE;
STAI_PRINT("  [_stai_network_check] flags: 0x%08x\n", net_ctx->_flags)
}


/*****************************************************************************/
STAI_API_ENTRY
stai_return_code stai_dl2c_init(
  stai_network* network)
{
  /* Memory where to store internal context is provided by applications as a raw byte buffer */
  _stai_dl2c_context* net_ctx = (_stai_dl2c_context*)(network);
  net_ctx->_return_code = STAI_SUCCESS;
  STAI_PRINT("[Entering Network Init] network(%p) context_size(%d)\n", net_ctx, (int32_t)sizeof(_stai_dl2c_context))

  _STAI_SET_ERROR(net_ctx, STAI_DL2C_CONTEXT_SIZE != sizeof(_stai_dl2c_context),
                 STAI_ERROR_NETWORK_INVALID_CONTEXT_SIZE, net_ctx->_return_code)

  {
    const _stai_dl2c_context _dl2c_context = {
      ._magic = STAI_MAGIC,
      ._signature = STAI_DL2C_MODEL_SIGNATURE,
      ._flags = STAI_DL2C_FLAGS,
      ._return_code = STAI_SUCCESS,
      ._callback = NULL,
      ._callback_cookie = NULL,
      ._activations = {
      (stai_ptr)g_dl2c_activations_1
      },
      ._weights = {
      (stai_ptr)g_dl2c_weights_array
      },
      ._inputs = {
    NULL},
      ._outputs = {
    NULL},
    };

    // Deep copy of internal context to opaque buffer provided by app
    *net_ctx = _dl2c_context;

    _stai_dl2c_check(net_ctx);
  }

  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl2c_deinit(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /*  Reset flags to initial state  */
  net_ctx->_flags = STAI_DL2C_FLAGS;
  return net_ctx->_return_code;
}

/*****************************************************************************/



/* Int quant #0 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08840194344520569f),
    AI_PACK_INTQ_ZP(-3)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08840463310480118f),
    AI_PACK_INTQ_ZP(-3)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_k_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.689320561126806e-05f),
    AI_PACK_INTQ_ZP(-13)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08840463310480118f),
    AI_PACK_INTQ_ZP(-3)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09217384457588196f),
    AI_PACK_INTQ_ZP(-14)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09258094429969788f),
    AI_PACK_INTQ_ZP(-15)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_q_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0014245195779949427f),
    AI_PACK_INTQ_ZP(5)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09258094429969788f),
    AI_PACK_INTQ_ZP(-15)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(4.831122875213623f),
    AI_PACK_INTQ_ZP(-26)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.8540298938751221f),
    AI_PACK_INTQ_ZP(-26)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut2_mha_1_mul_y0_DequantizeLinear_Output_const_4D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0006932419491931796f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #11 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06436857581138611f),
    AI_PACK_INTQ_ZP(7)))

/* Int quant #12 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06434761732816696f),
    AI_PACK_INTQ_ZP(7)))

/* Int quant #13 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_v_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0009909427026286721f),
    AI_PACK_INTQ_ZP(-13)))

/* Int quant #14 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06434761732816696f),
    AI_PACK_INTQ_ZP(7)))

/* Int quant #15 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05957232788205147f),
    AI_PACK_INTQ_ZP(8)))

/* Int quant #16 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05957232788205147f),
    AI_PACK_INTQ_ZP(8)))

/* Int quant #17 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.16120177507400513f),
    AI_PACK_INTQ_ZP(-14)))

/* Int quant #18 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.16095532476902008f),
    AI_PACK_INTQ_ZP(-14)))

/* Int quant #19 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n2_1_fut0_mha_1_o_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0006053481483832002f),
    AI_PACK_INTQ_ZP(5)))



/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 128, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 128, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3600, AI_STATIC)

/* Array#12 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3600, AI_STATIC)

/* Array#13 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3600, AI_STATIC)

/* Array#14 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut2_mha_1_mul_y0_DequantizeLinear_Output_const_4D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#15 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 128, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_Softmax0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3600, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#22 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul_10_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#23 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#24 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#25 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3840, AI_STATIC)

/* Array#26 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_output_array, AI_ARRAY_FORMAT_S8|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 3840, AI_STATIC)

/* Array#27 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_o_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 128, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output, AI_STATIC,
  10, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output_array, &reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output_array_intq)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D, AI_STATIC,
  12, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut0_mha_1_k_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array, &reatt_n2_1_fut0_mha_1_k_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_output, AI_STATIC,
  14, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_output_array, &reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_output_array_intq)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output0, AI_STATIC,
  11, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 1, 1, 32, 128),
  1, &reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output_array, &reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output_array_intq)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_output, AI_STATIC,
  31, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 32, 4), AI_STRIDE_INIT(4, 1, 1, 30, 960),
  1, &reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_output_array, &reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_output_array_intq)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output, AI_STATIC,
  32, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output_array, &reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output_array_intq)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D, AI_STATIC,
  34, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut0_mha_1_q_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array, &reatt_n2_1_fut0_mha_1_q_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_output, AI_STATIC,
  35, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_output_array, &reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_output_array_intq)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_output, AI_STATIC,
  27, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 1, 1, 32, 960),
  1, &reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_output_array, &reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_output_array_intq)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output0, AI_STATIC,
  33, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 1, 1, 32, 128),
  1, &reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output_array, &reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output_array_intq)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output, AI_STATIC,
  3, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output_array, NULL)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul0_output, AI_STATIC,
  4, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 30, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3600),
  1, &reatt_n2_1_fut0_mha_1_MatMul0_output_array, NULL)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output, AI_STATIC,
  26, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output_array, NULL)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output, AI_STATIC,
  30, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 32, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3840),
  1, &reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output_array, NULL)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_output, AI_STATIC,
  1, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 30, 4), AI_STRIDE_INIT(4, 1, 1, 30, 900),
  1, &reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_output_array, &reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_output_array_intq)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_mul0_output, AI_STATIC,
  18, 0x1,
  AI_SHAPE_INIT(4, 1, 30, 30, 4), AI_STRIDE_INIT(4, 1, 1, 30, 900),
  1, &reatt_n2_1_fut0_mha_1_mul0_output_array, &reatt_n2_1_fut0_mha_1_mul0_output_array_intq)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut2_mha_1_mul_y0_DequantizeLinear_Output_const_4D, AI_STATIC,
  44, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n2_1_fut2_mha_1_mul_y0_DequantizeLinear_Output_const_4D_array, &reatt_n2_1_fut2_mha_1_mul_y0_DequantizeLinear_Output_const_4D_array_intq)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output, AI_STATIC,
  38, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output_array, &reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output_array_intq)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D, AI_STATIC,
  40, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut0_mha_1_v_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array, &reatt_n2_1_fut0_mha_1_v_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_output, AI_STATIC,
  41, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_output_array, &reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_output_array_intq)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_output, AI_STATIC,
  29, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 1, 1, 32, 960),
  1, &reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_output_array, &reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_output_array_intq)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output0, AI_STATIC,
  39, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 1, 1, 32, 128),
  1, &reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output_array, &reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output_array_intq)

/* Tensor #22 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_output, AI_STATIC,
  7, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_output_array, NULL)

/* Tensor #23 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul_10_output, AI_STATIC,
  8, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &reatt_n2_1_fut0_mha_1_MatMul_10_output_array, NULL)

/* Tensor #24 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_Softmax0_output, AI_STATIC,
  9, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 30, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3600),
  1, &reatt_n2_1_fut0_mha_1_Softmax0_output_array, NULL)

/* Tensor #25 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_output, AI_STATIC,
  28, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_output_array, NULL)

/* Tensor #26 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_output, AI_STATIC,
  5, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 1, 1, 32, 960),
  1, &reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_output_array, &reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_output_array_intq)

/* Tensor #27 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_output, AI_STATIC,
  24, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 1, 1, 32, 128),
  1, &reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_output_array, &reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_output_array_intq)

/* Tensor #28 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_output, AI_STATIC,
  19, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_output_array, &reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_output_array_intq)

/* Tensor #29 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_o_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D, AI_STATIC,
  20, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut0_mha_1_o_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array, &reatt_n2_1_fut0_mha_1_o_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #30 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_output, AI_STATIC,
  21, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_output_array, &reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_output_array_intq)


AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_output, &reatt_n2_1_fut0_mha_1_k_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_layer, 33,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_chain,
  NULL, &reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_layer, 51,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_chain,
  NULL, &reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_output, &reatt_n2_1_fut0_mha_1_q_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_layer, 32,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_chain,
  NULL, &reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_layer, 50,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_chain,
  NULL, &reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output, &reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output, &reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_MatMul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul0_layer, 58,
  MATMUL_TYPE, 0x0, NULL,
  matmul, forward_matmul,
  &reatt_n2_1_fut0_mha_1_MatMul0_chain,
  NULL, &reatt_n2_1_fut0_mha_1_MatMul0_layer, AI_STATIC, 
  .alpha = 1.0, 
  .beta = 1.0, 
  .tA = 0, 
  .tB = 0, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_mul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_output, &reatt_n2_1_fut2_mha_1_mul_y0_DequantizeLinear_Output_const_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_mul0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_mul0_layer, 61,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fut0_mha_1_mul0_chain,
  NULL, &reatt_n2_1_fut0_mha_1_mul0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_output, &reatt_n2_1_fut0_mha_1_v_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_layer, 31,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_chain,
  NULL, &reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_layer, 49,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_chain,
  NULL, &reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &reatt_n2_1_fut0_mha_1_Softmax0_output, &reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_output, &reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_MatMul_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_MatMul_10_layer, 67,
  MATMUL_TYPE, 0x0, NULL,
  matmul, forward_matmul,
  &reatt_n2_1_fut0_mha_1_MatMul_10_chain,
  NULL, &reatt_n2_1_fut0_mha_1_MatMul_10_layer, AI_STATIC, 
  .alpha = 1.0, 
  .beta = 1.0, 
  .tA = 0, 
  .tB = 0, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_layer, 70,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_chain,
  NULL, &reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_output, &reatt_n2_1_fut0_mha_1_o_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_layer, 82,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_chain,
  NULL, &reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)
/**  Hybrid layers declarations section  *************************************/
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fut0_mha_1_k_1_BiasAdd0(_stai_dl2c_context* net_ctx)
{
  reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25920);
  reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25920);
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 396);
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 396);
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 29768);
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29768);
  _STAI_DL2C_EVENT_NODE_START_CB(33, 2, { reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_output.data->data,reatt_n2_1_fut0_mha_1_k_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_layer);
  _STAI_DL2C_EVENT_NODE_STOP_CB(33, 1, { reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output.data->data});
}
void forward_lite_transpose_reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0(_stai_dl2c_context* net_ctx)
{
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 29768);
  reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29768);
  reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_output_array.data = AI_PTR(net_ctx->_activations[0] + 25920);
  reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 25920);
  _STAI_DL2C_EVENT_NODE_START_CB(51, 1, { reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output0.data->data});
  forward_transpose(&reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_layer);
  _STAI_DL2C_EVENT_NODE_STOP_CB(51, 1, { reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fut0_mha_1_q_1_BiasAdd0(_stai_dl2c_context* net_ctx)
{
  reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 29768);
  reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29768);
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 140);
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 140);
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 10560);
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10560);
  _STAI_DL2C_EVENT_NODE_START_CB(32, 2, { reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_output.data->data,reatt_n2_1_fut0_mha_1_q_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_layer);
  _STAI_DL2C_EVENT_NODE_STOP_CB(32, 1, { reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output.data->data});
}
void forward_lite_transpose_reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0(_stai_dl2c_context* net_ctx)
{
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 10560);
  reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10560);
  reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_output_array.data = AI_PTR(net_ctx->_activations[0] + 6720);
  reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 6720);
  _STAI_DL2C_EVENT_NODE_START_CB(50, 1, { reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output0.data->data});
  forward_transpose(&reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_layer);
  _STAI_DL2C_EVENT_NODE_STOP_CB(50, 1, { reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_output.data->data});
}
void forward_lite_matmul_reatt_n2_1_fut0_mha_1_MatMul0(_stai_dl2c_context* net_ctx)
{
  reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 29768);
  reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29768);
  reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 14400);
  reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 14400);
  reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 29764);
  reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29764);
  reatt_n2_1_fut0_mha_1_MatMul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fut0_mha_1_MatMul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_DL2C_EVENT_NODE_START_CB(58, 3, { reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output.data->data,reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output.data->data,reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output.data->data});
  forward_matmul(&reatt_n2_1_fut0_mha_1_MatMul0_layer);
  _STAI_DL2C_EVENT_NODE_STOP_CB(58, 1, { reatt_n2_1_fut0_mha_1_MatMul0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fut0_mha_1_mul0(_stai_dl2c_context* net_ctx)
{
  reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 14400);
  reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 14400);
  reatt_n2_1_fut2_mha_1_mul_y0_DequantizeLinear_Output_const_4D_array.data = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n2_1_fut2_mha_1_mul_y0_DequantizeLinear_Output_const_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 8);
  reatt_n2_1_fut0_mha_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fut0_mha_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_DL2C_EVENT_NODE_START_CB(61, 2, { reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_output.data->data,reatt_n2_1_fut2_mha_1_mul_y0_DequantizeLinear_Output_const_4D.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fut0_mha_1_mul0_layer);
  _STAI_DL2C_EVENT_NODE_STOP_CB(61, 1, { reatt_n2_1_fut0_mha_1_mul0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fut0_mha_1_v_1_BiasAdd0(_stai_dl2c_context* net_ctx)
{
  reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 15424);
  reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15424);
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 12);
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19264);
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19264);
  _STAI_DL2C_EVENT_NODE_START_CB(31, 2, { reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_output.data->data,reatt_n2_1_fut0_mha_1_v_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_layer);
  _STAI_DL2C_EVENT_NODE_STOP_CB(31, 1, { reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output.data->data});
}
void forward_lite_transpose_reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0(_stai_dl2c_context* net_ctx)
{
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19264);
  reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19264);
  reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_output_array.data = AI_PTR(net_ctx->_activations[0] + 14400);
  reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 14400);
  _STAI_DL2C_EVENT_NODE_START_CB(49, 1, { reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output0.data->data});
  forward_transpose(&reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_layer);
  _STAI_DL2C_EVENT_NODE_STOP_CB(49, 1, { reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_output.data->data});
}
void forward_lite_matmul_reatt_n2_1_fut0_mha_1_MatMul_10(_stai_dl2c_context* net_ctx)
{
  reatt_n2_1_fut0_mha_1_Softmax0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fut0_mha_1_Softmax0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 29764);
  reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29764);
  reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 29760);
  reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29760);
  reatt_n2_1_fut0_mha_1_MatMul_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 14400);
  reatt_n2_1_fut0_mha_1_MatMul_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 14400);
  _STAI_DL2C_EVENT_NODE_START_CB(67, 3, { reatt_n2_1_fut0_mha_1_Softmax0_output.data->data,reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_output.data->data,reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_output.data->data});
  forward_matmul(&reatt_n2_1_fut0_mha_1_MatMul_10_layer);
  _STAI_DL2C_EVENT_NODE_STOP_CB(67, 1, { reatt_n2_1_fut0_mha_1_MatMul_10_output.data->data});
}
void forward_lite_transpose_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0(_stai_dl2c_context* net_ctx)
{
  reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_output_array.data = AI_PTR(net_ctx->_activations[0] + 3840);
  reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 3840);
  _STAI_DL2C_EVENT_NODE_START_CB(70, 1, { reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_output.data->data});
  forward_transpose(&reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_layer);
  _STAI_DL2C_EVENT_NODE_STOP_CB(70, 1, { reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n2_1_fut0_mha_1_o_1_BiasAdd0(_stai_dl2c_context* net_ctx)
{
  reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 7680);
  reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 7680);
  reatt_n2_1_fut0_mha_1_o_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 268);
  reatt_n2_1_fut0_mha_1_o_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 268);
  reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_output_array.data = AI_PTR(net_ctx->_outputs[0] + 0);
  reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_output_array.data_start = AI_PTR(net_ctx->_outputs[0] + 0);
  _STAI_DL2C_EVENT_NODE_START_CB(82, 2, { reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_output.data->data,reatt_n2_1_fut0_mha_1_o_1_BiasAdd_ReadVariableOp0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_layer);
  _STAI_DL2C_EVENT_NODE_STOP_CB(82, 1, { reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_output.data->data});
}

/*****************************************************************************/


static const ai_u32 reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 1;
static const ai_float reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_t_in_0_fmt_scale_const_f32 = 0.05957232788205147f;
static const ai_i8 reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_t_in_0_fmt_zero_const_s8 = 8;

static const ai_u32 reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 1;
static const ai_float reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_fmt_scale_const_f32 = 4.831122875213623f;
static const ai_i8 reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_fmt_zero_const_s8 = -26;

static const ai_u16 reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_shape_h_const_u16 = 30;
static const ai_u16 reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_shape_ch_const_u16 = 128;
static const ai_u16 reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_out_0_shape_ch_const_u16 = 128;
static const ai_i8 reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_fmt_zero_const_s8 = 5;
static const ai_i8 reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_out_0_fmt_zero_const_s8 = -3;
static const ai_float reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_fmt_scale_const_f32 = 0.028821656480431557f;
static const ai_float reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_out_0_fmt_scale_const_f32 = 0.08840194344520569f;
static const ai_float reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0030442173592746258f, 0.0026697523426264524f, 0.0020745964720845222f, 0.0024887784384191036f, 0.0027357246726751328f, 0.0037314067594707012f, 0.003287884406745434f, 0.0021063426975160837f, 0.0035386518575251102f, 0.003920665476471186f, 0.002670444082468748f, 0.0019545473624020815f, 0.001972493715584278f, 0.0030257198959589005f, 0.0022762746084481478f, 0.0029384351801127195f, 0.0027759778313338757f, 0.001616743509657681f, 0.003146868199110031f, 0.0016718691913411021f, 0.0021702605299651623f, 0.0027194847352802753f, 0.002268270356580615f, 0.002230522921308875f, 0.0017072203336283565f, 0.0019078431650996208f, 0.002649615751579404f, 0.002047196961939335f, 0.002154521644115448f, 0.0023659884463995695f, 0.002358141355216503f, 0.0026224569883197546f, 0.0017391659785062075f, 0.003182660322636366f, 0.0021991825196892023f, 0.0019500373164191842f, 0.002664548810571432f, 0.0020168637856841087f, 0.00204846472479403f, 0.002255628816783428f, 0.0018956804415211082f, 0.0024917495902627707f, 0.0026446899864822626f, 0.002566163195297122f, 0.002406173851341009f, 0.002247363096103072f, 0.00191111967433244f, 0.002674509771168232f, 0.002199909882619977f, 0.002024298068135977f, 0.002154171233996749f, 0.0022158038336783648f, 0.002102036029100418f, 0.0019661420956254005f, 0.0025944525841623545f, 0.0019882575143128633f, 0.002124195918440819f, 0.002346363849937916f, 0.002007346833124757f, 0.0023687805514782667f, 0.0022641471587121487f, 0.001976989209651947f, 0.00234967190772295f, 0.002767463680356741f, 0.0024892117362469435f, 0.0030457950197160244f, 0.0017720640171319246f, 0.002350844442844391f, 0.0030922761652618647f, 0.002720513613894582f, 0.002006980823352933f, 0.0023179128766059875f, 0.002685391576960683f, 0.0030899259727448225f, 0.002198000904172659f, 0.00227714772336185f, 0.002242577262222767f, 0.0021608101669698954f, 0.002018178813159466f, 0.002764421980828047f, 0.0026697800494730473f, 0.002368244342505932f, 0.001863693119958043f, 0.0026676857378333807f, 0.002588652539998293f, 0.0020299009047448635f, 0.0028761448338627815f, 0.002330286893993616f, 0.0021670819260179996f, 0.002267642877995968f, 0.0028562911320477724f, 0.0021505665499716997f, 0.0032228596974164248f, 0.0028016348369419575f, 0.0023239245638251305f, 0.0026106680743396282f, 0.0027289867866784334f, 0.0025164883118122816f, 0.0021642360370606184f, 0.002500026486814022f, 0.002464056946337223f, 0.002434673486277461f, 0.0019296319223940372f, 0.002827187767252326f, 0.002150896703824401f, 0.0031311872880905867f, 0.002170527121052146f, 0.0017765415832400322f, 0.0030515887774527073f, 0.0020535357762128115f, 0.0021423983853310347f, 0.003268393687903881f, 0.003477805759757757f, 0.0024684371892362833f, 0.0020283376798033714f, 0.0018127463990822434f, 0.0021887493785470724f, 0.0031605875119566917f, 0.0021998274605721235f, 0.0019324938766658306f, 0.0017982764402404428f, 0.002086707390844822f, 0.0023752443958073854f, 0.0022071055136621f, 0.001765551744028926f, 0.0025186447892338037f, 0.002727792365476489f, 0.002606748603284359f);
static const ai_layer_format_type reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_u32 reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3840;
static const ai_float reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_fmt_scale_const_f32 = 0.08840463310480118f;
static const ai_i8 reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_fmt_zero_const_s8 = -3;

static const ai_u16 reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_shape_h_const_u16 = 30;
static const ai_u16 reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_shape_ch_const_u16 = 128;
static const ai_u16 reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_out_0_shape_ch_const_u16 = 128;
static const ai_i8 reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_fmt_zero_const_s8 = 5;
static const ai_i8 reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_out_0_fmt_zero_const_s8 = -14;
static const ai_float reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_fmt_scale_const_f32 = 0.028821656480431557f;
static const ai_float reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_out_0_fmt_scale_const_f32 = 0.09217384457588196f;
static const ai_float reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00319608673453331f, 0.002055141143500805f, 0.0017936897929757833f, 0.002668061526492238f, 0.001751757925376296f, 0.002919370075687766f, 0.002181120915338397f, 0.0018906977493315935f, 0.002440677722916007f, 0.0026919080410152674f, 0.0023114364594221115f, 0.0015897185076028109f, 0.0018643168732523918f, 0.0037318668328225613f, 0.0015393809881061316f, 0.0025005321949720383f, 0.0029287051875144243f, 0.0020917935762554407f, 0.0028070828411728144f, 0.001496026525273919f, 0.0025892832782119513f, 0.003139378735795617f, 0.002251634607091546f, 0.0018271752633154392f, 0.002043609507381916f, 0.0016563525423407555f, 0.002706637606024742f, 0.0016680448316037655f, 0.001874892390333116f, 0.0019721544813364744f, 0.001756972400471568f, 0.0017916976939886808f, 0.0023580805864185095f, 0.001865030382759869f, 0.002810038859024644f, 0.0023276766296476126f, 0.0027897313702851534f, 0.0026985995937138796f, 0.002301962347701192f, 0.002468177815899253f, 0.0018168772803619504f, 0.0020698322914540768f, 0.0020811178255826235f, 0.0028009400703012943f, 0.002293509431183338f, 0.0018190174596384168f, 0.002382939914241433f, 0.002699559787288308f, 0.0023624140303581953f, 0.0019468081882223487f, 0.001629732782021165f, 0.0032267048954963684f, 0.002126404084265232f, 0.0022290030028671026f, 0.0018107910873368382f, 0.0025049997493624687f, 0.002512256847694516f, 0.0017220316221937537f, 0.0020669293589890003f, 0.0023247553035616875f, 0.0028381897136569023f, 0.0023590819910168648f, 0.0023058990482240915f, 0.0024882967118173838f, 0.003303742967545986f, 0.0029373064171522856f, 0.002094350755214691f, 0.0022624856792390347f, 0.0026310505345463753f, 0.002362138358876109f, 0.0021409292239695787f, 0.0018734084442257881f, 0.002064043888822198f, 0.0025155742187052965f, 0.002464852761477232f, 0.0021288227289915085f, 0.0023466586135327816f, 0.002844101982191205f, 0.0022637376096099615f, 0.002168628852814436f, 0.002678885357454419f, 0.0020266715437173843f, 0.001953168073669076f, 0.002049833070486784f, 0.0019584151450544596f, 0.0016425291541963816f, 0.0024515476543456316f, 0.0023835599422454834f, 0.002021671971306205f, 0.0022430368699133396f, 0.0023035702761262655f, 0.0018999918829649687f, 0.002286936854943633f, 0.002003482775762677f, 0.0018580603646114469f, 0.002520744688808918f, 0.002718627918511629f, 0.002286090748384595f, 0.0023981959093362093f, 0.002691221423447132f, 0.002587783383205533f, 0.002316865371540189f, 0.001692538382485509f, 0.002474572043865919f, 0.0019708969630301f, 0.0025678123347461224f, 0.0020648229401558638f, 0.0021067399065941572f, 0.0032702714670449495f, 0.0024972213432192802f, 0.0016267006285488605f, 0.002894175471737981f, 0.002249073004350066f, 0.0019023898057639599f, 0.001970728160813451f, 0.002042333362624049f, 0.0021594057325273752f, 0.00228860997594893f, 0.0020264906343072653f, 0.0025112400762736797f, 0.0017769477562978864f, 0.002608697162941098f, 0.002103249542415142f, 0.0023988857865333557f, 0.002027746755629778f, 0.0027237634640187025f, 0.0030561592429876328f, 0.0019464988727122545f);
static const ai_layer_format_type reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_u32 reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3840;
static const ai_float reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_fmt_scale_const_f32 = 0.09258094429969788f;
static const ai_i8 reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_fmt_zero_const_s8 = -15;


static const ai_u32 reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3600;
static const ai_float reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_t_out_0_fmt_scale_const_f32 = 4.831122875213623f;
static const ai_i8 reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_t_out_0_fmt_zero_const_s8 = -26;


static const ai_u32 reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3600;
static const ai_float reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_t_in_0_fmt_scale_const_f32 = 0.8540298938751221f;
static const ai_i8 reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_t_in_0_fmt_zero_const_s8 = -26;

static const ai_i32 reatt_n2_1_fut0_mha_1_Softmax0_t_in_0_shape_ch_h_w_prod_const_s32 = 3600;

static const ai_u16 reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_shape_h_const_u16 = 30;
static const ai_u16 reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_shape_ch_const_u16 = 128;
static const ai_u16 reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_out_0_shape_ch_const_u16 = 128;
static const ai_i8 reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_fmt_zero_const_s8 = 5;
static const ai_i8 reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_out_0_fmt_zero_const_s8 = 7;
static const ai_float reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_fmt_scale_const_f32 = 0.028821656480431557f;
static const ai_float reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_out_0_fmt_scale_const_f32 = 0.06436857581138611f;
static const ai_float reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0019426436629146338f, 0.0015585441142320633f, 0.0017352194990962744f, 0.002088850596919656f, 0.0018421271815896034f, 0.002060873433947563f, 0.0024926767218858004f, 0.0018072719685733318f, 0.001418686704710126f, 0.002564979949966073f, 0.0019550099968910217f, 0.001745498855598271f, 0.0013988896971568465f, 0.002353498013690114f, 0.0017481716349720955f, 0.0020154931116849184f, 0.0019561764784157276f, 0.0022742508444935083f, 0.0017301472835242748f, 0.001549919368699193f, 0.001828506588935852f, 0.0017691003158688545f, 0.0016862333286553621f, 0.0029086859431117773f, 0.0018500939477235079f, 0.0021706391125917435f, 0.001913333311676979f, 0.0017546802992001176f, 0.001585267367772758f, 0.0017928301822394133f, 0.0015298707876354456f, 0.0017236871644854546f, 0.0015145534416660666f, 0.0016868849052116275f, 0.0017554681980982423f, 0.0017507518641650677f, 0.0014295435976237059f, 0.0016177009092643857f, 0.0015747850993648171f, 0.001516612246632576f, 0.0016165535198524594f, 0.0019687546882778406f, 0.001656221691519022f, 0.001831625122576952f, 0.0015655183233320713f, 0.0018192880088463426f, 0.0016216704389080405f, 0.0014685324858874083f, 0.0013642502017319202f, 0.0019238722743466496f, 0.0015231617726385593f, 0.001649002660997212f, 0.001695666229352355f, 0.0016408830415457487f, 0.0016262908466160297f, 0.0017967240419238806f, 0.0016335304826498032f, 0.0014218032592907548f, 0.0021602713968604803f, 0.0016443514032289386f, 0.002000026637688279f, 0.0016463337233290076f, 0.0020647994242608547f, 0.0016984038520604372f, 0.001978574087843299f, 0.001859278534539044f, 0.0019256470259279013f, 0.0017302095657214522f, 0.0019425337668508291f, 0.001410030061379075f, 0.0015154778957366943f, 0.0015437256079167128f, 0.0019012270495295525f, 0.0019115130417048931f, 0.0017699849558994174f, 0.001999982399865985f, 0.0018641649512574077f, 0.0016637520166113973f, 0.001801411621272564f, 0.001501518185250461f, 0.0015711983432993293f, 0.0017161821015179157f, 0.0016894402215257287f, 0.0018778144149109721f, 0.0018053248059004545f, 0.0019158701179549098f, 0.0017647756030783057f, 0.0017541219713166356f, 0.0018343535484746099f, 0.0017732218839228153f, 0.0016862796619534492f, 0.0016030671540647745f, 0.0014502437552437186f, 0.0020235085394233465f, 0.0019237902015447617f, 0.001655911561101675f, 0.0018610515398904681f, 0.0014985939487814903f, 0.0014515803195536137f, 0.0015095785493031144f, 0.0015699868090450764f, 0.0016153700416907668f, 0.0014428338035941124f, 0.0020516489166766405f, 0.001639676047489047f, 0.0017236421117559075f, 0.0018285816768184304f, 0.001685742405243218f, 0.001428862102329731f, 0.0014230069937184453f, 0.0018180595943704247f, 0.00150585500523448f, 0.0013806828064844012f, 0.001749793766066432f, 0.001521762111224234f, 0.0017046923749148846f, 0.0021083427127450705f, 0.0018162012565881014f, 0.0017840852960944176f, 0.0018233550945296884f, 0.0018012822838500142f, 0.00155058060772717f, 0.001380941248498857f, 0.001829338725656271f, 0.0013826820068061352f, 0.001886993646621704f, 0.001632136874832213f, 0.001770115690305829f);
static const ai_layer_format_type reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_u32 reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3840;
static const ai_float reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_t_in_0_fmt_scale_const_f32 = 0.06434761732816696f;
static const ai_i8 reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_t_in_0_fmt_zero_const_s8 = 7;


static const ai_u32 reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3840;
static const ai_float reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_t_out_0_fmt_scale_const_f32 = 0.05957232788205147f;
static const ai_i8 reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_t_out_0_fmt_zero_const_s8 = 8;


static const ai_u16 reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_shape_h_const_u16 = 30;
static const ai_u16 reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_l_stride_1_const_u16 = 1;
static const ai_u16 reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_l_stride_0_const_u16 = 1;
static const ai_u16 reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_shape_ch_const_u16 = 128;
static const ai_u16 reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_out_0_shape_ch_const_u16 = 128;
static const ai_i8 reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_fmt_zero_const_s8 = 8;
static const ai_i8 reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_out_0_fmt_zero_const_s8 = -14;
static const ai_float reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_fmt_scale_const_f32 = 0.05957232788205147f;
static const ai_float reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_out_0_fmt_scale_const_f32 = 0.16120177507400513f;
static const ai_float reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.001606650184839964f, 0.0015739868395030499f, 0.0015719131333753467f, 0.0017768918769434094f, 0.0017625109758228064f, 0.0023037653882056475f, 0.001811962341889739f, 0.0016782722668722272f, 0.001691896002739668f, 0.0020514652132987976f, 0.0018609038088470697f, 0.0016046887030825019f, 0.0017151487991213799f, 0.0020059430971741676f, 0.0018114749109372497f, 0.0015882001025602221f, 0.0019211375620216131f, 0.0016210789326578379f, 0.0015088792424649f, 0.0015708456048741937f, 0.0018613870488479733f, 0.0015284062828868628f, 0.001770669943653047f, 0.001991026569157839f, 0.0016461532795801759f, 0.0018298986833542585f, 0.0018104122718796134f, 0.0017501143738627434f, 0.0023650899529457092f, 0.0018770324531942606f, 0.0017924250569194555f, 0.0015086610801517963f, 0.0014838670613244176f, 0.0017559014959260821f, 0.001735332072712481f, 0.0016939319903030992f, 0.0016988945426419377f, 0.0016581034287810326f, 0.0016017076559364796f, 0.0018586649093776941f, 0.0016665232833474874f, 0.0016023956704884768f, 0.0016414715209975839f, 0.001826141495257616f, 0.0018154961289837956f, 0.002097118878737092f, 0.0017088396707549691f, 0.0021791034378111362f, 0.0016182982362806797f, 0.0015213342849165201f, 0.0018526799976825714f, 0.001696384628303349f, 0.0017633178504183888f, 0.001788548892363906f, 0.0018976324936375022f, 0.002220174530521035f, 0.0020304566714912653f, 0.0017312390264123678f, 0.001652741339057684f, 0.001516752177849412f, 0.0015593708958476782f, 0.0015141575131565332f, 0.0020209657959640026f, 0.002140896860510111f, 0.0016119972569867969f, 0.0017117031384259462f, 0.0014282680349424481f, 0.0018165538785979152f, 0.0015555513091385365f, 0.0014411578886210918f, 0.0016855347203090787f, 0.0016657811356708407f, 0.0018192259594798088f, 0.002043923595920205f, 0.0015562138287350535f, 0.00190604361705482f, 0.002005261369049549f, 0.0018881555879488587f, 0.0020245721098035574f, 0.0017912996700033545f, 0.0016330163925886154f, 0.0015420683193951845f, 0.001498000812716782f, 0.0016594480257481337f, 0.0021067962516099215f, 0.002085354644805193f, 0.0015619088662788272f, 0.0016114127356559038f, 0.00137261429335922f, 0.0015969745581969619f, 0.0017699466552585363f, 0.0023144413717091084f, 0.001937895198352635f, 0.001963160466402769f, 0.0015267010312527418f, 0.001802933169528842f, 0.0013374359114095569f, 0.001608784543350339f, 0.0015106529463082552f, 0.0019268898759037256f, 0.0015659722266718745f, 0.0016849340172484517f, 0.0014393049059435725f, 0.0016004893695935607f, 0.0015777135267853737f, 0.001758933300152421f, 0.0016980315558612347f, 0.0016397095751017332f, 0.0017065429128706455f, 0.0018414389342069626f, 0.002130783861503005f, 0.0019751235377043486f, 0.0017820574576035142f, 0.002557729836553335f, 0.0016645193099975586f, 0.0017574885860085487f, 0.0018172826385125518f, 0.002120282268151641f, 0.0017871741438284516f, 0.0016383880283683538f, 0.0018735176417976618f, 0.0019054983276873827f, 0.0014644521288573742f, 0.0016159169608727098f, 0.0017894660122692585f, 0.002089414745569229f, 0.0016336740227416158f, 0.003256905358284712f);
static const ai_layer_format_type reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

STAI_API_ENTRY
stai_return_code stai_dl2c_run(
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


  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion */
  {
      const ai_i8* reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 0);
    ai_float* reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 29760);
  
  _STAI_DL2C_EVENT_NODE_START_CB(67, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_t_in_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_t_out_0_ptr_f32, reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_t_in_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(67, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion */
  {
      const ai_i8* reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 4);
    ai_float* reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 29764);
  
  _STAI_DL2C_EVENT_NODE_START_CB(58, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_ptr_f32, reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(58, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0 */
  {
      const ai_i8* reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_inputs[0] + 0);
    const ai_i8* reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 524);
    const ai_i32* reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 16908);
    ai_i8* reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 25920);
    ai_i16* reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 29768);
  
  _STAI_DL2C_EVENT_NODE_START_CB(15, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_shape_w_const_u16, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_shape_h_const_u16, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_l_stride_1_const_u16, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_l_stride_0_const_u16, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_shape_ch_const_u16, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_weight_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_out_0_shape_ch_const_u16, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_weight_1_ptr_const_s32, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_fmt_zero_const_s8, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_out_0_fmt_zero_const_s8, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_in_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_out_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_weight_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_l_out_ch_format_const_layer_format_type, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_out_0_ptr_s8, 1, 1024, reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_scratch_0_ptr_s16);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(15, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_k_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fut0_mha_1_k_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_k_1_BiasAdd0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0 */
  {
    
  forward_lite_transpose_reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion */
  {
      const ai_i8* reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 25920);
    ai_float* reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 14400);
  
  _STAI_DL2C_EVENT_NODE_START_CB(51, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_ptr_f32, reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(51, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0 */
  {
      const ai_i8* reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_inputs[0] + 0);
    const ai_i8* reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 17420);
    const ai_i32* reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 16908);
    ai_i8* reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 29768);
    ai_i16* reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 13376);
  
  _STAI_DL2C_EVENT_NODE_START_CB(14, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_shape_w_const_u16, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_shape_h_const_u16, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_l_stride_1_const_u16, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_l_stride_0_const_u16, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_shape_ch_const_u16, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_weight_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_out_0_shape_ch_const_u16, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_weight_1_ptr_const_s32, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_fmt_zero_const_s8, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_out_0_fmt_zero_const_s8, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_in_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_out_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_weight_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_l_out_ch_format_const_layer_format_type, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_out_0_ptr_s8, 1, 1024, reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_scratch_0_ptr_s16);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(14, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_q_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fut0_mha_1_q_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_q_1_BiasAdd0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0 */
  {
    
  forward_lite_transpose_reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion */
  {
      const ai_i8* reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 6720);
    ai_float* reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 29768);
  
  _STAI_DL2C_EVENT_NODE_START_CB(50, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_ptr_f32, reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(50, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_MatMul0 */
  {
    
  forward_lite_matmul_reatt_n2_1_fut0_mha_1_MatMul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_MatMul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion */
  {
      const ai_float* reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_i8* reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 14400);
  
  _STAI_DL2C_EVENT_NODE_START_CB(58, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_t_in_0_ptr_const_f32, reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_t_out_0_ptr_s8, reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_t_out_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(58, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fut0_mha_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion */
  {
      const ai_i8* reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_float* reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
  
  _STAI_DL2C_EVENT_NODE_START_CB(61, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_t_in_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_t_out_0_ptr_f32, reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_t_in_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(61, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_Softmax0 */
  {
      ai_handle reatt_n2_1_fut0_mha_1_Softmax0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 0);
    const ai_handle reatt_n2_1_fut0_mha_1_Softmax0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
  
  _STAI_DL2C_EVENT_NODE_START_CB(64, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_Softmax0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_softmax_if32of32(reatt_n2_1_fut0_mha_1_Softmax0_t_out_0_ptr_handle, reatt_n2_1_fut0_mha_1_Softmax0_t_in_0_ptr_const_handle, reatt_n2_1_fut0_mha_1_Softmax0_t_in_0_shape_ch_h_w_prod_const_s32, 900, 4);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(64, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_Softmax0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_Softmax0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0 */
  {
      const ai_i8* reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_inputs[0] + 0);
    const ai_i8* reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 33804);
    const ai_i32* reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 16908);
    ai_i8* reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 15424);
    ai_i16* reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 14400);
  
  _STAI_DL2C_EVENT_NODE_START_CB(13, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_shape_w_const_u16, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_shape_h_const_u16, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_l_stride_1_const_u16, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_l_stride_0_const_u16, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_shape_ch_const_u16, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_weight_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_out_0_shape_ch_const_u16, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_weight_1_ptr_const_s32, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_fmt_zero_const_s8, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_out_0_fmt_zero_const_s8, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_in_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_out_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_weight_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_l_out_ch_format_const_layer_format_type, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_out_0_ptr_s8, 1, 1024, reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_scratch_0_ptr_s16);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(13, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_v_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fut0_mha_1_v_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_v_1_BiasAdd0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0 */
  {
    
  forward_lite_transpose_reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion */
  {
      const ai_i8* reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 14400);
    ai_float* reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 29764);
  
  _STAI_DL2C_EVENT_NODE_START_CB(49, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_t_in_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_t_out_0_ptr_f32, reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_t_in_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(49, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_MatMul_10 */
  {
    
  forward_lite_matmul_reatt_n2_1_fut0_mha_1_MatMul_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_MatMul_10 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion */
  {
      const ai_float* reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 14400);
    ai_i8* reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
  
  _STAI_DL2C_EVENT_NODE_START_CB(67, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_t_in_0_ptr_const_f32, reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_t_out_0_ptr_s8, reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_t_out_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(67, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_pt_back_1_transpose0 */
  {
    
  forward_lite_transpose_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_pt_back_1_transpose0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0 */
  {
      const ai_i8* reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 3840);
    const ai_i8* reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 50188);
    const ai_i32* reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 16908);
    ai_i8* reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 7680);
    ai_i16* reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 0);
  
  _STAI_DL2C_EVENT_NODE_START_CB(76, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_shape_w_const_u16, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_shape_h_const_u16, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_l_stride_1_const_u16, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_l_stride_0_const_u16, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_shape_ch_const_u16, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_weight_0_ptr_const_s8, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_out_0_shape_ch_const_u16, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_weight_1_ptr_const_s32, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_fmt_zero_const_s8, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_out_0_fmt_zero_const_s8, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_in_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_out_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_weight_0_fmt_scale_const_f32, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_l_out_ch_format_const_layer_format_type, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_out_0_ptr_s8, 1, 1024, reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_scratch_0_ptr_s16);
    
  _STAI_DL2C_EVENT_NODE_STOP_CB(76, 1, {(stai_ptr) reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n2_1_fut0_mha_1_o_1_BiasAdd0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n2_1_fut0_mha_1_o_1_BiasAdd0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n2_1_fut0_mha_1_o_1_BiasAdd0 */
  return net_ctx->_return_code;
}

/*****************************************************************************/
/*  Getters APIs Section  */
STAI_API_ENTRY
stai_size stai_dl2c_get_context_size()
{
  return (stai_size)STAI_DL2C_CONTEXT_SIZE;
}

#if defined(HAVE_DL2C_INFO)
STAI_API_ENTRY
stai_return_code stai_dl2c_get_info(
  stai_network* network,
  stai_network_info* info)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, info==NULL, STAI_ERROR_NETWORK_INVALID_INFO, net_ctx->_return_code)

  // Copy of network info struct
  *info = g_dl2c_info;

  return STAI_SUCCESS;
}
#endif


STAI_API_ENTRY
stai_return_code stai_dl2c_get_activations(
  stai_network* network, stai_ptr* activations, stai_size* n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, !n_activations, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_activations = STAI_DL2C_ACTIVATIONS_NUM;
for (stai_size idx=0; activations && (idx<STAI_DL2C_ACTIVATIONS_NUM); idx++) {
    // get address of the activations buffers
    activations[idx] = net_ctx->_activations[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl2c_get_weights(
  stai_network* network, stai_ptr* weights, stai_size* n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_weights, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_weights = STAI_DL2C_WEIGHTS_NUM;
for (stai_size idx=0; weights && (idx<STAI_DL2C_WEIGHTS_NUM); idx++) {
    // get address of the weights buffers
    weights[idx] = net_ctx->_weights[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl2c_get_inputs(
  stai_network* network, stai_ptr* inputs, stai_size* n_inputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_inputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_inputs = STAI_DL2C_IN_NUM;
  for (stai_size idx=0; inputs && (idx<STAI_DL2C_IN_NUM); idx++) {
    inputs[idx] = net_ctx->_inputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl2c_get_outputs(
  stai_network* network, stai_ptr* outputs, stai_size* n_outputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_outputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_outputs = STAI_DL2C_OUT_NUM;
  for (stai_size idx=0; outputs && (idx<STAI_DL2C_OUT_NUM); idx++) {
    outputs[idx] = net_ctx->_outputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl2c_get_error(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /* return 1st generated error or STAI_SUCCESS if no errors so far */
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl2c_get_states(
  stai_network* network, stai_ptr* states, stai_size* n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_states, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  /* get the number of internals states (supporting multi-heap also for internal states) */
  *n_states = STAI_DL2C_STATES_NUM;

  STAI_UNUSED(states)
return net_ctx->_return_code;
}


/*****************************************************************************/
/*  Setters APIs Section  */

STAI_API_ENTRY
stai_return_code stai_dl2c_set_activations(
  stai_network* network,
  const stai_ptr* activations,
  const stai_size n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _activations_alignment[] = STAI_DL2C_ACTIVATIONS_ALIGNMENTS;
  STAI_PRINT("  [stai_dl2c_set_activations] network(%p) activations[%d]: %p\n\n", net_ctx, n_activations, activations)
  _STAI_SET_ERROR(net_ctx, !activations,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_activations!=STAI_DL2C_ACTIVATIONS_NUM,
                  STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_NUM, net_ctx->_return_code)

  for (stai_size idx=0; activations && idx<STAI_DL2C_ACTIVATIONS_NUM; idx++) {
    STAI_PRINT("  activation[%d]: %p\n", idx, activations[idx])
    _STAI_SET_ERROR(net_ctx, activations[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)activations[idx]) & (_activations_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_activations[idx] = activations[idx];
  }_stai_dl2c_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl2c_set_weights(
  stai_network* network,
  const stai_ptr* weights,
  const stai_size n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _weights_alignment[] = STAI_DL2C_WEIGHTS_ALIGNMENTS;
  _STAI_SET_ERROR(net_ctx, !weights,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_weights!=STAI_DL2C_WEIGHTS_NUM,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_NUM, net_ctx->_return_code)
  for (stai_size idx=0; weights && idx<STAI_DL2C_WEIGHTS_NUM; idx++) {
    STAI_PRINT("  weight[%d]: %p\n", idx, weights[idx])
    _STAI_SET_ERROR(net_ctx, weights[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)weights[idx]) & (_weights_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_weights[idx] = weights[idx];
  }_stai_dl2c_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl2c_set_inputs(
  stai_network* network,
  const stai_ptr* inputs,
  const stai_size n_inputs)
{
  const uintptr_t _inputs_alignment[] = STAI_DL2C_IN_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !inputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_inputs!=STAI_DL2C_IN_NUM,
                  STAI_ERROR_NETWORK_INVALID_IN_NUM, net_ctx->_return_code)

  for (stai_size idx=0; inputs && idx<STAI_DL2C_IN_NUM; idx++) {
    STAI_PRINT("  input[%d]: %p\n", idx, inputs[idx])
    _STAI_SET_ERROR(net_ctx, inputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)inputs[idx]) & (_inputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_inputs[idx] = inputs[idx];
  }

  _stai_dl2c_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl2c_set_outputs(
  stai_network* network,
  const stai_ptr* outputs,
  const stai_size n_outputs)
{
  const uintptr_t _outputs_alignment[] = STAI_DL2C_OUT_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !outputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_outputs!=STAI_DL2C_OUT_NUM,
                  STAI_ERROR_NETWORK_INVALID_OUT_NUM, net_ctx->_return_code)

  for (stai_size idx=0; outputs && idx<n_outputs; idx++) {
    STAI_PRINT("  output[%d]: %p\n", idx, outputs[idx])
    _STAI_SET_ERROR(net_ctx, outputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)outputs[idx]) & (_outputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_outputs[idx] = outputs[idx];
  }

  _stai_dl2c_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_dl2c_set_states(
  stai_network* network,
  const stai_ptr* states,
  const stai_size n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  STAI_UNUSED(states)
  STAI_UNUSED(n_states)
_stai_dl2c_check(net_ctx);
  return net_ctx->_return_code;
}

STAI_API_ENTRY
stai_return_code stai_dl2c_set_callback(
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
#undef _STAI_DL2C_EVENT_NODE_START_CB
#undef _STAI_DL2C_EVENT_NODE_STOP_CB
#undef _STAI_DL2C_MODEL_SIGNATURE
#undef _STAI_DL2C_DATETIME
#undef _STAI_DL2C_COMPILE_DATETIME

