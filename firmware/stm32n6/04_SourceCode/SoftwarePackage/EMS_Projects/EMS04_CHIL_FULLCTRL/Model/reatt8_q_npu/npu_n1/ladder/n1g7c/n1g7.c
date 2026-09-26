/**
  ******************************************************************************
  * @file    n1g7.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-09-25T02:22:37+0800
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
#include "n1g7.h"
#include "n1g7_details.h"
#include "n1g7_data.h"
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
#ifndef _STAI_N1G7_EVENT_NODE_START_CB
  #define _STAI_N1G7_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
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
  #define _STAI_N1G7_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_N1G7_EVENT_NODE_START_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_START_CB */

#ifdef STAI_EVENT_NODE_STOP_CB
#ifndef _STAI_N1G7_EVENT_NODE_STOP_CB
  #define _STAI_N1G7_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
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
  #define _STAI_N1G7_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_N1G7_EVENT_NODE_STOP_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_STOP_CB */


/*****************************************************************************/
#define _STAI_N1G7_MODEL_SIGNATURE     "0xe7b3b6577d9cf481f5d1876820bc3002"
#define _STAI_N1G7_DATETIME            "2026-09-25T02:22:37+0800"
#define _STAI_N1G7_COMPILE_DATETIME    __DATE__ " " __TIME__

#define _STAI_CONTEXT_ALIGNMENT        STAI_N1G7_CONTEXT_ALIGNMENT

/*****************************************************************************/
#define g_n1g7_activations_1     (NULL)




#if defined(HAVE_N1G7_INFO)
/*****************************************************************************/
static const stai_network_info g_n1g7_info = {
  .model_signature = _STAI_N1G7_MODEL_SIGNATURE,
  .c_compile_datetime = _STAI_N1G7_COMPILE_DATETIME,
  .c_model_name = STAI_N1G7_MODEL_NAME,
  .c_model_datetime = _STAI_N1G7_DATETIME,
  .c_model_signature = 0x0,
  .runtime_version = STAI_INIT_VERSION(12, 0, 1),
  .tool_version = STAI_INIT_VERSION(4, 0, 1),
  .api_version = STAI_INIT_VERSION(1, 0, 0),
  .n_macc = STAI_N1G7_MACC_NUM,
  .n_nodes = STAI_N1G7_NODES_NUM,
  .flags = STAI_N1G7_FLAGS,
  .n_inputs = STAI_N1G7_IN_NUM,
  .n_outputs = STAI_N1G7_OUT_NUM,
  .n_activations = STAI_N1G7_ACTIVATIONS_NUM,
  .n_weights = STAI_N1G7_WEIGHTS_NUM,
  .n_states = STAI_N1G7_STATES_NUM,
  .inputs = (stai_tensor[STAI_N1G7_IN_NUM]) {
    STAI_INIT_TENSOR(
      STAI_N1G7_IN_1_NAME,
      STAI_N1G7_IN_1_FLAGS,
      STAI_N1G7_IN_1_FORMAT,
      STAI_N1G7_IN_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 300, 96),
      STAI_DECLARE_ARRAY(float, 1, 0.10051041841506958f),
      STAI_DECLARE_ARRAY(int16_t, 1, -96)),
    },
    .outputs = (stai_tensor[STAI_N1G7_OUT_NUM]) {
    STAI_INIT_TENSOR(
      STAI_N1G7_OUT_1_NAME,
      STAI_N1G7_OUT_1_FLAGS,
      STAI_N1G7_OUT_1_FORMAT,
      STAI_N1G7_OUT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 1, 96),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .activations = (stai_tensor[STAI_N1G7_ACTIVATIONS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_N1G7_ACTIVATION_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_N1G7_ACTIVATION_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 115200),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_N1G7_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_N1G7_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_N1G7_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 100),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },

  .states = NULL
};
#endif

#define _STAI_CONTEXT_ACQUIRE(_net_ctx, _net_handle) \
  _stai_n1g7_context* _net_ctx = (_stai_n1g7_context*)(_net_handle); \
  STAI_ASSERT(_net_ctx != NULL) \
  _STAI_SET_ERROR(_net_ctx, _net_ctx->_magic != STAI_MAGIC, \
                  STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE, _net_ctx->_return_code)


/*****************************************************************************/
static
void _stai_n1g7_check(_stai_n1g7_context* net_ctx)
{
  stai_size idx;

// Check activations status
  for (idx=0; idx<STAI_N1G7_ACTIVATIONS_NUM; idx++) {
    if (net_ctx->_activations[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_N1G7_ACTIVATIONS_NUM) ? STAI_FLAG_ACTIVATIONS : STAI_FLAG_NONE;
// Check inputs status
  for (idx=0; idx<STAI_N1G7_IN_NUM; idx++) {
    if (net_ctx->_inputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_N1G7_IN_NUM) ? STAI_FLAG_INPUTS : STAI_FLAG_NONE;

  // Check outputs status
  for (idx=0; idx<STAI_N1G7_OUT_NUM; idx++) {
    if (net_ctx->_outputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_N1G7_OUT_NUM) ? STAI_FLAG_OUTPUTS : STAI_FLAG_NONE;

// Check weights status
  for (idx=0; idx<STAI_N1G7_WEIGHTS_NUM; idx++) {
    if (net_ctx->_weights[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_N1G7_WEIGHTS_NUM) ? STAI_FLAG_WEIGHTS : STAI_FLAG_NONE;
STAI_PRINT("  [_stai_network_check] flags: 0x%08x\n", net_ctx->_flags)
}


/*****************************************************************************/
STAI_API_ENTRY
stai_return_code stai_n1g7_init(
  stai_network* network)
{
  /* Memory where to store internal context is provided by applications as a raw byte buffer */
  _stai_n1g7_context* net_ctx = (_stai_n1g7_context*)(network);
  net_ctx->_return_code = STAI_SUCCESS;
  STAI_PRINT("[Entering Network Init] network(%p) context_size(%d)\n", net_ctx, (int32_t)sizeof(_stai_n1g7_context))

  _STAI_SET_ERROR(net_ctx, STAI_N1G7_CONTEXT_SIZE != sizeof(_stai_n1g7_context),
                 STAI_ERROR_NETWORK_INVALID_CONTEXT_SIZE, net_ctx->_return_code)

  {
    const _stai_n1g7_context _n1g7_context = {
      ._magic = STAI_MAGIC,
      ._signature = STAI_N1G7_MODEL_SIGNATURE,
      ._flags = STAI_N1G7_FLAGS,
      ._return_code = STAI_SUCCESS,
      ._callback = NULL,
      ._callback_cookie = NULL,
      ._activations = {
      (stai_ptr)g_n1g7_activations_1
      },
      ._weights = {
      (stai_ptr)g_n1g7_weights_array
      },
      ._inputs = {
    NULL},
      ._outputs = {
    NULL},
    };

    // Deep copy of internal context to opaque buffer provided by app
    *net_ctx = _n1g7_context;

    _stai_n1g7_check(net_ctx);
  }

  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_n1g7_deinit(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /*  Reset flags to initial state  */
  net_ctx->_flags = STAI_N1G7_FLAGS;
  return net_ctx->_return_code;
}

/*****************************************************************************/



/* Int quant #0 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_add5_1_Add0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10051041841506958f),
    AI_PACK_INTQ_ZP(-96)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.4802679419517517f),
    AI_PACK_INTQ_ZP(6)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007381719537079334f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921568859368563f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(reatt_n1_1_past_pool_1_mul0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06137390807271004f),
    AI_PACK_INTQ_ZP(-99)))



/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_add5_1_Add0_output_array, AI_ARRAY_FORMAT_S8|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 28800, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 96, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 384, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 300, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_mul0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 28800, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  past_pool_output_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 96, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_add5_1_Add0_output, AI_STATIC,
  1, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_add5_1_Add0_output_array, &reatt_n1_1_past_add5_1_Add0_output_array_intq)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_bias, AI_STATIC,
  5, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_bias_array, NULL)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_output, AI_STATIC,
  6, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_output_array, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_output_array_intq)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_scratch0, AI_STATIC,
  7, 0x0,
  AI_SHAPE_INIT(4, 1, 384, 1, 1), AI_STRIDE_INIT(4, 1, 1, 384, 384),
  1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_scratch0_array, NULL)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_weights, AI_STATIC,
  8, 0x1,
  AI_SHAPE_INIT(4, 96, 1, 1, 1), AI_STRIDE_INIT(4, 1, 96, 96, 96),
  1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_weights_array, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_weights_array_intq)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output, AI_STATIC,
  2, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output_array, &reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output_array_intq)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_mul0_output, AI_STATIC,
  10, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 1, 1, 96, 96),
  1, &reatt_n1_1_past_pool_1_mul0_output_array, &reatt_n1_1_past_pool_1_mul0_output_array_intq)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  past_pool_output, AI_STATIC,
  0, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &past_pool_output_array, NULL)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output, AI_STATIC,
  9, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output_array, NULL)


AI_TENSOR_CHAIN_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_add5_1_Add0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_weights, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_layer, 7,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSSA,
  &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_chain,
  NULL, &reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_layer, AI_STATIC, 
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
  reatt_n1_1_past_pool_1_mul0_layer, 19,
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
  past_pool_layer, 22,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &past_pool_chain,
  NULL, &past_pool_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &past_pool_neutral_value, 
)
/**  Hybrid layers declarations section  *************************************/
void forward_lite_conv2d_integer_SSSA_reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0(_stai_n1g7_context* net_ctx)
{
  reatt_n1_1_past_add5_1_Add0_output_array.data = AI_PTR(net_ctx->_inputs[0] + 0);
  reatt_n1_1_past_add5_1_Add0_output_array.data_start = AI_PTR(net_ctx->_inputs[0] + 0);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_weights_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 96);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 96);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 86616);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 86616);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 87000);
  reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 87000);
  _STAI_N1G7_EVENT_NODE_START_CB(7, 1, { reatt_n1_1_past_add5_1_Add0_output.data->data});
  forward_conv2d_integer_SSSA(&reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_layer);
  _STAI_N1G7_EVENT_NODE_STOP_CB(7, 1, { reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_output.data->data});
}
void forward_lite_eltwise_integer_INT8_reatt_n1_1_past_pool_1_mul0(_stai_n1g7_context* net_ctx)
{
  reatt_n1_1_past_add5_1_Add0_output_array.data = AI_PTR(net_ctx->_inputs[0] + 0);
  reatt_n1_1_past_add5_1_Add0_output_array.data_start = AI_PTR(net_ctx->_inputs[0] + 0);
  reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 86100);
  reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86100);
  reatt_n1_1_past_pool_1_mul0_output_array.data = AI_PTR(net_ctx->_activations[0] + 86400);
  reatt_n1_1_past_pool_1_mul0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 86400);
  _STAI_N1G7_EVENT_NODE_START_CB(19, 2, { reatt_n1_1_past_add5_1_Add0_output.data->data,reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output.data->data});
  forward_eltwise_integer_INT8(&reatt_n1_1_past_pool_1_mul0_layer);
  _STAI_N1G7_EVENT_NODE_STOP_CB(19, 1, { reatt_n1_1_past_pool_1_mul0_output.data->data});
}
void forward_lite_reduce_past_pool(_stai_n1g7_context* net_ctx)
{
  reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  past_pool_output_array.data = AI_PTR(net_ctx->_outputs[0] + 0);
  past_pool_output_array.data_start = AI_PTR(net_ctx->_outputs[0] + 0);
  _STAI_N1G7_EVENT_NODE_START_CB(22, 1, { reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output.data->data});
  forward_reduce(&past_pool_layer);
  _STAI_N1G7_EVENT_NODE_STOP_CB(22, 1, { past_pool_output.data->data});
}

/*****************************************************************************/



static const ai_u32 reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_fmt_scale_const_f32 = 0.4802679419517517f;
static const ai_i8 reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_fmt_zero_const_s8 = 6;

static const ai_i32 reatt_n1_1_past_pool_1_Softmax0_t_in_0_shape_ch_h_prod_const_s32 = 300;

static const ai_u32 reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_fmt_scale_const_f32 = 0.003921568859368563f;
static const ai_i8 reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_u32 reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 28800;
static const ai_float reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_fmt_scale_const_f32 = 0.06137390807271004f;
static const ai_i8 reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_fmt_zero_const_s8 = -99;

STAI_API_ENTRY
stai_return_code stai_n1g7_run(
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


  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0 */
  {
    
  forward_lite_conv2d_integer_SSSA_reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion */
  {
      const ai_i8* reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 87000);
    ai_float* reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 86100);
  
  _STAI_N1G7_EVENT_NODE_START_CB(7, 1, {(stai_ptr) reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_out_0_ptr_f32, reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_N1G7_EVENT_NODE_STOP_CB(7, 1, {(stai_ptr) reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_Softmax0 */
  {
      ai_handle reatt_n1_1_past_pool_1_Softmax0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 86100);
    const ai_handle reatt_n1_1_past_pool_1_Softmax0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 86100);
  
  _STAI_N1G7_EVENT_NODE_START_CB(13, 1, {(stai_ptr) reatt_n1_1_past_pool_1_Softmax0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_softmax_if32of32(reatt_n1_1_past_pool_1_Softmax0_t_out_0_ptr_handle, reatt_n1_1_past_pool_1_Softmax0_t_in_0_ptr_const_handle, reatt_n1_1_past_pool_1_Softmax0_t_in_0_shape_ch_h_prod_const_s32, 1, 300);
    
  _STAI_N1G7_EVENT_NODE_STOP_CB(13, 1, {(stai_ptr) reatt_n1_1_past_pool_1_Softmax0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_pool_1_Softmax0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion */
  {
      const ai_float* reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 86100);
    ai_i8* reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 86100);
  
  _STAI_N1G7_EVENT_NODE_START_CB(13, 1, {(stai_ptr) reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_in_0_ptr_const_f32, reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_ptr_s8, reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_fmt_scale_const_f32, reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_N1G7_EVENT_NODE_STOP_CB(13, 1, {(stai_ptr) reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_mul0 */
  {
    
  forward_lite_eltwise_integer_INT8_reatt_n1_1_past_pool_1_mul0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reatt_n1_1_past_pool_1_mul0 */
  /* LITE_KERNEL_SECTION BEGIN reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion */
  {
      const ai_i8* reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 86400);
    ai_float* reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
  
  _STAI_N1G7_EVENT_NODE_START_CB(19, 1, {(stai_ptr) reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_ptr_const_s8, reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_out_0_ptr_f32, reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_fmt_scale_const_f32, reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_N1G7_EVENT_NODE_STOP_CB(19, 1, {(stai_ptr) reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_t_out_0_ptr_f32});
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
stai_size stai_n1g7_get_context_size()
{
  return (stai_size)STAI_N1G7_CONTEXT_SIZE;
}

#if defined(HAVE_N1G7_INFO)
STAI_API_ENTRY
stai_return_code stai_n1g7_get_info(
  stai_network* network,
  stai_network_info* info)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, info==NULL, STAI_ERROR_NETWORK_INVALID_INFO, net_ctx->_return_code)

  // Copy of network info struct
  *info = g_n1g7_info;

  return STAI_SUCCESS;
}
#endif


STAI_API_ENTRY
stai_return_code stai_n1g7_get_activations(
  stai_network* network, stai_ptr* activations, stai_size* n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, !n_activations, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_activations = STAI_N1G7_ACTIVATIONS_NUM;
for (stai_size idx=0; activations && (idx<STAI_N1G7_ACTIVATIONS_NUM); idx++) {
    // get address of the activations buffers
    activations[idx] = net_ctx->_activations[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_n1g7_get_weights(
  stai_network* network, stai_ptr* weights, stai_size* n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_weights, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_weights = STAI_N1G7_WEIGHTS_NUM;
for (stai_size idx=0; weights && (idx<STAI_N1G7_WEIGHTS_NUM); idx++) {
    // get address of the weights buffers
    weights[idx] = net_ctx->_weights[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_n1g7_get_inputs(
  stai_network* network, stai_ptr* inputs, stai_size* n_inputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_inputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_inputs = STAI_N1G7_IN_NUM;
  for (stai_size idx=0; inputs && (idx<STAI_N1G7_IN_NUM); idx++) {
    inputs[idx] = net_ctx->_inputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_n1g7_get_outputs(
  stai_network* network, stai_ptr* outputs, stai_size* n_outputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_outputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_outputs = STAI_N1G7_OUT_NUM;
  for (stai_size idx=0; outputs && (idx<STAI_N1G7_OUT_NUM); idx++) {
    outputs[idx] = net_ctx->_outputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_n1g7_get_error(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /* return 1st generated error or STAI_SUCCESS if no errors so far */
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_n1g7_get_states(
  stai_network* network, stai_ptr* states, stai_size* n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_states, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  /* get the number of internals states (supporting multi-heap also for internal states) */
  *n_states = STAI_N1G7_STATES_NUM;

  STAI_UNUSED(states)
return net_ctx->_return_code;
}


/*****************************************************************************/
/*  Setters APIs Section  */

STAI_API_ENTRY
stai_return_code stai_n1g7_set_activations(
  stai_network* network,
  const stai_ptr* activations,
  const stai_size n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _activations_alignment[] = STAI_N1G7_ACTIVATIONS_ALIGNMENTS;
  STAI_PRINT("  [stai_n1g7_set_activations] network(%p) activations[%d]: %p\n\n", net_ctx, n_activations, activations)
  _STAI_SET_ERROR(net_ctx, !activations,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_activations!=STAI_N1G7_ACTIVATIONS_NUM,
                  STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_NUM, net_ctx->_return_code)

  for (stai_size idx=0; activations && idx<STAI_N1G7_ACTIVATIONS_NUM; idx++) {
    STAI_PRINT("  activation[%d]: %p\n", idx, activations[idx])
    _STAI_SET_ERROR(net_ctx, activations[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)activations[idx]) & (_activations_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_activations[idx] = activations[idx];
  }_stai_n1g7_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_n1g7_set_weights(
  stai_network* network,
  const stai_ptr* weights,
  const stai_size n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _weights_alignment[] = STAI_N1G7_WEIGHTS_ALIGNMENTS;
  _STAI_SET_ERROR(net_ctx, !weights,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_weights!=STAI_N1G7_WEIGHTS_NUM,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_NUM, net_ctx->_return_code)
  for (stai_size idx=0; weights && idx<STAI_N1G7_WEIGHTS_NUM; idx++) {
    STAI_PRINT("  weight[%d]: %p\n", idx, weights[idx])
    _STAI_SET_ERROR(net_ctx, weights[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)weights[idx]) & (_weights_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_weights[idx] = weights[idx];
  }_stai_n1g7_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_n1g7_set_inputs(
  stai_network* network,
  const stai_ptr* inputs,
  const stai_size n_inputs)
{
  const uintptr_t _inputs_alignment[] = STAI_N1G7_IN_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !inputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_inputs!=STAI_N1G7_IN_NUM,
                  STAI_ERROR_NETWORK_INVALID_IN_NUM, net_ctx->_return_code)

  for (stai_size idx=0; inputs && idx<STAI_N1G7_IN_NUM; idx++) {
    STAI_PRINT("  input[%d]: %p\n", idx, inputs[idx])
    _STAI_SET_ERROR(net_ctx, inputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)inputs[idx]) & (_inputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_inputs[idx] = inputs[idx];
  }

  _stai_n1g7_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_n1g7_set_outputs(
  stai_network* network,
  const stai_ptr* outputs,
  const stai_size n_outputs)
{
  const uintptr_t _outputs_alignment[] = STAI_N1G7_OUT_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !outputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_outputs!=STAI_N1G7_OUT_NUM,
                  STAI_ERROR_NETWORK_INVALID_OUT_NUM, net_ctx->_return_code)

  for (stai_size idx=0; outputs && idx<n_outputs; idx++) {
    STAI_PRINT("  output[%d]: %p\n", idx, outputs[idx])
    _STAI_SET_ERROR(net_ctx, outputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)outputs[idx]) & (_outputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_outputs[idx] = outputs[idx];
  }

  _stai_n1g7_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_n1g7_set_states(
  stai_network* network,
  const stai_ptr* states,
  const stai_size n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  STAI_UNUSED(states)
  STAI_UNUSED(n_states)
_stai_n1g7_check(net_ctx);
  return net_ctx->_return_code;
}

STAI_API_ENTRY
stai_return_code stai_n1g7_set_callback(
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
#undef _STAI_N1G7_EVENT_NODE_START_CB
#undef _STAI_N1G7_EVENT_NODE_STOP_CB
#undef _STAI_N1G7_MODEL_SIGNATURE
#undef _STAI_N1G7_DATETIME
#undef _STAI_N1G7_COMPILE_DATETIME

