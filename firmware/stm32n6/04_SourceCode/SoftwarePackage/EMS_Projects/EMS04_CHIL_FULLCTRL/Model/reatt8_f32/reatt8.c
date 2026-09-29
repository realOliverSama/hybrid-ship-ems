/**
  ******************************************************************************
  * @file    reatt8.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-09-29T21:44:28+0800
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
#include "reatt8.h"
#include "reatt8_details.h"
#include "reatt8_data.h"
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
#ifndef _STAI_REATT8_EVENT_NODE_START_CB
  #define _STAI_REATT8_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
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
  #define _STAI_REATT8_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_REATT8_EVENT_NODE_START_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_START_CB */

#ifdef STAI_EVENT_NODE_STOP_CB
#ifndef _STAI_REATT8_EVENT_NODE_STOP_CB
  #define _STAI_REATT8_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
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
  #define _STAI_REATT8_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_REATT8_EVENT_NODE_STOP_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_STOP_CB */


/*****************************************************************************/
#define _STAI_REATT8_MODEL_SIGNATURE     "0x9c1e61626753af70c5691eac4df4c868"
#define _STAI_REATT8_DATETIME            "2026-09-29T21:44:28+0800"
#define _STAI_REATT8_COMPILE_DATETIME    __DATE__ " " __TIME__

#define _STAI_CONTEXT_ALIGNMENT        STAI_REATT8_CONTEXT_ALIGNMENT

/*****************************************************************************/
#define g_reatt8_activations_1     (NULL)




#if defined(HAVE_REATT8_INFO)
/*****************************************************************************/
static const stai_network_info g_reatt8_info = {
  .model_signature = _STAI_REATT8_MODEL_SIGNATURE,
  .c_compile_datetime = _STAI_REATT8_COMPILE_DATETIME,
  .c_model_name = STAI_REATT8_MODEL_NAME,
  .c_model_datetime = _STAI_REATT8_DATETIME,
  .c_model_signature = 0x0,
  .runtime_version = STAI_INIT_VERSION(12, 0, 1),
  .tool_version = STAI_INIT_VERSION(4, 0, 1),
  .api_version = STAI_INIT_VERSION(1, 0, 0),
  .n_macc = STAI_REATT8_MACC_NUM,
  .n_nodes = STAI_REATT8_NODES_NUM,
  .flags = STAI_REATT8_FLAGS,
  .n_inputs = STAI_REATT8_IN_NUM,
  .n_outputs = STAI_REATT8_OUT_NUM,
  .n_activations = STAI_REATT8_ACTIVATIONS_NUM,
  .n_weights = STAI_REATT8_WEIGHTS_NUM,
  .n_states = STAI_REATT8_STATES_NUM,
  .inputs = (stai_tensor[STAI_REATT8_IN_NUM]) {
    STAI_INIT_TENSOR(
      STAI_REATT8_IN_1_NAME,
      STAI_REATT8_IN_1_FLAGS,
      STAI_REATT8_IN_1_FORMAT,
      STAI_REATT8_IN_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 300, 11),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    STAI_INIT_TENSOR(
      STAI_REATT8_IN_2_NAME,
      STAI_REATT8_IN_2_FLAGS,
      STAI_REATT8_IN_2_FORMAT,
      STAI_REATT8_IN_2_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 30, 6),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
    .outputs = (stai_tensor[STAI_REATT8_OUT_NUM]) {
    STAI_INIT_TENSOR(
      STAI_REATT8_OUT_1_NAME,
      STAI_REATT8_OUT_1_FLAGS,
      STAI_REATT8_OUT_1_FORMAT,
      STAI_REATT8_OUT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 30, 3),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .activations = (stai_tensor[STAI_REATT8_ACTIVATIONS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_REATT8_ACTIVATION_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_REATT8_ACTIVATION_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 476204),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_REATT8_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_REATT8_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_REATT8_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 4052912),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },

  .states = NULL
};
#endif

#define _STAI_CONTEXT_ACQUIRE(_net_ctx, _net_handle) \
  _stai_reatt8_context* _net_ctx = (_stai_reatt8_context*)(_net_handle); \
  STAI_ASSERT(_net_ctx != NULL) \
  _STAI_SET_ERROR(_net_ctx, _net_ctx->_magic != STAI_MAGIC, \
                  STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE, _net_ctx->_return_code)


/*****************************************************************************/
static
void _stai_reatt8_check(_stai_reatt8_context* net_ctx)
{
  stai_size idx;

// Check activations status
  for (idx=0; idx<STAI_REATT8_ACTIVATIONS_NUM; idx++) {
    if (net_ctx->_activations[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_REATT8_ACTIVATIONS_NUM) ? STAI_FLAG_ACTIVATIONS : STAI_FLAG_NONE;
// Check inputs status
  for (idx=0; idx<STAI_REATT8_IN_NUM; idx++) {
    if (net_ctx->_inputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_REATT8_IN_NUM) ? STAI_FLAG_INPUTS : STAI_FLAG_NONE;

  // Check outputs status
  for (idx=0; idx<STAI_REATT8_OUT_NUM; idx++) {
    if (net_ctx->_outputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_REATT8_OUT_NUM) ? STAI_FLAG_OUTPUTS : STAI_FLAG_NONE;

// Check weights status
  for (idx=0; idx<STAI_REATT8_WEIGHTS_NUM; idx++) {
    if (net_ctx->_weights[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_REATT8_WEIGHTS_NUM) ? STAI_FLAG_WEIGHTS : STAI_FLAG_NONE;
STAI_PRINT("  [_stai_network_check] flags: 0x%08x\n", net_ctx->_flags)
}


/*****************************************************************************/
STAI_API_ENTRY
stai_return_code stai_reatt8_init(
  stai_network* network)
{
  /* Memory where to store internal context is provided by applications as a raw byte buffer */
  _stai_reatt8_context* net_ctx = (_stai_reatt8_context*)(network);
  net_ctx->_return_code = STAI_SUCCESS;
  STAI_PRINT("[Entering Network Init] network(%p) context_size(%d)\n", net_ctx, (int32_t)sizeof(_stai_reatt8_context))

  _STAI_SET_ERROR(net_ctx, STAI_REATT8_CONTEXT_SIZE != sizeof(_stai_reatt8_context),
                 STAI_ERROR_NETWORK_INVALID_CONTEXT_SIZE, net_ctx->_return_code)

  {
    const _stai_reatt8_context _reatt8_context = {
      ._magic = STAI_MAGIC,
      ._signature = STAI_REATT8_MODEL_SIGNATURE,
      ._flags = STAI_REATT8_FLAGS,
      ._return_code = STAI_SUCCESS,
      ._callback = NULL,
      ._callback_cookie = NULL,
      ._activations = {
      (stai_ptr)g_reatt8_activations_1
      },
      ._weights = {
      (stai_ptr)g_reatt8_weights_array
      },
      ._inputs = {
    NULL,NULL},
      ._outputs = {
    NULL},
    };

    // Deep copy of internal context to opaque buffer provided by app
    *net_ctx = _reatt8_context;

    _stai_reatt8_check(net_ctx);
  }

  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reatt8_deinit(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /*  Reset flags to initial state  */
  net_ctx->_flags = STAI_REATT8_FLAGS;
  return net_ctx->_return_code;
}

/*****************************************************************************/





/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  serving_default_kf_input0_output_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 180, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  gemm_146_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  gemm_146_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 768, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_148_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant2_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  reduce_149_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  reduce_149_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_151_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  reduce_152_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  nl_154_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_155_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_fut0_ln1_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#12 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_158_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#13 */
AI_ARRAY_OBJ_DECLARE(
  nl_150_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#14 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_156_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#15 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_157_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_159_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  gemm_160_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  gemm_160_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_162_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant72_4D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  transpose_167_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#22 */
AI_ARRAY_OBJ_DECLARE(
  gemm_163_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#23 */
AI_ARRAY_OBJ_DECLARE(
  gemm_163_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#24 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_165_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#25 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant71_4D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#26 */
AI_ARRAY_OBJ_DECLARE(
  transpose_166_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#27 */
AI_ARRAY_OBJ_DECLARE(
  gemm_168_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3600, AI_STATIC)

/* Array#28 */
AI_ARRAY_OBJ_DECLARE(
  gemm_168_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#29 */
AI_ARRAY_OBJ_DECLARE(
  gemm_172_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#30 */
AI_ARRAY_OBJ_DECLARE(
  gemm_172_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#31 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_174_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#32 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant70_4D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#33 */
AI_ARRAY_OBJ_DECLARE(
  transpose_175_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#34 */
AI_ARRAY_OBJ_DECLARE(
  nl_171_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3600, AI_STATIC)

/* Array#35 */
AI_ARRAY_OBJ_DECLARE(
  gemm_176_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#36 */
AI_ARRAY_OBJ_DECLARE(
  gemm_176_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#37 */
AI_ARRAY_OBJ_DECLARE(
  transpose_178_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#38 */
AI_ARRAY_OBJ_DECLARE(
  gemm_180_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#39 */
AI_ARRAY_OBJ_DECLARE(
  gemm_180_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#40 */
AI_ARRAY_OBJ_DECLARE(
  gemm_180_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#41 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_181_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#42 */
AI_ARRAY_OBJ_DECLARE(
  reduce_182_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#43 */
AI_ARRAY_OBJ_DECLARE(
  reduce_182_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#44 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_184_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#45 */
AI_ARRAY_OBJ_DECLARE(
  reduce_185_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#46 */
AI_ARRAY_OBJ_DECLARE(
  nl_187_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#47 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_188_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#48 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_fut0_ln2_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#49 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_191_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#50 */
AI_ARRAY_OBJ_DECLARE(
  nl_183_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#51 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_189_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#52 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_190_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#53 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_192_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#54 */
AI_ARRAY_OBJ_DECLARE(
  gemm_201_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 15360, AI_STATIC)

/* Array#55 */
AI_ARRAY_OBJ_DECLARE(
  gemm_201_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 65536, AI_STATIC)

/* Array#56 */
AI_ARRAY_OBJ_DECLARE(
  gemm_201_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 512, AI_STATIC)

/* Array#57 */
AI_ARRAY_OBJ_DECLARE(
  nl_204_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 15360, AI_STATIC)

/* Array#58 */
AI_ARRAY_OBJ_DECLARE(
  gemm_213_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#59 */
AI_ARRAY_OBJ_DECLARE(
  gemm_213_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 65536, AI_STATIC)

/* Array#60 */
AI_ARRAY_OBJ_DECLARE(
  gemm_213_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#61 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_216_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#62 */
AI_ARRAY_OBJ_DECLARE(
  reduce_217_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#63 */
AI_ARRAY_OBJ_DECLARE(
  reduce_217_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#64 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_219_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#65 */
AI_ARRAY_OBJ_DECLARE(
  reduce_220_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#66 */
AI_ARRAY_OBJ_DECLARE(
  nl_222_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#67 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_223_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#68 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_fut1_ln1_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#69 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_226_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#70 */
AI_ARRAY_OBJ_DECLARE(
  nl_218_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#71 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_224_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#72 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_225_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#73 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_227_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#74 */
AI_ARRAY_OBJ_DECLARE(
  gemm_228_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#75 */
AI_ARRAY_OBJ_DECLARE(
  gemm_228_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#76 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_230_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#77 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant66_4D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#78 */
AI_ARRAY_OBJ_DECLARE(
  transpose_235_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#79 */
AI_ARRAY_OBJ_DECLARE(
  gemm_231_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#80 */
AI_ARRAY_OBJ_DECLARE(
  gemm_231_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#81 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_233_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#82 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant65_4D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#83 */
AI_ARRAY_OBJ_DECLARE(
  transpose_234_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#84 */
AI_ARRAY_OBJ_DECLARE(
  gemm_236_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3600, AI_STATIC)

/* Array#85 */
AI_ARRAY_OBJ_DECLARE(
  gemm_236_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#86 */
AI_ARRAY_OBJ_DECLARE(
  gemm_240_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#87 */
AI_ARRAY_OBJ_DECLARE(
  gemm_240_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#88 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_242_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#89 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant64_4D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#90 */
AI_ARRAY_OBJ_DECLARE(
  transpose_243_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#91 */
AI_ARRAY_OBJ_DECLARE(
  nl_239_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3600, AI_STATIC)

/* Array#92 */
AI_ARRAY_OBJ_DECLARE(
  gemm_244_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#93 */
AI_ARRAY_OBJ_DECLARE(
  gemm_244_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#94 */
AI_ARRAY_OBJ_DECLARE(
  transpose_246_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#95 */
AI_ARRAY_OBJ_DECLARE(
  gemm_248_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#96 */
AI_ARRAY_OBJ_DECLARE(
  gemm_248_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#97 */
AI_ARRAY_OBJ_DECLARE(
  gemm_248_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#98 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_249_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#99 */
AI_ARRAY_OBJ_DECLARE(
  reduce_250_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#100 */
AI_ARRAY_OBJ_DECLARE(
  reduce_250_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#101 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_252_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#102 */
AI_ARRAY_OBJ_DECLARE(
  reduce_253_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#103 */
AI_ARRAY_OBJ_DECLARE(
  nl_255_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#104 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_256_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#105 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_fut1_ln2_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#106 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_259_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#107 */
AI_ARRAY_OBJ_DECLARE(
  nl_251_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#108 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_257_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#109 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_258_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#110 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_260_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#111 */
AI_ARRAY_OBJ_DECLARE(
  gemm_269_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 15360, AI_STATIC)

/* Array#112 */
AI_ARRAY_OBJ_DECLARE(
  gemm_269_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 65536, AI_STATIC)

/* Array#113 */
AI_ARRAY_OBJ_DECLARE(
  gemm_269_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 512, AI_STATIC)

/* Array#114 */
AI_ARRAY_OBJ_DECLARE(
  nl_272_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 15360, AI_STATIC)

/* Array#115 */
AI_ARRAY_OBJ_DECLARE(
  gemm_281_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#116 */
AI_ARRAY_OBJ_DECLARE(
  gemm_281_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 65536, AI_STATIC)

/* Array#117 */
AI_ARRAY_OBJ_DECLARE(
  gemm_281_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#118 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_284_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#119 */
AI_ARRAY_OBJ_DECLARE(
  reduce_285_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#120 */
AI_ARRAY_OBJ_DECLARE(
  reduce_285_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#121 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_287_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#122 */
AI_ARRAY_OBJ_DECLARE(
  reduce_288_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#123 */
AI_ARRAY_OBJ_DECLARE(
  nl_290_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#124 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_291_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#125 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_fut2_ln1_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#126 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_294_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#127 */
AI_ARRAY_OBJ_DECLARE(
  nl_286_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#128 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_292_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#129 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_293_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#130 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_295_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#131 */
AI_ARRAY_OBJ_DECLARE(
  gemm_296_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#132 */
AI_ARRAY_OBJ_DECLARE(
  gemm_296_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#133 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_298_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#134 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant60_4D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#135 */
AI_ARRAY_OBJ_DECLARE(
  transpose_303_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#136 */
AI_ARRAY_OBJ_DECLARE(
  gemm_299_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#137 */
AI_ARRAY_OBJ_DECLARE(
  gemm_299_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#138 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_301_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#139 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant59_4D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#140 */
AI_ARRAY_OBJ_DECLARE(
  transpose_302_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#141 */
AI_ARRAY_OBJ_DECLARE(
  gemm_304_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3600, AI_STATIC)

/* Array#142 */
AI_ARRAY_OBJ_DECLARE(
  gemm_304_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#143 */
AI_ARRAY_OBJ_DECLARE(
  gemm_308_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#144 */
AI_ARRAY_OBJ_DECLARE(
  gemm_308_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#145 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_310_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#146 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant58_4D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#147 */
AI_ARRAY_OBJ_DECLARE(
  transpose_311_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#148 */
AI_ARRAY_OBJ_DECLARE(
  nl_307_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3600, AI_STATIC)

/* Array#149 */
AI_ARRAY_OBJ_DECLARE(
  gemm_312_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#150 */
AI_ARRAY_OBJ_DECLARE(
  gemm_312_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#151 */
AI_ARRAY_OBJ_DECLARE(
  transpose_314_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#152 */
AI_ARRAY_OBJ_DECLARE(
  gemm_316_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#153 */
AI_ARRAY_OBJ_DECLARE(
  gemm_316_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#154 */
AI_ARRAY_OBJ_DECLARE(
  gemm_316_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#155 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_317_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#156 */
AI_ARRAY_OBJ_DECLARE(
  reduce_318_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#157 */
AI_ARRAY_OBJ_DECLARE(
  reduce_318_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#158 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_320_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#159 */
AI_ARRAY_OBJ_DECLARE(
  reduce_321_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#160 */
AI_ARRAY_OBJ_DECLARE(
  nl_323_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#161 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_324_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#162 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_fut2_ln2_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#163 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_327_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#164 */
AI_ARRAY_OBJ_DECLARE(
  nl_319_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#165 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_325_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#166 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_326_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#167 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_328_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#168 */
AI_ARRAY_OBJ_DECLARE(
  gemm_337_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 15360, AI_STATIC)

/* Array#169 */
AI_ARRAY_OBJ_DECLARE(
  gemm_337_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 65536, AI_STATIC)

/* Array#170 */
AI_ARRAY_OBJ_DECLARE(
  gemm_337_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 512, AI_STATIC)

/* Array#171 */
AI_ARRAY_OBJ_DECLARE(
  nl_340_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 15360, AI_STATIC)

/* Array#172 */
AI_ARRAY_OBJ_DECLARE(
  gemm_349_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#173 */
AI_ARRAY_OBJ_DECLARE(
  gemm_349_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 65536, AI_STATIC)

/* Array#174 */
AI_ARRAY_OBJ_DECLARE(
  gemm_349_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#175 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_352_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#176 */
AI_ARRAY_OBJ_DECLARE(
  reduce_353_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#177 */
AI_ARRAY_OBJ_DECLARE(
  reduce_353_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#178 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_355_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#179 */
AI_ARRAY_OBJ_DECLARE(
  reduce_356_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#180 */
AI_ARRAY_OBJ_DECLARE(
  nl_358_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#181 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_359_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#182 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_fut3_ln1_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#183 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_362_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#184 */
AI_ARRAY_OBJ_DECLARE(
  nl_354_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#185 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_360_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#186 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_361_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#187 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_363_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#188 */
AI_ARRAY_OBJ_DECLARE(
  gemm_364_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#189 */
AI_ARRAY_OBJ_DECLARE(
  gemm_364_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#190 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_366_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#191 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant54_4D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#192 */
AI_ARRAY_OBJ_DECLARE(
  transpose_371_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#193 */
AI_ARRAY_OBJ_DECLARE(
  gemm_367_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#194 */
AI_ARRAY_OBJ_DECLARE(
  gemm_367_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#195 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_369_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#196 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant53_4D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#197 */
AI_ARRAY_OBJ_DECLARE(
  transpose_370_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#198 */
AI_ARRAY_OBJ_DECLARE(
  gemm_372_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3600, AI_STATIC)

/* Array#199 */
AI_ARRAY_OBJ_DECLARE(
  gemm_372_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#200 */
AI_ARRAY_OBJ_DECLARE(
  gemm_376_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#201 */
AI_ARRAY_OBJ_DECLARE(
  gemm_376_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#202 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_378_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#203 */
AI_ARRAY_OBJ_DECLARE(
  arith_constant52_4D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#204 */
AI_ARRAY_OBJ_DECLARE(
  transpose_379_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#205 */
AI_ARRAY_OBJ_DECLARE(
  nl_375_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3600, AI_STATIC)

/* Array#206 */
AI_ARRAY_OBJ_DECLARE(
  gemm_380_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#207 */
AI_ARRAY_OBJ_DECLARE(
  gemm_380_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#208 */
AI_ARRAY_OBJ_DECLARE(
  transpose_382_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#209 */
AI_ARRAY_OBJ_DECLARE(
  gemm_384_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#210 */
AI_ARRAY_OBJ_DECLARE(
  gemm_384_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#211 */
AI_ARRAY_OBJ_DECLARE(
  gemm_384_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#212 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_385_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#213 */
AI_ARRAY_OBJ_DECLARE(
  reduce_386_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#214 */
AI_ARRAY_OBJ_DECLARE(
  reduce_386_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#215 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_388_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#216 */
AI_ARRAY_OBJ_DECLARE(
  reduce_389_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#217 */
AI_ARRAY_OBJ_DECLARE(
  nl_391_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#218 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_392_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#219 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_fut3_ln2_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#220 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_395_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#221 */
AI_ARRAY_OBJ_DECLARE(
  nl_387_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#222 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_393_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#223 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_394_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#224 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_396_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#225 */
AI_ARRAY_OBJ_DECLARE(
  gemm_405_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 15360, AI_STATIC)

/* Array#226 */
AI_ARRAY_OBJ_DECLARE(
  gemm_405_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 65536, AI_STATIC)

/* Array#227 */
AI_ARRAY_OBJ_DECLARE(
  gemm_405_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 512, AI_STATIC)

/* Array#228 */
AI_ARRAY_OBJ_DECLARE(
  nl_408_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 15360, AI_STATIC)

/* Array#229 */
AI_ARRAY_OBJ_DECLARE(
  gemm_417_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#230 */
AI_ARRAY_OBJ_DECLARE(
  gemm_417_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 65536, AI_STATIC)

/* Array#231 */
AI_ARRAY_OBJ_DECLARE(
  gemm_417_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#232 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_420_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#233 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_1_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#234 */
AI_ARRAY_OBJ_DECLARE(
  pad_3_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28992, AI_STATIC)

/* Array#235 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_5_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#236 */
AI_ARRAY_OBJ_DECLARE(
  reduce_7_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#237 */
AI_ARRAY_OBJ_DECLARE(
  reduce_7_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#238 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_9_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#239 */
AI_ARRAY_OBJ_DECLARE(
  reduce_10_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#240 */
AI_ARRAY_OBJ_DECLARE(
  nl_12_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#241 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_13_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#242 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_past_ln0_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 96, AI_STATIC)

/* Array#243 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_16_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#244 */
AI_ARRAY_OBJ_DECLARE(
  nl_8_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#245 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_14_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#246 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_15_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#247 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_17_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#248 */
AI_ARRAY_OBJ_DECLARE(
  nl_18_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#249 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_19_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#250 */
AI_ARRAY_OBJ_DECLARE(
  pad_20_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 29184, AI_STATIC)

/* Array#251 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_23_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#252 */
AI_ARRAY_OBJ_DECLARE(
  reduce_27_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#253 */
AI_ARRAY_OBJ_DECLARE(
  reduce_27_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#254 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_29_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#255 */
AI_ARRAY_OBJ_DECLARE(
  reduce_30_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#256 */
AI_ARRAY_OBJ_DECLARE(
  nl_32_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#257 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_33_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#258 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_past_ln1_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 96, AI_STATIC)

/* Array#259 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_36_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#260 */
AI_ARRAY_OBJ_DECLARE(
  nl_28_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#261 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_34_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#262 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_35_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#263 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_37_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#264 */
AI_ARRAY_OBJ_DECLARE(
  nl_38_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#265 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_39_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#266 */
AI_ARRAY_OBJ_DECLARE(
  pad_40_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 29568, AI_STATIC)

/* Array#267 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_43_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#268 */
AI_ARRAY_OBJ_DECLARE(
  reduce_47_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#269 */
AI_ARRAY_OBJ_DECLARE(
  reduce_47_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#270 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_49_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#271 */
AI_ARRAY_OBJ_DECLARE(
  reduce_50_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#272 */
AI_ARRAY_OBJ_DECLARE(
  nl_52_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#273 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_53_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#274 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_past_ln2_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 96, AI_STATIC)

/* Array#275 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_56_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#276 */
AI_ARRAY_OBJ_DECLARE(
  nl_48_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#277 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_54_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#278 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_55_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#279 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_57_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#280 */
AI_ARRAY_OBJ_DECLARE(
  nl_58_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#281 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_59_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#282 */
AI_ARRAY_OBJ_DECLARE(
  pad_60_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30336, AI_STATIC)

/* Array#283 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_63_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#284 */
AI_ARRAY_OBJ_DECLARE(
  reduce_67_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#285 */
AI_ARRAY_OBJ_DECLARE(
  reduce_67_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#286 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_69_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#287 */
AI_ARRAY_OBJ_DECLARE(
  reduce_70_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#288 */
AI_ARRAY_OBJ_DECLARE(
  nl_72_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#289 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_73_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#290 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_past_ln3_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 96, AI_STATIC)

/* Array#291 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_76_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#292 */
AI_ARRAY_OBJ_DECLARE(
  nl_68_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#293 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_74_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#294 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_75_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#295 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_77_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#296 */
AI_ARRAY_OBJ_DECLARE(
  nl_78_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#297 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_79_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#298 */
AI_ARRAY_OBJ_DECLARE(
  pad_80_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 31872, AI_STATIC)

/* Array#299 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_83_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#300 */
AI_ARRAY_OBJ_DECLARE(
  reduce_87_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#301 */
AI_ARRAY_OBJ_DECLARE(
  reduce_87_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#302 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_89_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#303 */
AI_ARRAY_OBJ_DECLARE(
  reduce_90_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#304 */
AI_ARRAY_OBJ_DECLARE(
  nl_92_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#305 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_93_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#306 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_past_ln4_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 96, AI_STATIC)

/* Array#307 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_96_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#308 */
AI_ARRAY_OBJ_DECLARE(
  nl_88_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#309 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_94_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#310 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_95_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#311 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_97_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#312 */
AI_ARRAY_OBJ_DECLARE(
  nl_98_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#313 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_99_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#314 */
AI_ARRAY_OBJ_DECLARE(
  pad_100_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 34944, AI_STATIC)

/* Array#315 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_103_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#316 */
AI_ARRAY_OBJ_DECLARE(
  reduce_107_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#317 */
AI_ARRAY_OBJ_DECLARE(
  reduce_107_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#318 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_109_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#319 */
AI_ARRAY_OBJ_DECLARE(
  reduce_110_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#320 */
AI_ARRAY_OBJ_DECLARE(
  nl_112_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#321 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_113_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#322 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_past_ln5_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 96, AI_STATIC)

/* Array#323 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_116_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#324 */
AI_ARRAY_OBJ_DECLARE(
  nl_108_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#325 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_114_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#326 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_115_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#327 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_117_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#328 */
AI_ARRAY_OBJ_DECLARE(
  nl_118_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#329 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_119_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#330 */
AI_ARRAY_OBJ_DECLARE(
  gemm_128_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#331 */
AI_ARRAY_OBJ_DECLARE(
  gemm_128_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 96, AI_STATIC)

/* Array#332 */
AI_ARRAY_OBJ_DECLARE(
  nl_131_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 300, AI_STATIC)

/* Array#333 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_133_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28800, AI_STATIC)

/* Array#334 */
AI_ARRAY_OBJ_DECLARE(
  reduce_134_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 96, AI_STATIC)

/* Array#335 */
AI_ARRAY_OBJ_DECLARE(
  tile_137_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2880, AI_STATIC)

/* Array#336 */
AI_ARRAY_OBJ_DECLARE(
  concat_421_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 6720, AI_STATIC)

/* Array#337 */
AI_ARRAY_OBJ_DECLARE(
  reduce_422_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#338 */
AI_ARRAY_OBJ_DECLARE(
  reduce_422_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#339 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_424_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 6720, AI_STATIC)

/* Array#340 */
AI_ARRAY_OBJ_DECLARE(
  reduce_425_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#341 */
AI_ARRAY_OBJ_DECLARE(
  nl_427_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#342 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_428_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 6720, AI_STATIC)

/* Array#343 */
AI_ARRAY_OBJ_DECLARE(
  functional_1_fusion_ln_1_Reshape_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 224, AI_STATIC)

/* Array#344 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_431_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 6720, AI_STATIC)

/* Array#345 */
AI_ARRAY_OBJ_DECLARE(
  nl_423_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 30, AI_STATIC)

/* Array#346 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_429_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 6720, AI_STATIC)

/* Array#347 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_430_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 6720, AI_STATIC)

/* Array#348 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_432_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 6720, AI_STATIC)

/* Array#349 */
AI_ARRAY_OBJ_DECLARE(
  gemm_441_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#350 */
AI_ARRAY_OBJ_DECLARE(
  gemm_441_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 28672, AI_STATIC)

/* Array#351 */
AI_ARRAY_OBJ_DECLARE(
  gemm_441_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#352 */
AI_ARRAY_OBJ_DECLARE(
  nl_444_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#353 */
AI_ARRAY_OBJ_DECLARE(
  gemm_453_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#354 */
AI_ARRAY_OBJ_DECLARE(
  gemm_453_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#355 */
AI_ARRAY_OBJ_DECLARE(
  gemm_453_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#356 */
AI_ARRAY_OBJ_DECLARE(
  nl_456_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3840, AI_STATIC)

/* Array#357 */
AI_ARRAY_OBJ_DECLARE(
  gemm_465_output_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 90, AI_STATIC)

/* Array#358 */
AI_ARRAY_OBJ_DECLARE(
  gemm_465_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 384, AI_STATIC)

/* Array#359 */
AI_ARRAY_OBJ_DECLARE(
  gemm_465_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 3, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  gemm_146_output, AI_STATIC,
  199, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_146_output_array, NULL)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  gemm_146_weights, AI_STATIC,
  200, 0x0,
  AI_SHAPE_INIT(4, 6, 128, 1, 1), AI_STRIDE_INIT(4, 4, 24, 3072, 3072),
  1, &gemm_146_weights_array, NULL)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  serving_default_kf_input0_output, AI_STATIC,
  421, 0x0,
  AI_SHAPE_INIT(4, 1, 6, 1, 30), AI_STRIDE_INIT(4, 4, 4, 24, 24),
  1, &serving_default_kf_input0_output_array, NULL)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant2, AI_STATIC,
  0, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &arith_constant2_array, NULL)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_148_output, AI_STATIC,
  49, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_148_output_array, NULL)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  reduce_149_output, AI_STATIC,
  363, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_149_output_array, NULL)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_151_output, AI_STATIC,
  51, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_151_output_array, NULL)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  reduce_149_Mul_output, AI_STATIC,
  361, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_149_Mul_output_array, NULL)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  reduce_152_output, AI_STATIC,
  366, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_152_output_array, NULL)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_155_output, AI_STATIC,
  52, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_155_output_array, NULL)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_fut0_ln1_1_Reshape, AI_STATIC,
  182, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &functional_1_fut0_ln1_1_Reshape_array, NULL)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  nl_154_output, AI_STATIC,
  305, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_154_output_array, NULL)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_158_output, AI_STATIC,
  57, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_158_output_array, NULL)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_156_output, AI_STATIC,
  53, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_156_output_array, NULL)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  nl_150_output, AI_STATIC,
  304, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_150_output_array, NULL)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_157_output, AI_STATIC,
  55, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_157_output_array, NULL)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_159_output, AI_STATIC,
  58, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_159_output_array, NULL)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  gemm_160_output, AI_STATIC,
  201, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_160_output_array, NULL)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  gemm_160_weights, AI_STATIC,
  203, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_160_weights_array, NULL)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant72_4D, AI_STATIC,
  12, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 1), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &arith_constant72_4D_array, NULL)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_162_output, AI_STATIC,
  62, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &eltwise_162_output_array, NULL)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  gemm_160_output0, AI_STATIC,
  202, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &gemm_160_output_array, NULL)

/* Tensor #22 */
AI_TENSOR_OBJ_DECLARE(
  transpose_167_output, AI_STATIC,
  424, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 32, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3840),
  1, &transpose_167_output_array, NULL)

/* Tensor #23 */
AI_TENSOR_OBJ_DECLARE(
  gemm_163_output, AI_STATIC,
  204, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_163_output_array, NULL)

/* Tensor #24 */
AI_TENSOR_OBJ_DECLARE(
  gemm_163_weights, AI_STATIC,
  206, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_163_weights_array, NULL)

/* Tensor #25 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant71_4D, AI_STATIC,
  11, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 1), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &arith_constant71_4D_array, NULL)

/* Tensor #26 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_165_output, AI_STATIC,
  63, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &eltwise_165_output_array, NULL)

/* Tensor #27 */
AI_TENSOR_OBJ_DECLARE(
  gemm_163_output0, AI_STATIC,
  205, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &gemm_163_output_array, NULL)

/* Tensor #28 */
AI_TENSOR_OBJ_DECLARE(
  transpose_166_output, AI_STATIC,
  423, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &transpose_166_output_array, NULL)

/* Tensor #29 */
AI_TENSOR_OBJ_DECLARE(
  gemm_168_bias, AI_STATIC,
  207, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &gemm_168_bias_array, NULL)

/* Tensor #30 */
AI_TENSOR_OBJ_DECLARE(
  gemm_168_output, AI_STATIC,
  208, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 30, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3600),
  1, &gemm_168_output_array, NULL)

/* Tensor #31 */
AI_TENSOR_OBJ_DECLARE(
  gemm_172_output, AI_STATIC,
  209, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_172_output_array, NULL)

/* Tensor #32 */
AI_TENSOR_OBJ_DECLARE(
  gemm_172_weights, AI_STATIC,
  211, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_172_weights_array, NULL)

/* Tensor #33 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant70_4D, AI_STATIC,
  10, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 1), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &arith_constant70_4D_array, NULL)

/* Tensor #34 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_174_output, AI_STATIC,
  68, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &eltwise_174_output_array, NULL)

/* Tensor #35 */
AI_TENSOR_OBJ_DECLARE(
  gemm_172_output0, AI_STATIC,
  210, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &gemm_172_output_array, NULL)

/* Tensor #36 */
AI_TENSOR_OBJ_DECLARE(
  transpose_175_output, AI_STATIC,
  425, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &transpose_175_output_array, NULL)

/* Tensor #37 */
AI_TENSOR_OBJ_DECLARE(
  gemm_176_bias, AI_STATIC,
  212, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &gemm_176_bias_array, NULL)

/* Tensor #38 */
AI_TENSOR_OBJ_DECLARE(
  gemm_176_output, AI_STATIC,
  213, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &gemm_176_output_array, NULL)

/* Tensor #39 */
AI_TENSOR_OBJ_DECLARE(
  nl_171_output, AI_STATIC,
  306, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 30, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3600),
  1, &nl_171_output_array, NULL)

/* Tensor #40 */
AI_TENSOR_OBJ_DECLARE(
  transpose_178_output, AI_STATIC,
  426, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &transpose_178_output_array, NULL)

/* Tensor #41 */
AI_TENSOR_OBJ_DECLARE(
  gemm_180_bias, AI_STATIC,
  214, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_180_bias_array, NULL)

/* Tensor #42 */
AI_TENSOR_OBJ_DECLARE(
  gemm_180_output, AI_STATIC,
  215, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_180_output_array, NULL)

/* Tensor #43 */
AI_TENSOR_OBJ_DECLARE(
  gemm_180_weights, AI_STATIC,
  216, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_180_weights_array, NULL)

/* Tensor #44 */
AI_TENSOR_OBJ_DECLARE(
  transpose_178_output0, AI_STATIC,
  427, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &transpose_178_output_array, NULL)

/* Tensor #45 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_181_output, AI_STATIC,
  70, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_181_output_array, NULL)

/* Tensor #46 */
AI_TENSOR_OBJ_DECLARE(
  reduce_182_output, AI_STATIC,
  368, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_182_output_array, NULL)

/* Tensor #47 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_184_output, AI_STATIC,
  71, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_184_output_array, NULL)

/* Tensor #48 */
AI_TENSOR_OBJ_DECLARE(
  reduce_182_Mul_output, AI_STATIC,
  367, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_182_Mul_output_array, NULL)

/* Tensor #49 */
AI_TENSOR_OBJ_DECLARE(
  reduce_185_output, AI_STATIC,
  370, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_185_output_array, NULL)

/* Tensor #50 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_188_output, AI_STATIC,
  72, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_188_output_array, NULL)

/* Tensor #51 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_fut0_ln2_1_Reshape, AI_STATIC,
  183, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &functional_1_fut0_ln2_1_Reshape_array, NULL)

/* Tensor #52 */
AI_TENSOR_OBJ_DECLARE(
  nl_187_output, AI_STATIC,
  308, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_187_output_array, NULL)

/* Tensor #53 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_191_output, AI_STATIC,
  76, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_191_output_array, NULL)

/* Tensor #54 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_189_output, AI_STATIC,
  73, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_189_output_array, NULL)

/* Tensor #55 */
AI_TENSOR_OBJ_DECLARE(
  nl_183_output, AI_STATIC,
  307, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_183_output_array, NULL)

/* Tensor #56 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_190_output, AI_STATIC,
  75, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_190_output_array, NULL)

/* Tensor #57 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_192_output, AI_STATIC,
  77, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_192_output_array, NULL)

/* Tensor #58 */
AI_TENSOR_OBJ_DECLARE(
  gemm_201_bias, AI_STATIC,
  217, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 1), AI_STRIDE_INIT(4, 4, 4, 2048, 2048),
  1, &gemm_201_bias_array, NULL)

/* Tensor #59 */
AI_TENSOR_OBJ_DECLARE(
  gemm_201_output, AI_STATIC,
  218, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 30), AI_STRIDE_INIT(4, 4, 4, 2048, 2048),
  1, &gemm_201_output_array, NULL)

/* Tensor #60 */
AI_TENSOR_OBJ_DECLARE(
  gemm_201_weights, AI_STATIC,
  219, 0x0,
  AI_SHAPE_INIT(4, 128, 512, 1, 1), AI_STRIDE_INIT(4, 4, 512, 262144, 262144),
  1, &gemm_201_weights_array, NULL)

/* Tensor #61 */
AI_TENSOR_OBJ_DECLARE(
  gemm_213_bias, AI_STATIC,
  220, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_213_bias_array, NULL)

/* Tensor #62 */
AI_TENSOR_OBJ_DECLARE(
  gemm_213_output, AI_STATIC,
  221, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_213_output_array, NULL)

/* Tensor #63 */
AI_TENSOR_OBJ_DECLARE(
  gemm_213_weights, AI_STATIC,
  222, 0x0,
  AI_SHAPE_INIT(4, 512, 128, 1, 1), AI_STRIDE_INIT(4, 4, 2048, 262144, 262144),
  1, &gemm_213_weights_array, NULL)

/* Tensor #64 */
AI_TENSOR_OBJ_DECLARE(
  nl_204_output, AI_STATIC,
  310, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 30), AI_STRIDE_INIT(4, 4, 4, 2048, 2048),
  1, &nl_204_output_array, NULL)

/* Tensor #65 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_216_output, AI_STATIC,
  79, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_216_output_array, NULL)

/* Tensor #66 */
AI_TENSOR_OBJ_DECLARE(
  reduce_217_output, AI_STATIC,
  372, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_217_output_array, NULL)

/* Tensor #67 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_219_output, AI_STATIC,
  80, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_219_output_array, NULL)

/* Tensor #68 */
AI_TENSOR_OBJ_DECLARE(
  reduce_217_Mul_output, AI_STATIC,
  371, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_217_Mul_output_array, NULL)

/* Tensor #69 */
AI_TENSOR_OBJ_DECLARE(
  reduce_220_output, AI_STATIC,
  374, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_220_output_array, NULL)

/* Tensor #70 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_223_output, AI_STATIC,
  81, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_223_output_array, NULL)

/* Tensor #71 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_fut1_ln1_1_Reshape, AI_STATIC,
  184, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &functional_1_fut1_ln1_1_Reshape_array, NULL)

/* Tensor #72 */
AI_TENSOR_OBJ_DECLARE(
  nl_222_output, AI_STATIC,
  312, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_222_output_array, NULL)

/* Tensor #73 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_226_output, AI_STATIC,
  85, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_226_output_array, NULL)

/* Tensor #74 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_224_output, AI_STATIC,
  82, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_224_output_array, NULL)

/* Tensor #75 */
AI_TENSOR_OBJ_DECLARE(
  nl_218_output, AI_STATIC,
  311, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_218_output_array, NULL)

/* Tensor #76 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_225_output, AI_STATIC,
  84, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_225_output_array, NULL)

/* Tensor #77 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_227_output, AI_STATIC,
  86, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_227_output_array, NULL)

/* Tensor #78 */
AI_TENSOR_OBJ_DECLARE(
  gemm_228_output, AI_STATIC,
  223, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_228_output_array, NULL)

/* Tensor #79 */
AI_TENSOR_OBJ_DECLARE(
  gemm_228_weights, AI_STATIC,
  225, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_228_weights_array, NULL)

/* Tensor #80 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant66_4D, AI_STATIC,
  9, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 1), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &arith_constant66_4D_array, NULL)

/* Tensor #81 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_230_output, AI_STATIC,
  87, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &eltwise_230_output_array, NULL)

/* Tensor #82 */
AI_TENSOR_OBJ_DECLARE(
  gemm_228_output0, AI_STATIC,
  224, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &gemm_228_output_array, NULL)

/* Tensor #83 */
AI_TENSOR_OBJ_DECLARE(
  transpose_235_output, AI_STATIC,
  429, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 32, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3840),
  1, &transpose_235_output_array, NULL)

/* Tensor #84 */
AI_TENSOR_OBJ_DECLARE(
  gemm_231_output, AI_STATIC,
  226, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_231_output_array, NULL)

/* Tensor #85 */
AI_TENSOR_OBJ_DECLARE(
  gemm_231_weights, AI_STATIC,
  228, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_231_weights_array, NULL)

/* Tensor #86 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant65_4D, AI_STATIC,
  8, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 1), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &arith_constant65_4D_array, NULL)

/* Tensor #87 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_233_output, AI_STATIC,
  88, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &eltwise_233_output_array, NULL)

/* Tensor #88 */
AI_TENSOR_OBJ_DECLARE(
  gemm_231_output0, AI_STATIC,
  227, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &gemm_231_output_array, NULL)

/* Tensor #89 */
AI_TENSOR_OBJ_DECLARE(
  transpose_234_output, AI_STATIC,
  428, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &transpose_234_output_array, NULL)

/* Tensor #90 */
AI_TENSOR_OBJ_DECLARE(
  gemm_236_bias, AI_STATIC,
  229, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &gemm_236_bias_array, NULL)

/* Tensor #91 */
AI_TENSOR_OBJ_DECLARE(
  gemm_236_output, AI_STATIC,
  230, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 30, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3600),
  1, &gemm_236_output_array, NULL)

/* Tensor #92 */
AI_TENSOR_OBJ_DECLARE(
  gemm_240_output, AI_STATIC,
  231, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_240_output_array, NULL)

/* Tensor #93 */
AI_TENSOR_OBJ_DECLARE(
  gemm_240_weights, AI_STATIC,
  233, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_240_weights_array, NULL)

/* Tensor #94 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant64_4D, AI_STATIC,
  7, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 1), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &arith_constant64_4D_array, NULL)

/* Tensor #95 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_242_output, AI_STATIC,
  90, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &eltwise_242_output_array, NULL)

/* Tensor #96 */
AI_TENSOR_OBJ_DECLARE(
  gemm_240_output0, AI_STATIC,
  232, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &gemm_240_output_array, NULL)

/* Tensor #97 */
AI_TENSOR_OBJ_DECLARE(
  transpose_243_output, AI_STATIC,
  430, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &transpose_243_output_array, NULL)

/* Tensor #98 */
AI_TENSOR_OBJ_DECLARE(
  gemm_244_bias, AI_STATIC,
  234, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &gemm_244_bias_array, NULL)

/* Tensor #99 */
AI_TENSOR_OBJ_DECLARE(
  gemm_244_output, AI_STATIC,
  235, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &gemm_244_output_array, NULL)

/* Tensor #100 */
AI_TENSOR_OBJ_DECLARE(
  nl_239_output, AI_STATIC,
  313, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 30, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3600),
  1, &nl_239_output_array, NULL)

/* Tensor #101 */
AI_TENSOR_OBJ_DECLARE(
  transpose_246_output, AI_STATIC,
  431, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &transpose_246_output_array, NULL)

/* Tensor #102 */
AI_TENSOR_OBJ_DECLARE(
  gemm_248_bias, AI_STATIC,
  236, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_248_bias_array, NULL)

/* Tensor #103 */
AI_TENSOR_OBJ_DECLARE(
  gemm_248_output, AI_STATIC,
  237, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_248_output_array, NULL)

/* Tensor #104 */
AI_TENSOR_OBJ_DECLARE(
  gemm_248_weights, AI_STATIC,
  238, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_248_weights_array, NULL)

/* Tensor #105 */
AI_TENSOR_OBJ_DECLARE(
  transpose_246_output0, AI_STATIC,
  432, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &transpose_246_output_array, NULL)

/* Tensor #106 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_249_output, AI_STATIC,
  91, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_249_output_array, NULL)

/* Tensor #107 */
AI_TENSOR_OBJ_DECLARE(
  reduce_250_output, AI_STATIC,
  376, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_250_output_array, NULL)

/* Tensor #108 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_252_output, AI_STATIC,
  92, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_252_output_array, NULL)

/* Tensor #109 */
AI_TENSOR_OBJ_DECLARE(
  reduce_250_Mul_output, AI_STATIC,
  375, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_250_Mul_output_array, NULL)

/* Tensor #110 */
AI_TENSOR_OBJ_DECLARE(
  reduce_253_output, AI_STATIC,
  378, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_253_output_array, NULL)

/* Tensor #111 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_256_output, AI_STATIC,
  93, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_256_output_array, NULL)

/* Tensor #112 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_fut1_ln2_1_Reshape, AI_STATIC,
  185, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &functional_1_fut1_ln2_1_Reshape_array, NULL)

/* Tensor #113 */
AI_TENSOR_OBJ_DECLARE(
  nl_255_output, AI_STATIC,
  315, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_255_output_array, NULL)

/* Tensor #114 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_259_output, AI_STATIC,
  97, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_259_output_array, NULL)

/* Tensor #115 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_257_output, AI_STATIC,
  94, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_257_output_array, NULL)

/* Tensor #116 */
AI_TENSOR_OBJ_DECLARE(
  nl_251_output, AI_STATIC,
  314, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_251_output_array, NULL)

/* Tensor #117 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_258_output, AI_STATIC,
  96, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_258_output_array, NULL)

/* Tensor #118 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_260_output, AI_STATIC,
  98, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_260_output_array, NULL)

/* Tensor #119 */
AI_TENSOR_OBJ_DECLARE(
  gemm_269_bias, AI_STATIC,
  239, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 1), AI_STRIDE_INIT(4, 4, 4, 2048, 2048),
  1, &gemm_269_bias_array, NULL)

/* Tensor #120 */
AI_TENSOR_OBJ_DECLARE(
  gemm_269_output, AI_STATIC,
  240, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 30), AI_STRIDE_INIT(4, 4, 4, 2048, 2048),
  1, &gemm_269_output_array, NULL)

/* Tensor #121 */
AI_TENSOR_OBJ_DECLARE(
  gemm_269_weights, AI_STATIC,
  241, 0x0,
  AI_SHAPE_INIT(4, 128, 512, 1, 1), AI_STRIDE_INIT(4, 4, 512, 262144, 262144),
  1, &gemm_269_weights_array, NULL)

/* Tensor #122 */
AI_TENSOR_OBJ_DECLARE(
  gemm_281_bias, AI_STATIC,
  242, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_281_bias_array, NULL)

/* Tensor #123 */
AI_TENSOR_OBJ_DECLARE(
  gemm_281_output, AI_STATIC,
  243, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_281_output_array, NULL)

/* Tensor #124 */
AI_TENSOR_OBJ_DECLARE(
  gemm_281_weights, AI_STATIC,
  244, 0x0,
  AI_SHAPE_INIT(4, 512, 128, 1, 1), AI_STRIDE_INIT(4, 4, 2048, 262144, 262144),
  1, &gemm_281_weights_array, NULL)

/* Tensor #125 */
AI_TENSOR_OBJ_DECLARE(
  nl_272_output, AI_STATIC,
  316, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 30), AI_STRIDE_INIT(4, 4, 4, 2048, 2048),
  1, &nl_272_output_array, NULL)

/* Tensor #126 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_284_output, AI_STATIC,
  99, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_284_output_array, NULL)

/* Tensor #127 */
AI_TENSOR_OBJ_DECLARE(
  reduce_285_output, AI_STATIC,
  382, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_285_output_array, NULL)

/* Tensor #128 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_287_output, AI_STATIC,
  100, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_287_output_array, NULL)

/* Tensor #129 */
AI_TENSOR_OBJ_DECLARE(
  reduce_285_Mul_output, AI_STATIC,
  381, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_285_Mul_output_array, NULL)

/* Tensor #130 */
AI_TENSOR_OBJ_DECLARE(
  reduce_288_output, AI_STATIC,
  384, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_288_output_array, NULL)

/* Tensor #131 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_291_output, AI_STATIC,
  101, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_291_output_array, NULL)

/* Tensor #132 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_fut2_ln1_1_Reshape, AI_STATIC,
  186, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &functional_1_fut2_ln1_1_Reshape_array, NULL)

/* Tensor #133 */
AI_TENSOR_OBJ_DECLARE(
  nl_290_output, AI_STATIC,
  319, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_290_output_array, NULL)

/* Tensor #134 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_294_output, AI_STATIC,
  105, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_294_output_array, NULL)

/* Tensor #135 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_292_output, AI_STATIC,
  102, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_292_output_array, NULL)

/* Tensor #136 */
AI_TENSOR_OBJ_DECLARE(
  nl_286_output, AI_STATIC,
  317, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_286_output_array, NULL)

/* Tensor #137 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_293_output, AI_STATIC,
  104, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_293_output_array, NULL)

/* Tensor #138 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_295_output, AI_STATIC,
  106, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_295_output_array, NULL)

/* Tensor #139 */
AI_TENSOR_OBJ_DECLARE(
  gemm_296_output, AI_STATIC,
  245, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_296_output_array, NULL)

/* Tensor #140 */
AI_TENSOR_OBJ_DECLARE(
  gemm_296_weights, AI_STATIC,
  247, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_296_weights_array, NULL)

/* Tensor #141 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant60_4D, AI_STATIC,
  6, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 1), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &arith_constant60_4D_array, NULL)

/* Tensor #142 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_298_output, AI_STATIC,
  107, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &eltwise_298_output_array, NULL)

/* Tensor #143 */
AI_TENSOR_OBJ_DECLARE(
  gemm_296_output0, AI_STATIC,
  246, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &gemm_296_output_array, NULL)

/* Tensor #144 */
AI_TENSOR_OBJ_DECLARE(
  transpose_303_output, AI_STATIC,
  434, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 32, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3840),
  1, &transpose_303_output_array, NULL)

/* Tensor #145 */
AI_TENSOR_OBJ_DECLARE(
  gemm_299_output, AI_STATIC,
  248, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_299_output_array, NULL)

/* Tensor #146 */
AI_TENSOR_OBJ_DECLARE(
  gemm_299_weights, AI_STATIC,
  250, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_299_weights_array, NULL)

/* Tensor #147 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant59_4D, AI_STATIC,
  5, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 1), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &arith_constant59_4D_array, NULL)

/* Tensor #148 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_301_output, AI_STATIC,
  109, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &eltwise_301_output_array, NULL)

/* Tensor #149 */
AI_TENSOR_OBJ_DECLARE(
  gemm_299_output0, AI_STATIC,
  249, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &gemm_299_output_array, NULL)

/* Tensor #150 */
AI_TENSOR_OBJ_DECLARE(
  transpose_302_output, AI_STATIC,
  433, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &transpose_302_output_array, NULL)

/* Tensor #151 */
AI_TENSOR_OBJ_DECLARE(
  gemm_304_bias, AI_STATIC,
  251, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &gemm_304_bias_array, NULL)

/* Tensor #152 */
AI_TENSOR_OBJ_DECLARE(
  gemm_304_output, AI_STATIC,
  252, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 30, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3600),
  1, &gemm_304_output_array, NULL)

/* Tensor #153 */
AI_TENSOR_OBJ_DECLARE(
  gemm_308_output, AI_STATIC,
  253, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_308_output_array, NULL)

/* Tensor #154 */
AI_TENSOR_OBJ_DECLARE(
  gemm_308_weights, AI_STATIC,
  255, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_308_weights_array, NULL)

/* Tensor #155 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant58_4D, AI_STATIC,
  4, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 1), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &arith_constant58_4D_array, NULL)

/* Tensor #156 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_310_output, AI_STATIC,
  111, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &eltwise_310_output_array, NULL)

/* Tensor #157 */
AI_TENSOR_OBJ_DECLARE(
  gemm_308_output0, AI_STATIC,
  254, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &gemm_308_output_array, NULL)

/* Tensor #158 */
AI_TENSOR_OBJ_DECLARE(
  transpose_311_output, AI_STATIC,
  435, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &transpose_311_output_array, NULL)

/* Tensor #159 */
AI_TENSOR_OBJ_DECLARE(
  gemm_312_bias, AI_STATIC,
  256, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &gemm_312_bias_array, NULL)

/* Tensor #160 */
AI_TENSOR_OBJ_DECLARE(
  gemm_312_output, AI_STATIC,
  257, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &gemm_312_output_array, NULL)

/* Tensor #161 */
AI_TENSOR_OBJ_DECLARE(
  nl_307_output, AI_STATIC,
  320, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 30, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3600),
  1, &nl_307_output_array, NULL)

/* Tensor #162 */
AI_TENSOR_OBJ_DECLARE(
  transpose_314_output, AI_STATIC,
  436, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &transpose_314_output_array, NULL)

/* Tensor #163 */
AI_TENSOR_OBJ_DECLARE(
  gemm_316_bias, AI_STATIC,
  258, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_316_bias_array, NULL)

/* Tensor #164 */
AI_TENSOR_OBJ_DECLARE(
  gemm_316_output, AI_STATIC,
  259, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_316_output_array, NULL)

/* Tensor #165 */
AI_TENSOR_OBJ_DECLARE(
  gemm_316_weights, AI_STATIC,
  260, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_316_weights_array, NULL)

/* Tensor #166 */
AI_TENSOR_OBJ_DECLARE(
  transpose_314_output0, AI_STATIC,
  437, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &transpose_314_output_array, NULL)

/* Tensor #167 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_317_output, AI_STATIC,
  112, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_317_output_array, NULL)

/* Tensor #168 */
AI_TENSOR_OBJ_DECLARE(
  reduce_318_output, AI_STATIC,
  388, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_318_output_array, NULL)

/* Tensor #169 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_320_output, AI_STATIC,
  113, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_320_output_array, NULL)

/* Tensor #170 */
AI_TENSOR_OBJ_DECLARE(
  reduce_318_Mul_output, AI_STATIC,
  387, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_318_Mul_output_array, NULL)

/* Tensor #171 */
AI_TENSOR_OBJ_DECLARE(
  reduce_321_output, AI_STATIC,
  390, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_321_output_array, NULL)

/* Tensor #172 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_324_output, AI_STATIC,
  114, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_324_output_array, NULL)

/* Tensor #173 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_fut2_ln2_1_Reshape, AI_STATIC,
  187, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &functional_1_fut2_ln2_1_Reshape_array, NULL)

/* Tensor #174 */
AI_TENSOR_OBJ_DECLARE(
  nl_323_output, AI_STATIC,
  322, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_323_output_array, NULL)

/* Tensor #175 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_327_output, AI_STATIC,
  118, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_327_output_array, NULL)

/* Tensor #176 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_325_output, AI_STATIC,
  115, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_325_output_array, NULL)

/* Tensor #177 */
AI_TENSOR_OBJ_DECLARE(
  nl_319_output, AI_STATIC,
  321, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_319_output_array, NULL)

/* Tensor #178 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_326_output, AI_STATIC,
  117, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_326_output_array, NULL)

/* Tensor #179 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_328_output, AI_STATIC,
  119, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_328_output_array, NULL)

/* Tensor #180 */
AI_TENSOR_OBJ_DECLARE(
  gemm_337_bias, AI_STATIC,
  261, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 1), AI_STRIDE_INIT(4, 4, 4, 2048, 2048),
  1, &gemm_337_bias_array, NULL)

/* Tensor #181 */
AI_TENSOR_OBJ_DECLARE(
  gemm_337_output, AI_STATIC,
  262, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 30), AI_STRIDE_INIT(4, 4, 4, 2048, 2048),
  1, &gemm_337_output_array, NULL)

/* Tensor #182 */
AI_TENSOR_OBJ_DECLARE(
  gemm_337_weights, AI_STATIC,
  263, 0x0,
  AI_SHAPE_INIT(4, 128, 512, 1, 1), AI_STRIDE_INIT(4, 4, 512, 262144, 262144),
  1, &gemm_337_weights_array, NULL)

/* Tensor #183 */
AI_TENSOR_OBJ_DECLARE(
  gemm_349_bias, AI_STATIC,
  264, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_349_bias_array, NULL)

/* Tensor #184 */
AI_TENSOR_OBJ_DECLARE(
  gemm_349_output, AI_STATIC,
  265, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_349_output_array, NULL)

/* Tensor #185 */
AI_TENSOR_OBJ_DECLARE(
  gemm_349_weights, AI_STATIC,
  266, 0x0,
  AI_SHAPE_INIT(4, 512, 128, 1, 1), AI_STRIDE_INIT(4, 4, 2048, 262144, 262144),
  1, &gemm_349_weights_array, NULL)

/* Tensor #186 */
AI_TENSOR_OBJ_DECLARE(
  nl_340_output, AI_STATIC,
  324, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 30), AI_STRIDE_INIT(4, 4, 4, 2048, 2048),
  1, &nl_340_output_array, NULL)

/* Tensor #187 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_352_output, AI_STATIC,
  122, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_352_output_array, NULL)

/* Tensor #188 */
AI_TENSOR_OBJ_DECLARE(
  reduce_353_output, AI_STATIC,
  392, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_353_output_array, NULL)

/* Tensor #189 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_355_output, AI_STATIC,
  123, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_355_output_array, NULL)

/* Tensor #190 */
AI_TENSOR_OBJ_DECLARE(
  reduce_353_Mul_output, AI_STATIC,
  391, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_353_Mul_output_array, NULL)

/* Tensor #191 */
AI_TENSOR_OBJ_DECLARE(
  reduce_356_output, AI_STATIC,
  394, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_356_output_array, NULL)

/* Tensor #192 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_359_output, AI_STATIC,
  124, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_359_output_array, NULL)

/* Tensor #193 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_fut3_ln1_1_Reshape, AI_STATIC,
  188, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &functional_1_fut3_ln1_1_Reshape_array, NULL)

/* Tensor #194 */
AI_TENSOR_OBJ_DECLARE(
  nl_358_output, AI_STATIC,
  326, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_358_output_array, NULL)

/* Tensor #195 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_362_output, AI_STATIC,
  130, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_362_output_array, NULL)

/* Tensor #196 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_360_output, AI_STATIC,
  127, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_360_output_array, NULL)

/* Tensor #197 */
AI_TENSOR_OBJ_DECLARE(
  nl_354_output, AI_STATIC,
  325, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_354_output_array, NULL)

/* Tensor #198 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_361_output, AI_STATIC,
  129, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_361_output_array, NULL)

/* Tensor #199 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_363_output, AI_STATIC,
  131, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_363_output_array, NULL)

/* Tensor #200 */
AI_TENSOR_OBJ_DECLARE(
  gemm_364_output, AI_STATIC,
  267, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_364_output_array, NULL)

/* Tensor #201 */
AI_TENSOR_OBJ_DECLARE(
  gemm_364_weights, AI_STATIC,
  269, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_364_weights_array, NULL)

/* Tensor #202 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant54_4D, AI_STATIC,
  3, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 1), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &arith_constant54_4D_array, NULL)

/* Tensor #203 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_366_output, AI_STATIC,
  132, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &eltwise_366_output_array, NULL)

/* Tensor #204 */
AI_TENSOR_OBJ_DECLARE(
  gemm_364_output0, AI_STATIC,
  268, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &gemm_364_output_array, NULL)

/* Tensor #205 */
AI_TENSOR_OBJ_DECLARE(
  transpose_371_output, AI_STATIC,
  439, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 32, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3840),
  1, &transpose_371_output_array, NULL)

/* Tensor #206 */
AI_TENSOR_OBJ_DECLARE(
  gemm_367_output, AI_STATIC,
  270, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_367_output_array, NULL)

/* Tensor #207 */
AI_TENSOR_OBJ_DECLARE(
  gemm_367_weights, AI_STATIC,
  272, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_367_weights_array, NULL)

/* Tensor #208 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant53_4D, AI_STATIC,
  2, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 1), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &arith_constant53_4D_array, NULL)

/* Tensor #209 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_369_output, AI_STATIC,
  133, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &eltwise_369_output_array, NULL)

/* Tensor #210 */
AI_TENSOR_OBJ_DECLARE(
  gemm_367_output0, AI_STATIC,
  271, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &gemm_367_output_array, NULL)

/* Tensor #211 */
AI_TENSOR_OBJ_DECLARE(
  transpose_370_output, AI_STATIC,
  438, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &transpose_370_output_array, NULL)

/* Tensor #212 */
AI_TENSOR_OBJ_DECLARE(
  gemm_372_bias, AI_STATIC,
  273, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &gemm_372_bias_array, NULL)

/* Tensor #213 */
AI_TENSOR_OBJ_DECLARE(
  gemm_372_output, AI_STATIC,
  274, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 30, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3600),
  1, &gemm_372_output_array, NULL)

/* Tensor #214 */
AI_TENSOR_OBJ_DECLARE(
  gemm_376_output, AI_STATIC,
  275, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_376_output_array, NULL)

/* Tensor #215 */
AI_TENSOR_OBJ_DECLARE(
  gemm_376_weights, AI_STATIC,
  277, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_376_weights_array, NULL)

/* Tensor #216 */
AI_TENSOR_OBJ_DECLARE(
  arith_constant52_4D, AI_STATIC,
  1, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 1), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &arith_constant52_4D_array, NULL)

/* Tensor #217 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_378_output, AI_STATIC,
  136, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &eltwise_378_output_array, NULL)

/* Tensor #218 */
AI_TENSOR_OBJ_DECLARE(
  gemm_376_output0, AI_STATIC,
  276, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &gemm_376_output_array, NULL)

/* Tensor #219 */
AI_TENSOR_OBJ_DECLARE(
  transpose_379_output, AI_STATIC,
  440, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &transpose_379_output_array, NULL)

/* Tensor #220 */
AI_TENSOR_OBJ_DECLARE(
  gemm_380_bias, AI_STATIC,
  278, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &gemm_380_bias_array, NULL)

/* Tensor #221 */
AI_TENSOR_OBJ_DECLARE(
  gemm_380_output, AI_STATIC,
  279, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 30, 4), AI_STRIDE_INIT(4, 4, 4, 128, 3840),
  1, &gemm_380_output_array, NULL)

/* Tensor #222 */
AI_TENSOR_OBJ_DECLARE(
  nl_375_output, AI_STATIC,
  327, 0x0,
  AI_SHAPE_INIT(4, 1, 30, 30, 4), AI_STRIDE_INIT(4, 4, 4, 120, 3600),
  1, &nl_375_output_array, NULL)

/* Tensor #223 */
AI_TENSOR_OBJ_DECLARE(
  transpose_382_output, AI_STATIC,
  441, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 4, 30), AI_STRIDE_INIT(4, 4, 4, 128, 512),
  1, &transpose_382_output_array, NULL)

/* Tensor #224 */
AI_TENSOR_OBJ_DECLARE(
  gemm_384_bias, AI_STATIC,
  280, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_384_bias_array, NULL)

/* Tensor #225 */
AI_TENSOR_OBJ_DECLARE(
  gemm_384_output, AI_STATIC,
  281, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_384_output_array, NULL)

/* Tensor #226 */
AI_TENSOR_OBJ_DECLARE(
  gemm_384_weights, AI_STATIC,
  282, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_384_weights_array, NULL)

/* Tensor #227 */
AI_TENSOR_OBJ_DECLARE(
  transpose_382_output0, AI_STATIC,
  442, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &transpose_382_output_array, NULL)

/* Tensor #228 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_385_output, AI_STATIC,
  138, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_385_output_array, NULL)

/* Tensor #229 */
AI_TENSOR_OBJ_DECLARE(
  reduce_386_output, AI_STATIC,
  396, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_386_output_array, NULL)

/* Tensor #230 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_388_output, AI_STATIC,
  139, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_388_output_array, NULL)

/* Tensor #231 */
AI_TENSOR_OBJ_DECLARE(
  reduce_386_Mul_output, AI_STATIC,
  395, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_386_Mul_output_array, NULL)

/* Tensor #232 */
AI_TENSOR_OBJ_DECLARE(
  reduce_389_output, AI_STATIC,
  398, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_389_output_array, NULL)

/* Tensor #233 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_392_output, AI_STATIC,
  140, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_392_output_array, NULL)

/* Tensor #234 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_fut3_ln2_1_Reshape, AI_STATIC,
  189, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &functional_1_fut3_ln2_1_Reshape_array, NULL)

/* Tensor #235 */
AI_TENSOR_OBJ_DECLARE(
  nl_391_output, AI_STATIC,
  330, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_391_output_array, NULL)

/* Tensor #236 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_395_output, AI_STATIC,
  144, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_395_output_array, NULL)

/* Tensor #237 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_393_output, AI_STATIC,
  141, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_393_output_array, NULL)

/* Tensor #238 */
AI_TENSOR_OBJ_DECLARE(
  nl_387_output, AI_STATIC,
  328, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_387_output_array, NULL)

/* Tensor #239 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_394_output, AI_STATIC,
  143, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_394_output_array, NULL)

/* Tensor #240 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_396_output, AI_STATIC,
  145, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_396_output_array, NULL)

/* Tensor #241 */
AI_TENSOR_OBJ_DECLARE(
  gemm_405_bias, AI_STATIC,
  283, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 1), AI_STRIDE_INIT(4, 4, 4, 2048, 2048),
  1, &gemm_405_bias_array, NULL)

/* Tensor #242 */
AI_TENSOR_OBJ_DECLARE(
  gemm_405_output, AI_STATIC,
  284, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 30), AI_STRIDE_INIT(4, 4, 4, 2048, 2048),
  1, &gemm_405_output_array, NULL)

/* Tensor #243 */
AI_TENSOR_OBJ_DECLARE(
  gemm_405_weights, AI_STATIC,
  285, 0x0,
  AI_SHAPE_INIT(4, 128, 512, 1, 1), AI_STRIDE_INIT(4, 4, 512, 262144, 262144),
  1, &gemm_405_weights_array, NULL)

/* Tensor #244 */
AI_TENSOR_OBJ_DECLARE(
  gemm_417_bias, AI_STATIC,
  286, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_417_bias_array, NULL)

/* Tensor #245 */
AI_TENSOR_OBJ_DECLARE(
  gemm_417_output, AI_STATIC,
  287, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_417_output_array, NULL)

/* Tensor #246 */
AI_TENSOR_OBJ_DECLARE(
  gemm_417_weights, AI_STATIC,
  288, 0x0,
  AI_SHAPE_INIT(4, 512, 128, 1, 1), AI_STRIDE_INIT(4, 4, 2048, 262144, 262144),
  1, &gemm_417_weights_array, NULL)

/* Tensor #247 */
AI_TENSOR_OBJ_DECLARE(
  nl_408_output, AI_STATIC,
  331, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 30), AI_STRIDE_INIT(4, 4, 4, 2048, 2048),
  1, &nl_408_output_array, NULL)

/* Tensor #248 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_420_output, AI_STATIC,
  147, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &eltwise_420_output_array, NULL)

/* Tensor #249 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_1_output0, AI_STATIC,
  19, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &conv2d_1_output_array, NULL)

/* Tensor #250 */
AI_TENSOR_OBJ_DECLARE(
  pad_3_output, AI_STATIC,
  348, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 302), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &pad_3_output_array, NULL)

/* Tensor #251 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_5_output0, AI_STATIC,
  30, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &conv2d_5_output_array, NULL)

/* Tensor #252 */
AI_TENSOR_OBJ_DECLARE(
  reduce_7_output, AI_STATIC,
  414, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_7_output_array, NULL)

/* Tensor #253 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_9_output, AI_STATIC,
  180, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_9_output_array, NULL)

/* Tensor #254 */
AI_TENSOR_OBJ_DECLARE(
  reduce_7_Mul_output, AI_STATIC,
  412, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_7_Mul_output_array, NULL)

/* Tensor #255 */
AI_TENSOR_OBJ_DECLARE(
  reduce_10_output, AI_STATIC,
  356, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_10_output_array, NULL)

/* Tensor #256 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_13_output, AI_STATIC,
  48, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_13_output_array, NULL)

/* Tensor #257 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_past_ln0_1_Reshape, AI_STATIC,
  190, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &functional_1_past_ln0_1_Reshape_array, NULL)

/* Tensor #258 */
AI_TENSOR_OBJ_DECLARE(
  nl_12_output, AI_STATIC,
  301, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_12_output_array, NULL)

/* Tensor #259 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_16_output, AI_STATIC,
  67, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_16_output_array, NULL)

/* Tensor #260 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_14_output, AI_STATIC,
  50, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_14_output_array, NULL)

/* Tensor #261 */
AI_TENSOR_OBJ_DECLARE(
  nl_8_output, AI_STATIC,
  343, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_8_output_array, NULL)

/* Tensor #262 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_15_output, AI_STATIC,
  60, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_15_output_array, NULL)

/* Tensor #263 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_17_output, AI_STATIC,
  69, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_17_output_array, NULL)

/* Tensor #264 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_19_output, AI_STATIC,
  78, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_19_output_array, NULL)

/* Tensor #265 */
AI_TENSOR_OBJ_DECLARE(
  nl_18_output, AI_STATIC,
  309, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &nl_18_output_array, NULL)

/* Tensor #266 */
AI_TENSOR_OBJ_DECLARE(
  pad_20_output, AI_STATIC,
  347, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 304), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &pad_20_output_array, NULL)

/* Tensor #267 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_23_output, AI_STATIC,
  23, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &conv2d_23_output_array, NULL)

/* Tensor #268 */
AI_TENSOR_OBJ_DECLARE(
  reduce_27_output, AI_STATIC,
  380, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_27_output_array, NULL)

/* Tensor #269 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_29_output, AI_STATIC,
  108, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_29_output_array, NULL)

/* Tensor #270 */
AI_TENSOR_OBJ_DECLARE(
  reduce_27_Mul_output, AI_STATIC,
  379, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_27_Mul_output_array, NULL)

/* Tensor #271 */
AI_TENSOR_OBJ_DECLARE(
  reduce_30_output, AI_STATIC,
  386, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_30_output_array, NULL)

/* Tensor #272 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_33_output, AI_STATIC,
  120, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_33_output_array, NULL)

/* Tensor #273 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_past_ln1_1_Reshape, AI_STATIC,
  191, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &functional_1_past_ln1_1_Reshape_array, NULL)

/* Tensor #274 */
AI_TENSOR_OBJ_DECLARE(
  nl_32_output, AI_STATIC,
  323, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_32_output_array, NULL)

/* Tensor #275 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_36_output, AI_STATIC,
  134, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_36_output_array, NULL)

/* Tensor #276 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_34_output, AI_STATIC,
  121, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_34_output_array, NULL)

/* Tensor #277 */
AI_TENSOR_OBJ_DECLARE(
  nl_28_output, AI_STATIC,
  318, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_28_output_array, NULL)

/* Tensor #278 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_35_output, AI_STATIC,
  126, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_35_output_array, NULL)

/* Tensor #279 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_37_output, AI_STATIC,
  137, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_37_output_array, NULL)

/* Tensor #280 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_39_output, AI_STATIC,
  146, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_39_output_array, NULL)

/* Tensor #281 */
AI_TENSOR_OBJ_DECLARE(
  nl_38_output, AI_STATIC,
  329, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &nl_38_output_array, NULL)

/* Tensor #282 */
AI_TENSOR_OBJ_DECLARE(
  pad_40_output, AI_STATIC,
  350, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 308), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &pad_40_output_array, NULL)

/* Tensor #283 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_43_output, AI_STATIC,
  26, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &conv2d_43_output_array, NULL)

/* Tensor #284 */
AI_TENSOR_OBJ_DECLARE(
  reduce_47_output, AI_STATIC,
  405, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_47_output_array, NULL)

/* Tensor #285 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_49_output, AI_STATIC,
  156, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_49_output_array, NULL)

/* Tensor #286 */
AI_TENSOR_OBJ_DECLARE(
  reduce_47_Mul_output, AI_STATIC,
  404, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_47_Mul_output_array, NULL)

/* Tensor #287 */
AI_TENSOR_OBJ_DECLARE(
  reduce_50_output, AI_STATIC,
  407, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_50_output_array, NULL)

/* Tensor #288 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_53_output, AI_STATIC,
  157, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_53_output_array, NULL)

/* Tensor #289 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_past_ln2_1_Reshape, AI_STATIC,
  192, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &functional_1_past_ln2_1_Reshape_array, NULL)

/* Tensor #290 */
AI_TENSOR_OBJ_DECLARE(
  nl_52_output, AI_STATIC,
  337, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_52_output_array, NULL)

/* Tensor #291 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_56_output, AI_STATIC,
  161, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_56_output_array, NULL)

/* Tensor #292 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_54_output, AI_STATIC,
  158, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_54_output_array, NULL)

/* Tensor #293 */
AI_TENSOR_OBJ_DECLARE(
  nl_48_output, AI_STATIC,
  336, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_48_output_array, NULL)

/* Tensor #294 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_55_output, AI_STATIC,
  160, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_55_output_array, NULL)

/* Tensor #295 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_57_output, AI_STATIC,
  162, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_57_output_array, NULL)

/* Tensor #296 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_59_output, AI_STATIC,
  163, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_59_output_array, NULL)

/* Tensor #297 */
AI_TENSOR_OBJ_DECLARE(
  nl_58_output, AI_STATIC,
  338, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &nl_58_output_array, NULL)

/* Tensor #298 */
AI_TENSOR_OBJ_DECLARE(
  pad_60_output, AI_STATIC,
  351, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 316), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &pad_60_output_array, NULL)

/* Tensor #299 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_63_output, AI_STATIC,
  34, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &conv2d_63_output_array, NULL)

/* Tensor #300 */
AI_TENSOR_OBJ_DECLARE(
  reduce_67_output, AI_STATIC,
  409, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_67_output_array, NULL)

/* Tensor #301 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_69_output, AI_STATIC,
  164, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_69_output_array, NULL)

/* Tensor #302 */
AI_TENSOR_OBJ_DECLARE(
  reduce_67_Mul_output, AI_STATIC,
  408, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_67_Mul_output_array, NULL)

/* Tensor #303 */
AI_TENSOR_OBJ_DECLARE(
  reduce_70_output, AI_STATIC,
  411, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_70_output_array, NULL)

/* Tensor #304 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_73_output, AI_STATIC,
  165, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_73_output_array, NULL)

/* Tensor #305 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_past_ln3_1_Reshape, AI_STATIC,
  193, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &functional_1_past_ln3_1_Reshape_array, NULL)

/* Tensor #306 */
AI_TENSOR_OBJ_DECLARE(
  nl_72_output, AI_STATIC,
  340, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_72_output_array, NULL)

/* Tensor #307 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_76_output, AI_STATIC,
  169, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_76_output_array, NULL)

/* Tensor #308 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_74_output, AI_STATIC,
  166, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_74_output_array, NULL)

/* Tensor #309 */
AI_TENSOR_OBJ_DECLARE(
  nl_68_output, AI_STATIC,
  339, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_68_output_array, NULL)

/* Tensor #310 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_75_output, AI_STATIC,
  168, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_75_output_array, NULL)

/* Tensor #311 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_77_output, AI_STATIC,
  170, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_77_output_array, NULL)

/* Tensor #312 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_79_output, AI_STATIC,
  171, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_79_output_array, NULL)

/* Tensor #313 */
AI_TENSOR_OBJ_DECLARE(
  nl_78_output, AI_STATIC,
  341, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &nl_78_output_array, NULL)

/* Tensor #314 */
AI_TENSOR_OBJ_DECLARE(
  pad_80_output, AI_STATIC,
  352, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 332), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &pad_80_output_array, NULL)

/* Tensor #315 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_83_output, AI_STATIC,
  37, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &conv2d_83_output_array, NULL)

/* Tensor #316 */
AI_TENSOR_OBJ_DECLARE(
  reduce_87_output, AI_STATIC,
  416, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_87_output_array, NULL)

/* Tensor #317 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_89_output, AI_STATIC,
  172, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_89_output_array, NULL)

/* Tensor #318 */
AI_TENSOR_OBJ_DECLARE(
  reduce_87_Mul_output, AI_STATIC,
  415, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_87_Mul_output_array, NULL)

/* Tensor #319 */
AI_TENSOR_OBJ_DECLARE(
  reduce_90_output, AI_STATIC,
  418, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_90_output_array, NULL)

/* Tensor #320 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_93_output, AI_STATIC,
  173, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_93_output_array, NULL)

/* Tensor #321 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_past_ln4_1_Reshape, AI_STATIC,
  194, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &functional_1_past_ln4_1_Reshape_array, NULL)

/* Tensor #322 */
AI_TENSOR_OBJ_DECLARE(
  nl_92_output, AI_STATIC,
  344, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_92_output_array, NULL)

/* Tensor #323 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_96_output, AI_STATIC,
  177, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_96_output_array, NULL)

/* Tensor #324 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_94_output, AI_STATIC,
  174, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_94_output_array, NULL)

/* Tensor #325 */
AI_TENSOR_OBJ_DECLARE(
  nl_88_output, AI_STATIC,
  342, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_88_output_array, NULL)

/* Tensor #326 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_95_output, AI_STATIC,
  176, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_95_output_array, NULL)

/* Tensor #327 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_97_output, AI_STATIC,
  178, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_97_output_array, NULL)

/* Tensor #328 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_99_output, AI_STATIC,
  179, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_99_output_array, NULL)

/* Tensor #329 */
AI_TENSOR_OBJ_DECLARE(
  nl_98_output, AI_STATIC,
  345, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &nl_98_output_array, NULL)

/* Tensor #330 */
AI_TENSOR_OBJ_DECLARE(
  pad_100_output, AI_STATIC,
  346, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 364), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &pad_100_output_array, NULL)

/* Tensor #331 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_103_output, AI_STATIC,
  15, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &conv2d_103_output_array, NULL)

/* Tensor #332 */
AI_TENSOR_OBJ_DECLARE(
  reduce_107_output, AI_STATIC,
  354, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_107_output_array, NULL)

/* Tensor #333 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_109_output, AI_STATIC,
  39, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_109_output_array, NULL)

/* Tensor #334 */
AI_TENSOR_OBJ_DECLARE(
  reduce_107_Mul_output, AI_STATIC,
  353, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_107_Mul_output_array, NULL)

/* Tensor #335 */
AI_TENSOR_OBJ_DECLARE(
  reduce_110_output, AI_STATIC,
  358, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_110_output_array, NULL)

/* Tensor #336 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_113_output, AI_STATIC,
  40, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_113_output_array, NULL)

/* Tensor #337 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_past_ln5_1_Reshape, AI_STATIC,
  195, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &functional_1_past_ln5_1_Reshape_array, NULL)

/* Tensor #338 */
AI_TENSOR_OBJ_DECLARE(
  nl_112_output, AI_STATIC,
  299, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_112_output_array, NULL)

/* Tensor #339 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_116_output, AI_STATIC,
  44, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_116_output_array, NULL)

/* Tensor #340 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_114_output, AI_STATIC,
  41, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_114_output_array, NULL)

/* Tensor #341 */
AI_TENSOR_OBJ_DECLARE(
  nl_108_output, AI_STATIC,
  298, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_108_output_array, NULL)

/* Tensor #342 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_115_output, AI_STATIC,
  43, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_115_output_array, NULL)

/* Tensor #343 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_117_output, AI_STATIC,
  45, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_117_output_array, NULL)

/* Tensor #344 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_119_output, AI_STATIC,
  46, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_119_output_array, NULL)

/* Tensor #345 */
AI_TENSOR_OBJ_DECLARE(
  nl_118_output, AI_STATIC,
  300, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &nl_118_output_array, NULL)

/* Tensor #346 */
AI_TENSOR_OBJ_DECLARE(
  gemm_128_output, AI_STATIC,
  196, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &gemm_128_output_array, NULL)

/* Tensor #347 */
AI_TENSOR_OBJ_DECLARE(
  gemm_128_weights, AI_STATIC,
  198, 0x0,
  AI_SHAPE_INIT(4, 96, 1, 1, 1), AI_STRIDE_INIT(4, 4, 384, 384, 384),
  1, &gemm_128_weights_array, NULL)

/* Tensor #348 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_133_output, AI_STATIC,
  47, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 300), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &eltwise_133_output_array, NULL)

/* Tensor #349 */
AI_TENSOR_OBJ_DECLARE(
  nl_131_output0, AI_STATIC,
  303, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 300), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_131_output_array, NULL)

/* Tensor #350 */
AI_TENSOR_OBJ_DECLARE(
  reduce_134_output, AI_STATIC,
  359, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 1), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &reduce_134_output_array, NULL)

/* Tensor #351 */
AI_TENSOR_OBJ_DECLARE(
  tile_137_output, AI_STATIC,
  422, 0x0,
  AI_SHAPE_INIT(4, 1, 96, 1, 30), AI_STRIDE_INIT(4, 4, 4, 384, 384),
  1, &tile_137_output_array, NULL)

/* Tensor #352 */
AI_TENSOR_OBJ_DECLARE(
  concat_421_output, AI_STATIC,
  13, 0x0,
  AI_SHAPE_INIT(4, 1, 224, 1, 30), AI_STRIDE_INIT(4, 4, 4, 896, 896),
  1, &concat_421_output_array, NULL)

/* Tensor #353 */
AI_TENSOR_OBJ_DECLARE(
  reduce_422_output, AI_STATIC,
  401, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_422_output_array, NULL)

/* Tensor #354 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_424_output, AI_STATIC,
  148, 0x0,
  AI_SHAPE_INIT(4, 1, 224, 1, 30), AI_STRIDE_INIT(4, 4, 4, 896, 896),
  1, &eltwise_424_output_array, NULL)

/* Tensor #355 */
AI_TENSOR_OBJ_DECLARE(
  reduce_422_Mul_output, AI_STATIC,
  399, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_422_Mul_output_array, NULL)

/* Tensor #356 */
AI_TENSOR_OBJ_DECLARE(
  reduce_425_output, AI_STATIC,
  403, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &reduce_425_output_array, NULL)

/* Tensor #357 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_428_output, AI_STATIC,
  149, 0x0,
  AI_SHAPE_INIT(4, 1, 224, 1, 30), AI_STRIDE_INIT(4, 4, 4, 896, 896),
  1, &eltwise_428_output_array, NULL)

/* Tensor #358 */
AI_TENSOR_OBJ_DECLARE(
  functional_1_fusion_ln_1_Reshape, AI_STATIC,
  181, 0x0,
  AI_SHAPE_INIT(4, 1, 224, 1, 1), AI_STRIDE_INIT(4, 4, 4, 896, 896),
  1, &functional_1_fusion_ln_1_Reshape_array, NULL)

/* Tensor #359 */
AI_TENSOR_OBJ_DECLARE(
  nl_427_output, AI_STATIC,
  333, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_427_output_array, NULL)

/* Tensor #360 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_431_output, AI_STATIC,
  154, 0x0,
  AI_SHAPE_INIT(4, 1, 224, 1, 30), AI_STRIDE_INIT(4, 4, 4, 896, 896),
  1, &eltwise_431_output_array, NULL)

/* Tensor #361 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_429_output, AI_STATIC,
  150, 0x0,
  AI_SHAPE_INIT(4, 1, 224, 1, 30), AI_STRIDE_INIT(4, 4, 4, 896, 896),
  1, &eltwise_429_output_array, NULL)

/* Tensor #362 */
AI_TENSOR_OBJ_DECLARE(
  nl_423_output, AI_STATIC,
  332, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 30), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &nl_423_output_array, NULL)

/* Tensor #363 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_430_output, AI_STATIC,
  152, 0x0,
  AI_SHAPE_INIT(4, 1, 224, 1, 30), AI_STRIDE_INIT(4, 4, 4, 896, 896),
  1, &eltwise_430_output_array, NULL)

/* Tensor #364 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_432_output, AI_STATIC,
  155, 0x0,
  AI_SHAPE_INIT(4, 1, 224, 1, 30), AI_STRIDE_INIT(4, 4, 4, 896, 896),
  1, &eltwise_432_output_array, NULL)

/* Tensor #365 */
AI_TENSOR_OBJ_DECLARE(
  gemm_441_bias, AI_STATIC,
  289, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_441_bias_array, NULL)

/* Tensor #366 */
AI_TENSOR_OBJ_DECLARE(
  gemm_441_output, AI_STATIC,
  290, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_441_output_array, NULL)

/* Tensor #367 */
AI_TENSOR_OBJ_DECLARE(
  gemm_441_weights, AI_STATIC,
  291, 0x0,
  AI_SHAPE_INIT(4, 224, 128, 1, 1), AI_STRIDE_INIT(4, 4, 896, 114688, 114688),
  1, &gemm_441_weights_array, NULL)

/* Tensor #368 */
AI_TENSOR_OBJ_DECLARE(
  gemm_453_bias, AI_STATIC,
  292, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_453_bias_array, NULL)

/* Tensor #369 */
AI_TENSOR_OBJ_DECLARE(
  gemm_453_output, AI_STATIC,
  293, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &gemm_453_output_array, NULL)

/* Tensor #370 */
AI_TENSOR_OBJ_DECLARE(
  gemm_453_weights, AI_STATIC,
  294, 0x0,
  AI_SHAPE_INIT(4, 128, 128, 1, 1), AI_STRIDE_INIT(4, 4, 512, 65536, 65536),
  1, &gemm_453_weights_array, NULL)

/* Tensor #371 */
AI_TENSOR_OBJ_DECLARE(
  nl_444_output, AI_STATIC,
  334, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &nl_444_output_array, NULL)

/* Tensor #372 */
AI_TENSOR_OBJ_DECLARE(
  gemm_465_bias, AI_STATIC,
  295, 0x0,
  AI_SHAPE_INIT(4, 1, 3, 1, 1), AI_STRIDE_INIT(4, 4, 4, 12, 12),
  1, &gemm_465_bias_array, NULL)

/* Tensor #373 */
AI_TENSOR_OBJ_DECLARE(
  gemm_465_output, AI_STATIC,
  296, 0x0,
  AI_SHAPE_INIT(4, 1, 3, 1, 30), AI_STRIDE_INIT(4, 4, 4, 12, 12),
  1, &gemm_465_output_array, NULL)

/* Tensor #374 */
AI_TENSOR_OBJ_DECLARE(
  gemm_465_weights, AI_STATIC,
  297, 0x0,
  AI_SHAPE_INIT(4, 128, 3, 1, 1), AI_STRIDE_INIT(4, 4, 512, 1536, 1536),
  1, &gemm_465_weights_array, NULL)

/* Tensor #375 */
AI_TENSOR_OBJ_DECLARE(
  nl_456_output, AI_STATIC,
  335, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 30), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &nl_456_output_array, NULL)


AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_146_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &serving_default_kf_input0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_146_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_146_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_146_layer, 146,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_146_chain,
  NULL, &gemm_146_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_148_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_146_output, &arith_constant2),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_148_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_148_layer, 148,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_148_chain,
  NULL, &eltwise_148_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float reduce_149_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_149_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_149_neutral_value_data, reduce_149_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_149_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_148_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_149_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_149_layer, 149,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_149_chain,
  NULL, &reduce_149_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_149_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_151_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_148_output, &reduce_149_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_151_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_151_layer, 151,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_151_chain,
  NULL, &eltwise_151_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_152_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_152_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_152_neutral_value_data, reduce_152_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_152_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_151_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_152_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_152_layer, 153,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_152_chain,
  NULL, &reduce_152_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_152_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_155_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_154_output, &functional_1_fut0_ln1_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_155_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_155_layer, 155,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_155_chain,
  NULL, &eltwise_155_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_158_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_148_output, &eltwise_155_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_158_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_158_layer, 158,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_158_chain,
  NULL, &eltwise_158_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_156_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_150_output, &eltwise_155_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_156_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_156_layer, 156,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_156_chain,
  NULL, &eltwise_156_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_159_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_158_output, &eltwise_157_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_159_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_159_layer, 159,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_159_chain,
  NULL, &eltwise_159_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_160_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_159_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_160_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_160_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_160_layer, 160,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_160_chain,
  NULL, &gemm_160_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_162_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_160_output0, &arith_constant72_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_162_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_162_layer, 162,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_162_chain,
  NULL, &eltwise_162_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_167_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_162_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_167_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_167_layer, 167,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_167_chain,
  NULL, &transpose_167_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_163_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_159_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_163_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_163_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_163_layer, 163,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_163_chain,
  NULL, &gemm_163_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_165_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_163_output0, &arith_constant71_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_165_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_165_layer, 165,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_165_chain,
  NULL, &eltwise_165_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_166_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_165_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_166_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_166_layer, 166,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_166_chain,
  NULL, &transpose_166_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_168_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &transpose_166_output, &transpose_167_output, &gemm_168_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_168_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_168_layer, 168,
  MATMUL_TYPE, 0x0, NULL,
  matmul, forward_matmul,
  &gemm_168_chain,
  NULL, &gemm_168_layer, AI_STATIC, 
  .alpha = 1.0, 
  .beta = 1.0, 
  .tA = 0, 
  .tB = 0, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_172_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_159_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_172_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_172_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_172_layer, 172,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_172_chain,
  NULL, &gemm_172_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_174_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_172_output0, &arith_constant70_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_174_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_174_layer, 174,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_174_chain,
  NULL, &eltwise_174_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_175_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_174_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_175_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_175_layer, 175,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_175_chain,
  NULL, &transpose_175_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_176_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &nl_171_output, &transpose_175_output, &gemm_176_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_176_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_176_layer, 176,
  MATMUL_TYPE, 0x0, NULL,
  matmul, forward_matmul,
  &gemm_176_chain,
  NULL, &gemm_176_layer, AI_STATIC, 
  .alpha = 1.0, 
  .beta = 1.0, 
  .tA = 0, 
  .tB = 0, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_178_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_176_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_178_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_178_layer, 178,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_178_chain,
  NULL, &transpose_178_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_180_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_178_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_180_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_180_weights, &gemm_180_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_180_layer, 180,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_180_chain,
  NULL, &gemm_180_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_181_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_148_output, &gemm_180_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_181_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_181_layer, 181,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_181_chain,
  NULL, &eltwise_181_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float reduce_182_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_182_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_182_neutral_value_data, reduce_182_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_182_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_181_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_182_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_182_layer, 182,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_182_chain,
  NULL, &reduce_182_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_182_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_184_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_181_output, &reduce_182_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_184_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_184_layer, 184,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_184_chain,
  NULL, &eltwise_184_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_185_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_185_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_185_neutral_value_data, reduce_185_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_185_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_184_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_185_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_185_layer, 186,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_185_chain,
  NULL, &reduce_185_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_185_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_188_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_187_output, &functional_1_fut0_ln2_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_188_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_188_layer, 188,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_188_chain,
  NULL, &eltwise_188_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_191_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_181_output, &eltwise_188_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_191_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_191_layer, 191,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_191_chain,
  NULL, &eltwise_191_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_189_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_183_output, &eltwise_188_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_189_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_189_layer, 189,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_189_chain,
  NULL, &eltwise_189_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_192_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_191_output, &eltwise_190_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_192_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_192_layer, 192,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_192_chain,
  NULL, &eltwise_192_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_201_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_192_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_201_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_201_weights, &gemm_201_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_201_layer, 203,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_201_chain,
  NULL, &gemm_201_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_213_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &nl_204_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_213_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_213_weights, &gemm_213_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_213_layer, 215,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_213_chain,
  NULL, &gemm_213_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_216_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_181_output, &gemm_213_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_216_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_216_layer, 216,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_216_chain,
  NULL, &eltwise_216_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float reduce_217_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_217_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_217_neutral_value_data, reduce_217_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_217_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_216_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_217_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_217_layer, 217,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_217_chain,
  NULL, &reduce_217_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_217_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_219_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_216_output, &reduce_217_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_219_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_219_layer, 219,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_219_chain,
  NULL, &eltwise_219_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_220_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_220_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_220_neutral_value_data, reduce_220_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_220_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_219_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_220_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_220_layer, 221,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_220_chain,
  NULL, &reduce_220_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_220_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_223_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_222_output, &functional_1_fut1_ln1_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_223_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_223_layer, 223,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_223_chain,
  NULL, &eltwise_223_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_226_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_216_output, &eltwise_223_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_226_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_226_layer, 226,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_226_chain,
  NULL, &eltwise_226_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_224_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_218_output, &eltwise_223_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_224_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_224_layer, 224,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_224_chain,
  NULL, &eltwise_224_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_227_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_226_output, &eltwise_225_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_227_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_227_layer, 227,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_227_chain,
  NULL, &eltwise_227_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_228_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_227_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_228_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_228_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_228_layer, 228,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_228_chain,
  NULL, &gemm_228_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_230_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_228_output0, &arith_constant66_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_230_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_230_layer, 230,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_230_chain,
  NULL, &eltwise_230_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_235_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_230_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_235_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_235_layer, 235,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_235_chain,
  NULL, &transpose_235_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_231_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_227_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_231_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_231_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_231_layer, 231,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_231_chain,
  NULL, &gemm_231_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_233_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_231_output0, &arith_constant65_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_233_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_233_layer, 233,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_233_chain,
  NULL, &eltwise_233_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_234_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_233_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_234_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_234_layer, 234,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_234_chain,
  NULL, &transpose_234_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_236_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &transpose_234_output, &transpose_235_output, &gemm_236_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_236_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_236_layer, 236,
  MATMUL_TYPE, 0x0, NULL,
  matmul, forward_matmul,
  &gemm_236_chain,
  NULL, &gemm_236_layer, AI_STATIC, 
  .alpha = 1.0, 
  .beta = 1.0, 
  .tA = 0, 
  .tB = 0, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_240_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_227_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_240_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_240_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_240_layer, 240,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_240_chain,
  NULL, &gemm_240_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_242_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_240_output0, &arith_constant64_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_242_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_242_layer, 242,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_242_chain,
  NULL, &eltwise_242_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_243_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_242_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_243_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_243_layer, 243,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_243_chain,
  NULL, &transpose_243_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_244_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &nl_239_output, &transpose_243_output, &gemm_244_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_244_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_244_layer, 244,
  MATMUL_TYPE, 0x0, NULL,
  matmul, forward_matmul,
  &gemm_244_chain,
  NULL, &gemm_244_layer, AI_STATIC, 
  .alpha = 1.0, 
  .beta = 1.0, 
  .tA = 0, 
  .tB = 0, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_246_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_244_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_246_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_246_layer, 246,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_246_chain,
  NULL, &transpose_246_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_248_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_246_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_248_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_248_weights, &gemm_248_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_248_layer, 248,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_248_chain,
  NULL, &gemm_248_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_249_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_216_output, &gemm_248_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_249_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_249_layer, 249,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_249_chain,
  NULL, &eltwise_249_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float reduce_250_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_250_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_250_neutral_value_data, reduce_250_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_250_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_249_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_250_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_250_layer, 250,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_250_chain,
  NULL, &reduce_250_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_250_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_252_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_249_output, &reduce_250_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_252_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_252_layer, 252,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_252_chain,
  NULL, &eltwise_252_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_253_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_253_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_253_neutral_value_data, reduce_253_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_253_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_252_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_253_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_253_layer, 254,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_253_chain,
  NULL, &reduce_253_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_253_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_256_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_255_output, &functional_1_fut1_ln2_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_256_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_256_layer, 256,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_256_chain,
  NULL, &eltwise_256_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_259_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_249_output, &eltwise_256_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_259_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_259_layer, 259,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_259_chain,
  NULL, &eltwise_259_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_257_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_251_output, &eltwise_256_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_257_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_257_layer, 257,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_257_chain,
  NULL, &eltwise_257_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_260_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_259_output, &eltwise_258_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_260_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_260_layer, 260,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_260_chain,
  NULL, &eltwise_260_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_269_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_260_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_269_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_269_weights, &gemm_269_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_269_layer, 271,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_269_chain,
  NULL, &gemm_269_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_281_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &nl_272_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_281_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_281_weights, &gemm_281_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_281_layer, 283,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_281_chain,
  NULL, &gemm_281_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_284_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_249_output, &gemm_281_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_284_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_284_layer, 284,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_284_chain,
  NULL, &eltwise_284_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float reduce_285_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_285_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_285_neutral_value_data, reduce_285_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_285_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_284_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_285_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_285_layer, 285,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_285_chain,
  NULL, &reduce_285_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_285_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_287_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_284_output, &reduce_285_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_287_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_287_layer, 287,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_287_chain,
  NULL, &eltwise_287_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_288_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_288_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_288_neutral_value_data, reduce_288_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_288_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_287_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_288_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_288_layer, 289,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_288_chain,
  NULL, &reduce_288_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_288_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_291_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_290_output, &functional_1_fut2_ln1_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_291_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_291_layer, 291,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_291_chain,
  NULL, &eltwise_291_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_294_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_284_output, &eltwise_291_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_294_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_294_layer, 294,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_294_chain,
  NULL, &eltwise_294_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_292_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_286_output, &eltwise_291_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_292_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_292_layer, 292,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_292_chain,
  NULL, &eltwise_292_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_295_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_294_output, &eltwise_293_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_295_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_295_layer, 295,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_295_chain,
  NULL, &eltwise_295_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_296_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_295_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_296_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_296_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_296_layer, 296,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_296_chain,
  NULL, &gemm_296_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_298_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_296_output0, &arith_constant60_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_298_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_298_layer, 298,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_298_chain,
  NULL, &eltwise_298_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_303_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_298_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_303_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_303_layer, 303,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_303_chain,
  NULL, &transpose_303_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_299_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_295_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_299_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_299_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_299_layer, 299,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_299_chain,
  NULL, &gemm_299_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_301_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_299_output0, &arith_constant59_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_301_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_301_layer, 301,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_301_chain,
  NULL, &eltwise_301_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_302_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_301_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_302_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_302_layer, 302,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_302_chain,
  NULL, &transpose_302_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_304_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &transpose_302_output, &transpose_303_output, &gemm_304_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_304_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_304_layer, 304,
  MATMUL_TYPE, 0x0, NULL,
  matmul, forward_matmul,
  &gemm_304_chain,
  NULL, &gemm_304_layer, AI_STATIC, 
  .alpha = 1.0, 
  .beta = 1.0, 
  .tA = 0, 
  .tB = 0, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_308_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_295_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_308_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_308_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_308_layer, 308,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_308_chain,
  NULL, &gemm_308_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_310_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_308_output0, &arith_constant58_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_310_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_310_layer, 310,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_310_chain,
  NULL, &eltwise_310_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_311_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_310_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_311_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_311_layer, 311,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_311_chain,
  NULL, &transpose_311_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_312_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &nl_307_output, &transpose_311_output, &gemm_312_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_312_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_312_layer, 312,
  MATMUL_TYPE, 0x0, NULL,
  matmul, forward_matmul,
  &gemm_312_chain,
  NULL, &gemm_312_layer, AI_STATIC, 
  .alpha = 1.0, 
  .beta = 1.0, 
  .tA = 0, 
  .tB = 0, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_314_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_312_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_314_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_314_layer, 314,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_314_chain,
  NULL, &transpose_314_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_316_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_314_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_316_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_316_weights, &gemm_316_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_316_layer, 316,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_316_chain,
  NULL, &gemm_316_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_317_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_284_output, &gemm_316_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_317_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_317_layer, 317,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_317_chain,
  NULL, &eltwise_317_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float reduce_318_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_318_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_318_neutral_value_data, reduce_318_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_318_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_317_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_318_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_318_layer, 318,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_318_chain,
  NULL, &reduce_318_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_318_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_320_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_317_output, &reduce_318_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_320_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_320_layer, 320,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_320_chain,
  NULL, &eltwise_320_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_321_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_321_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_321_neutral_value_data, reduce_321_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_321_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_320_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_321_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_321_layer, 322,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_321_chain,
  NULL, &reduce_321_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_321_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_324_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_323_output, &functional_1_fut2_ln2_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_324_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_324_layer, 324,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_324_chain,
  NULL, &eltwise_324_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_327_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_317_output, &eltwise_324_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_327_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_327_layer, 327,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_327_chain,
  NULL, &eltwise_327_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_325_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_319_output, &eltwise_324_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_325_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_325_layer, 325,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_325_chain,
  NULL, &eltwise_325_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_328_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_327_output, &eltwise_326_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_328_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_328_layer, 328,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_328_chain,
  NULL, &eltwise_328_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_337_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_328_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_337_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_337_weights, &gemm_337_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_337_layer, 339,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_337_chain,
  NULL, &gemm_337_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_349_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &nl_340_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_349_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_349_weights, &gemm_349_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_349_layer, 351,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_349_chain,
  NULL, &gemm_349_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_352_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_317_output, &gemm_349_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_352_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_352_layer, 352,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_352_chain,
  NULL, &eltwise_352_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float reduce_353_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_353_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_353_neutral_value_data, reduce_353_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_353_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_352_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_353_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_353_layer, 353,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_353_chain,
  NULL, &reduce_353_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_353_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_355_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_352_output, &reduce_353_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_355_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_355_layer, 355,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_355_chain,
  NULL, &eltwise_355_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_356_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_356_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_356_neutral_value_data, reduce_356_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_356_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_355_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_356_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_356_layer, 357,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_356_chain,
  NULL, &reduce_356_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_356_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_359_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_358_output, &functional_1_fut3_ln1_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_359_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_359_layer, 359,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_359_chain,
  NULL, &eltwise_359_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_362_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_352_output, &eltwise_359_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_362_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_362_layer, 362,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_362_chain,
  NULL, &eltwise_362_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_360_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_354_output, &eltwise_359_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_360_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_360_layer, 360,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_360_chain,
  NULL, &eltwise_360_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_363_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_362_output, &eltwise_361_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_363_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_363_layer, 363,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_363_chain,
  NULL, &eltwise_363_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_364_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_363_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_364_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_364_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_364_layer, 364,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_364_chain,
  NULL, &gemm_364_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_366_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_364_output0, &arith_constant54_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_366_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_366_layer, 366,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_366_chain,
  NULL, &eltwise_366_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_371_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_366_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_371_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_371_layer, 371,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_371_chain,
  NULL, &transpose_371_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_CHANNEL, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_367_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_363_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_367_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_367_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_367_layer, 367,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_367_chain,
  NULL, &gemm_367_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_369_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_367_output0, &arith_constant53_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_369_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_369_layer, 369,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_369_chain,
  NULL, &eltwise_369_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_370_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_369_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_370_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_370_layer, 370,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_370_chain,
  NULL, &transpose_370_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_372_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &transpose_370_output, &transpose_371_output, &gemm_372_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_372_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_372_layer, 372,
  MATMUL_TYPE, 0x0, NULL,
  matmul, forward_matmul,
  &gemm_372_chain,
  NULL, &gemm_372_layer, AI_STATIC, 
  .alpha = 1.0, 
  .beta = 1.0, 
  .tA = 0, 
  .tB = 0, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_376_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_363_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_376_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_376_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_376_layer, 376,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_376_chain,
  NULL, &gemm_376_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_378_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_376_output0, &arith_constant52_4D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_378_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_378_layer, 378,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_378_chain,
  NULL, &eltwise_378_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_379_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_378_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_379_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_379_layer, 379,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_379_chain,
  NULL, &transpose_379_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_380_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &nl_375_output, &transpose_379_output, &gemm_380_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_380_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_380_layer, 380,
  MATMUL_TYPE, 0x0, NULL,
  matmul, forward_matmul,
  &gemm_380_chain,
  NULL, &gemm_380_layer, AI_STATIC, 
  .alpha = 1.0, 
  .beta = 1.0, 
  .tA = 0, 
  .tB = 0, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  transpose_382_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_380_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_382_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  transpose_382_layer, 382,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &transpose_382_chain,
  NULL, &transpose_382_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_384_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &transpose_382_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_384_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_384_weights, &gemm_384_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_384_layer, 384,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_384_chain,
  NULL, &gemm_384_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_385_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_352_output, &gemm_384_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_385_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_385_layer, 385,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_385_chain,
  NULL, &eltwise_385_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float reduce_386_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_386_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_386_neutral_value_data, reduce_386_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_386_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_385_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_386_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_386_layer, 386,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_386_chain,
  NULL, &reduce_386_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_386_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_388_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_385_output, &reduce_386_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_388_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_388_layer, 388,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_388_chain,
  NULL, &eltwise_388_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_389_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_389_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_389_neutral_value_data, reduce_389_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_389_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_388_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_389_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_389_layer, 390,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_389_chain,
  NULL, &reduce_389_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_389_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_392_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_391_output, &functional_1_fut3_ln2_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_392_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_392_layer, 392,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_392_chain,
  NULL, &eltwise_392_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_395_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_385_output, &eltwise_392_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_395_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_395_layer, 395,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_395_chain,
  NULL, &eltwise_395_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_393_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_387_output, &eltwise_392_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_393_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_393_layer, 393,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_393_chain,
  NULL, &eltwise_393_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_396_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_395_output, &eltwise_394_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_396_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_396_layer, 396,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_396_chain,
  NULL, &eltwise_396_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_405_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_396_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_405_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_405_weights, &gemm_405_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_405_layer, 407,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_405_chain,
  NULL, &gemm_405_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_417_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &nl_408_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_417_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_417_weights, &gemm_417_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_417_layer, 419,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_417_chain,
  NULL, &gemm_417_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_420_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_385_output, &gemm_417_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_420_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_420_layer, 420,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_420_chain,
  NULL, &eltwise_420_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float pad_3_value_data[] = { 0.0 };
AI_ARRAY_OBJ_DECLARE(
    pad_3_value, AI_ARRAY_FORMAT_FLOAT,
    pad_3_value_data, pad_3_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  pad_3_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_1_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &pad_3_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  pad_3_layer, 3,
  PAD_TYPE, 0x0, NULL,
  pad, forward_pad,
  &pad_3_chain,
  NULL, &pad_3_layer, AI_STATIC, 
  .value = &pad_3_value, 
  .mode = AI_PAD_CONSTANT, 
  .pads = AI_SHAPE_INIT(4, 2, 0, 0, 0), 
)


AI_STATIC_CONST ai_float reduce_7_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_7_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_7_neutral_value_data, reduce_7_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_7_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_5_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_7_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_7_layer, 7,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_7_chain,
  NULL, &reduce_7_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_7_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_9_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_5_output0, &reduce_7_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_9_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_9_layer, 9,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_9_chain,
  NULL, &eltwise_9_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_10_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_10_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_10_neutral_value_data, reduce_10_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_10_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_9_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_10_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_10_layer, 11,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_10_chain,
  NULL, &reduce_10_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_10_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_13_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_12_output, &functional_1_past_ln0_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_13_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_13_layer, 13,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_13_chain,
  NULL, &eltwise_13_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_16_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_5_output0, &eltwise_13_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_16_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_16_layer, 16,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_16_chain,
  NULL, &eltwise_16_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_14_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_8_output, &eltwise_13_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_14_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_14_layer, 14,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_14_chain,
  NULL, &eltwise_14_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_17_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_16_output, &eltwise_15_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_17_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_17_layer, 17,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_17_chain,
  NULL, &eltwise_17_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_19_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_1_output0, &nl_18_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_19_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_19_layer, 19,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_19_chain,
  NULL, &eltwise_19_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float pad_20_value_data[] = { 0.0 };
AI_ARRAY_OBJ_DECLARE(
    pad_20_value, AI_ARRAY_FORMAT_FLOAT,
    pad_20_value_data, pad_20_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  pad_20_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_19_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &pad_20_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  pad_20_layer, 20,
  PAD_TYPE, 0x0, NULL,
  pad, forward_pad,
  &pad_20_chain,
  NULL, &pad_20_layer, AI_STATIC, 
  .value = &pad_20_value, 
  .mode = AI_PAD_CONSTANT, 
  .pads = AI_SHAPE_INIT(4, 4, 0, 0, 0), 
)


AI_STATIC_CONST ai_float reduce_27_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_27_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_27_neutral_value_data, reduce_27_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_27_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_23_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_27_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_27_layer, 27,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_27_chain,
  NULL, &reduce_27_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_27_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_29_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_23_output, &reduce_27_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_29_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_29_layer, 29,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_29_chain,
  NULL, &eltwise_29_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_30_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_30_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_30_neutral_value_data, reduce_30_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_30_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_29_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_30_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_30_layer, 31,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_30_chain,
  NULL, &reduce_30_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_30_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_33_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_32_output, &functional_1_past_ln1_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_33_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_33_layer, 33,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_33_chain,
  NULL, &eltwise_33_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_36_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_23_output, &eltwise_33_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_36_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_36_layer, 36,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_36_chain,
  NULL, &eltwise_36_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_34_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_28_output, &eltwise_33_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_34_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_34_layer, 34,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_34_chain,
  NULL, &eltwise_34_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_37_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_36_output, &eltwise_35_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_37_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_37_layer, 37,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_37_chain,
  NULL, &eltwise_37_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_39_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_19_output, &nl_38_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_39_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_39_layer, 39,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_39_chain,
  NULL, &eltwise_39_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float pad_40_value_data[] = { 0.0 };
AI_ARRAY_OBJ_DECLARE(
    pad_40_value, AI_ARRAY_FORMAT_FLOAT,
    pad_40_value_data, pad_40_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  pad_40_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_39_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &pad_40_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  pad_40_layer, 40,
  PAD_TYPE, 0x0, NULL,
  pad, forward_pad,
  &pad_40_chain,
  NULL, &pad_40_layer, AI_STATIC, 
  .value = &pad_40_value, 
  .mode = AI_PAD_CONSTANT, 
  .pads = AI_SHAPE_INIT(4, 8, 0, 0, 0), 
)


AI_STATIC_CONST ai_float reduce_47_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_47_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_47_neutral_value_data, reduce_47_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_47_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_43_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_47_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_47_layer, 47,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_47_chain,
  NULL, &reduce_47_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_47_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_49_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_43_output, &reduce_47_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_49_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_49_layer, 49,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_49_chain,
  NULL, &eltwise_49_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_50_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_50_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_50_neutral_value_data, reduce_50_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_50_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_49_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_50_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_50_layer, 51,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_50_chain,
  NULL, &reduce_50_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_50_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_53_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_52_output, &functional_1_past_ln2_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_53_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_53_layer, 53,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_53_chain,
  NULL, &eltwise_53_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_56_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_43_output, &eltwise_53_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_56_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_56_layer, 56,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_56_chain,
  NULL, &eltwise_56_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_54_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_48_output, &eltwise_53_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_54_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_54_layer, 54,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_54_chain,
  NULL, &eltwise_54_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_57_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_56_output, &eltwise_55_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_57_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_57_layer, 57,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_57_chain,
  NULL, &eltwise_57_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_59_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_39_output, &nl_58_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_59_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_59_layer, 59,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_59_chain,
  NULL, &eltwise_59_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float pad_60_value_data[] = { 0.0 };
AI_ARRAY_OBJ_DECLARE(
    pad_60_value, AI_ARRAY_FORMAT_FLOAT,
    pad_60_value_data, pad_60_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  pad_60_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_59_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &pad_60_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  pad_60_layer, 60,
  PAD_TYPE, 0x0, NULL,
  pad, forward_pad,
  &pad_60_chain,
  NULL, &pad_60_layer, AI_STATIC, 
  .value = &pad_60_value, 
  .mode = AI_PAD_CONSTANT, 
  .pads = AI_SHAPE_INIT(4, 16, 0, 0, 0), 
)


AI_STATIC_CONST ai_float reduce_67_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_67_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_67_neutral_value_data, reduce_67_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_67_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_63_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_67_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_67_layer, 67,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_67_chain,
  NULL, &reduce_67_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_67_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_69_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_63_output, &reduce_67_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_69_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_69_layer, 69,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_69_chain,
  NULL, &eltwise_69_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_70_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_70_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_70_neutral_value_data, reduce_70_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_70_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_69_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_70_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_70_layer, 71,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_70_chain,
  NULL, &reduce_70_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_70_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_73_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_72_output, &functional_1_past_ln3_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_73_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_73_layer, 73,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_73_chain,
  NULL, &eltwise_73_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_76_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_63_output, &eltwise_73_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_76_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_76_layer, 76,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_76_chain,
  NULL, &eltwise_76_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_74_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_68_output, &eltwise_73_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_74_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_74_layer, 74,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_74_chain,
  NULL, &eltwise_74_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_77_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_76_output, &eltwise_75_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_77_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_77_layer, 77,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_77_chain,
  NULL, &eltwise_77_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_79_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_59_output, &nl_78_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_79_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_79_layer, 79,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_79_chain,
  NULL, &eltwise_79_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float pad_80_value_data[] = { 0.0 };
AI_ARRAY_OBJ_DECLARE(
    pad_80_value, AI_ARRAY_FORMAT_FLOAT,
    pad_80_value_data, pad_80_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  pad_80_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_79_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &pad_80_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  pad_80_layer, 80,
  PAD_TYPE, 0x0, NULL,
  pad, forward_pad,
  &pad_80_chain,
  NULL, &pad_80_layer, AI_STATIC, 
  .value = &pad_80_value, 
  .mode = AI_PAD_CONSTANT, 
  .pads = AI_SHAPE_INIT(4, 32, 0, 0, 0), 
)


AI_STATIC_CONST ai_float reduce_87_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_87_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_87_neutral_value_data, reduce_87_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_87_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_83_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_87_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_87_layer, 87,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_87_chain,
  NULL, &reduce_87_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_87_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_89_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_83_output, &reduce_87_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_89_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_89_layer, 89,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_89_chain,
  NULL, &eltwise_89_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_90_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_90_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_90_neutral_value_data, reduce_90_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_90_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_89_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_90_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_90_layer, 91,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_90_chain,
  NULL, &reduce_90_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_90_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_93_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_92_output, &functional_1_past_ln4_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_93_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_93_layer, 93,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_93_chain,
  NULL, &eltwise_93_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_96_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_83_output, &eltwise_93_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_96_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_96_layer, 96,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_96_chain,
  NULL, &eltwise_96_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_94_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_88_output, &eltwise_93_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_94_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_94_layer, 94,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_94_chain,
  NULL, &eltwise_94_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_97_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_96_output, &eltwise_95_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_97_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_97_layer, 97,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_97_chain,
  NULL, &eltwise_97_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_99_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_79_output, &nl_98_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_99_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_99_layer, 99,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_99_chain,
  NULL, &eltwise_99_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)


AI_STATIC_CONST ai_float pad_100_value_data[] = { 0.0 };
AI_ARRAY_OBJ_DECLARE(
    pad_100_value, AI_ARRAY_FORMAT_FLOAT,
    pad_100_value_data, pad_100_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  pad_100_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_99_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &pad_100_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  pad_100_layer, 100,
  PAD_TYPE, 0x0, NULL,
  pad, forward_pad,
  &pad_100_chain,
  NULL, &pad_100_layer, AI_STATIC, 
  .value = &pad_100_value, 
  .mode = AI_PAD_CONSTANT, 
  .pads = AI_SHAPE_INIT(4, 64, 0, 0, 0), 
)


AI_STATIC_CONST ai_float reduce_107_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_107_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_107_neutral_value_data, reduce_107_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_107_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conv2d_103_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_107_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_107_layer, 107,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_107_chain,
  NULL, &reduce_107_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_107_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_109_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_103_output, &reduce_107_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_109_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_109_layer, 109,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_109_chain,
  NULL, &eltwise_109_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_110_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_110_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_110_neutral_value_data, reduce_110_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_110_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_109_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_110_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_110_layer, 111,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_110_chain,
  NULL, &reduce_110_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_110_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_113_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_112_output, &functional_1_past_ln5_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_113_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_113_layer, 113,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_113_chain,
  NULL, &eltwise_113_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_116_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_103_output, &eltwise_113_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_116_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_116_layer, 116,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_116_chain,
  NULL, &eltwise_116_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_114_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_108_output, &eltwise_113_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_114_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_114_layer, 114,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_114_chain,
  NULL, &eltwise_114_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_117_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_116_output, &eltwise_115_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_117_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_117_layer, 117,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_117_chain,
  NULL, &eltwise_117_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_119_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_99_output, &nl_118_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_119_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_119_layer, 119,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_119_chain,
  NULL, &eltwise_119_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_128_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_119_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_128_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_128_weights),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_128_layer, 128,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_128_chain,
  NULL, &gemm_128_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_133_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_119_output, &nl_131_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_133_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_133_layer, 133,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_133_chain,
  NULL, &eltwise_133_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)


AI_STATIC_CONST ai_float reduce_134_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_134_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_134_neutral_value_data, reduce_134_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_134_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_133_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_134_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_134_layer, 134,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_134_chain,
  NULL, &reduce_134_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_134_neutral_value, 
)


AI_STATIC_CONST ai_i16 tile_137_repeats_data[] = { 30, 1, 1, 1, 1 };
AI_ARRAY_OBJ_DECLARE(
    tile_137_repeats, AI_ARRAY_FORMAT_S16,
    tile_137_repeats_data, tile_137_repeats_data, 5, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  tile_137_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_134_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &tile_137_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  tile_137_layer, 137,
  TILE_TYPE, 0x0, NULL,
  tile, forward_tile,
  &tile_137_chain,
  NULL, &tile_137_layer, AI_STATIC, 
  .repeats = &tile_137_repeats, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  concat_421_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_420_output, &tile_137_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &concat_421_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  concat_421_layer, 421,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &concat_421_chain,
  NULL, &concat_421_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_float reduce_422_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_422_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_422_neutral_value_data, reduce_422_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_422_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &concat_421_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_422_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_422_layer, 422,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_422_chain,
  NULL, &reduce_422_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_422_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_424_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &concat_421_output, &reduce_422_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_424_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_424_layer, 424,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_424_chain,
  NULL, &eltwise_424_layer, AI_STATIC, 
  .operation = ai_squared_diff, 
  .buffer_operation = ai_squared_diff_buffer, 
)


AI_STATIC_CONST ai_float reduce_425_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    reduce_425_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    reduce_425_neutral_value_data, reduce_425_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  reduce_425_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_424_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &reduce_425_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  reduce_425_layer, 426,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &reduce_425_chain,
  NULL, &reduce_425_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &reduce_425_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_428_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_427_output, &functional_1_fusion_ln_1_Reshape),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_428_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_428_layer, 428,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_428_chain,
  NULL, &eltwise_428_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_431_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &concat_421_output, &eltwise_428_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_431_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_431_layer, 431,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_431_chain,
  NULL, &eltwise_431_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_429_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &nl_423_output, &eltwise_428_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_429_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_429_layer, 429,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_429_chain,
  NULL, &eltwise_429_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_432_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_431_output, &eltwise_430_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_432_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_432_layer, 432,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &eltwise_432_chain,
  NULL, &eltwise_432_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_441_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_432_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_441_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_441_weights, &gemm_441_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_441_layer, 443,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_441_chain,
  NULL, &gemm_441_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_453_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &nl_444_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_453_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_453_weights, &gemm_453_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_453_layer, 455,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_453_chain,
  NULL, &gemm_453_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_465_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &nl_456_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_465_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_465_weights, &gemm_465_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  gemm_465_layer, 467,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &gemm_465_chain,
  NULL, &gemm_465_layer, AI_STATIC, 
)
/**  Hybrid layers declarations section  *************************************/
void forward_lite_dense_gemm_146(_stai_reatt8_context* net_ctx)
{
  serving_default_kf_input0_output_array.data = AI_PTR(net_ctx->_inputs[1] + 0);
  serving_default_kf_input0_output_array.data_start = AI_PTR(net_ctx->_inputs[1] + 0);
  gemm_146_weights_array.data = AI_PTR(net_ctx->_weights[0] + 26528);
  gemm_146_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 26528);
  gemm_146_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_146_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(146, 1, { serving_default_kf_input0_output.data->data});
  forward_dense(&gemm_146_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(146, 1, { gemm_146_output.data->data});
}
void forward_lite_eltwise_eltwise_148(_stai_reatt8_context* net_ctx)
{
  gemm_146_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_146_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  arith_constant2_array.data = AI_PTR(net_ctx->_weights[0] + 11168);
  arith_constant2_array.data_start = AI_PTR(net_ctx->_weights[0] + 11168);
  eltwise_148_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_148_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(148, 2, { gemm_146_output.data->data,arith_constant2.data->data});
  forward_eltwise(&eltwise_148_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(148, 1, { eltwise_148_output.data->data});
}
void forward_lite_reduce_reduce_149(_stai_reatt8_context* net_ctx)
{
  eltwise_148_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_148_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  reduce_149_output_array.data = AI_PTR(net_ctx->_activations[0] + 45960);
  reduce_149_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 45960);
  _STAI_REATT8_EVENT_NODE_START_CB(149, 1, { eltwise_148_output.data->data});
  forward_reduce(&reduce_149_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(149, 1, { reduce_149_output.data->data});
}
void forward_lite_eltwise_eltwise_151(_stai_reatt8_context* net_ctx)
{
  eltwise_148_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_148_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  reduce_149_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 45960);
  reduce_149_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 45960);
  eltwise_151_output_array.data = AI_PTR(net_ctx->_activations[0] + 30600);
  eltwise_151_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30600);
  _STAI_REATT8_EVENT_NODE_START_CB(151, 2, { eltwise_148_output.data->data,reduce_149_Mul_output.data->data});
  forward_eltwise(&eltwise_151_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(151, 1, { eltwise_151_output.data->data});
}
void forward_lite_reduce_reduce_152(_stai_reatt8_context* net_ctx)
{
  eltwise_151_output_array.data = AI_PTR(net_ctx->_activations[0] + 30600);
  eltwise_151_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30600);
  reduce_152_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  reduce_152_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(153, 1, { eltwise_151_output.data->data});
  forward_reduce(&reduce_152_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(153, 1, { reduce_152_output.data->data});
}
void forward_lite_eltwise_eltwise_155(_stai_reatt8_context* net_ctx)
{
  nl_154_output_array.data = AI_PTR(net_ctx->_activations[0] + 45840);
  nl_154_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 45840);
  functional_1_fut0_ln1_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 10656);
  functional_1_fut0_ln1_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 10656);
  eltwise_155_output_array.data = AI_PTR(net_ctx->_activations[0] + 30480);
  eltwise_155_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30480);
  _STAI_REATT8_EVENT_NODE_START_CB(155, 2, { nl_154_output.data->data,functional_1_fut0_ln1_1_Reshape.data->data});
  forward_eltwise(&eltwise_155_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(155, 1, { eltwise_155_output.data->data});
}
void forward_lite_eltwise_eltwise_158(_stai_reatt8_context* net_ctx)
{
  eltwise_148_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_148_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_155_output_array.data = AI_PTR(net_ctx->_activations[0] + 30480);
  eltwise_155_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30480);
  eltwise_158_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_158_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(158, 2, { eltwise_148_output.data->data,eltwise_155_output.data->data});
  forward_eltwise(&eltwise_158_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(158, 1, { eltwise_158_output.data->data});
}
void forward_lite_eltwise_eltwise_156(_stai_reatt8_context* net_ctx)
{
  nl_150_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  nl_150_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_155_output_array.data = AI_PTR(net_ctx->_activations[0] + 30480);
  eltwise_155_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30480);
  eltwise_156_output_array.data = AI_PTR(net_ctx->_activations[0] + 30480);
  eltwise_156_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30480);
  _STAI_REATT8_EVENT_NODE_START_CB(156, 2, { nl_150_output.data->data,eltwise_155_output.data->data});
  forward_eltwise(&eltwise_156_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(156, 1, { eltwise_156_output.data->data});
}
void forward_lite_eltwise_eltwise_159(_stai_reatt8_context* net_ctx)
{
  eltwise_158_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_158_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_157_output_array.data = AI_PTR(net_ctx->_activations[0] + 30480);
  eltwise_157_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30480);
  eltwise_159_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_159_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(159, 2, { eltwise_158_output.data->data,eltwise_157_output.data->data});
  forward_eltwise(&eltwise_159_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(159, 1, { eltwise_159_output.data->data});
}
void forward_lite_dense_gemm_160(_stai_reatt8_context* net_ctx)
{
  eltwise_159_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_159_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  gemm_160_weights_array.data = AI_PTR(net_ctx->_weights[0] + 30636);
  gemm_160_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 30636);
  gemm_160_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_160_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(160, 1, { eltwise_159_output.data->data});
  forward_dense(&gemm_160_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(160, 1, { gemm_160_output.data->data});
}
void forward_lite_eltwise_eltwise_162(_stai_reatt8_context* net_ctx)
{
  gemm_160_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_160_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  arith_constant72_4D_array.data = AI_PTR(net_ctx->_weights[0] + 32);
  arith_constant72_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 32);
  eltwise_162_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_162_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(162, 2, { gemm_160_output0.data->data,arith_constant72_4D.data->data});
  forward_eltwise(&eltwise_162_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(162, 1, { eltwise_162_output.data->data});
}
void forward_lite_transpose_transpose_167(_stai_reatt8_context* net_ctx)
{
  eltwise_162_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_162_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  transpose_167_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_167_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(167, 1, { eltwise_162_output.data->data});
  forward_transpose(&transpose_167_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(167, 1, { transpose_167_output.data->data});
}
void forward_lite_dense_gemm_163(_stai_reatt8_context* net_ctx)
{
  eltwise_159_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_159_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  gemm_163_weights_array.data = AI_PTR(net_ctx->_weights[0] + 96172);
  gemm_163_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 96172);
  gemm_163_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_163_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(163, 1, { eltwise_159_output.data->data});
  forward_dense(&gemm_163_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(163, 1, { gemm_163_output.data->data});
}
void forward_lite_eltwise_eltwise_165(_stai_reatt8_context* net_ctx)
{
  gemm_163_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_163_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  arith_constant71_4D_array.data = AI_PTR(net_ctx->_weights[0] + 544);
  arith_constant71_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 544);
  eltwise_165_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_165_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(165, 2, { gemm_163_output0.data->data,arith_constant71_4D.data->data});
  forward_eltwise(&eltwise_165_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(165, 1, { eltwise_165_output.data->data});
}
void forward_lite_transpose_transpose_166(_stai_reatt8_context* net_ctx)
{
  eltwise_165_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_165_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  transpose_166_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  transpose_166_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(166, 1, { eltwise_165_output.data->data});
  forward_transpose(&transpose_166_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(166, 1, { transpose_166_output.data->data});
}
void forward_lite_matmul_gemm_168(_stai_reatt8_context* net_ctx)
{
  transpose_166_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  transpose_166_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  transpose_167_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_167_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_168_bias_array.data = AI_PTR(net_ctx->_weights[0] + 28);
  gemm_168_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 28);
  gemm_168_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_168_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(168, 3, { transpose_166_output.data->data,transpose_167_output.data->data,gemm_168_bias.data->data});
  forward_matmul(&gemm_168_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(168, 1, { gemm_168_output.data->data});
}
void forward_lite_dense_gemm_172(_stai_reatt8_context* net_ctx)
{
  eltwise_159_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_159_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  gemm_172_weights_array.data = AI_PTR(net_ctx->_weights[0] + 161948);
  gemm_172_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 161948);
  gemm_172_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  gemm_172_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(172, 1, { eltwise_159_output.data->data});
  forward_dense(&gemm_172_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(172, 1, { gemm_172_output.data->data});
}
void forward_lite_eltwise_eltwise_174(_stai_reatt8_context* net_ctx)
{
  gemm_172_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  gemm_172_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  arith_constant70_4D_array.data = AI_PTR(net_ctx->_weights[0] + 1056);
  arith_constant70_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 1056);
  eltwise_174_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_174_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(174, 2, { gemm_172_output0.data->data,arith_constant70_4D.data->data});
  forward_eltwise(&eltwise_174_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(174, 1, { eltwise_174_output.data->data});
}
void forward_lite_transpose_transpose_175(_stai_reatt8_context* net_ctx)
{
  eltwise_174_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_174_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_175_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  transpose_175_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(175, 1, { eltwise_174_output.data->data});
  forward_transpose(&transpose_175_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(175, 1, { transpose_175_output.data->data});
}
void forward_lite_matmul_gemm_176(_stai_reatt8_context* net_ctx)
{
  nl_171_output_array.data = AI_PTR(net_ctx->_activations[0] + 29760);
  nl_171_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29760);
  transpose_175_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  transpose_175_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  gemm_176_bias_array.data = AI_PTR(net_ctx->_weights[0] + 24);
  gemm_176_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 24);
  gemm_176_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  gemm_176_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(176, 3, { nl_171_output.data->data,transpose_175_output.data->data,gemm_176_bias.data->data});
  forward_matmul(&gemm_176_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(176, 1, { gemm_176_output.data->data});
}
void forward_lite_transpose_transpose_178(_stai_reatt8_context* net_ctx)
{
  gemm_176_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  gemm_176_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_178_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_178_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(178, 1, { gemm_176_output.data->data});
  forward_transpose(&transpose_178_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(178, 1, { transpose_178_output.data->data});
}
void forward_lite_dense_gemm_180(_stai_reatt8_context* net_ctx)
{
  transpose_178_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_178_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_180_weights_array.data = AI_PTR(net_ctx->_weights[0] + 227484);
  gemm_180_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 227484);
  gemm_180_bias_array.data = AI_PTR(net_ctx->_weights[0] + 293020);
  gemm_180_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 293020);
  gemm_180_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_180_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(180, 1, { transpose_178_output0.data->data});
  forward_dense(&gemm_180_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(180, 1, { gemm_180_output.data->data});
}
void forward_lite_eltwise_eltwise_181(_stai_reatt8_context* net_ctx)
{
  eltwise_148_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_148_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_180_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_180_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_181_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_181_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(181, 2, { eltwise_148_output.data->data,gemm_180_output.data->data});
  forward_eltwise(&eltwise_181_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(181, 1, { eltwise_181_output.data->data});
}
void forward_lite_reduce_reduce_182(_stai_reatt8_context* net_ctx)
{
  eltwise_181_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_181_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_182_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  reduce_182_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(182, 1, { eltwise_181_output.data->data});
  forward_reduce(&reduce_182_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(182, 1, { reduce_182_output.data->data});
}
void forward_lite_eltwise_eltwise_184(_stai_reatt8_context* net_ctx)
{
  eltwise_181_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_181_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_182_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 30840);
  reduce_182_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30840);
  eltwise_184_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_184_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  _STAI_REATT8_EVENT_NODE_START_CB(184, 2, { eltwise_181_output.data->data,reduce_182_Mul_output.data->data});
  forward_eltwise(&eltwise_184_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(184, 1, { eltwise_184_output.data->data});
}
void forward_lite_reduce_reduce_185(_stai_reatt8_context* net_ctx)
{
  eltwise_184_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_184_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  reduce_185_output_array.data = AI_PTR(net_ctx->_activations[0] + 46320);
  reduce_185_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46320);
  _STAI_REATT8_EVENT_NODE_START_CB(186, 1, { eltwise_184_output.data->data});
  forward_reduce(&reduce_185_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(186, 1, { reduce_185_output.data->data});
}
void forward_lite_eltwise_eltwise_188(_stai_reatt8_context* net_ctx)
{
  nl_187_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  nl_187_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  functional_1_fut0_ln2_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 10144);
  functional_1_fut0_ln2_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 10144);
  eltwise_188_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_188_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(188, 2, { nl_187_output.data->data,functional_1_fut0_ln2_1_Reshape.data->data});
  forward_eltwise(&eltwise_188_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(188, 1, { eltwise_188_output.data->data});
}
void forward_lite_eltwise_eltwise_191(_stai_reatt8_context* net_ctx)
{
  eltwise_181_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_181_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_188_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_188_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_191_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_191_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(191, 2, { eltwise_181_output.data->data,eltwise_188_output.data->data});
  forward_eltwise(&eltwise_191_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(191, 1, { eltwise_191_output.data->data});
}
void forward_lite_eltwise_eltwise_189(_stai_reatt8_context* net_ctx)
{
  nl_183_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  nl_183_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_188_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_188_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_189_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_189_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(189, 2, { nl_183_output.data->data,eltwise_188_output.data->data});
  forward_eltwise(&eltwise_189_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(189, 1, { eltwise_189_output.data->data});
}
void forward_lite_eltwise_eltwise_192(_stai_reatt8_context* net_ctx)
{
  eltwise_191_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_191_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_190_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_190_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_192_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_192_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(192, 2, { eltwise_191_output.data->data,eltwise_190_output.data->data});
  forward_eltwise(&eltwise_192_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(192, 1, { eltwise_192_output.data->data});
}
void forward_lite_dense_gemm_201(_stai_reatt8_context* net_ctx)
{
  eltwise_192_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_192_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_201_weights_array.data = AI_PTR(net_ctx->_weights[0] + 294044);
  gemm_201_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 294044);
  gemm_201_bias_array.data = AI_PTR(net_ctx->_weights[0] + 556188);
  gemm_201_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 556188);
  gemm_201_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_201_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(203, 1, { eltwise_192_output.data->data});
  forward_dense(&gemm_201_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(203, 1, { gemm_201_output.data->data});
}
void forward_lite_dense_gemm_213(_stai_reatt8_context* net_ctx)
{
  nl_204_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  nl_204_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_213_weights_array.data = AI_PTR(net_ctx->_weights[0] + 558236);
  gemm_213_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 558236);
  gemm_213_bias_array.data = AI_PTR(net_ctx->_weights[0] + 820380);
  gemm_213_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 820380);
  gemm_213_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_213_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(215, 1, { nl_204_output.data->data});
  forward_dense(&gemm_213_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(215, 1, { gemm_213_output.data->data});
}
void forward_lite_eltwise_eltwise_216(_stai_reatt8_context* net_ctx)
{
  eltwise_181_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_181_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_213_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_213_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_216_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_216_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(216, 2, { eltwise_181_output.data->data,gemm_213_output.data->data});
  forward_eltwise(&eltwise_216_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(216, 1, { eltwise_216_output.data->data});
}
void forward_lite_reduce_reduce_217(_stai_reatt8_context* net_ctx)
{
  eltwise_216_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_216_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  reduce_217_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_217_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(217, 1, { eltwise_216_output.data->data});
  forward_reduce(&reduce_217_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(217, 1, { reduce_217_output.data->data});
}
void forward_lite_eltwise_eltwise_219(_stai_reatt8_context* net_ctx)
{
  eltwise_216_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_216_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  reduce_217_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 15480);
  reduce_217_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15480);
  eltwise_219_output_array.data = AI_PTR(net_ctx->_activations[0] + 15600);
  eltwise_219_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15600);
  _STAI_REATT8_EVENT_NODE_START_CB(219, 2, { eltwise_216_output.data->data,reduce_217_Mul_output.data->data});
  forward_eltwise(&eltwise_219_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(219, 1, { eltwise_219_output.data->data});
}
void forward_lite_reduce_reduce_220(_stai_reatt8_context* net_ctx)
{
  eltwise_219_output_array.data = AI_PTR(net_ctx->_activations[0] + 15600);
  eltwise_219_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15600);
  reduce_220_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_220_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(221, 1, { eltwise_219_output.data->data});
  forward_reduce(&reduce_220_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(221, 1, { reduce_220_output.data->data});
}
void forward_lite_eltwise_eltwise_223(_stai_reatt8_context* net_ctx)
{
  nl_222_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_222_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  functional_1_fut1_ln1_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 9632);
  functional_1_fut1_ln1_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 9632);
  eltwise_223_output_array.data = AI_PTR(net_ctx->_activations[0] + 15600);
  eltwise_223_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15600);
  _STAI_REATT8_EVENT_NODE_START_CB(223, 2, { nl_222_output.data->data,functional_1_fut1_ln1_1_Reshape.data->data});
  forward_eltwise(&eltwise_223_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(223, 1, { eltwise_223_output.data->data});
}
void forward_lite_eltwise_eltwise_226(_stai_reatt8_context* net_ctx)
{
  eltwise_216_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_216_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_223_output_array.data = AI_PTR(net_ctx->_activations[0] + 15600);
  eltwise_223_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15600);
  eltwise_226_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_226_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(226, 2, { eltwise_216_output.data->data,eltwise_223_output.data->data});
  forward_eltwise(&eltwise_226_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(226, 1, { eltwise_226_output.data->data});
}
void forward_lite_eltwise_eltwise_224(_stai_reatt8_context* net_ctx)
{
  nl_218_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_218_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_223_output_array.data = AI_PTR(net_ctx->_activations[0] + 15600);
  eltwise_223_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15600);
  eltwise_224_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_224_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(224, 2, { nl_218_output.data->data,eltwise_223_output.data->data});
  forward_eltwise(&eltwise_224_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(224, 1, { eltwise_224_output.data->data});
}
void forward_lite_eltwise_eltwise_227(_stai_reatt8_context* net_ctx)
{
  eltwise_226_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_226_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_225_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_225_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_227_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_227_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(227, 2, { eltwise_226_output.data->data,eltwise_225_output.data->data});
  forward_eltwise(&eltwise_227_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(227, 1, { eltwise_227_output.data->data});
}
void forward_lite_dense_gemm_228(_stai_reatt8_context* net_ctx)
{
  eltwise_227_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_227_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_228_weights_array.data = AI_PTR(net_ctx->_weights[0] + 821404);
  gemm_228_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 821404);
  gemm_228_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_228_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(228, 1, { eltwise_227_output.data->data});
  forward_dense(&gemm_228_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(228, 1, { gemm_228_output.data->data});
}
void forward_lite_eltwise_eltwise_230(_stai_reatt8_context* net_ctx)
{
  gemm_228_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_228_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  arith_constant66_4D_array.data = AI_PTR(net_ctx->_weights[0] + 1568);
  arith_constant66_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 1568);
  eltwise_230_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_230_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(230, 2, { gemm_228_output0.data->data,arith_constant66_4D.data->data});
  forward_eltwise(&eltwise_230_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(230, 1, { eltwise_230_output.data->data});
}
void forward_lite_transpose_transpose_235(_stai_reatt8_context* net_ctx)
{
  eltwise_230_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_230_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_235_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_235_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(235, 1, { eltwise_230_output.data->data});
  forward_transpose(&transpose_235_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(235, 1, { transpose_235_output.data->data});
}
void forward_lite_dense_gemm_231(_stai_reatt8_context* net_ctx)
{
  eltwise_227_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_227_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_231_weights_array.data = AI_PTR(net_ctx->_weights[0] + 886940);
  gemm_231_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 886940);
  gemm_231_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  gemm_231_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(231, 1, { eltwise_227_output.data->data});
  forward_dense(&gemm_231_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(231, 1, { gemm_231_output.data->data});
}
void forward_lite_eltwise_eltwise_233(_stai_reatt8_context* net_ctx)
{
  gemm_231_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  gemm_231_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  arith_constant65_4D_array.data = AI_PTR(net_ctx->_weights[0] + 2080);
  arith_constant65_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 2080);
  eltwise_233_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_233_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(233, 2, { gemm_231_output0.data->data,arith_constant65_4D.data->data});
  forward_eltwise(&eltwise_233_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(233, 1, { eltwise_233_output.data->data});
}
void forward_lite_transpose_transpose_234(_stai_reatt8_context* net_ctx)
{
  eltwise_233_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_233_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  transpose_234_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_234_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(234, 1, { eltwise_233_output.data->data});
  forward_transpose(&transpose_234_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(234, 1, { transpose_234_output.data->data});
}
void forward_lite_matmul_gemm_236(_stai_reatt8_context* net_ctx)
{
  transpose_234_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_234_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_235_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_235_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_236_bias_array.data = AI_PTR(net_ctx->_weights[0] + 20);
  gemm_236_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 20);
  gemm_236_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  gemm_236_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(236, 3, { transpose_234_output.data->data,transpose_235_output.data->data,gemm_236_bias.data->data});
  forward_matmul(&gemm_236_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(236, 1, { gemm_236_output.data->data});
}
void forward_lite_dense_gemm_240(_stai_reatt8_context* net_ctx)
{
  eltwise_227_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_227_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_240_weights_array.data = AI_PTR(net_ctx->_weights[0] + 952476);
  gemm_240_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 952476);
  gemm_240_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_240_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(240, 1, { eltwise_227_output.data->data});
  forward_dense(&gemm_240_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(240, 1, { gemm_240_output.data->data});
}
void forward_lite_eltwise_eltwise_242(_stai_reatt8_context* net_ctx)
{
  gemm_240_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_240_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  arith_constant64_4D_array.data = AI_PTR(net_ctx->_weights[0] + 2592);
  arith_constant64_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 2592);
  eltwise_242_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_242_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(242, 2, { gemm_240_output0.data->data,arith_constant64_4D.data->data});
  forward_eltwise(&eltwise_242_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(242, 1, { eltwise_242_output.data->data});
}
void forward_lite_transpose_transpose_243(_stai_reatt8_context* net_ctx)
{
  eltwise_242_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_242_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  transpose_243_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_243_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(243, 1, { eltwise_242_output.data->data});
  forward_transpose(&transpose_243_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(243, 1, { transpose_243_output.data->data});
}
void forward_lite_matmul_gemm_244(_stai_reatt8_context* net_ctx)
{
  nl_239_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  nl_239_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_243_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_243_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_244_bias_array.data = AI_PTR(net_ctx->_weights[0] + 16);
  gemm_244_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 16);
  gemm_244_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_244_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(244, 3, { nl_239_output.data->data,transpose_243_output.data->data,gemm_244_bias.data->data});
  forward_matmul(&gemm_244_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(244, 1, { gemm_244_output.data->data});
}
void forward_lite_transpose_transpose_246(_stai_reatt8_context* net_ctx)
{
  gemm_244_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_244_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  transpose_246_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_246_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(246, 1, { gemm_244_output.data->data});
  forward_transpose(&transpose_246_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(246, 1, { transpose_246_output.data->data});
}
void forward_lite_dense_gemm_248(_stai_reatt8_context* net_ctx)
{
  transpose_246_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_246_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_248_weights_array.data = AI_PTR(net_ctx->_weights[0] + 1018012);
  gemm_248_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 1018012);
  gemm_248_bias_array.data = AI_PTR(net_ctx->_weights[0] + 1083548);
  gemm_248_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 1083548);
  gemm_248_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_248_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(248, 1, { transpose_246_output0.data->data});
  forward_dense(&gemm_248_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(248, 1, { gemm_248_output.data->data});
}
void forward_lite_eltwise_eltwise_249(_stai_reatt8_context* net_ctx)
{
  eltwise_216_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_216_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_248_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_248_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_249_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_249_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(249, 2, { eltwise_216_output.data->data,gemm_248_output.data->data});
  forward_eltwise(&eltwise_249_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(249, 1, { eltwise_249_output.data->data});
}
void forward_lite_reduce_reduce_250(_stai_reatt8_context* net_ctx)
{
  eltwise_249_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_249_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_250_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  reduce_250_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(250, 1, { eltwise_249_output.data->data});
  forward_reduce(&reduce_250_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(250, 1, { reduce_250_output.data->data});
}
void forward_lite_eltwise_eltwise_252(_stai_reatt8_context* net_ctx)
{
  eltwise_249_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_249_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_250_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 30840);
  reduce_250_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30840);
  eltwise_252_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_252_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  _STAI_REATT8_EVENT_NODE_START_CB(252, 2, { eltwise_249_output.data->data,reduce_250_Mul_output.data->data});
  forward_eltwise(&eltwise_252_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(252, 1, { eltwise_252_output.data->data});
}
void forward_lite_reduce_reduce_253(_stai_reatt8_context* net_ctx)
{
  eltwise_252_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_252_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  reduce_253_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  reduce_253_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(254, 1, { eltwise_252_output.data->data});
  forward_reduce(&reduce_253_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(254, 1, { reduce_253_output.data->data});
}
void forward_lite_eltwise_eltwise_256(_stai_reatt8_context* net_ctx)
{
  nl_255_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  nl_255_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  functional_1_fut1_ln2_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 9120);
  functional_1_fut1_ln2_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 9120);
  eltwise_256_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_256_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  _STAI_REATT8_EVENT_NODE_START_CB(256, 2, { nl_255_output.data->data,functional_1_fut1_ln2_1_Reshape.data->data});
  forward_eltwise(&eltwise_256_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(256, 1, { eltwise_256_output.data->data});
}
void forward_lite_eltwise_eltwise_259(_stai_reatt8_context* net_ctx)
{
  eltwise_249_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_249_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_256_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_256_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_259_output_array.data = AI_PTR(net_ctx->_activations[0] + 46320);
  eltwise_259_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46320);
  _STAI_REATT8_EVENT_NODE_START_CB(259, 2, { eltwise_249_output.data->data,eltwise_256_output.data->data});
  forward_eltwise(&eltwise_259_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(259, 1, { eltwise_259_output.data->data});
}
void forward_lite_eltwise_eltwise_257(_stai_reatt8_context* net_ctx)
{
  nl_251_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  nl_251_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_256_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_256_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_257_output_array.data = AI_PTR(net_ctx->_activations[0] + 61680);
  eltwise_257_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61680);
  _STAI_REATT8_EVENT_NODE_START_CB(257, 2, { nl_251_output.data->data,eltwise_256_output.data->data});
  forward_eltwise(&eltwise_257_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(257, 1, { eltwise_257_output.data->data});
}
void forward_lite_eltwise_eltwise_260(_stai_reatt8_context* net_ctx)
{
  eltwise_259_output_array.data = AI_PTR(net_ctx->_activations[0] + 46320);
  eltwise_259_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46320);
  eltwise_258_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_258_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_260_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_260_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_REATT8_EVENT_NODE_START_CB(260, 2, { eltwise_259_output.data->data,eltwise_258_output.data->data});
  forward_eltwise(&eltwise_260_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(260, 1, { eltwise_260_output.data->data});
}
void forward_lite_dense_gemm_269(_stai_reatt8_context* net_ctx)
{
  eltwise_260_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_260_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  gemm_269_weights_array.data = AI_PTR(net_ctx->_weights[0] + 1084572);
  gemm_269_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 1084572);
  gemm_269_bias_array.data = AI_PTR(net_ctx->_weights[0] + 1346716);
  gemm_269_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 1346716);
  gemm_269_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_269_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(271, 1, { eltwise_260_output.data->data});
  forward_dense(&gemm_269_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(271, 1, { gemm_269_output.data->data});
}
void forward_lite_dense_gemm_281(_stai_reatt8_context* net_ctx)
{
  nl_272_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  nl_272_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_281_weights_array.data = AI_PTR(net_ctx->_weights[0] + 1348764);
  gemm_281_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 1348764);
  gemm_281_bias_array.data = AI_PTR(net_ctx->_weights[0] + 1610908);
  gemm_281_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 1610908);
  gemm_281_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  gemm_281_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_REATT8_EVENT_NODE_START_CB(283, 1, { nl_272_output.data->data});
  forward_dense(&gemm_281_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(283, 1, { gemm_281_output.data->data});
}
void forward_lite_eltwise_eltwise_284(_stai_reatt8_context* net_ctx)
{
  eltwise_249_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_249_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_281_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  gemm_281_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_284_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_284_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(284, 2, { eltwise_249_output.data->data,gemm_281_output.data->data});
  forward_eltwise(&eltwise_284_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(284, 1, { eltwise_284_output.data->data});
}
void forward_lite_reduce_reduce_285(_stai_reatt8_context* net_ctx)
{
  eltwise_284_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_284_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  reduce_285_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_285_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(285, 1, { eltwise_284_output.data->data});
  forward_reduce(&reduce_285_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(285, 1, { reduce_285_output.data->data});
}
void forward_lite_eltwise_eltwise_287(_stai_reatt8_context* net_ctx)
{
  eltwise_284_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_284_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  reduce_285_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 15480);
  reduce_285_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15480);
  eltwise_287_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_287_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(287, 2, { eltwise_284_output.data->data,reduce_285_Mul_output.data->data});
  forward_eltwise(&eltwise_287_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(287, 1, { eltwise_287_output.data->data});
}
void forward_lite_reduce_reduce_288(_stai_reatt8_context* net_ctx)
{
  eltwise_287_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_287_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  reduce_288_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_288_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(289, 1, { eltwise_287_output.data->data});
  forward_reduce(&reduce_288_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(289, 1, { reduce_288_output.data->data});
}
void forward_lite_eltwise_eltwise_291(_stai_reatt8_context* net_ctx)
{
  nl_290_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_290_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  functional_1_fut2_ln1_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 8608);
  functional_1_fut2_ln1_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 8608);
  eltwise_291_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_291_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(291, 2, { nl_290_output.data->data,functional_1_fut2_ln1_1_Reshape.data->data});
  forward_eltwise(&eltwise_291_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(291, 1, { eltwise_291_output.data->data});
}
void forward_lite_eltwise_eltwise_294(_stai_reatt8_context* net_ctx)
{
  eltwise_284_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_284_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_291_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_291_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_294_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_294_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(294, 2, { eltwise_284_output.data->data,eltwise_291_output.data->data});
  forward_eltwise(&eltwise_294_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(294, 1, { eltwise_294_output.data->data});
}
void forward_lite_eltwise_eltwise_292(_stai_reatt8_context* net_ctx)
{
  nl_286_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_286_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_291_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_291_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_292_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_292_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(292, 2, { nl_286_output.data->data,eltwise_291_output.data->data});
  forward_eltwise(&eltwise_292_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(292, 1, { eltwise_292_output.data->data});
}
void forward_lite_eltwise_eltwise_295(_stai_reatt8_context* net_ctx)
{
  eltwise_294_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_294_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_293_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_293_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_295_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_295_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(295, 2, { eltwise_294_output.data->data,eltwise_293_output.data->data});
  forward_eltwise(&eltwise_295_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(295, 1, { eltwise_295_output.data->data});
}
void forward_lite_dense_gemm_296(_stai_reatt8_context* net_ctx)
{
  eltwise_295_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_295_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_296_weights_array.data = AI_PTR(net_ctx->_weights[0] + 1611932);
  gemm_296_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 1611932);
  gemm_296_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_296_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(296, 1, { eltwise_295_output.data->data});
  forward_dense(&gemm_296_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(296, 1, { gemm_296_output.data->data});
}
void forward_lite_eltwise_eltwise_298(_stai_reatt8_context* net_ctx)
{
  gemm_296_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_296_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  arith_constant60_4D_array.data = AI_PTR(net_ctx->_weights[0] + 3104);
  arith_constant60_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 3104);
  eltwise_298_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_298_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(298, 2, { gemm_296_output0.data->data,arith_constant60_4D.data->data});
  forward_eltwise(&eltwise_298_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(298, 1, { eltwise_298_output.data->data});
}
void forward_lite_transpose_transpose_303(_stai_reatt8_context* net_ctx)
{
  eltwise_298_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_298_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_303_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_303_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(303, 1, { eltwise_298_output.data->data});
  forward_transpose(&transpose_303_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(303, 1, { transpose_303_output.data->data});
}
void forward_lite_dense_gemm_299(_stai_reatt8_context* net_ctx)
{
  eltwise_295_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_295_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_299_weights_array.data = AI_PTR(net_ctx->_weights[0] + 1677468);
  gemm_299_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 1677468);
  gemm_299_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  gemm_299_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(299, 1, { eltwise_295_output.data->data});
  forward_dense(&gemm_299_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(299, 1, { gemm_299_output.data->data});
}
void forward_lite_eltwise_eltwise_301(_stai_reatt8_context* net_ctx)
{
  gemm_299_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  gemm_299_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  arith_constant59_4D_array.data = AI_PTR(net_ctx->_weights[0] + 3616);
  arith_constant59_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 3616);
  eltwise_301_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_301_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(301, 2, { gemm_299_output0.data->data,arith_constant59_4D.data->data});
  forward_eltwise(&eltwise_301_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(301, 1, { eltwise_301_output.data->data});
}
void forward_lite_transpose_transpose_302(_stai_reatt8_context* net_ctx)
{
  eltwise_301_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_301_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  transpose_302_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_302_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(302, 1, { eltwise_301_output.data->data});
  forward_transpose(&transpose_302_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(302, 1, { transpose_302_output.data->data});
}
void forward_lite_matmul_gemm_304(_stai_reatt8_context* net_ctx)
{
  transpose_302_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_302_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_303_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_303_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_304_bias_array.data = AI_PTR(net_ctx->_weights[0] + 12);
  gemm_304_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 12);
  gemm_304_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  gemm_304_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(304, 3, { transpose_302_output.data->data,transpose_303_output.data->data,gemm_304_bias.data->data});
  forward_matmul(&gemm_304_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(304, 1, { gemm_304_output.data->data});
}
void forward_lite_dense_gemm_308(_stai_reatt8_context* net_ctx)
{
  eltwise_295_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_295_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_308_weights_array.data = AI_PTR(net_ctx->_weights[0] + 1743004);
  gemm_308_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 1743004);
  gemm_308_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_308_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(308, 1, { eltwise_295_output.data->data});
  forward_dense(&gemm_308_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(308, 1, { gemm_308_output.data->data});
}
void forward_lite_eltwise_eltwise_310(_stai_reatt8_context* net_ctx)
{
  gemm_308_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_308_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  arith_constant58_4D_array.data = AI_PTR(net_ctx->_weights[0] + 4128);
  arith_constant58_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 4128);
  eltwise_310_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_310_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(310, 2, { gemm_308_output0.data->data,arith_constant58_4D.data->data});
  forward_eltwise(&eltwise_310_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(310, 1, { eltwise_310_output.data->data});
}
void forward_lite_transpose_transpose_311(_stai_reatt8_context* net_ctx)
{
  eltwise_310_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_310_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  transpose_311_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_311_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(311, 1, { eltwise_310_output.data->data});
  forward_transpose(&transpose_311_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(311, 1, { transpose_311_output.data->data});
}
void forward_lite_matmul_gemm_312(_stai_reatt8_context* net_ctx)
{
  nl_307_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  nl_307_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_311_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_311_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_312_bias_array.data = AI_PTR(net_ctx->_weights[0] + 8);
  gemm_312_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 8);
  gemm_312_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_312_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(312, 3, { nl_307_output.data->data,transpose_311_output.data->data,gemm_312_bias.data->data});
  forward_matmul(&gemm_312_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(312, 1, { gemm_312_output.data->data});
}
void forward_lite_transpose_transpose_314(_stai_reatt8_context* net_ctx)
{
  gemm_312_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_312_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  transpose_314_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_314_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(314, 1, { gemm_312_output.data->data});
  forward_transpose(&transpose_314_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(314, 1, { transpose_314_output.data->data});
}
void forward_lite_dense_gemm_316(_stai_reatt8_context* net_ctx)
{
  transpose_314_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_314_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_316_weights_array.data = AI_PTR(net_ctx->_weights[0] + 1808540);
  gemm_316_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 1808540);
  gemm_316_bias_array.data = AI_PTR(net_ctx->_weights[0] + 1874076);
  gemm_316_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 1874076);
  gemm_316_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_316_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(316, 1, { transpose_314_output0.data->data});
  forward_dense(&gemm_316_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(316, 1, { gemm_316_output.data->data});
}
void forward_lite_eltwise_eltwise_317(_stai_reatt8_context* net_ctx)
{
  eltwise_284_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_284_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_316_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_316_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_317_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_317_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(317, 2, { eltwise_284_output.data->data,gemm_316_output.data->data});
  forward_eltwise(&eltwise_317_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(317, 1, { eltwise_317_output.data->data});
}
void forward_lite_reduce_reduce_318(_stai_reatt8_context* net_ctx)
{
  eltwise_317_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_317_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_318_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  reduce_318_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(318, 1, { eltwise_317_output.data->data});
  forward_reduce(&reduce_318_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(318, 1, { reduce_318_output.data->data});
}
void forward_lite_eltwise_eltwise_320(_stai_reatt8_context* net_ctx)
{
  eltwise_317_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_317_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_318_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 30840);
  reduce_318_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30840);
  eltwise_320_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_320_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  _STAI_REATT8_EVENT_NODE_START_CB(320, 2, { eltwise_317_output.data->data,reduce_318_Mul_output.data->data});
  forward_eltwise(&eltwise_320_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(320, 1, { eltwise_320_output.data->data});
}
void forward_lite_reduce_reduce_321(_stai_reatt8_context* net_ctx)
{
  eltwise_320_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_320_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  reduce_321_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  reduce_321_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(322, 1, { eltwise_320_output.data->data});
  forward_reduce(&reduce_321_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(322, 1, { reduce_321_output.data->data});
}
void forward_lite_eltwise_eltwise_324(_stai_reatt8_context* net_ctx)
{
  nl_323_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  nl_323_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  functional_1_fut2_ln2_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 8096);
  functional_1_fut2_ln2_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 8096);
  eltwise_324_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_324_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  _STAI_REATT8_EVENT_NODE_START_CB(324, 2, { nl_323_output.data->data,functional_1_fut2_ln2_1_Reshape.data->data});
  forward_eltwise(&eltwise_324_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(324, 1, { eltwise_324_output.data->data});
}
void forward_lite_eltwise_eltwise_327(_stai_reatt8_context* net_ctx)
{
  eltwise_317_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_317_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_324_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_324_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_327_output_array.data = AI_PTR(net_ctx->_activations[0] + 46320);
  eltwise_327_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46320);
  _STAI_REATT8_EVENT_NODE_START_CB(327, 2, { eltwise_317_output.data->data,eltwise_324_output.data->data});
  forward_eltwise(&eltwise_327_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(327, 1, { eltwise_327_output.data->data});
}
void forward_lite_eltwise_eltwise_325(_stai_reatt8_context* net_ctx)
{
  nl_319_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  nl_319_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_324_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_324_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_325_output_array.data = AI_PTR(net_ctx->_activations[0] + 61680);
  eltwise_325_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61680);
  _STAI_REATT8_EVENT_NODE_START_CB(325, 2, { nl_319_output.data->data,eltwise_324_output.data->data});
  forward_eltwise(&eltwise_325_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(325, 1, { eltwise_325_output.data->data});
}
void forward_lite_eltwise_eltwise_328(_stai_reatt8_context* net_ctx)
{
  eltwise_327_output_array.data = AI_PTR(net_ctx->_activations[0] + 46320);
  eltwise_327_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46320);
  eltwise_326_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_326_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_328_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_328_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_REATT8_EVENT_NODE_START_CB(328, 2, { eltwise_327_output.data->data,eltwise_326_output.data->data});
  forward_eltwise(&eltwise_328_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(328, 1, { eltwise_328_output.data->data});
}
void forward_lite_dense_gemm_337(_stai_reatt8_context* net_ctx)
{
  eltwise_328_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_328_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  gemm_337_weights_array.data = AI_PTR(net_ctx->_weights[0] + 1875100);
  gemm_337_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 1875100);
  gemm_337_bias_array.data = AI_PTR(net_ctx->_weights[0] + 2137244);
  gemm_337_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 2137244);
  gemm_337_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_337_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(339, 1, { eltwise_328_output.data->data});
  forward_dense(&gemm_337_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(339, 1, { gemm_337_output.data->data});
}
void forward_lite_dense_gemm_349(_stai_reatt8_context* net_ctx)
{
  nl_340_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  nl_340_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_349_weights_array.data = AI_PTR(net_ctx->_weights[0] + 2139292);
  gemm_349_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 2139292);
  gemm_349_bias_array.data = AI_PTR(net_ctx->_weights[0] + 2401436);
  gemm_349_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 2401436);
  gemm_349_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  gemm_349_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_REATT8_EVENT_NODE_START_CB(351, 1, { nl_340_output.data->data});
  forward_dense(&gemm_349_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(351, 1, { gemm_349_output.data->data});
}
void forward_lite_eltwise_eltwise_352(_stai_reatt8_context* net_ctx)
{
  eltwise_317_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_317_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_349_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  gemm_349_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_352_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_352_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(352, 2, { eltwise_317_output.data->data,gemm_349_output.data->data});
  forward_eltwise(&eltwise_352_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(352, 1, { eltwise_352_output.data->data});
}
void forward_lite_reduce_reduce_353(_stai_reatt8_context* net_ctx)
{
  eltwise_352_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_352_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  reduce_353_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_353_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(353, 1, { eltwise_352_output.data->data});
  forward_reduce(&reduce_353_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(353, 1, { reduce_353_output.data->data});
}
void forward_lite_eltwise_eltwise_355(_stai_reatt8_context* net_ctx)
{
  eltwise_352_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_352_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  reduce_353_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 15480);
  reduce_353_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15480);
  eltwise_355_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_355_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(355, 2, { eltwise_352_output.data->data,reduce_353_Mul_output.data->data});
  forward_eltwise(&eltwise_355_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(355, 1, { eltwise_355_output.data->data});
}
void forward_lite_reduce_reduce_356(_stai_reatt8_context* net_ctx)
{
  eltwise_355_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_355_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  reduce_356_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_356_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(357, 1, { eltwise_355_output.data->data});
  forward_reduce(&reduce_356_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(357, 1, { reduce_356_output.data->data});
}
void forward_lite_eltwise_eltwise_359(_stai_reatt8_context* net_ctx)
{
  nl_358_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_358_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  functional_1_fut3_ln1_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 7584);
  functional_1_fut3_ln1_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 7584);
  eltwise_359_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_359_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(359, 2, { nl_358_output.data->data,functional_1_fut3_ln1_1_Reshape.data->data});
  forward_eltwise(&eltwise_359_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(359, 1, { eltwise_359_output.data->data});
}
void forward_lite_eltwise_eltwise_362(_stai_reatt8_context* net_ctx)
{
  eltwise_352_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_352_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_359_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_359_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_362_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_362_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(362, 2, { eltwise_352_output.data->data,eltwise_359_output.data->data});
  forward_eltwise(&eltwise_362_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(362, 1, { eltwise_362_output.data->data});
}
void forward_lite_eltwise_eltwise_360(_stai_reatt8_context* net_ctx)
{
  nl_354_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_354_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_359_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_359_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_360_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_360_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(360, 2, { nl_354_output.data->data,eltwise_359_output.data->data});
  forward_eltwise(&eltwise_360_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(360, 1, { eltwise_360_output.data->data});
}
void forward_lite_eltwise_eltwise_363(_stai_reatt8_context* net_ctx)
{
  eltwise_362_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_362_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_361_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_361_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_363_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_363_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(363, 2, { eltwise_362_output.data->data,eltwise_361_output.data->data});
  forward_eltwise(&eltwise_363_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(363, 1, { eltwise_363_output.data->data});
}
void forward_lite_dense_gemm_364(_stai_reatt8_context* net_ctx)
{
  eltwise_363_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_363_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_364_weights_array.data = AI_PTR(net_ctx->_weights[0] + 2402460);
  gemm_364_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 2402460);
  gemm_364_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_364_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(364, 1, { eltwise_363_output.data->data});
  forward_dense(&gemm_364_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(364, 1, { gemm_364_output.data->data});
}
void forward_lite_eltwise_eltwise_366(_stai_reatt8_context* net_ctx)
{
  gemm_364_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_364_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  arith_constant54_4D_array.data = AI_PTR(net_ctx->_weights[0] + 4640);
  arith_constant54_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 4640);
  eltwise_366_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_366_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(366, 2, { gemm_364_output0.data->data,arith_constant54_4D.data->data});
  forward_eltwise(&eltwise_366_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(366, 1, { eltwise_366_output.data->data});
}
void forward_lite_transpose_transpose_371(_stai_reatt8_context* net_ctx)
{
  eltwise_366_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  eltwise_366_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_371_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_371_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(371, 1, { eltwise_366_output.data->data});
  forward_transpose(&transpose_371_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(371, 1, { transpose_371_output.data->data});
}
void forward_lite_dense_gemm_367(_stai_reatt8_context* net_ctx)
{
  eltwise_363_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_363_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_367_weights_array.data = AI_PTR(net_ctx->_weights[0] + 2467996);
  gemm_367_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 2467996);
  gemm_367_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  gemm_367_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(367, 1, { eltwise_363_output.data->data});
  forward_dense(&gemm_367_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(367, 1, { gemm_367_output.data->data});
}
void forward_lite_eltwise_eltwise_369(_stai_reatt8_context* net_ctx)
{
  gemm_367_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  gemm_367_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  arith_constant53_4D_array.data = AI_PTR(net_ctx->_weights[0] + 5152);
  arith_constant53_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 5152);
  eltwise_369_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_369_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(369, 2, { gemm_367_output0.data->data,arith_constant53_4D.data->data});
  forward_eltwise(&eltwise_369_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(369, 1, { eltwise_369_output.data->data});
}
void forward_lite_transpose_transpose_370(_stai_reatt8_context* net_ctx)
{
  eltwise_369_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_369_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  transpose_370_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_370_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_REATT8_EVENT_NODE_START_CB(370, 1, { eltwise_369_output.data->data});
  forward_transpose(&transpose_370_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(370, 1, { transpose_370_output.data->data});
}
void forward_lite_matmul_gemm_372(_stai_reatt8_context* net_ctx)
{
  transpose_370_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_370_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_371_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_371_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_372_bias_array.data = AI_PTR(net_ctx->_weights[0] + 4);
  gemm_372_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 4);
  gemm_372_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  gemm_372_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_REATT8_EVENT_NODE_START_CB(372, 3, { transpose_370_output.data->data,transpose_371_output.data->data,gemm_372_bias.data->data});
  forward_matmul(&gemm_372_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(372, 1, { gemm_372_output.data->data});
}
void forward_lite_dense_gemm_376(_stai_reatt8_context* net_ctx)
{
  eltwise_363_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_363_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_376_weights_array.data = AI_PTR(net_ctx->_weights[0] + 2533532);
  gemm_376_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 2533532);
  gemm_376_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_376_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(376, 1, { eltwise_363_output.data->data});
  forward_dense(&gemm_376_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(376, 1, { gemm_376_output.data->data});
}
void forward_lite_eltwise_eltwise_378(_stai_reatt8_context* net_ctx)
{
  gemm_376_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_376_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  arith_constant52_4D_array.data = AI_PTR(net_ctx->_weights[0] + 5664);
  arith_constant52_4D_array.data_start = AI_PTR(net_ctx->_weights[0] + 5664);
  eltwise_378_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_378_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(378, 2, { gemm_376_output0.data->data,arith_constant52_4D.data->data});
  forward_eltwise(&eltwise_378_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(378, 1, { eltwise_378_output.data->data});
}
void forward_lite_transpose_transpose_379(_stai_reatt8_context* net_ctx)
{
  eltwise_378_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_378_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  transpose_379_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_379_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(379, 1, { eltwise_378_output.data->data});
  forward_transpose(&transpose_379_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(379, 1, { transpose_379_output.data->data});
}
void forward_lite_matmul_gemm_380(_stai_reatt8_context* net_ctx)
{
  nl_375_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  nl_375_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  transpose_379_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_379_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_380_bias_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  gemm_380_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  gemm_380_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_380_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(380, 3, { nl_375_output.data->data,transpose_379_output.data->data,gemm_380_bias.data->data});
  forward_matmul(&gemm_380_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(380, 1, { gemm_380_output.data->data});
}
void forward_lite_transpose_transpose_382(_stai_reatt8_context* net_ctx)
{
  gemm_380_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_380_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  transpose_382_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_382_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(382, 1, { gemm_380_output.data->data});
  forward_transpose(&transpose_382_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(382, 1, { transpose_382_output.data->data});
}
void forward_lite_dense_gemm_384(_stai_reatt8_context* net_ctx)
{
  transpose_382_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  transpose_382_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_384_weights_array.data = AI_PTR(net_ctx->_weights[0] + 2599068);
  gemm_384_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 2599068);
  gemm_384_bias_array.data = AI_PTR(net_ctx->_weights[0] + 2664604);
  gemm_384_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 2664604);
  gemm_384_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_384_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _STAI_REATT8_EVENT_NODE_START_CB(384, 1, { transpose_382_output0.data->data});
  forward_dense(&gemm_384_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(384, 1, { gemm_384_output.data->data});
}
void forward_lite_eltwise_eltwise_385(_stai_reatt8_context* net_ctx)
{
  eltwise_352_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  eltwise_352_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_384_output_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  gemm_384_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  eltwise_385_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_385_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(385, 2, { eltwise_352_output.data->data,gemm_384_output.data->data});
  forward_eltwise(&eltwise_385_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(385, 1, { eltwise_385_output.data->data});
}
void forward_lite_reduce_reduce_386(_stai_reatt8_context* net_ctx)
{
  eltwise_385_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_385_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_386_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  reduce_386_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(386, 1, { eltwise_385_output.data->data});
  forward_reduce(&reduce_386_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(386, 1, { reduce_386_output.data->data});
}
void forward_lite_eltwise_eltwise_388(_stai_reatt8_context* net_ctx)
{
  eltwise_385_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_385_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_386_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 30840);
  reduce_386_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30840);
  eltwise_388_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_388_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  _STAI_REATT8_EVENT_NODE_START_CB(388, 2, { eltwise_385_output.data->data,reduce_386_Mul_output.data->data});
  forward_eltwise(&eltwise_388_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(388, 1, { eltwise_388_output.data->data});
}
void forward_lite_reduce_reduce_389(_stai_reatt8_context* net_ctx)
{
  eltwise_388_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_388_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  reduce_389_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  reduce_389_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(390, 1, { eltwise_388_output.data->data});
  forward_reduce(&reduce_389_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(390, 1, { reduce_389_output.data->data});
}
void forward_lite_eltwise_eltwise_392(_stai_reatt8_context* net_ctx)
{
  nl_391_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  nl_391_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  functional_1_fut3_ln2_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 7072);
  functional_1_fut3_ln2_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 7072);
  eltwise_392_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_392_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_REATT8_EVENT_NODE_START_CB(392, 2, { nl_391_output.data->data,functional_1_fut3_ln2_1_Reshape.data->data});
  forward_eltwise(&eltwise_392_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(392, 1, { eltwise_392_output.data->data});
}
void forward_lite_eltwise_eltwise_395(_stai_reatt8_context* net_ctx)
{
  eltwise_385_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_385_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_392_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_392_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_395_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_395_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  _STAI_REATT8_EVENT_NODE_START_CB(395, 2, { eltwise_385_output.data->data,eltwise_392_output.data->data});
  forward_eltwise(&eltwise_395_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(395, 1, { eltwise_395_output.data->data});
}
void forward_lite_eltwise_eltwise_393(_stai_reatt8_context* net_ctx)
{
  nl_387_output_array.data = AI_PTR(net_ctx->_activations[0] + 15240);
  nl_387_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15240);
  eltwise_392_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_392_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_393_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_393_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_REATT8_EVENT_NODE_START_CB(393, 2, { nl_387_output.data->data,eltwise_392_output.data->data});
  forward_eltwise(&eltwise_393_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(393, 1, { eltwise_393_output.data->data});
}
void forward_lite_eltwise_eltwise_396(_stai_reatt8_context* net_ctx)
{
  eltwise_395_output_array.data = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_395_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30960);
  eltwise_394_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_394_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_396_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_396_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_REATT8_EVENT_NODE_START_CB(396, 2, { eltwise_395_output.data->data,eltwise_394_output.data->data});
  forward_eltwise(&eltwise_396_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(396, 1, { eltwise_396_output.data->data});
}
void forward_lite_dense_gemm_405(_stai_reatt8_context* net_ctx)
{
  eltwise_396_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_396_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  gemm_405_weights_array.data = AI_PTR(net_ctx->_weights[0] + 2665628);
  gemm_405_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 2665628);
  gemm_405_bias_array.data = AI_PTR(net_ctx->_weights[0] + 2927772);
  gemm_405_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 2927772);
  gemm_405_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_405_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  _STAI_REATT8_EVENT_NODE_START_CB(407, 1, { eltwise_396_output.data->data});
  forward_dense(&gemm_405_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(407, 1, { gemm_405_output.data->data});
}
void forward_lite_dense_gemm_417(_stai_reatt8_context* net_ctx)
{
  nl_408_output_array.data = AI_PTR(net_ctx->_activations[0] + 30720);
  nl_408_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 30720);
  gemm_417_weights_array.data = AI_PTR(net_ctx->_weights[0] + 2929820);
  gemm_417_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 2929820);
  gemm_417_bias_array.data = AI_PTR(net_ctx->_weights[0] + 3191964);
  gemm_417_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 3191964);
  gemm_417_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  gemm_417_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_REATT8_EVENT_NODE_START_CB(419, 1, { nl_408_output.data->data});
  forward_dense(&gemm_417_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(419, 1, { gemm_417_output.data->data});
}
void forward_lite_eltwise_eltwise_420(_stai_reatt8_context* net_ctx)
{
  eltwise_385_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_385_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_417_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  gemm_417_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  eltwise_420_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  eltwise_420_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATT8_EVENT_NODE_START_CB(420, 2, { eltwise_385_output.data->data,gemm_417_output.data->data});
  forward_eltwise(&eltwise_420_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(420, 1, { eltwise_420_output.data->data});
}
void forward_lite_pad_pad_3(_stai_reatt8_context* net_ctx)
{
  conv2d_1_output_array.data = AI_PTR(net_ctx->_activations[0] + 15404);
  conv2d_1_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15404);
  pad_3_output_array.data = AI_PTR(net_ctx->_activations[0] + 130604);
  pad_3_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130604);
  _STAI_REATT8_EVENT_NODE_START_CB(3, 1, { conv2d_1_output0.data->data});
  forward_pad(&pad_3_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(3, 1, { pad_3_output.data->data});
}
void forward_lite_reduce_reduce_7(_stai_reatt8_context* net_ctx)
{
  conv2d_5_output_array.data = AI_PTR(net_ctx->_activations[0] + 247724);
  conv2d_5_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 247724);
  reduce_7_output_array.data = AI_PTR(net_ctx->_activations[0] + 130604);
  reduce_7_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130604);
  _STAI_REATT8_EVENT_NODE_START_CB(7, 1, { conv2d_5_output0.data->data});
  forward_reduce(&reduce_7_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(7, 1, { reduce_7_output.data->data});
}
void forward_lite_eltwise_eltwise_9(_stai_reatt8_context* net_ctx)
{
  conv2d_5_output_array.data = AI_PTR(net_ctx->_activations[0] + 247724);
  conv2d_5_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 247724);
  reduce_7_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 246524);
  reduce_7_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 246524);
  eltwise_9_output_array.data = AI_PTR(net_ctx->_activations[0] + 130604);
  eltwise_9_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130604);
  _STAI_REATT8_EVENT_NODE_START_CB(9, 2, { conv2d_5_output0.data->data,reduce_7_Mul_output.data->data});
  forward_eltwise(&eltwise_9_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(9, 1, { eltwise_9_output.data->data});
}
void forward_lite_reduce_reduce_10(_stai_reatt8_context* net_ctx)
{
  eltwise_9_output_array.data = AI_PTR(net_ctx->_activations[0] + 130604);
  eltwise_9_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130604);
  reduce_10_output_array.data = AI_PTR(net_ctx->_activations[0] + 362924);
  reduce_10_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 362924);
  _STAI_REATT8_EVENT_NODE_START_CB(11, 1, { eltwise_9_output.data->data});
  forward_reduce(&reduce_10_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(11, 1, { reduce_10_output.data->data});
}
void forward_lite_eltwise_eltwise_13(_stai_reatt8_context* net_ctx)
{
  nl_12_output_array.data = AI_PTR(net_ctx->_activations[0] + 362924);
  nl_12_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 362924);
  functional_1_past_ln0_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 3194396);
  functional_1_past_ln0_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 3194396);
  eltwise_13_output_array.data = AI_PTR(net_ctx->_activations[0] + 130604);
  eltwise_13_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130604);
  _STAI_REATT8_EVENT_NODE_START_CB(13, 2, { nl_12_output.data->data,functional_1_past_ln0_1_Reshape.data->data});
  forward_eltwise(&eltwise_13_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(13, 1, { eltwise_13_output.data->data});
}
void forward_lite_eltwise_eltwise_16(_stai_reatt8_context* net_ctx)
{
  conv2d_5_output_array.data = AI_PTR(net_ctx->_activations[0] + 247724);
  conv2d_5_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 247724);
  eltwise_13_output_array.data = AI_PTR(net_ctx->_activations[0] + 130604);
  eltwise_13_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130604);
  eltwise_16_output_array.data = AI_PTR(net_ctx->_activations[0] + 247724);
  eltwise_16_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 247724);
  _STAI_REATT8_EVENT_NODE_START_CB(16, 2, { conv2d_5_output0.data->data,eltwise_13_output.data->data});
  forward_eltwise(&eltwise_16_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(16, 1, { eltwise_16_output.data->data});
}
void forward_lite_eltwise_eltwise_14(_stai_reatt8_context* net_ctx)
{
  nl_8_output_array.data = AI_PTR(net_ctx->_activations[0] + 362924);
  nl_8_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 362924);
  eltwise_13_output_array.data = AI_PTR(net_ctx->_activations[0] + 130604);
  eltwise_13_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130604);
  eltwise_14_output_array.data = AI_PTR(net_ctx->_activations[0] + 130604);
  eltwise_14_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130604);
  _STAI_REATT8_EVENT_NODE_START_CB(14, 2, { nl_8_output.data->data,eltwise_13_output.data->data});
  forward_eltwise(&eltwise_14_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(14, 1, { eltwise_14_output.data->data});
}
void forward_lite_eltwise_eltwise_17(_stai_reatt8_context* net_ctx)
{
  eltwise_16_output_array.data = AI_PTR(net_ctx->_activations[0] + 247724);
  eltwise_16_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 247724);
  eltwise_15_output_array.data = AI_PTR(net_ctx->_activations[0] + 130604);
  eltwise_15_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130604);
  eltwise_17_output_array.data = AI_PTR(net_ctx->_activations[0] + 247724);
  eltwise_17_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 247724);
  _STAI_REATT8_EVENT_NODE_START_CB(17, 2, { eltwise_16_output.data->data,eltwise_15_output.data->data});
  forward_eltwise(&eltwise_17_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(17, 1, { eltwise_17_output.data->data});
}
void forward_lite_eltwise_eltwise_19(_stai_reatt8_context* net_ctx)
{
  conv2d_1_output_array.data = AI_PTR(net_ctx->_activations[0] + 15404);
  conv2d_1_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15404);
  nl_18_output_array.data = AI_PTR(net_ctx->_activations[0] + 130604);
  nl_18_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130604);
  eltwise_19_output_array.data = AI_PTR(net_ctx->_activations[0] + 245804);
  eltwise_19_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245804);
  _STAI_REATT8_EVENT_NODE_START_CB(19, 2, { conv2d_1_output0.data->data,nl_18_output.data->data});
  forward_eltwise(&eltwise_19_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(19, 1, { eltwise_19_output.data->data});
}
void forward_lite_pad_pad_20(_stai_reatt8_context* net_ctx)
{
  eltwise_19_output_array.data = AI_PTR(net_ctx->_activations[0] + 245804);
  eltwise_19_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245804);
  pad_20_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  pad_20_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(20, 1, { eltwise_19_output.data->data});
  forward_pad(&pad_20_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(20, 1, { pad_20_output.data->data});
}
void forward_lite_reduce_reduce_27(_stai_reatt8_context* net_ctx)
{
  conv2d_23_output_array.data = AI_PTR(net_ctx->_activations[0] + 361004);
  conv2d_23_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 361004);
  reduce_27_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_27_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(27, 1, { conv2d_23_output.data->data});
  forward_reduce(&reduce_27_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(27, 1, { reduce_27_output.data->data});
}
void forward_lite_eltwise_eltwise_29(_stai_reatt8_context* net_ctx)
{
  conv2d_23_output_array.data = AI_PTR(net_ctx->_activations[0] + 361004);
  conv2d_23_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 361004);
  reduce_27_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 16560);
  reduce_27_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 16560);
  eltwise_29_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_29_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  _STAI_REATT8_EVENT_NODE_START_CB(29, 2, { conv2d_23_output.data->data,reduce_27_Mul_output.data->data});
  forward_eltwise(&eltwise_29_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(29, 1, { eltwise_29_output.data->data});
}
void forward_lite_reduce_reduce_30(_stai_reatt8_context* net_ctx)
{
  eltwise_29_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_29_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  reduce_30_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_30_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(31, 1, { eltwise_29_output.data->data});
  forward_reduce(&reduce_30_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(31, 1, { reduce_30_output.data->data});
}
void forward_lite_eltwise_eltwise_33(_stai_reatt8_context* net_ctx)
{
  nl_32_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_32_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  functional_1_past_ln1_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 3194012);
  functional_1_past_ln1_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 3194012);
  eltwise_33_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_33_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  _STAI_REATT8_EVENT_NODE_START_CB(33, 2, { nl_32_output.data->data,functional_1_past_ln1_1_Reshape.data->data});
  forward_eltwise(&eltwise_33_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(33, 1, { eltwise_33_output.data->data});
}
void forward_lite_eltwise_eltwise_36(_stai_reatt8_context* net_ctx)
{
  conv2d_23_output_array.data = AI_PTR(net_ctx->_activations[0] + 361004);
  conv2d_23_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 361004);
  eltwise_33_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_33_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_36_output_array.data = AI_PTR(net_ctx->_activations[0] + 361004);
  eltwise_36_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 361004);
  _STAI_REATT8_EVENT_NODE_START_CB(36, 2, { conv2d_23_output.data->data,eltwise_33_output.data->data});
  forward_eltwise(&eltwise_36_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(36, 1, { eltwise_36_output.data->data});
}
void forward_lite_eltwise_eltwise_34(_stai_reatt8_context* net_ctx)
{
  nl_28_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_28_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_33_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_33_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_34_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_34_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  _STAI_REATT8_EVENT_NODE_START_CB(34, 2, { nl_28_output.data->data,eltwise_33_output.data->data});
  forward_eltwise(&eltwise_34_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(34, 1, { eltwise_34_output.data->data});
}
void forward_lite_eltwise_eltwise_37(_stai_reatt8_context* net_ctx)
{
  eltwise_36_output_array.data = AI_PTR(net_ctx->_activations[0] + 361004);
  eltwise_36_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 361004);
  eltwise_35_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_35_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_37_output_array.data = AI_PTR(net_ctx->_activations[0] + 361004);
  eltwise_37_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 361004);
  _STAI_REATT8_EVENT_NODE_START_CB(37, 2, { eltwise_36_output.data->data,eltwise_35_output.data->data});
  forward_eltwise(&eltwise_37_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(37, 1, { eltwise_37_output.data->data});
}
void forward_lite_eltwise_eltwise_39(_stai_reatt8_context* net_ctx)
{
  eltwise_19_output_array.data = AI_PTR(net_ctx->_activations[0] + 245804);
  eltwise_19_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245804);
  nl_38_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_38_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_39_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  eltwise_39_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  _STAI_REATT8_EVENT_NODE_START_CB(39, 2, { eltwise_19_output.data->data,nl_38_output.data->data});
  forward_eltwise(&eltwise_39_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(39, 1, { eltwise_39_output.data->data});
}
void forward_lite_pad_pad_40(_stai_reatt8_context* net_ctx)
{
  eltwise_39_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  eltwise_39_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  pad_40_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  pad_40_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  _STAI_REATT8_EVENT_NODE_START_CB(40, 1, { eltwise_39_output.data->data});
  forward_pad(&pad_40_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(40, 1, { pad_40_output.data->data});
}
void forward_lite_reduce_reduce_47(_stai_reatt8_context* net_ctx)
{
  conv2d_43_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  conv2d_43_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_47_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  reduce_47_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  _STAI_REATT8_EVENT_NODE_START_CB(47, 1, { conv2d_43_output.data->data});
  forward_reduce(&reduce_47_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(47, 1, { reduce_47_output.data->data});
}
void forward_lite_eltwise_eltwise_49(_stai_reatt8_context* net_ctx)
{
  conv2d_43_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  conv2d_43_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_47_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 246960);
  reduce_47_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 246960);
  eltwise_49_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_49_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  _STAI_REATT8_EVENT_NODE_START_CB(49, 2, { conv2d_43_output.data->data,reduce_47_Mul_output.data->data});
  forward_eltwise(&eltwise_49_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(49, 1, { eltwise_49_output.data->data});
}
void forward_lite_reduce_reduce_50(_stai_reatt8_context* net_ctx)
{
  eltwise_49_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_49_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  reduce_50_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  reduce_50_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  _STAI_REATT8_EVENT_NODE_START_CB(51, 1, { eltwise_49_output.data->data});
  forward_reduce(&reduce_50_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(51, 1, { reduce_50_output.data->data});
}
void forward_lite_eltwise_eltwise_53(_stai_reatt8_context* net_ctx)
{
  nl_52_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  nl_52_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  functional_1_past_ln2_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 3193628);
  functional_1_past_ln2_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 3193628);
  eltwise_53_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_53_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  _STAI_REATT8_EVENT_NODE_START_CB(53, 2, { nl_52_output.data->data,functional_1_past_ln2_1_Reshape.data->data});
  forward_eltwise(&eltwise_53_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(53, 1, { eltwise_53_output.data->data});
}
void forward_lite_eltwise_eltwise_56(_stai_reatt8_context* net_ctx)
{
  conv2d_43_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  conv2d_43_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_53_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_53_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_56_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_56_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(56, 2, { conv2d_43_output.data->data,eltwise_53_output.data->data});
  forward_eltwise(&eltwise_56_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(56, 1, { eltwise_56_output.data->data});
}
void forward_lite_eltwise_eltwise_54(_stai_reatt8_context* net_ctx)
{
  nl_48_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  nl_48_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  eltwise_53_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_53_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_54_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_54_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  _STAI_REATT8_EVENT_NODE_START_CB(54, 2, { nl_48_output.data->data,eltwise_53_output.data->data});
  forward_eltwise(&eltwise_54_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(54, 1, { eltwise_54_output.data->data});
}
void forward_lite_eltwise_eltwise_57(_stai_reatt8_context* net_ctx)
{
  eltwise_56_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_56_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_55_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_55_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_57_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_57_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(57, 2, { eltwise_56_output.data->data,eltwise_55_output.data->data});
  forward_eltwise(&eltwise_57_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(57, 1, { eltwise_57_output.data->data});
}
void forward_lite_eltwise_eltwise_59(_stai_reatt8_context* net_ctx)
{
  eltwise_39_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  eltwise_39_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  nl_58_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  nl_58_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  eltwise_59_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_59_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(59, 2, { eltwise_39_output.data->data,nl_58_output.data->data});
  forward_eltwise(&eltwise_59_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(59, 1, { eltwise_59_output.data->data});
}
void forward_lite_pad_pad_60(_stai_reatt8_context* net_ctx)
{
  eltwise_59_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_59_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  pad_60_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  pad_60_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  _STAI_REATT8_EVENT_NODE_START_CB(60, 1, { eltwise_59_output.data->data});
  forward_pad(&pad_60_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(60, 1, { pad_60_output.data->data});
}
void forward_lite_reduce_reduce_67(_stai_reatt8_context* net_ctx)
{
  conv2d_63_output_array.data = AI_PTR(net_ctx->_activations[0] + 251904);
  conv2d_63_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 251904);
  reduce_67_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  reduce_67_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  _STAI_REATT8_EVENT_NODE_START_CB(67, 1, { conv2d_63_output.data->data});
  forward_reduce(&reduce_67_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(67, 1, { reduce_67_output.data->data});
}
void forward_lite_eltwise_eltwise_69(_stai_reatt8_context* net_ctx)
{
  conv2d_63_output_array.data = AI_PTR(net_ctx->_activations[0] + 251904);
  conv2d_63_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 251904);
  reduce_67_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 131760);
  reduce_67_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131760);
  eltwise_69_output_array.data = AI_PTR(net_ctx->_activations[0] + 132960);
  eltwise_69_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 132960);
  _STAI_REATT8_EVENT_NODE_START_CB(69, 2, { conv2d_63_output.data->data,reduce_67_Mul_output.data->data});
  forward_eltwise(&eltwise_69_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(69, 1, { eltwise_69_output.data->data});
}
void forward_lite_reduce_reduce_70(_stai_reatt8_context* net_ctx)
{
  eltwise_69_output_array.data = AI_PTR(net_ctx->_activations[0] + 132960);
  eltwise_69_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 132960);
  reduce_70_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  reduce_70_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  _STAI_REATT8_EVENT_NODE_START_CB(71, 1, { eltwise_69_output.data->data});
  forward_reduce(&reduce_70_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(71, 1, { reduce_70_output.data->data});
}
void forward_lite_eltwise_eltwise_73(_stai_reatt8_context* net_ctx)
{
  nl_72_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  nl_72_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  functional_1_past_ln3_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 3193244);
  functional_1_past_ln3_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 3193244);
  eltwise_73_output_array.data = AI_PTR(net_ctx->_activations[0] + 132960);
  eltwise_73_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 132960);
  _STAI_REATT8_EVENT_NODE_START_CB(73, 2, { nl_72_output.data->data,functional_1_past_ln3_1_Reshape.data->data});
  forward_eltwise(&eltwise_73_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(73, 1, { eltwise_73_output.data->data});
}
void forward_lite_eltwise_eltwise_76(_stai_reatt8_context* net_ctx)
{
  conv2d_63_output_array.data = AI_PTR(net_ctx->_activations[0] + 251904);
  conv2d_63_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 251904);
  eltwise_73_output_array.data = AI_PTR(net_ctx->_activations[0] + 132960);
  eltwise_73_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 132960);
  eltwise_76_output_array.data = AI_PTR(net_ctx->_activations[0] + 251904);
  eltwise_76_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 251904);
  _STAI_REATT8_EVENT_NODE_START_CB(76, 2, { conv2d_63_output.data->data,eltwise_73_output.data->data});
  forward_eltwise(&eltwise_76_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(76, 1, { eltwise_76_output.data->data});
}
void forward_lite_eltwise_eltwise_74(_stai_reatt8_context* net_ctx)
{
  nl_68_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  nl_68_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  eltwise_73_output_array.data = AI_PTR(net_ctx->_activations[0] + 132960);
  eltwise_73_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 132960);
  eltwise_74_output_array.data = AI_PTR(net_ctx->_activations[0] + 132960);
  eltwise_74_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 132960);
  _STAI_REATT8_EVENT_NODE_START_CB(74, 2, { nl_68_output.data->data,eltwise_73_output.data->data});
  forward_eltwise(&eltwise_74_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(74, 1, { eltwise_74_output.data->data});
}
void forward_lite_eltwise_eltwise_77(_stai_reatt8_context* net_ctx)
{
  eltwise_76_output_array.data = AI_PTR(net_ctx->_activations[0] + 251904);
  eltwise_76_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 251904);
  eltwise_75_output_array.data = AI_PTR(net_ctx->_activations[0] + 132960);
  eltwise_75_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 132960);
  eltwise_77_output_array.data = AI_PTR(net_ctx->_activations[0] + 251904);
  eltwise_77_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 251904);
  _STAI_REATT8_EVENT_NODE_START_CB(77, 2, { eltwise_76_output.data->data,eltwise_75_output.data->data});
  forward_eltwise(&eltwise_77_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(77, 1, { eltwise_77_output.data->data});
}
void forward_lite_eltwise_eltwise_79(_stai_reatt8_context* net_ctx)
{
  eltwise_59_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_59_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_78_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  nl_78_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  eltwise_79_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  eltwise_79_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  _STAI_REATT8_EVENT_NODE_START_CB(79, 2, { eltwise_59_output.data->data,nl_78_output.data->data});
  forward_eltwise(&eltwise_79_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(79, 1, { eltwise_79_output.data->data});
}
void forward_lite_pad_pad_80(_stai_reatt8_context* net_ctx)
{
  eltwise_79_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  eltwise_79_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  pad_80_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  pad_80_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(80, 1, { eltwise_79_output.data->data});
  forward_pad(&pad_80_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(80, 1, { pad_80_output.data->data});
}
void forward_lite_reduce_reduce_87(_stai_reatt8_context* net_ctx)
{
  conv2d_83_output_array.data = AI_PTR(net_ctx->_activations[0] + 360960);
  conv2d_83_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 360960);
  reduce_87_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_87_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(87, 1, { conv2d_83_output.data->data});
  forward_reduce(&reduce_87_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(87, 1, { reduce_87_output.data->data});
}
void forward_lite_eltwise_eltwise_89(_stai_reatt8_context* net_ctx)
{
  conv2d_83_output_array.data = AI_PTR(net_ctx->_activations[0] + 360960);
  conv2d_83_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 360960);
  reduce_87_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 16560);
  reduce_87_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 16560);
  eltwise_89_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_89_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  _STAI_REATT8_EVENT_NODE_START_CB(89, 2, { conv2d_83_output.data->data,reduce_87_Mul_output.data->data});
  forward_eltwise(&eltwise_89_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(89, 1, { eltwise_89_output.data->data});
}
void forward_lite_reduce_reduce_90(_stai_reatt8_context* net_ctx)
{
  eltwise_89_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_89_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  reduce_90_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_90_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(91, 1, { eltwise_89_output.data->data});
  forward_reduce(&reduce_90_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(91, 1, { reduce_90_output.data->data});
}
void forward_lite_eltwise_eltwise_93(_stai_reatt8_context* net_ctx)
{
  nl_92_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_92_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  functional_1_past_ln4_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 3192860);
  functional_1_past_ln4_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 3192860);
  eltwise_93_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_93_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  _STAI_REATT8_EVENT_NODE_START_CB(93, 2, { nl_92_output.data->data,functional_1_past_ln4_1_Reshape.data->data});
  forward_eltwise(&eltwise_93_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(93, 1, { eltwise_93_output.data->data});
}
void forward_lite_eltwise_eltwise_96(_stai_reatt8_context* net_ctx)
{
  conv2d_83_output_array.data = AI_PTR(net_ctx->_activations[0] + 360960);
  conv2d_83_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 360960);
  eltwise_93_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_93_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_96_output_array.data = AI_PTR(net_ctx->_activations[0] + 360960);
  eltwise_96_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 360960);
  _STAI_REATT8_EVENT_NODE_START_CB(96, 2, { conv2d_83_output.data->data,eltwise_93_output.data->data});
  forward_eltwise(&eltwise_96_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(96, 1, { eltwise_96_output.data->data});
}
void forward_lite_eltwise_eltwise_94(_stai_reatt8_context* net_ctx)
{
  nl_88_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_88_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_93_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_93_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_94_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_94_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  _STAI_REATT8_EVENT_NODE_START_CB(94, 2, { nl_88_output.data->data,eltwise_93_output.data->data});
  forward_eltwise(&eltwise_94_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(94, 1, { eltwise_94_output.data->data});
}
void forward_lite_eltwise_eltwise_97(_stai_reatt8_context* net_ctx)
{
  eltwise_96_output_array.data = AI_PTR(net_ctx->_activations[0] + 360960);
  eltwise_96_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 360960);
  eltwise_95_output_array.data = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_95_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 17760);
  eltwise_97_output_array.data = AI_PTR(net_ctx->_activations[0] + 360960);
  eltwise_97_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 360960);
  _STAI_REATT8_EVENT_NODE_START_CB(97, 2, { eltwise_96_output.data->data,eltwise_95_output.data->data});
  forward_eltwise(&eltwise_97_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(97, 1, { eltwise_97_output.data->data});
}
void forward_lite_eltwise_eltwise_99(_stai_reatt8_context* net_ctx)
{
  eltwise_79_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  eltwise_79_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  nl_98_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_98_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_99_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  eltwise_99_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  _STAI_REATT8_EVENT_NODE_START_CB(99, 2, { eltwise_79_output.data->data,nl_98_output.data->data});
  forward_eltwise(&eltwise_99_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(99, 1, { eltwise_99_output.data->data});
}
void forward_lite_pad_pad_100(_stai_reatt8_context* net_ctx)
{
  eltwise_99_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  eltwise_99_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  pad_100_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  pad_100_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  _STAI_REATT8_EVENT_NODE_START_CB(100, 1, { eltwise_99_output.data->data});
  forward_pad(&pad_100_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(100, 1, { pad_100_output.data->data});
}
void forward_lite_reduce_reduce_107(_stai_reatt8_context* net_ctx)
{
  conv2d_103_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  conv2d_103_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_107_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  reduce_107_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  _STAI_REATT8_EVENT_NODE_START_CB(107, 1, { conv2d_103_output.data->data});
  forward_reduce(&reduce_107_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(107, 1, { reduce_107_output.data->data});
}
void forward_lite_eltwise_eltwise_109(_stai_reatt8_context* net_ctx)
{
  conv2d_103_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  conv2d_103_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_107_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 246960);
  reduce_107_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 246960);
  eltwise_109_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_109_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  _STAI_REATT8_EVENT_NODE_START_CB(109, 2, { conv2d_103_output.data->data,reduce_107_Mul_output.data->data});
  forward_eltwise(&eltwise_109_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(109, 1, { eltwise_109_output.data->data});
}
void forward_lite_reduce_reduce_110(_stai_reatt8_context* net_ctx)
{
  eltwise_109_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_109_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  reduce_110_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  reduce_110_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  _STAI_REATT8_EVENT_NODE_START_CB(111, 1, { eltwise_109_output.data->data});
  forward_reduce(&reduce_110_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(111, 1, { reduce_110_output.data->data});
}
void forward_lite_eltwise_eltwise_113(_stai_reatt8_context* net_ctx)
{
  nl_112_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  nl_112_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  functional_1_past_ln5_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 3192476);
  functional_1_past_ln5_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 3192476);
  eltwise_113_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_113_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  _STAI_REATT8_EVENT_NODE_START_CB(113, 2, { nl_112_output.data->data,functional_1_past_ln5_1_Reshape.data->data});
  forward_eltwise(&eltwise_113_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(113, 1, { eltwise_113_output.data->data});
}
void forward_lite_eltwise_eltwise_116(_stai_reatt8_context* net_ctx)
{
  conv2d_103_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  conv2d_103_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_113_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_113_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_116_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_116_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(116, 2, { conv2d_103_output.data->data,eltwise_113_output.data->data});
  forward_eltwise(&eltwise_116_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(116, 1, { eltwise_116_output.data->data});
}
void forward_lite_eltwise_eltwise_114(_stai_reatt8_context* net_ctx)
{
  nl_108_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  nl_108_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  eltwise_113_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_113_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_114_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_114_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  _STAI_REATT8_EVENT_NODE_START_CB(114, 2, { nl_108_output.data->data,eltwise_113_output.data->data});
  forward_eltwise(&eltwise_114_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(114, 1, { eltwise_114_output.data->data});
}
void forward_lite_eltwise_eltwise_117(_stai_reatt8_context* net_ctx)
{
  eltwise_116_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_116_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_115_output_array.data = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_115_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 248160);
  eltwise_117_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_117_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(117, 2, { eltwise_116_output.data->data,eltwise_115_output.data->data});
  forward_eltwise(&eltwise_117_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(117, 1, { eltwise_117_output.data->data});
}
void forward_lite_eltwise_eltwise_119(_stai_reatt8_context* net_ctx)
{
  eltwise_99_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  eltwise_99_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  nl_118_output_array.data = AI_PTR(net_ctx->_activations[0] + 245760);
  nl_118_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 245760);
  eltwise_119_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_119_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(119, 2, { eltwise_99_output.data->data,nl_118_output.data->data});
  forward_eltwise(&eltwise_119_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(119, 1, { eltwise_119_output.data->data});
}
void forward_lite_dense_gemm_128(_stai_reatt8_context* net_ctx)
{
  eltwise_119_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_119_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_128_weights_array.data = AI_PTR(net_ctx->_weights[0] + 3867936);
  gemm_128_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 3867936);
  gemm_128_output_array.data = AI_PTR(net_ctx->_activations[0] + 130560);
  gemm_128_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 130560);
  _STAI_REATT8_EVENT_NODE_START_CB(128, 1, { eltwise_119_output.data->data});
  forward_dense(&gemm_128_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(128, 1, { gemm_128_output.data->data});
}
void forward_lite_eltwise_eltwise_133(_stai_reatt8_context* net_ctx)
{
  eltwise_119_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  eltwise_119_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_131_output_array.data = AI_PTR(net_ctx->_activations[0] + 131760);
  nl_131_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131760);
  eltwise_133_output_array.data = AI_PTR(net_ctx->_activations[0] + 132960);
  eltwise_133_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 132960);
  _STAI_REATT8_EVENT_NODE_START_CB(133, 2, { eltwise_119_output.data->data,nl_131_output0.data->data});
  forward_eltwise(&eltwise_133_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(133, 1, { eltwise_133_output.data->data});
}
void forward_lite_reduce_reduce_134(_stai_reatt8_context* net_ctx)
{
  eltwise_133_output_array.data = AI_PTR(net_ctx->_activations[0] + 132960);
  eltwise_133_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 132960);
  reduce_134_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_134_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  _STAI_REATT8_EVENT_NODE_START_CB(134, 1, { eltwise_133_output.data->data});
  forward_reduce(&reduce_134_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(134, 1, { reduce_134_output.data->data});
}
void forward_lite_tile_tile_137(_stai_reatt8_context* net_ctx)
{
  reduce_134_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  reduce_134_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  tile_137_output_array.data = AI_PTR(net_ctx->_activations[0] + 15744);
  tile_137_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15744);
  _STAI_REATT8_EVENT_NODE_START_CB(137, 1, { reduce_134_output.data->data});
  forward_tile(&tile_137_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(137, 1, { tile_137_output.data->data});
}
void forward_lite_concat_concat_421(_stai_reatt8_context* net_ctx)
{
  eltwise_420_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  eltwise_420_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  tile_137_output_array.data = AI_PTR(net_ctx->_activations[0] + 15744);
  tile_137_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15744);
  concat_421_output_array.data = AI_PTR(net_ctx->_activations[0] + 27264);
  concat_421_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 27264);
  _STAI_REATT8_EVENT_NODE_START_CB(421, 2, { eltwise_420_output.data->data,tile_137_output.data->data});
  forward_concat(&concat_421_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(421, 1, { concat_421_output.data->data});
}
void forward_lite_reduce_reduce_422(_stai_reatt8_context* net_ctx)
{
  concat_421_output_array.data = AI_PTR(net_ctx->_activations[0] + 27264);
  concat_421_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 27264);
  reduce_422_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reduce_422_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATT8_EVENT_NODE_START_CB(422, 1, { concat_421_output.data->data});
  forward_reduce(&reduce_422_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(422, 1, { reduce_422_output.data->data});
}
void forward_lite_eltwise_eltwise_424(_stai_reatt8_context* net_ctx)
{
  concat_421_output_array.data = AI_PTR(net_ctx->_activations[0] + 27264);
  concat_421_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 27264);
  reduce_422_Mul_output_array.data = AI_PTR(net_ctx->_activations[0] + 120);
  reduce_422_Mul_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 120);
  eltwise_424_output_array.data = AI_PTR(net_ctx->_activations[0] + 240);
  eltwise_424_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 240);
  _STAI_REATT8_EVENT_NODE_START_CB(424, 2, { concat_421_output.data->data,reduce_422_Mul_output.data->data});
  forward_eltwise(&eltwise_424_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(424, 1, { eltwise_424_output.data->data});
}
void forward_lite_reduce_reduce_425(_stai_reatt8_context* net_ctx)
{
  eltwise_424_output_array.data = AI_PTR(net_ctx->_activations[0] + 240);
  eltwise_424_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 240);
  reduce_425_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  reduce_425_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATT8_EVENT_NODE_START_CB(426, 1, { eltwise_424_output.data->data});
  forward_reduce(&reduce_425_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(426, 1, { reduce_425_output.data->data});
}
void forward_lite_eltwise_eltwise_428(_stai_reatt8_context* net_ctx)
{
  nl_427_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  nl_427_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  functional_1_fusion_ln_1_Reshape_array.data = AI_PTR(net_ctx->_weights[0] + 6176);
  functional_1_fusion_ln_1_Reshape_array.data_start = AI_PTR(net_ctx->_weights[0] + 6176);
  eltwise_428_output_array.data = AI_PTR(net_ctx->_activations[0] + 240);
  eltwise_428_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 240);
  _STAI_REATT8_EVENT_NODE_START_CB(428, 2, { nl_427_output.data->data,functional_1_fusion_ln_1_Reshape.data->data});
  forward_eltwise(&eltwise_428_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(428, 1, { eltwise_428_output.data->data});
}
void forward_lite_eltwise_eltwise_431(_stai_reatt8_context* net_ctx)
{
  concat_421_output_array.data = AI_PTR(net_ctx->_activations[0] + 27264);
  concat_421_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 27264);
  eltwise_428_output_array.data = AI_PTR(net_ctx->_activations[0] + 240);
  eltwise_428_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 240);
  eltwise_431_output_array.data = AI_PTR(net_ctx->_activations[0] + 54144);
  eltwise_431_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 54144);
  _STAI_REATT8_EVENT_NODE_START_CB(431, 2, { concat_421_output.data->data,eltwise_428_output.data->data});
  forward_eltwise(&eltwise_431_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(431, 1, { eltwise_431_output.data->data});
}
void forward_lite_eltwise_eltwise_429(_stai_reatt8_context* net_ctx)
{
  nl_423_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  nl_423_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  eltwise_428_output_array.data = AI_PTR(net_ctx->_activations[0] + 240);
  eltwise_428_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 240);
  eltwise_429_output_array.data = AI_PTR(net_ctx->_activations[0] + 27120);
  eltwise_429_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 27120);
  _STAI_REATT8_EVENT_NODE_START_CB(429, 2, { nl_423_output.data->data,eltwise_428_output.data->data});
  forward_eltwise(&eltwise_429_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(429, 1, { eltwise_429_output.data->data});
}
void forward_lite_eltwise_eltwise_432(_stai_reatt8_context* net_ctx)
{
  eltwise_431_output_array.data = AI_PTR(net_ctx->_activations[0] + 54144);
  eltwise_431_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 54144);
  eltwise_430_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  eltwise_430_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  eltwise_432_output_array.data = AI_PTR(net_ctx->_activations[0] + 26880);
  eltwise_432_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26880);
  _STAI_REATT8_EVENT_NODE_START_CB(432, 2, { eltwise_431_output.data->data,eltwise_430_output.data->data});
  forward_eltwise(&eltwise_432_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(432, 1, { eltwise_432_output.data->data});
}
void forward_lite_dense_gemm_441(_stai_reatt8_context* net_ctx)
{
  eltwise_432_output_array.data = AI_PTR(net_ctx->_activations[0] + 26880);
  eltwise_432_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 26880);
  gemm_441_weights_array.data = AI_PTR(net_ctx->_weights[0] + 3870116);
  gemm_441_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 3870116);
  gemm_441_bias_array.data = AI_PTR(net_ctx->_weights[0] + 3984804);
  gemm_441_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 3984804);
  gemm_441_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  gemm_441_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATT8_EVENT_NODE_START_CB(443, 1, { eltwise_432_output.data->data});
  forward_dense(&gemm_441_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(443, 1, { gemm_441_output.data->data});
}
void forward_lite_dense_gemm_453(_stai_reatt8_context* net_ctx)
{
  nl_444_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_444_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_453_weights_array.data = AI_PTR(net_ctx->_weights[0] + 3985316);
  gemm_453_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 3985316);
  gemm_453_bias_array.data = AI_PTR(net_ctx->_weights[0] + 4050852);
  gemm_453_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 4050852);
  gemm_453_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  gemm_453_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_REATT8_EVENT_NODE_START_CB(455, 1, { nl_444_output.data->data});
  forward_dense(&gemm_453_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(455, 1, { gemm_453_output.data->data});
}
void forward_lite_dense_gemm_465(_stai_reatt8_context* net_ctx)
{
  nl_456_output_array.data = AI_PTR(net_ctx->_activations[0] + 15360);
  nl_456_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15360);
  gemm_465_weights_array.data = AI_PTR(net_ctx->_weights[0] + 4051364);
  gemm_465_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 4051364);
  gemm_465_bias_array.data = AI_PTR(net_ctx->_weights[0] + 4052900);
  gemm_465_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 4052900);
  gemm_465_output_array.data = AI_PTR(net_ctx->_outputs[0] + 0);
  gemm_465_output_array.data_start = AI_PTR(net_ctx->_outputs[0] + 0);
  _STAI_REATT8_EVENT_NODE_START_CB(467, 1, { nl_456_output.data->data});
  forward_dense(&gemm_465_layer);
  _STAI_REATT8_EVENT_NODE_STOP_CB(467, 1, { gemm_465_output.data->data});
}

/*****************************************************************************/









static const ai_i32 nl_154_t_in_0_shape_ch_h_prod_const_s32 = 30;



static const ai_i32 nl_150_t_in_0_shape_ch_h_prod_const_s32 = 30;












static const ai_i32 nl_171_t_in_0_shape_ch_h_w_prod_const_s32 = 3600;













static const ai_i32 nl_187_t_in_0_shape_ch_h_prod_const_s32 = 30;



static const ai_i32 nl_183_t_in_0_shape_ch_h_prod_const_s32 = 30;





static const ai_i32 nl_204_t_in_0_shape_ch_h_prod_const_s32 = 15360;
static const ai_float nl_204_l_params_const_f32[] = LITE_ARRAY_VALUES(0);








static const ai_i32 nl_222_t_in_0_shape_ch_h_prod_const_s32 = 30;



static const ai_i32 nl_218_t_in_0_shape_ch_h_prod_const_s32 = 30;












static const ai_i32 nl_239_t_in_0_shape_ch_h_w_prod_const_s32 = 3600;













static const ai_i32 nl_255_t_in_0_shape_ch_h_prod_const_s32 = 30;



static const ai_i32 nl_251_t_in_0_shape_ch_h_prod_const_s32 = 30;





static const ai_i32 nl_272_t_in_0_shape_ch_h_prod_const_s32 = 15360;
static const ai_float nl_272_l_params_const_f32[] = LITE_ARRAY_VALUES(0);








static const ai_i32 nl_290_t_in_0_shape_ch_h_prod_const_s32 = 30;



static const ai_i32 nl_286_t_in_0_shape_ch_h_prod_const_s32 = 30;












static const ai_i32 nl_307_t_in_0_shape_ch_h_w_prod_const_s32 = 3600;













static const ai_i32 nl_323_t_in_0_shape_ch_h_prod_const_s32 = 30;



static const ai_i32 nl_319_t_in_0_shape_ch_h_prod_const_s32 = 30;





static const ai_i32 nl_340_t_in_0_shape_ch_h_prod_const_s32 = 15360;
static const ai_float nl_340_l_params_const_f32[] = LITE_ARRAY_VALUES(0);








static const ai_i32 nl_358_t_in_0_shape_ch_h_prod_const_s32 = 30;



static const ai_i32 nl_354_t_in_0_shape_ch_h_prod_const_s32 = 30;












static const ai_i32 nl_375_t_in_0_shape_ch_h_w_prod_const_s32 = 3600;













static const ai_i32 nl_391_t_in_0_shape_ch_h_prod_const_s32 = 30;



static const ai_i32 nl_387_t_in_0_shape_ch_h_prod_const_s32 = 30;





static const ai_i32 nl_408_t_in_0_shape_ch_h_prod_const_s32 = 15360;
static const ai_float nl_408_l_params_const_f32[] = LITE_ARRAY_VALUES(0);



static const ai_u32 conv2d_1_t_in_0_shape_ch_const_u32 = 11;
static const ai_u32 conv2d_1_t_out_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_1_t_in_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_1_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_1_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_1_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_1_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_1_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_1_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_1_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_1_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_1_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_1_l_dilation_H_const_u16 = 1;
static const ai_u16 conv2d_1_l_dilation_W_const_u16 = 1;


static const ai_u32 conv2d_5_t_in_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_5_t_out_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_5_t_in_0_shape_w_const_u32 = 302;
static const ai_u32 conv2d_5_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_5_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_5_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_5_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_5_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_5_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_5_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_5_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_5_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_5_l_dilation_H_const_u16 = 1;
static const ai_u16 conv2d_5_l_dilation_W_const_u16 = 1;






static const ai_i32 nl_12_t_in_0_shape_ch_h_prod_const_s32 = 300;



static const ai_i32 nl_8_t_in_0_shape_ch_h_prod_const_s32 = 300;




static const ai_i32 nl_18_t_in_0_shape_ch_h_prod_const_s32 = 28800;
static const ai_float nl_18_l_params_const_f32[] = LITE_ARRAY_VALUES(0);



static const ai_u32 conv2d_23_t_in_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_23_t_out_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_23_t_in_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_23_t_in_0_shape_h_const_u32 = 304;
static const ai_u32 conv2d_23_t_out_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_23_t_out_0_shape_h_const_u32 = 300;
static const ai_u32 conv2d_23_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_23_t_weight_0_shape_h_const_u32 = 3;
static const ai_i32 conv2d_23_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_23_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_23_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_23_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_23_l_dilation_W_const_u16 = 1;
static const ai_u16 conv2d_23_l_dilation_H_const_u16 = 2;
static const ai_size conv2d_23_v_n_groups_const_size = 1;






static const ai_i32 nl_32_t_in_0_shape_ch_h_prod_const_s32 = 300;



static const ai_i32 nl_28_t_in_0_shape_ch_h_prod_const_s32 = 300;




static const ai_i32 nl_38_t_in_0_shape_ch_h_prod_const_s32 = 28800;
static const ai_float nl_38_l_params_const_f32[] = LITE_ARRAY_VALUES(0);



static const ai_u32 conv2d_43_t_in_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_43_t_out_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_43_t_in_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_43_t_in_0_shape_h_const_u32 = 308;
static const ai_u32 conv2d_43_t_out_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_43_t_out_0_shape_h_const_u32 = 300;
static const ai_u32 conv2d_43_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_43_t_weight_0_shape_h_const_u32 = 3;
static const ai_i32 conv2d_43_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_43_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_43_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_43_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_43_l_dilation_W_const_u16 = 1;
static const ai_u16 conv2d_43_l_dilation_H_const_u16 = 4;
static const ai_size conv2d_43_v_n_groups_const_size = 1;






static const ai_i32 nl_52_t_in_0_shape_ch_h_prod_const_s32 = 300;



static const ai_i32 nl_48_t_in_0_shape_ch_h_prod_const_s32 = 300;




static const ai_i32 nl_58_t_in_0_shape_ch_h_prod_const_s32 = 28800;
static const ai_float nl_58_l_params_const_f32[] = LITE_ARRAY_VALUES(0);



static const ai_u32 conv2d_63_t_in_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_63_t_out_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_63_t_in_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_63_t_in_0_shape_h_const_u32 = 316;
static const ai_u32 conv2d_63_t_out_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_63_t_out_0_shape_h_const_u32 = 300;
static const ai_u32 conv2d_63_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_63_t_weight_0_shape_h_const_u32 = 3;
static const ai_i32 conv2d_63_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_63_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_63_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_63_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_63_l_dilation_W_const_u16 = 1;
static const ai_u16 conv2d_63_l_dilation_H_const_u16 = 8;
static const ai_size conv2d_63_v_n_groups_const_size = 1;






static const ai_i32 nl_72_t_in_0_shape_ch_h_prod_const_s32 = 300;



static const ai_i32 nl_68_t_in_0_shape_ch_h_prod_const_s32 = 300;




static const ai_i32 nl_78_t_in_0_shape_ch_h_prod_const_s32 = 28800;
static const ai_float nl_78_l_params_const_f32[] = LITE_ARRAY_VALUES(0);



static const ai_u32 conv2d_83_t_in_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_83_t_out_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_83_t_in_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_83_t_in_0_shape_h_const_u32 = 332;
static const ai_u32 conv2d_83_t_out_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_83_t_out_0_shape_h_const_u32 = 300;
static const ai_u32 conv2d_83_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_83_t_weight_0_shape_h_const_u32 = 3;
static const ai_i32 conv2d_83_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_83_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_83_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_83_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_83_l_dilation_W_const_u16 = 1;
static const ai_u16 conv2d_83_l_dilation_H_const_u16 = 16;
static const ai_size conv2d_83_v_n_groups_const_size = 1;






static const ai_i32 nl_92_t_in_0_shape_ch_h_prod_const_s32 = 300;



static const ai_i32 nl_88_t_in_0_shape_ch_h_prod_const_s32 = 300;




static const ai_i32 nl_98_t_in_0_shape_ch_h_prod_const_s32 = 28800;
static const ai_float nl_98_l_params_const_f32[] = LITE_ARRAY_VALUES(0);



static const ai_u32 conv2d_103_t_in_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_103_t_out_0_shape_ch_const_u32 = 96;
static const ai_u32 conv2d_103_t_in_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_103_t_in_0_shape_h_const_u32 = 364;
static const ai_u32 conv2d_103_t_out_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_103_t_out_0_shape_h_const_u32 = 300;
static const ai_u32 conv2d_103_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 conv2d_103_t_weight_0_shape_h_const_u32 = 3;
static const ai_i32 conv2d_103_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_103_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_103_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_103_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_103_l_dilation_W_const_u16 = 1;
static const ai_u16 conv2d_103_l_dilation_H_const_u16 = 32;
static const ai_size conv2d_103_v_n_groups_const_size = 1;






static const ai_i32 nl_112_t_in_0_shape_ch_h_prod_const_s32 = 300;



static const ai_i32 nl_108_t_in_0_shape_ch_h_prod_const_s32 = 300;




static const ai_i32 nl_118_t_in_0_shape_ch_h_prod_const_s32 = 28800;
static const ai_float nl_118_l_params_const_f32[] = LITE_ARRAY_VALUES(0);



static const ai_i32 nl_131_t_in_0_shape_ch_h_prod_const_s32 = 300;










static const ai_i32 nl_427_t_in_0_shape_ch_h_prod_const_s32 = 30;



static const ai_i32 nl_423_t_in_0_shape_ch_h_prod_const_s32 = 30;





static const ai_i32 nl_444_t_in_0_shape_ch_h_prod_const_s32 = 3840;
static const ai_float nl_444_l_params_const_f32[] = LITE_ARRAY_VALUES(0);


static const ai_i32 nl_456_t_in_0_shape_ch_h_prod_const_s32 = 3840;
static const ai_float nl_456_l_params_const_f32[] = LITE_ARRAY_VALUES(0);

STAI_API_ENTRY
stai_return_code stai_reatt8_run(
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


  /* LITE_KERNEL_SECTION BEGIN gemm_146 */
  {
    
  forward_lite_dense_gemm_146(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_146 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_148 */
  {
    
  forward_lite_eltwise_eltwise_148(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_148 */
  /* LITE_KERNEL_SECTION BEGIN reduce_149 */
  {
    
  forward_lite_reduce_reduce_149(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_149 */
  /* LITE_KERNEL_SECTION BEGIN reduce_149_Mul */
  {
      ai_float* reduce_149_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 45960);
    const ai_float* reduce_149_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 45960);
    const ai_float* reduce_149_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_149_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(149, 1, {(stai_ptr) reduce_149_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_149_Mul_t_out_0_ptr_f32, reduce_149_Mul_t_in_0_ptr_const_f32, reduce_149_Mul_t_weight_0_ptr_const_f32, reduce_149_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(149, 1, {(stai_ptr) reduce_149_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_149_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_151 */
  {
    
  forward_lite_eltwise_eltwise_151(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_151 */
  /* LITE_KERNEL_SECTION BEGIN reduce_152 */
  {
    
  forward_lite_reduce_reduce_152(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_152 */
  /* LITE_KERNEL_SECTION BEGIN reduce_152_Mul */
  {
      ai_float* reduce_152_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 61440);
    const ai_float* reduce_152_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 61440);
    const ai_float* reduce_152_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_152_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(153, 1, {(stai_ptr) reduce_152_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_152_Mul_t_out_0_ptr_f32, reduce_152_Mul_t_in_0_ptr_const_f32, reduce_152_Mul_t_weight_0_ptr_const_f32, reduce_152_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(153, 1, {(stai_ptr) reduce_152_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_152_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_154 */
  {
      ai_handle nl_154_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 45840);
    const ai_handle nl_154_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 61440);
  
  _STAI_REATT8_EVENT_NODE_START_CB(154, 1, {(stai_ptr) nl_154_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_154_t_out_0_ptr_handle, nl_154_t_in_0_ptr_const_handle, nl_154_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(154, 1, {(stai_ptr) nl_154_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_154 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_155 */
  {
    
  forward_lite_eltwise_eltwise_155(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_155 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_158 */
  {
    
  forward_lite_eltwise_eltwise_158(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_158 */
  /* LITE_KERNEL_SECTION BEGIN nl_150 */
  {
      ai_handle nl_150_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 76800);
    const ai_handle nl_150_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 45960);
  
  _STAI_REATT8_EVENT_NODE_START_CB(150, 1, {(stai_ptr) nl_150_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_150_t_out_0_ptr_handle, nl_150_t_in_0_ptr_const_handle, nl_150_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(150, 1, {(stai_ptr) nl_150_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_150 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_156 */
  {
    
  forward_lite_eltwise_eltwise_156(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_156 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_157 */
  {
      ai_float* eltwise_157_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30480);
    const ai_float* eltwise_157_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 30480);
    const ai_float* eltwise_157_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29612);
    const ai_float* eltwise_157_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 30124);
  
  _STAI_REATT8_EVENT_NODE_START_CB(157, 1, {(stai_ptr) eltwise_157_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_157_t_out_0_ptr_f32, eltwise_157_t_in_0_ptr_const_f32, eltwise_157_t_weight_0_ptr_const_f32, eltwise_157_t_weight_1_ptr_const_f32, (ai_u32)(3840), (ai_size)(128));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(157, 1, {(stai_ptr) eltwise_157_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_157 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_159 */
  {
    
  forward_lite_eltwise_eltwise_159(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_159 */
  /* LITE_KERNEL_SECTION BEGIN gemm_160 */
  {
    
  forward_lite_dense_gemm_160(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_160 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_162 */
  {
    
  forward_lite_eltwise_eltwise_162(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_162 */
  /* LITE_KERNEL_SECTION BEGIN transpose_167 */
  {
    
  forward_lite_transpose_transpose_167(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_167 */
  /* LITE_KERNEL_SECTION BEGIN gemm_163 */
  {
    
  forward_lite_dense_gemm_163(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_163 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_165 */
  {
    
  forward_lite_eltwise_eltwise_165(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_165 */
  /* LITE_KERNEL_SECTION BEGIN transpose_166 */
  {
    
  forward_lite_transpose_transpose_166(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_166 */
  /* LITE_KERNEL_SECTION BEGIN gemm_168 */
  {
    
  forward_lite_matmul_gemm_168(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_168 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_169 */
  {
      ai_float* eltwise_169_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* eltwise_169_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 30720);
    const ai_float* eltwise_169_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 161708);
    const ai_float* eltwise_169_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 161828);
  
  _STAI_REATT8_EVENT_NODE_START_CB(169, 1, {(stai_ptr) eltwise_169_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_169_t_out_0_ptr_f32, eltwise_169_t_in_0_ptr_const_f32, eltwise_169_t_weight_0_ptr_const_f32, eltwise_169_t_weight_1_ptr_const_f32, (ai_u32)(3600), (ai_size)(30));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(169, 1, {(stai_ptr) eltwise_169_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_169 */
  /* LITE_KERNEL_SECTION BEGIN nl_171 */
  {
      ai_handle nl_171_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 29760);
    const ai_handle nl_171_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
  
  _STAI_REATT8_EVENT_NODE_START_CB(171, 1, {(stai_ptr) nl_171_t_in_0_ptr_const_handle});
    
  forward_lite_nl_softmax_if32of32(nl_171_t_out_0_ptr_handle, nl_171_t_in_0_ptr_const_handle, nl_171_t_in_0_shape_ch_h_w_prod_const_s32, 1, 30);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(171, 1, {(stai_ptr) nl_171_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_171 */
  /* LITE_KERNEL_SECTION BEGIN gemm_172 */
  {
    
  forward_lite_dense_gemm_172(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_172 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_174 */
  {
    
  forward_lite_eltwise_eltwise_174(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_174 */
  /* LITE_KERNEL_SECTION BEGIN transpose_175 */
  {
    
  forward_lite_transpose_transpose_175(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_175 */
  /* LITE_KERNEL_SECTION BEGIN gemm_176 */
  {
    
  forward_lite_matmul_gemm_176(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_176 */
  /* LITE_KERNEL_SECTION BEGIN transpose_178 */
  {
    
  forward_lite_transpose_transpose_178(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_178 */
  /* LITE_KERNEL_SECTION BEGIN gemm_180 */
  {
    
  forward_lite_dense_gemm_180(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_180 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_181 */
  {
    
  forward_lite_eltwise_eltwise_181(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_181 */
  /* LITE_KERNEL_SECTION BEGIN reduce_182 */
  {
    
  forward_lite_reduce_reduce_182(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_182 */
  /* LITE_KERNEL_SECTION BEGIN reduce_182_Mul */
  {
      ai_float* reduce_182_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30840);
    const ai_float* reduce_182_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 30720);
    const ai_float* reduce_182_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_182_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(182, 1, {(stai_ptr) reduce_182_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_182_Mul_t_out_0_ptr_f32, reduce_182_Mul_t_in_0_ptr_const_f32, reduce_182_Mul_t_weight_0_ptr_const_f32, reduce_182_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(182, 1, {(stai_ptr) reduce_182_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_182_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_184 */
  {
    
  forward_lite_eltwise_eltwise_184(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_184 */
  /* LITE_KERNEL_SECTION BEGIN reduce_185 */
  {
    
  forward_lite_reduce_reduce_185(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_185 */
  /* LITE_KERNEL_SECTION BEGIN reduce_185_Mul */
  {
      ai_float* reduce_185_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30720);
    const ai_float* reduce_185_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 46320);
    const ai_float* reduce_185_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_185_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(186, 1, {(stai_ptr) reduce_185_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_185_Mul_t_out_0_ptr_f32, reduce_185_Mul_t_in_0_ptr_const_f32, reduce_185_Mul_t_weight_0_ptr_const_f32, reduce_185_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(186, 1, {(stai_ptr) reduce_185_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_185_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_187 */
  {
      ai_handle nl_187_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 30960);
    const ai_handle nl_187_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30720);
  
  _STAI_REATT8_EVENT_NODE_START_CB(187, 1, {(stai_ptr) nl_187_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_187_t_out_0_ptr_handle, nl_187_t_in_0_ptr_const_handle, nl_187_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(187, 1, {(stai_ptr) nl_187_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_187 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_188 */
  {
    
  forward_lite_eltwise_eltwise_188(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_188 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_191 */
  {
    
  forward_lite_eltwise_eltwise_191(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_191 */
  /* LITE_KERNEL_SECTION BEGIN nl_183 */
  {
      ai_handle nl_183_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 92160);
    const ai_handle nl_183_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30840);
  
  _STAI_REATT8_EVENT_NODE_START_CB(183, 1, {(stai_ptr) nl_183_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_183_t_out_0_ptr_handle, nl_183_t_in_0_ptr_const_handle, nl_183_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(183, 1, {(stai_ptr) nl_183_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_183 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_189 */
  {
    
  forward_lite_eltwise_eltwise_189(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_189 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_190 */
  {
      ai_float* eltwise_190_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 76800);
    const ai_float* eltwise_190_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 76800);
    const ai_float* eltwise_190_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29612);
    const ai_float* eltwise_190_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 293532);
  
  _STAI_REATT8_EVENT_NODE_START_CB(190, 1, {(stai_ptr) eltwise_190_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_190_t_out_0_ptr_f32, eltwise_190_t_in_0_ptr_const_f32, eltwise_190_t_weight_0_ptr_const_f32, eltwise_190_t_weight_1_ptr_const_f32, (ai_u32)(3840), (ai_size)(128));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(190, 1, {(stai_ptr) eltwise_190_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_190 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_192 */
  {
    
  forward_lite_eltwise_eltwise_192(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_192 */
  /* LITE_KERNEL_SECTION BEGIN gemm_201 */
  {
    
  forward_lite_dense_gemm_201(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_201 */
  /* LITE_KERNEL_SECTION BEGIN nl_204 */
  {
      ai_handle nl_204_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 46080);
    const ai_handle nl_204_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 46080);
  
  _STAI_REATT8_EVENT_NODE_START_CB(204, 1, {(stai_ptr) nl_204_t_in_0_ptr_const_handle});
    
  forward_lite_nl_gelu_if32of32(nl_204_t_out_0_ptr_handle, nl_204_t_in_0_ptr_const_handle, nl_204_t_in_0_shape_ch_h_prod_const_s32, (ai_handle)(nl_204_l_params_const_f32));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(204, 1, {(stai_ptr) nl_204_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_204 */
  /* LITE_KERNEL_SECTION BEGIN gemm_213 */
  {
    
  forward_lite_dense_gemm_213(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_213 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_216 */
  {
    
  forward_lite_eltwise_eltwise_216(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_216 */
  /* LITE_KERNEL_SECTION BEGIN reduce_217 */
  {
    
  forward_lite_reduce_reduce_217(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_217 */
  /* LITE_KERNEL_SECTION BEGIN reduce_217_Mul */
  {
      ai_float* reduce_217_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15480);
    const ai_float* reduce_217_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* reduce_217_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_217_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(217, 1, {(stai_ptr) reduce_217_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_217_Mul_t_out_0_ptr_f32, reduce_217_Mul_t_in_0_ptr_const_f32, reduce_217_Mul_t_weight_0_ptr_const_f32, reduce_217_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(217, 1, {(stai_ptr) reduce_217_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_217_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_219 */
  {
    
  forward_lite_eltwise_eltwise_219(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_219 */
  /* LITE_KERNEL_SECTION BEGIN reduce_220 */
  {
    
  forward_lite_reduce_reduce_220(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_220 */
  /* LITE_KERNEL_SECTION BEGIN reduce_220_Mul */
  {
      ai_float* reduce_220_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15600);
    const ai_float* reduce_220_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* reduce_220_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_220_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(221, 1, {(stai_ptr) reduce_220_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_220_Mul_t_out_0_ptr_f32, reduce_220_Mul_t_in_0_ptr_const_f32, reduce_220_Mul_t_weight_0_ptr_const_f32, reduce_220_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(221, 1, {(stai_ptr) reduce_220_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_220_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_222 */
  {
      ai_handle nl_222_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_222_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15600);
  
  _STAI_REATT8_EVENT_NODE_START_CB(222, 1, {(stai_ptr) nl_222_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_222_t_out_0_ptr_handle, nl_222_t_in_0_ptr_const_handle, nl_222_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(222, 1, {(stai_ptr) nl_222_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_222 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_223 */
  {
    
  forward_lite_eltwise_eltwise_223(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_223 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_226 */
  {
    
  forward_lite_eltwise_eltwise_226(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_226 */
  /* LITE_KERNEL_SECTION BEGIN nl_218 */
  {
      ai_handle nl_218_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_218_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15480);
  
  _STAI_REATT8_EVENT_NODE_START_CB(218, 1, {(stai_ptr) nl_218_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_218_t_out_0_ptr_handle, nl_218_t_in_0_ptr_const_handle, nl_218_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(218, 1, {(stai_ptr) nl_218_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_218 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_224 */
  {
    
  forward_lite_eltwise_eltwise_224(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_224 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_225 */
  {
      ai_float* eltwise_225_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* eltwise_225_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 76800);
    const ai_float* eltwise_225_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29612);
    const ai_float* eltwise_225_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 820892);
  
  _STAI_REATT8_EVENT_NODE_START_CB(225, 1, {(stai_ptr) eltwise_225_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_225_t_out_0_ptr_f32, eltwise_225_t_in_0_ptr_const_f32, eltwise_225_t_weight_0_ptr_const_f32, eltwise_225_t_weight_1_ptr_const_f32, (ai_u32)(3840), (ai_size)(128));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(225, 1, {(stai_ptr) eltwise_225_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_225 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_227 */
  {
    
  forward_lite_eltwise_eltwise_227(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_227 */
  /* LITE_KERNEL_SECTION BEGIN gemm_228 */
  {
    
  forward_lite_dense_gemm_228(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_228 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_230 */
  {
    
  forward_lite_eltwise_eltwise_230(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_230 */
  /* LITE_KERNEL_SECTION BEGIN transpose_235 */
  {
    
  forward_lite_transpose_transpose_235(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_235 */
  /* LITE_KERNEL_SECTION BEGIN gemm_231 */
  {
    
  forward_lite_dense_gemm_231(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_231 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_233 */
  {
    
  forward_lite_eltwise_eltwise_233(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_233 */
  /* LITE_KERNEL_SECTION BEGIN transpose_234 */
  {
    
  forward_lite_transpose_transpose_234(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_234 */
  /* LITE_KERNEL_SECTION BEGIN gemm_236 */
  {
    
  forward_lite_matmul_gemm_236(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_236 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_237 */
  {
      ai_float* eltwise_237_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* eltwise_237_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 76800);
    const ai_float* eltwise_237_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 161708);
    const ai_float* eltwise_237_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 161828);
  
  _STAI_REATT8_EVENT_NODE_START_CB(237, 1, {(stai_ptr) eltwise_237_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_237_t_out_0_ptr_f32, eltwise_237_t_in_0_ptr_const_f32, eltwise_237_t_weight_0_ptr_const_f32, eltwise_237_t_weight_1_ptr_const_f32, (ai_u32)(3600), (ai_size)(30));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(237, 1, {(stai_ptr) eltwise_237_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_237 */
  /* LITE_KERNEL_SECTION BEGIN nl_239 */
  {
      ai_handle nl_239_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 61440);
    const ai_handle nl_239_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
  
  _STAI_REATT8_EVENT_NODE_START_CB(239, 1, {(stai_ptr) nl_239_t_in_0_ptr_const_handle});
    
  forward_lite_nl_softmax_if32of32(nl_239_t_out_0_ptr_handle, nl_239_t_in_0_ptr_const_handle, nl_239_t_in_0_shape_ch_h_w_prod_const_s32, 1, 30);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(239, 1, {(stai_ptr) nl_239_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_239 */
  /* LITE_KERNEL_SECTION BEGIN gemm_240 */
  {
    
  forward_lite_dense_gemm_240(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_240 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_242 */
  {
    
  forward_lite_eltwise_eltwise_242(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_242 */
  /* LITE_KERNEL_SECTION BEGIN transpose_243 */
  {
    
  forward_lite_transpose_transpose_243(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_243 */
  /* LITE_KERNEL_SECTION BEGIN gemm_244 */
  {
    
  forward_lite_matmul_gemm_244(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_244 */
  /* LITE_KERNEL_SECTION BEGIN transpose_246 */
  {
    
  forward_lite_transpose_transpose_246(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_246 */
  /* LITE_KERNEL_SECTION BEGIN gemm_248 */
  {
    
  forward_lite_dense_gemm_248(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_248 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_249 */
  {
    
  forward_lite_eltwise_eltwise_249(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_249 */
  /* LITE_KERNEL_SECTION BEGIN reduce_250 */
  {
    
  forward_lite_reduce_reduce_250(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_250 */
  /* LITE_KERNEL_SECTION BEGIN reduce_250_Mul */
  {
      ai_float* reduce_250_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30840);
    const ai_float* reduce_250_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 30720);
    const ai_float* reduce_250_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_250_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(250, 1, {(stai_ptr) reduce_250_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_250_Mul_t_out_0_ptr_f32, reduce_250_Mul_t_in_0_ptr_const_f32, reduce_250_Mul_t_weight_0_ptr_const_f32, reduce_250_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(250, 1, {(stai_ptr) reduce_250_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_250_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_252 */
  {
    
  forward_lite_eltwise_eltwise_252(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_252 */
  /* LITE_KERNEL_SECTION BEGIN reduce_253 */
  {
    
  forward_lite_reduce_reduce_253(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_253 */
  /* LITE_KERNEL_SECTION BEGIN reduce_253_Mul */
  {
      ai_float* reduce_253_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30960);
    const ai_float* reduce_253_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 30720);
    const ai_float* reduce_253_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_253_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(254, 1, {(stai_ptr) reduce_253_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_253_Mul_t_out_0_ptr_f32, reduce_253_Mul_t_in_0_ptr_const_f32, reduce_253_Mul_t_weight_0_ptr_const_f32, reduce_253_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(254, 1, {(stai_ptr) reduce_253_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_253_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_255 */
  {
      ai_handle nl_255_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 30720);
    const ai_handle nl_255_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30960);
  
  _STAI_REATT8_EVENT_NODE_START_CB(255, 1, {(stai_ptr) nl_255_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_255_t_out_0_ptr_handle, nl_255_t_in_0_ptr_const_handle, nl_255_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(255, 1, {(stai_ptr) nl_255_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_255 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_256 */
  {
    
  forward_lite_eltwise_eltwise_256(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_256 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_259 */
  {
    
  forward_lite_eltwise_eltwise_259(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_259 */
  /* LITE_KERNEL_SECTION BEGIN nl_251 */
  {
      ai_handle nl_251_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 30720);
    const ai_handle nl_251_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30840);
  
  _STAI_REATT8_EVENT_NODE_START_CB(251, 1, {(stai_ptr) nl_251_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_251_t_out_0_ptr_handle, nl_251_t_in_0_ptr_const_handle, nl_251_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(251, 1, {(stai_ptr) nl_251_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_251 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_257 */
  {
    
  forward_lite_eltwise_eltwise_257(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_257 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_258 */
  {
      ai_float* eltwise_258_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30720);
    const ai_float* eltwise_258_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 61680);
    const ai_float* eltwise_258_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29612);
    const ai_float* eltwise_258_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 1084060);
  
  _STAI_REATT8_EVENT_NODE_START_CB(258, 1, {(stai_ptr) eltwise_258_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_258_t_out_0_ptr_f32, eltwise_258_t_in_0_ptr_const_f32, eltwise_258_t_weight_0_ptr_const_f32, eltwise_258_t_weight_1_ptr_const_f32, (ai_u32)(3840), (ai_size)(128));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(258, 1, {(stai_ptr) eltwise_258_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_258 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_260 */
  {
    
  forward_lite_eltwise_eltwise_260(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_260 */
  /* LITE_KERNEL_SECTION BEGIN gemm_269 */
  {
    
  forward_lite_dense_gemm_269(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_269 */
  /* LITE_KERNEL_SECTION BEGIN nl_272 */
  {
      ai_handle nl_272_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 30720);
    const ai_handle nl_272_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30720);
  
  _STAI_REATT8_EVENT_NODE_START_CB(272, 1, {(stai_ptr) nl_272_t_in_0_ptr_const_handle});
    
  forward_lite_nl_gelu_if32of32(nl_272_t_out_0_ptr_handle, nl_272_t_in_0_ptr_const_handle, nl_272_t_in_0_shape_ch_h_prod_const_s32, (ai_handle)(nl_272_l_params_const_f32));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(272, 1, {(stai_ptr) nl_272_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_272 */
  /* LITE_KERNEL_SECTION BEGIN gemm_281 */
  {
    
  forward_lite_dense_gemm_281(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_281 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_284 */
  {
    
  forward_lite_eltwise_eltwise_284(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_284 */
  /* LITE_KERNEL_SECTION BEGIN reduce_285 */
  {
    
  forward_lite_reduce_reduce_285(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_285 */
  /* LITE_KERNEL_SECTION BEGIN reduce_285_Mul */
  {
      ai_float* reduce_285_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15480);
    const ai_float* reduce_285_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* reduce_285_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_285_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(285, 1, {(stai_ptr) reduce_285_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_285_Mul_t_out_0_ptr_f32, reduce_285_Mul_t_in_0_ptr_const_f32, reduce_285_Mul_t_weight_0_ptr_const_f32, reduce_285_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(285, 1, {(stai_ptr) reduce_285_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_285_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_287 */
  {
    
  forward_lite_eltwise_eltwise_287(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_287 */
  /* LITE_KERNEL_SECTION BEGIN reduce_288 */
  {
    
  forward_lite_reduce_reduce_288(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_288 */
  /* LITE_KERNEL_SECTION BEGIN reduce_288_Mul */
  {
      ai_float* reduce_288_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15600);
    const ai_float* reduce_288_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* reduce_288_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_288_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(289, 1, {(stai_ptr) reduce_288_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_288_Mul_t_out_0_ptr_f32, reduce_288_Mul_t_in_0_ptr_const_f32, reduce_288_Mul_t_weight_0_ptr_const_f32, reduce_288_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(289, 1, {(stai_ptr) reduce_288_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_288_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_290 */
  {
      ai_handle nl_290_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_290_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15600);
  
  _STAI_REATT8_EVENT_NODE_START_CB(290, 1, {(stai_ptr) nl_290_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_290_t_out_0_ptr_handle, nl_290_t_in_0_ptr_const_handle, nl_290_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(290, 1, {(stai_ptr) nl_290_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_290 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_291 */
  {
    
  forward_lite_eltwise_eltwise_291(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_291 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_294 */
  {
    
  forward_lite_eltwise_eltwise_294(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_294 */
  /* LITE_KERNEL_SECTION BEGIN nl_286 */
  {
      ai_handle nl_286_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_286_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15480);
  
  _STAI_REATT8_EVENT_NODE_START_CB(286, 1, {(stai_ptr) nl_286_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_286_t_out_0_ptr_handle, nl_286_t_in_0_ptr_const_handle, nl_286_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(286, 1, {(stai_ptr) nl_286_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_286 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_292 */
  {
    
  forward_lite_eltwise_eltwise_292(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_292 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_293 */
  {
      ai_float* eltwise_293_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* eltwise_293_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 76800);
    const ai_float* eltwise_293_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29612);
    const ai_float* eltwise_293_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 1611420);
  
  _STAI_REATT8_EVENT_NODE_START_CB(293, 1, {(stai_ptr) eltwise_293_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_293_t_out_0_ptr_f32, eltwise_293_t_in_0_ptr_const_f32, eltwise_293_t_weight_0_ptr_const_f32, eltwise_293_t_weight_1_ptr_const_f32, (ai_u32)(3840), (ai_size)(128));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(293, 1, {(stai_ptr) eltwise_293_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_293 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_295 */
  {
    
  forward_lite_eltwise_eltwise_295(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_295 */
  /* LITE_KERNEL_SECTION BEGIN gemm_296 */
  {
    
  forward_lite_dense_gemm_296(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_296 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_298 */
  {
    
  forward_lite_eltwise_eltwise_298(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_298 */
  /* LITE_KERNEL_SECTION BEGIN transpose_303 */
  {
    
  forward_lite_transpose_transpose_303(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_303 */
  /* LITE_KERNEL_SECTION BEGIN gemm_299 */
  {
    
  forward_lite_dense_gemm_299(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_299 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_301 */
  {
    
  forward_lite_eltwise_eltwise_301(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_301 */
  /* LITE_KERNEL_SECTION BEGIN transpose_302 */
  {
    
  forward_lite_transpose_transpose_302(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_302 */
  /* LITE_KERNEL_SECTION BEGIN gemm_304 */
  {
    
  forward_lite_matmul_gemm_304(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_304 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_305 */
  {
      ai_float* eltwise_305_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* eltwise_305_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 76800);
    const ai_float* eltwise_305_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 161708);
    const ai_float* eltwise_305_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 161828);
  
  _STAI_REATT8_EVENT_NODE_START_CB(305, 1, {(stai_ptr) eltwise_305_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_305_t_out_0_ptr_f32, eltwise_305_t_in_0_ptr_const_f32, eltwise_305_t_weight_0_ptr_const_f32, eltwise_305_t_weight_1_ptr_const_f32, (ai_u32)(3600), (ai_size)(30));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(305, 1, {(stai_ptr) eltwise_305_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_305 */
  /* LITE_KERNEL_SECTION BEGIN nl_307 */
  {
      ai_handle nl_307_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 61440);
    const ai_handle nl_307_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
  
  _STAI_REATT8_EVENT_NODE_START_CB(307, 1, {(stai_ptr) nl_307_t_in_0_ptr_const_handle});
    
  forward_lite_nl_softmax_if32of32(nl_307_t_out_0_ptr_handle, nl_307_t_in_0_ptr_const_handle, nl_307_t_in_0_shape_ch_h_w_prod_const_s32, 1, 30);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(307, 1, {(stai_ptr) nl_307_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_307 */
  /* LITE_KERNEL_SECTION BEGIN gemm_308 */
  {
    
  forward_lite_dense_gemm_308(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_308 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_310 */
  {
    
  forward_lite_eltwise_eltwise_310(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_310 */
  /* LITE_KERNEL_SECTION BEGIN transpose_311 */
  {
    
  forward_lite_transpose_transpose_311(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_311 */
  /* LITE_KERNEL_SECTION BEGIN gemm_312 */
  {
    
  forward_lite_matmul_gemm_312(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_312 */
  /* LITE_KERNEL_SECTION BEGIN transpose_314 */
  {
    
  forward_lite_transpose_transpose_314(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_314 */
  /* LITE_KERNEL_SECTION BEGIN gemm_316 */
  {
    
  forward_lite_dense_gemm_316(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_316 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_317 */
  {
    
  forward_lite_eltwise_eltwise_317(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_317 */
  /* LITE_KERNEL_SECTION BEGIN reduce_318 */
  {
    
  forward_lite_reduce_reduce_318(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_318 */
  /* LITE_KERNEL_SECTION BEGIN reduce_318_Mul */
  {
      ai_float* reduce_318_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30840);
    const ai_float* reduce_318_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 30720);
    const ai_float* reduce_318_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_318_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(318, 1, {(stai_ptr) reduce_318_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_318_Mul_t_out_0_ptr_f32, reduce_318_Mul_t_in_0_ptr_const_f32, reduce_318_Mul_t_weight_0_ptr_const_f32, reduce_318_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(318, 1, {(stai_ptr) reduce_318_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_318_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_320 */
  {
    
  forward_lite_eltwise_eltwise_320(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_320 */
  /* LITE_KERNEL_SECTION BEGIN reduce_321 */
  {
    
  forward_lite_reduce_reduce_321(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_321 */
  /* LITE_KERNEL_SECTION BEGIN reduce_321_Mul */
  {
      ai_float* reduce_321_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30960);
    const ai_float* reduce_321_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 30720);
    const ai_float* reduce_321_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_321_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(322, 1, {(stai_ptr) reduce_321_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_321_Mul_t_out_0_ptr_f32, reduce_321_Mul_t_in_0_ptr_const_f32, reduce_321_Mul_t_weight_0_ptr_const_f32, reduce_321_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(322, 1, {(stai_ptr) reduce_321_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_321_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_323 */
  {
      ai_handle nl_323_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 30720);
    const ai_handle nl_323_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30960);
  
  _STAI_REATT8_EVENT_NODE_START_CB(323, 1, {(stai_ptr) nl_323_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_323_t_out_0_ptr_handle, nl_323_t_in_0_ptr_const_handle, nl_323_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(323, 1, {(stai_ptr) nl_323_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_323 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_324 */
  {
    
  forward_lite_eltwise_eltwise_324(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_324 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_327 */
  {
    
  forward_lite_eltwise_eltwise_327(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_327 */
  /* LITE_KERNEL_SECTION BEGIN nl_319 */
  {
      ai_handle nl_319_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 30720);
    const ai_handle nl_319_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30840);
  
  _STAI_REATT8_EVENT_NODE_START_CB(319, 1, {(stai_ptr) nl_319_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_319_t_out_0_ptr_handle, nl_319_t_in_0_ptr_const_handle, nl_319_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(319, 1, {(stai_ptr) nl_319_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_319 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_325 */
  {
    
  forward_lite_eltwise_eltwise_325(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_325 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_326 */
  {
      ai_float* eltwise_326_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30720);
    const ai_float* eltwise_326_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 61680);
    const ai_float* eltwise_326_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29612);
    const ai_float* eltwise_326_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 1874588);
  
  _STAI_REATT8_EVENT_NODE_START_CB(326, 1, {(stai_ptr) eltwise_326_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_326_t_out_0_ptr_f32, eltwise_326_t_in_0_ptr_const_f32, eltwise_326_t_weight_0_ptr_const_f32, eltwise_326_t_weight_1_ptr_const_f32, (ai_u32)(3840), (ai_size)(128));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(326, 1, {(stai_ptr) eltwise_326_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_326 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_328 */
  {
    
  forward_lite_eltwise_eltwise_328(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_328 */
  /* LITE_KERNEL_SECTION BEGIN gemm_337 */
  {
    
  forward_lite_dense_gemm_337(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_337 */
  /* LITE_KERNEL_SECTION BEGIN nl_340 */
  {
      ai_handle nl_340_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 30720);
    const ai_handle nl_340_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30720);
  
  _STAI_REATT8_EVENT_NODE_START_CB(340, 1, {(stai_ptr) nl_340_t_in_0_ptr_const_handle});
    
  forward_lite_nl_gelu_if32of32(nl_340_t_out_0_ptr_handle, nl_340_t_in_0_ptr_const_handle, nl_340_t_in_0_shape_ch_h_prod_const_s32, (ai_handle)(nl_340_l_params_const_f32));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(340, 1, {(stai_ptr) nl_340_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_340 */
  /* LITE_KERNEL_SECTION BEGIN gemm_349 */
  {
    
  forward_lite_dense_gemm_349(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_349 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_352 */
  {
    
  forward_lite_eltwise_eltwise_352(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_352 */
  /* LITE_KERNEL_SECTION BEGIN reduce_353 */
  {
    
  forward_lite_reduce_reduce_353(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_353 */
  /* LITE_KERNEL_SECTION BEGIN reduce_353_Mul */
  {
      ai_float* reduce_353_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15480);
    const ai_float* reduce_353_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* reduce_353_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_353_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(353, 1, {(stai_ptr) reduce_353_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_353_Mul_t_out_0_ptr_f32, reduce_353_Mul_t_in_0_ptr_const_f32, reduce_353_Mul_t_weight_0_ptr_const_f32, reduce_353_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(353, 1, {(stai_ptr) reduce_353_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_353_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_355 */
  {
    
  forward_lite_eltwise_eltwise_355(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_355 */
  /* LITE_KERNEL_SECTION BEGIN reduce_356 */
  {
    
  forward_lite_reduce_reduce_356(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_356 */
  /* LITE_KERNEL_SECTION BEGIN reduce_356_Mul */
  {
      ai_float* reduce_356_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15600);
    const ai_float* reduce_356_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* reduce_356_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_356_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(357, 1, {(stai_ptr) reduce_356_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_356_Mul_t_out_0_ptr_f32, reduce_356_Mul_t_in_0_ptr_const_f32, reduce_356_Mul_t_weight_0_ptr_const_f32, reduce_356_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(357, 1, {(stai_ptr) reduce_356_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_356_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_358 */
  {
      ai_handle nl_358_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_358_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15600);
  
  _STAI_REATT8_EVENT_NODE_START_CB(358, 1, {(stai_ptr) nl_358_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_358_t_out_0_ptr_handle, nl_358_t_in_0_ptr_const_handle, nl_358_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(358, 1, {(stai_ptr) nl_358_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_358 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_359 */
  {
    
  forward_lite_eltwise_eltwise_359(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_359 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_362 */
  {
    
  forward_lite_eltwise_eltwise_362(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_362 */
  /* LITE_KERNEL_SECTION BEGIN nl_354 */
  {
      ai_handle nl_354_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_354_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15480);
  
  _STAI_REATT8_EVENT_NODE_START_CB(354, 1, {(stai_ptr) nl_354_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_354_t_out_0_ptr_handle, nl_354_t_in_0_ptr_const_handle, nl_354_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(354, 1, {(stai_ptr) nl_354_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_354 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_360 */
  {
    
  forward_lite_eltwise_eltwise_360(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_360 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_361 */
  {
      ai_float* eltwise_361_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* eltwise_361_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 76800);
    const ai_float* eltwise_361_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29612);
    const ai_float* eltwise_361_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 2401948);
  
  _STAI_REATT8_EVENT_NODE_START_CB(361, 1, {(stai_ptr) eltwise_361_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_361_t_out_0_ptr_f32, eltwise_361_t_in_0_ptr_const_f32, eltwise_361_t_weight_0_ptr_const_f32, eltwise_361_t_weight_1_ptr_const_f32, (ai_u32)(3840), (ai_size)(128));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(361, 1, {(stai_ptr) eltwise_361_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_361 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_363 */
  {
    
  forward_lite_eltwise_eltwise_363(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_363 */
  /* LITE_KERNEL_SECTION BEGIN gemm_364 */
  {
    
  forward_lite_dense_gemm_364(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_364 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_366 */
  {
    
  forward_lite_eltwise_eltwise_366(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_366 */
  /* LITE_KERNEL_SECTION BEGIN transpose_371 */
  {
    
  forward_lite_transpose_transpose_371(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_371 */
  /* LITE_KERNEL_SECTION BEGIN gemm_367 */
  {
    
  forward_lite_dense_gemm_367(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_367 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_369 */
  {
    
  forward_lite_eltwise_eltwise_369(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_369 */
  /* LITE_KERNEL_SECTION BEGIN transpose_370 */
  {
    
  forward_lite_transpose_transpose_370(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_370 */
  /* LITE_KERNEL_SECTION BEGIN gemm_372 */
  {
    
  forward_lite_matmul_gemm_372(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_372 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_373 */
  {
      ai_float* eltwise_373_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* eltwise_373_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 76800);
    const ai_float* eltwise_373_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 161708);
    const ai_float* eltwise_373_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 161828);
  
  _STAI_REATT8_EVENT_NODE_START_CB(373, 1, {(stai_ptr) eltwise_373_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_373_t_out_0_ptr_f32, eltwise_373_t_in_0_ptr_const_f32, eltwise_373_t_weight_0_ptr_const_f32, eltwise_373_t_weight_1_ptr_const_f32, (ai_u32)(3600), (ai_size)(30));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(373, 1, {(stai_ptr) eltwise_373_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_373 */
  /* LITE_KERNEL_SECTION BEGIN nl_375 */
  {
      ai_handle nl_375_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 61440);
    const ai_handle nl_375_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
  
  _STAI_REATT8_EVENT_NODE_START_CB(375, 1, {(stai_ptr) nl_375_t_in_0_ptr_const_handle});
    
  forward_lite_nl_softmax_if32of32(nl_375_t_out_0_ptr_handle, nl_375_t_in_0_ptr_const_handle, nl_375_t_in_0_shape_ch_h_w_prod_const_s32, 1, 30);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(375, 1, {(stai_ptr) nl_375_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_375 */
  /* LITE_KERNEL_SECTION BEGIN gemm_376 */
  {
    
  forward_lite_dense_gemm_376(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_376 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_378 */
  {
    
  forward_lite_eltwise_eltwise_378(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_378 */
  /* LITE_KERNEL_SECTION BEGIN transpose_379 */
  {
    
  forward_lite_transpose_transpose_379(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_379 */
  /* LITE_KERNEL_SECTION BEGIN gemm_380 */
  {
    
  forward_lite_matmul_gemm_380(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_380 */
  /* LITE_KERNEL_SECTION BEGIN transpose_382 */
  {
    
  forward_lite_transpose_transpose_382(net_ctx);
  }
  /* LITE_KERNEL_SECTION END transpose_382 */
  /* LITE_KERNEL_SECTION BEGIN gemm_384 */
  {
    
  forward_lite_dense_gemm_384(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_384 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_385 */
  {
    
  forward_lite_eltwise_eltwise_385(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_385 */
  /* LITE_KERNEL_SECTION BEGIN reduce_386 */
  {
    
  forward_lite_reduce_reduce_386(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_386 */
  /* LITE_KERNEL_SECTION BEGIN reduce_386_Mul */
  {
      ai_float* reduce_386_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30840);
    const ai_float* reduce_386_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 30720);
    const ai_float* reduce_386_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_386_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(386, 1, {(stai_ptr) reduce_386_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_386_Mul_t_out_0_ptr_f32, reduce_386_Mul_t_in_0_ptr_const_f32, reduce_386_Mul_t_weight_0_ptr_const_f32, reduce_386_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(386, 1, {(stai_ptr) reduce_386_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_386_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_388 */
  {
    
  forward_lite_eltwise_eltwise_388(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_388 */
  /* LITE_KERNEL_SECTION BEGIN reduce_389 */
  {
    
  forward_lite_reduce_reduce_389(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_389 */
  /* LITE_KERNEL_SECTION BEGIN reduce_389_Mul */
  {
      ai_float* reduce_389_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 30960);
    const ai_float* reduce_389_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 30720);
    const ai_float* reduce_389_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29600);
    const ai_float* reduce_389_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(390, 1, {(stai_ptr) reduce_389_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_389_Mul_t_out_0_ptr_f32, reduce_389_Mul_t_in_0_ptr_const_f32, reduce_389_Mul_t_weight_0_ptr_const_f32, reduce_389_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(390, 1, {(stai_ptr) reduce_389_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_389_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_391 */
  {
      ai_handle nl_391_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 30720);
    const ai_handle nl_391_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30960);
  
  _STAI_REATT8_EVENT_NODE_START_CB(391, 1, {(stai_ptr) nl_391_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_391_t_out_0_ptr_handle, nl_391_t_in_0_ptr_const_handle, nl_391_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(391, 1, {(stai_ptr) nl_391_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_391 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_392 */
  {
    
  forward_lite_eltwise_eltwise_392(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_392 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_395 */
  {
    
  forward_lite_eltwise_eltwise_395(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_395 */
  /* LITE_KERNEL_SECTION BEGIN nl_387 */
  {
      ai_handle nl_387_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15240);
    const ai_handle nl_387_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30840);
  
  _STAI_REATT8_EVENT_NODE_START_CB(387, 1, {(stai_ptr) nl_387_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_387_t_out_0_ptr_handle, nl_387_t_in_0_ptr_const_handle, nl_387_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(387, 1, {(stai_ptr) nl_387_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_387 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_393 */
  {
    
  forward_lite_eltwise_eltwise_393(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_393 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_394 */
  {
      ai_float* eltwise_394_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 92160);
    const ai_float* eltwise_394_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 92160);
    const ai_float* eltwise_394_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29612);
    const ai_float* eltwise_394_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 2665116);
  
  _STAI_REATT8_EVENT_NODE_START_CB(394, 1, {(stai_ptr) eltwise_394_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_394_t_out_0_ptr_f32, eltwise_394_t_in_0_ptr_const_f32, eltwise_394_t_weight_0_ptr_const_f32, eltwise_394_t_weight_1_ptr_const_f32, (ai_u32)(3840), (ai_size)(128));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(394, 1, {(stai_ptr) eltwise_394_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_394 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_396 */
  {
    
  forward_lite_eltwise_eltwise_396(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_396 */
  /* LITE_KERNEL_SECTION BEGIN gemm_405 */
  {
    
  forward_lite_dense_gemm_405(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_405 */
  /* LITE_KERNEL_SECTION BEGIN nl_408 */
  {
      ai_handle nl_408_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 30720);
    const ai_handle nl_408_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 30720);
  
  _STAI_REATT8_EVENT_NODE_START_CB(408, 1, {(stai_ptr) nl_408_t_in_0_ptr_const_handle});
    
  forward_lite_nl_gelu_if32of32(nl_408_t_out_0_ptr_handle, nl_408_t_in_0_ptr_const_handle, nl_408_t_in_0_shape_ch_h_prod_const_s32, (ai_handle)(nl_408_l_params_const_f32));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(408, 1, {(stai_ptr) nl_408_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_408 */
  /* LITE_KERNEL_SECTION BEGIN gemm_417 */
  {
    
  forward_lite_dense_gemm_417(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_417 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_420 */
  {
    
  forward_lite_eltwise_eltwise_420(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_420 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_1 */
  {
      const ai_float* conv2d_1_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_inputs[0] + 0);
    ai_float* conv2d_1_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15404);
    const ai_u8* conv2d_1_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3194780);
    const ai_u8* conv2d_1_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3199004);
    ai_float* conv2d_1_t_scratch_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
  
  _STAI_REATT8_EVENT_NODE_START_CB(1, 1, {(stai_ptr) conv2d_1_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32(conv2d_1_t_in_0_ptr_const_f32, conv2d_1_t_out_0_ptr_f32, conv2d_1_t_weight_0_ptr_const_u8, conv2d_1_t_weight_1_ptr_const_u8, conv2d_1_t_scratch_0_ptr_f32, conv2d_1_t_in_0_shape_ch_const_u32, conv2d_1_t_out_0_shape_ch_const_u32, conv2d_1_t_in_0_shape_w_const_u32, conv2d_1_t_in_0_shape_h_const_u32, conv2d_1_t_out_0_shape_w_const_u32, conv2d_1_t_out_0_shape_h_const_u32, conv2d_1_t_weight_0_shape_w_const_u32, conv2d_1_t_weight_0_shape_h_const_u32, conv2d_1_l_pad_W_0_const_s32, conv2d_1_l_pad_H_0_const_s32, conv2d_1_l_stride_1_const_u16, conv2d_1_l_stride_0_const_u16, 1, 1, conv2d_1_l_dilation_H_const_u16, conv2d_1_l_dilation_W_const_u16, (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(1, 1, {(stai_ptr) conv2d_1_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_1 */
  /* LITE_KERNEL_SECTION BEGIN pad_3 */
  {
    
  forward_lite_pad_pad_3(net_ctx);
  }
  /* LITE_KERNEL_SECTION END pad_3 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_5 */
  {
      const ai_float* conv2d_5_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 130604);
    ai_float* conv2d_5_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 247724);
    const ai_u8* conv2d_5_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3199388);
    const ai_u8* conv2d_5_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3309980);
    ai_float* conv2d_5_t_scratch_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 246572);
  
  _STAI_REATT8_EVENT_NODE_START_CB(5, 1, {(stai_ptr) conv2d_5_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32(conv2d_5_t_in_0_ptr_const_f32, conv2d_5_t_out_0_ptr_f32, conv2d_5_t_weight_0_ptr_const_u8, conv2d_5_t_weight_1_ptr_const_u8, conv2d_5_t_scratch_0_ptr_f32, conv2d_5_t_in_0_shape_ch_const_u32, conv2d_5_t_out_0_shape_ch_const_u32, conv2d_5_t_in_0_shape_w_const_u32, conv2d_5_t_in_0_shape_h_const_u32, conv2d_5_t_out_0_shape_w_const_u32, conv2d_5_t_out_0_shape_h_const_u32, conv2d_5_t_weight_0_shape_w_const_u32, conv2d_5_t_weight_0_shape_h_const_u32, conv2d_5_l_pad_W_0_const_s32, conv2d_5_l_pad_H_0_const_s32, conv2d_5_l_stride_1_const_u16, conv2d_5_l_stride_0_const_u16, 1, 3, conv2d_5_l_dilation_H_const_u16, conv2d_5_l_dilation_W_const_u16, (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(5, 1, {(stai_ptr) conv2d_5_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_5 */
  /* LITE_KERNEL_SECTION BEGIN reduce_7 */
  {
    
  forward_lite_reduce_reduce_7(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_7 */
  /* LITE_KERNEL_SECTION BEGIN reduce_7_Mul */
  {
      ai_float* reduce_7_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 246524);
    const ai_float* reduce_7_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 130604);
    const ai_float* reduce_7_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310364);
    const ai_float* reduce_7_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(7, 1, {(stai_ptr) reduce_7_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_7_Mul_t_out_0_ptr_f32, reduce_7_Mul_t_in_0_ptr_const_f32, reduce_7_Mul_t_weight_0_ptr_const_f32, reduce_7_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(7, 1, {(stai_ptr) reduce_7_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_7_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_9 */
  {
    
  forward_lite_eltwise_eltwise_9(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_9 */
  /* LITE_KERNEL_SECTION BEGIN reduce_10 */
  {
    
  forward_lite_reduce_reduce_10(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_10 */
  /* LITE_KERNEL_SECTION BEGIN reduce_10_Mul */
  {
      ai_float* reduce_10_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 130604);
    const ai_float* reduce_10_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 362924);
    const ai_float* reduce_10_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310364);
    const ai_float* reduce_10_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(11, 1, {(stai_ptr) reduce_10_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_10_Mul_t_out_0_ptr_f32, reduce_10_Mul_t_in_0_ptr_const_f32, reduce_10_Mul_t_weight_0_ptr_const_f32, reduce_10_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(11, 1, {(stai_ptr) reduce_10_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_10_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_12 */
  {
      ai_handle nl_12_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 362924);
    const ai_handle nl_12_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 130604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(12, 1, {(stai_ptr) nl_12_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_12_t_out_0_ptr_handle, nl_12_t_in_0_ptr_const_handle, nl_12_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(12, 1, {(stai_ptr) nl_12_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_12 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_13 */
  {
    
  forward_lite_eltwise_eltwise_13(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_13 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_16 */
  {
    
  forward_lite_eltwise_eltwise_16(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_16 */
  /* LITE_KERNEL_SECTION BEGIN nl_8 */
  {
      ai_handle nl_8_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 362924);
    const ai_handle nl_8_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 246524);
  
  _STAI_REATT8_EVENT_NODE_START_CB(8, 1, {(stai_ptr) nl_8_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_8_t_out_0_ptr_handle, nl_8_t_in_0_ptr_const_handle, nl_8_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(8, 1, {(stai_ptr) nl_8_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_8 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_14 */
  {
    
  forward_lite_eltwise_eltwise_14(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_14 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_15 */
  {
      ai_float* eltwise_15_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 130604);
    const ai_float* eltwise_15_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 130604);
    const ai_float* eltwise_15_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310368);
    const ai_float* eltwise_15_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310752);
  
  _STAI_REATT8_EVENT_NODE_START_CB(15, 1, {(stai_ptr) eltwise_15_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_15_t_out_0_ptr_f32, eltwise_15_t_in_0_ptr_const_f32, eltwise_15_t_weight_0_ptr_const_f32, eltwise_15_t_weight_1_ptr_const_f32, (ai_u32)(28800), (ai_size)(96));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(15, 1, {(stai_ptr) eltwise_15_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_15 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_17 */
  {
    
  forward_lite_eltwise_eltwise_17(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_17 */
  /* LITE_KERNEL_SECTION BEGIN nl_18 */
  {
      ai_handle nl_18_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 130604);
    const ai_handle nl_18_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 247724);
  
  _STAI_REATT8_EVENT_NODE_START_CB(18, 1, {(stai_ptr) nl_18_t_in_0_ptr_const_handle});
    
  forward_lite_nl_gelu_if32of32(nl_18_t_out_0_ptr_handle, nl_18_t_in_0_ptr_const_handle, nl_18_t_in_0_shape_ch_h_prod_const_s32, (ai_handle)(nl_18_l_params_const_f32));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(18, 1, {(stai_ptr) nl_18_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_18 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_19 */
  {
    
  forward_lite_eltwise_eltwise_19(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_19 */
  /* LITE_KERNEL_SECTION BEGIN pad_20 */
  {
    
  forward_lite_pad_pad_20(net_ctx);
  }
  /* LITE_KERNEL_SECTION END pad_20 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_23 */
  {
      const ai_float* conv2d_23_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    ai_float* conv2d_23_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 361004);
    const ai_u8* conv2d_23_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3311136);
    const ai_u8* conv2d_23_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3421728);
  
  _STAI_REATT8_EVENT_NODE_START_CB(26, 1, {(stai_ptr) conv2d_23_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_23_t_in_0_ptr_const_f32, conv2d_23_t_out_0_ptr_f32, conv2d_23_t_weight_0_ptr_const_u8, conv2d_23_t_weight_1_ptr_const_u8, conv2d_23_t_in_0_shape_ch_const_u32, conv2d_23_t_out_0_shape_ch_const_u32, conv2d_23_t_in_0_shape_w_const_u32, conv2d_23_t_in_0_shape_h_const_u32, conv2d_23_t_out_0_shape_w_const_u32, conv2d_23_t_out_0_shape_h_const_u32, conv2d_23_t_weight_0_shape_w_const_u32, conv2d_23_t_weight_0_shape_h_const_u32, conv2d_23_l_pad_W_0_const_s32, conv2d_23_l_pad_H_0_const_s32, conv2d_23_l_stride_1_const_u16, conv2d_23_l_stride_0_const_u16, 5, 1, conv2d_23_l_dilation_W_const_u16, conv2d_23_l_dilation_H_const_u16, conv2d_23_v_n_groups_const_size);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(26, 1, {(stai_ptr) conv2d_23_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_23 */
  /* LITE_KERNEL_SECTION BEGIN reduce_27 */
  {
    
  forward_lite_reduce_reduce_27(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_27 */
  /* LITE_KERNEL_SECTION BEGIN reduce_27_Mul */
  {
      ai_float* reduce_27_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 16560);
    const ai_float* reduce_27_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* reduce_27_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310364);
    const ai_float* reduce_27_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(27, 1, {(stai_ptr) reduce_27_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_27_Mul_t_out_0_ptr_f32, reduce_27_Mul_t_in_0_ptr_const_f32, reduce_27_Mul_t_weight_0_ptr_const_f32, reduce_27_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(27, 1, {(stai_ptr) reduce_27_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_27_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_29 */
  {
    
  forward_lite_eltwise_eltwise_29(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_29 */
  /* LITE_KERNEL_SECTION BEGIN reduce_30 */
  {
    
  forward_lite_reduce_reduce_30(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_30 */
  /* LITE_KERNEL_SECTION BEGIN reduce_30_Mul */
  {
      ai_float* reduce_30_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 17760);
    const ai_float* reduce_30_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* reduce_30_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310364);
    const ai_float* reduce_30_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(31, 1, {(stai_ptr) reduce_30_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_30_Mul_t_out_0_ptr_f32, reduce_30_Mul_t_in_0_ptr_const_f32, reduce_30_Mul_t_weight_0_ptr_const_f32, reduce_30_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(31, 1, {(stai_ptr) reduce_30_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_30_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_32 */
  {
      ai_handle nl_32_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_32_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 17760);
  
  _STAI_REATT8_EVENT_NODE_START_CB(32, 1, {(stai_ptr) nl_32_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_32_t_out_0_ptr_handle, nl_32_t_in_0_ptr_const_handle, nl_32_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(32, 1, {(stai_ptr) nl_32_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_32 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_33 */
  {
    
  forward_lite_eltwise_eltwise_33(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_33 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_36 */
  {
    
  forward_lite_eltwise_eltwise_36(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_36 */
  /* LITE_KERNEL_SECTION BEGIN nl_28 */
  {
      ai_handle nl_28_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_28_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 16560);
  
  _STAI_REATT8_EVENT_NODE_START_CB(28, 1, {(stai_ptr) nl_28_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_28_t_out_0_ptr_handle, nl_28_t_in_0_ptr_const_handle, nl_28_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(28, 1, {(stai_ptr) nl_28_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_28 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_34 */
  {
    
  forward_lite_eltwise_eltwise_34(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_34 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_35 */
  {
      ai_float* eltwise_35_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 17760);
    const ai_float* eltwise_35_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 17760);
    const ai_float* eltwise_35_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310368);
    const ai_float* eltwise_35_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3422112);
  
  _STAI_REATT8_EVENT_NODE_START_CB(35, 1, {(stai_ptr) eltwise_35_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_35_t_out_0_ptr_f32, eltwise_35_t_in_0_ptr_const_f32, eltwise_35_t_weight_0_ptr_const_f32, eltwise_35_t_weight_1_ptr_const_f32, (ai_u32)(28800), (ai_size)(96));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(35, 1, {(stai_ptr) eltwise_35_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_35 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_37 */
  {
    
  forward_lite_eltwise_eltwise_37(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_37 */
  /* LITE_KERNEL_SECTION BEGIN nl_38 */
  {
      ai_handle nl_38_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_38_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 361004);
  
  _STAI_REATT8_EVENT_NODE_START_CB(38, 1, {(stai_ptr) nl_38_t_in_0_ptr_const_handle});
    
  forward_lite_nl_gelu_if32of32(nl_38_t_out_0_ptr_handle, nl_38_t_in_0_ptr_const_handle, nl_38_t_in_0_shape_ch_h_prod_const_s32, (ai_handle)(nl_38_l_params_const_f32));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(38, 1, {(stai_ptr) nl_38_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_38 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_39 */
  {
    
  forward_lite_eltwise_eltwise_39(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_39 */
  /* LITE_KERNEL_SECTION BEGIN pad_40 */
  {
    
  forward_lite_pad_pad_40(net_ctx);
  }
  /* LITE_KERNEL_SECTION END pad_40 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_43 */
  {
      const ai_float* conv2d_43_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 245760);
    ai_float* conv2d_43_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_u8* conv2d_43_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3422496);
    const ai_u8* conv2d_43_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3533088);
  
  _STAI_REATT8_EVENT_NODE_START_CB(46, 1, {(stai_ptr) conv2d_43_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_43_t_in_0_ptr_const_f32, conv2d_43_t_out_0_ptr_f32, conv2d_43_t_weight_0_ptr_const_u8, conv2d_43_t_weight_1_ptr_const_u8, conv2d_43_t_in_0_shape_ch_const_u32, conv2d_43_t_out_0_shape_ch_const_u32, conv2d_43_t_in_0_shape_w_const_u32, conv2d_43_t_in_0_shape_h_const_u32, conv2d_43_t_out_0_shape_w_const_u32, conv2d_43_t_out_0_shape_h_const_u32, conv2d_43_t_weight_0_shape_w_const_u32, conv2d_43_t_weight_0_shape_h_const_u32, conv2d_43_l_pad_W_0_const_s32, conv2d_43_l_pad_H_0_const_s32, conv2d_43_l_stride_1_const_u16, conv2d_43_l_stride_0_const_u16, 9, 1, conv2d_43_l_dilation_W_const_u16, conv2d_43_l_dilation_H_const_u16, conv2d_43_v_n_groups_const_size);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(46, 1, {(stai_ptr) conv2d_43_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_43 */
  /* LITE_KERNEL_SECTION BEGIN reduce_47 */
  {
    
  forward_lite_reduce_reduce_47(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_47 */
  /* LITE_KERNEL_SECTION BEGIN reduce_47_Mul */
  {
      ai_float* reduce_47_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 246960);
    const ai_float* reduce_47_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 245760);
    const ai_float* reduce_47_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310364);
    const ai_float* reduce_47_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(47, 1, {(stai_ptr) reduce_47_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_47_Mul_t_out_0_ptr_f32, reduce_47_Mul_t_in_0_ptr_const_f32, reduce_47_Mul_t_weight_0_ptr_const_f32, reduce_47_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(47, 1, {(stai_ptr) reduce_47_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_47_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_49 */
  {
    
  forward_lite_eltwise_eltwise_49(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_49 */
  /* LITE_KERNEL_SECTION BEGIN reduce_50 */
  {
    
  forward_lite_reduce_reduce_50(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_50 */
  /* LITE_KERNEL_SECTION BEGIN reduce_50_Mul */
  {
      ai_float* reduce_50_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 248160);
    const ai_float* reduce_50_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 245760);
    const ai_float* reduce_50_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310364);
    const ai_float* reduce_50_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(51, 1, {(stai_ptr) reduce_50_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_50_Mul_t_out_0_ptr_f32, reduce_50_Mul_t_in_0_ptr_const_f32, reduce_50_Mul_t_weight_0_ptr_const_f32, reduce_50_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(51, 1, {(stai_ptr) reduce_50_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_50_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_52 */
  {
      ai_handle nl_52_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 245760);
    const ai_handle nl_52_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 248160);
  
  _STAI_REATT8_EVENT_NODE_START_CB(52, 1, {(stai_ptr) nl_52_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_52_t_out_0_ptr_handle, nl_52_t_in_0_ptr_const_handle, nl_52_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(52, 1, {(stai_ptr) nl_52_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_52 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_53 */
  {
    
  forward_lite_eltwise_eltwise_53(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_53 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_56 */
  {
    
  forward_lite_eltwise_eltwise_56(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_56 */
  /* LITE_KERNEL_SECTION BEGIN nl_48 */
  {
      ai_handle nl_48_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 245760);
    const ai_handle nl_48_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 246960);
  
  _STAI_REATT8_EVENT_NODE_START_CB(48, 1, {(stai_ptr) nl_48_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_48_t_out_0_ptr_handle, nl_48_t_in_0_ptr_const_handle, nl_48_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(48, 1, {(stai_ptr) nl_48_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_48 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_54 */
  {
    
  forward_lite_eltwise_eltwise_54(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_54 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_55 */
  {
      ai_float* eltwise_55_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 248160);
    const ai_float* eltwise_55_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 248160);
    const ai_float* eltwise_55_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310368);
    const ai_float* eltwise_55_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3533472);
  
  _STAI_REATT8_EVENT_NODE_START_CB(55, 1, {(stai_ptr) eltwise_55_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_55_t_out_0_ptr_f32, eltwise_55_t_in_0_ptr_const_f32, eltwise_55_t_weight_0_ptr_const_f32, eltwise_55_t_weight_1_ptr_const_f32, (ai_u32)(28800), (ai_size)(96));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(55, 1, {(stai_ptr) eltwise_55_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_55 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_57 */
  {
    
  forward_lite_eltwise_eltwise_57(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_57 */
  /* LITE_KERNEL_SECTION BEGIN nl_58 */
  {
      ai_handle nl_58_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 245760);
    const ai_handle nl_58_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
  
  _STAI_REATT8_EVENT_NODE_START_CB(58, 1, {(stai_ptr) nl_58_t_in_0_ptr_const_handle});
    
  forward_lite_nl_gelu_if32of32(nl_58_t_out_0_ptr_handle, nl_58_t_in_0_ptr_const_handle, nl_58_t_in_0_shape_ch_h_prod_const_s32, (ai_handle)(nl_58_l_params_const_f32));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(58, 1, {(stai_ptr) nl_58_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_58 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_59 */
  {
    
  forward_lite_eltwise_eltwise_59(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_59 */
  /* LITE_KERNEL_SECTION BEGIN pad_60 */
  {
    
  forward_lite_pad_pad_60(net_ctx);
  }
  /* LITE_KERNEL_SECTION END pad_60 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_63 */
  {
      const ai_float* conv2d_63_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 130560);
    ai_float* conv2d_63_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 251904);
    const ai_u8* conv2d_63_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3533856);
    const ai_u8* conv2d_63_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3644448);
  
  _STAI_REATT8_EVENT_NODE_START_CB(66, 1, {(stai_ptr) conv2d_63_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_63_t_in_0_ptr_const_f32, conv2d_63_t_out_0_ptr_f32, conv2d_63_t_weight_0_ptr_const_u8, conv2d_63_t_weight_1_ptr_const_u8, conv2d_63_t_in_0_shape_ch_const_u32, conv2d_63_t_out_0_shape_ch_const_u32, conv2d_63_t_in_0_shape_w_const_u32, conv2d_63_t_in_0_shape_h_const_u32, conv2d_63_t_out_0_shape_w_const_u32, conv2d_63_t_out_0_shape_h_const_u32, conv2d_63_t_weight_0_shape_w_const_u32, conv2d_63_t_weight_0_shape_h_const_u32, conv2d_63_l_pad_W_0_const_s32, conv2d_63_l_pad_H_0_const_s32, conv2d_63_l_stride_1_const_u16, conv2d_63_l_stride_0_const_u16, 17, 1, conv2d_63_l_dilation_W_const_u16, conv2d_63_l_dilation_H_const_u16, conv2d_63_v_n_groups_const_size);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(66, 1, {(stai_ptr) conv2d_63_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_63 */
  /* LITE_KERNEL_SECTION BEGIN reduce_67 */
  {
    
  forward_lite_reduce_reduce_67(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_67 */
  /* LITE_KERNEL_SECTION BEGIN reduce_67_Mul */
  {
      ai_float* reduce_67_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 131760);
    const ai_float* reduce_67_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 130560);
    const ai_float* reduce_67_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310364);
    const ai_float* reduce_67_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(67, 1, {(stai_ptr) reduce_67_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_67_Mul_t_out_0_ptr_f32, reduce_67_Mul_t_in_0_ptr_const_f32, reduce_67_Mul_t_weight_0_ptr_const_f32, reduce_67_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(67, 1, {(stai_ptr) reduce_67_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_67_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_69 */
  {
    
  forward_lite_eltwise_eltwise_69(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_69 */
  /* LITE_KERNEL_SECTION BEGIN reduce_70 */
  {
    
  forward_lite_reduce_reduce_70(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_70 */
  /* LITE_KERNEL_SECTION BEGIN reduce_70_Mul */
  {
      ai_float* reduce_70_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 132960);
    const ai_float* reduce_70_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 130560);
    const ai_float* reduce_70_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310364);
    const ai_float* reduce_70_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(71, 1, {(stai_ptr) reduce_70_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_70_Mul_t_out_0_ptr_f32, reduce_70_Mul_t_in_0_ptr_const_f32, reduce_70_Mul_t_weight_0_ptr_const_f32, reduce_70_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(71, 1, {(stai_ptr) reduce_70_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_70_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_72 */
  {
      ai_handle nl_72_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 130560);
    const ai_handle nl_72_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 132960);
  
  _STAI_REATT8_EVENT_NODE_START_CB(72, 1, {(stai_ptr) nl_72_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_72_t_out_0_ptr_handle, nl_72_t_in_0_ptr_const_handle, nl_72_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(72, 1, {(stai_ptr) nl_72_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_72 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_73 */
  {
    
  forward_lite_eltwise_eltwise_73(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_73 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_76 */
  {
    
  forward_lite_eltwise_eltwise_76(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_76 */
  /* LITE_KERNEL_SECTION BEGIN nl_68 */
  {
      ai_handle nl_68_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 130560);
    const ai_handle nl_68_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 131760);
  
  _STAI_REATT8_EVENT_NODE_START_CB(68, 1, {(stai_ptr) nl_68_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_68_t_out_0_ptr_handle, nl_68_t_in_0_ptr_const_handle, nl_68_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(68, 1, {(stai_ptr) nl_68_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_68 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_74 */
  {
    
  forward_lite_eltwise_eltwise_74(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_74 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_75 */
  {
      ai_float* eltwise_75_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 132960);
    const ai_float* eltwise_75_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 132960);
    const ai_float* eltwise_75_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310368);
    const ai_float* eltwise_75_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3644832);
  
  _STAI_REATT8_EVENT_NODE_START_CB(75, 1, {(stai_ptr) eltwise_75_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_75_t_out_0_ptr_f32, eltwise_75_t_in_0_ptr_const_f32, eltwise_75_t_weight_0_ptr_const_f32, eltwise_75_t_weight_1_ptr_const_f32, (ai_u32)(28800), (ai_size)(96));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(75, 1, {(stai_ptr) eltwise_75_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_75 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_77 */
  {
    
  forward_lite_eltwise_eltwise_77(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_77 */
  /* LITE_KERNEL_SECTION BEGIN nl_78 */
  {
      ai_handle nl_78_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 130560);
    const ai_handle nl_78_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 251904);
  
  _STAI_REATT8_EVENT_NODE_START_CB(78, 1, {(stai_ptr) nl_78_t_in_0_ptr_const_handle});
    
  forward_lite_nl_gelu_if32of32(nl_78_t_out_0_ptr_handle, nl_78_t_in_0_ptr_const_handle, nl_78_t_in_0_shape_ch_h_prod_const_s32, (ai_handle)(nl_78_l_params_const_f32));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(78, 1, {(stai_ptr) nl_78_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_78 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_79 */
  {
    
  forward_lite_eltwise_eltwise_79(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_79 */
  /* LITE_KERNEL_SECTION BEGIN pad_80 */
  {
    
  forward_lite_pad_pad_80(net_ctx);
  }
  /* LITE_KERNEL_SECTION END pad_80 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_83 */
  {
      const ai_float* conv2d_83_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    ai_float* conv2d_83_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 360960);
    const ai_u8* conv2d_83_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3645216);
    const ai_u8* conv2d_83_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3755808);
  
  _STAI_REATT8_EVENT_NODE_START_CB(86, 1, {(stai_ptr) conv2d_83_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_83_t_in_0_ptr_const_f32, conv2d_83_t_out_0_ptr_f32, conv2d_83_t_weight_0_ptr_const_u8, conv2d_83_t_weight_1_ptr_const_u8, conv2d_83_t_in_0_shape_ch_const_u32, conv2d_83_t_out_0_shape_ch_const_u32, conv2d_83_t_in_0_shape_w_const_u32, conv2d_83_t_in_0_shape_h_const_u32, conv2d_83_t_out_0_shape_w_const_u32, conv2d_83_t_out_0_shape_h_const_u32, conv2d_83_t_weight_0_shape_w_const_u32, conv2d_83_t_weight_0_shape_h_const_u32, conv2d_83_l_pad_W_0_const_s32, conv2d_83_l_pad_H_0_const_s32, conv2d_83_l_stride_1_const_u16, conv2d_83_l_stride_0_const_u16, 33, 1, conv2d_83_l_dilation_W_const_u16, conv2d_83_l_dilation_H_const_u16, conv2d_83_v_n_groups_const_size);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(86, 1, {(stai_ptr) conv2d_83_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_83 */
  /* LITE_KERNEL_SECTION BEGIN reduce_87 */
  {
    
  forward_lite_reduce_reduce_87(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_87 */
  /* LITE_KERNEL_SECTION BEGIN reduce_87_Mul */
  {
      ai_float* reduce_87_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 16560);
    const ai_float* reduce_87_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* reduce_87_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310364);
    const ai_float* reduce_87_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(87, 1, {(stai_ptr) reduce_87_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_87_Mul_t_out_0_ptr_f32, reduce_87_Mul_t_in_0_ptr_const_f32, reduce_87_Mul_t_weight_0_ptr_const_f32, reduce_87_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(87, 1, {(stai_ptr) reduce_87_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_87_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_89 */
  {
    
  forward_lite_eltwise_eltwise_89(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_89 */
  /* LITE_KERNEL_SECTION BEGIN reduce_90 */
  {
    
  forward_lite_reduce_reduce_90(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_90 */
  /* LITE_KERNEL_SECTION BEGIN reduce_90_Mul */
  {
      ai_float* reduce_90_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 17760);
    const ai_float* reduce_90_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_float* reduce_90_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310364);
    const ai_float* reduce_90_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(91, 1, {(stai_ptr) reduce_90_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_90_Mul_t_out_0_ptr_f32, reduce_90_Mul_t_in_0_ptr_const_f32, reduce_90_Mul_t_weight_0_ptr_const_f32, reduce_90_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(91, 1, {(stai_ptr) reduce_90_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_90_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_92 */
  {
      ai_handle nl_92_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_92_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 17760);
  
  _STAI_REATT8_EVENT_NODE_START_CB(92, 1, {(stai_ptr) nl_92_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_92_t_out_0_ptr_handle, nl_92_t_in_0_ptr_const_handle, nl_92_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(92, 1, {(stai_ptr) nl_92_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_92 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_93 */
  {
    
  forward_lite_eltwise_eltwise_93(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_93 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_96 */
  {
    
  forward_lite_eltwise_eltwise_96(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_96 */
  /* LITE_KERNEL_SECTION BEGIN nl_88 */
  {
      ai_handle nl_88_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_88_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 16560);
  
  _STAI_REATT8_EVENT_NODE_START_CB(88, 1, {(stai_ptr) nl_88_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_88_t_out_0_ptr_handle, nl_88_t_in_0_ptr_const_handle, nl_88_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(88, 1, {(stai_ptr) nl_88_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_88 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_94 */
  {
    
  forward_lite_eltwise_eltwise_94(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_94 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_95 */
  {
      ai_float* eltwise_95_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 17760);
    const ai_float* eltwise_95_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 17760);
    const ai_float* eltwise_95_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310368);
    const ai_float* eltwise_95_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3756192);
  
  _STAI_REATT8_EVENT_NODE_START_CB(95, 1, {(stai_ptr) eltwise_95_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_95_t_out_0_ptr_f32, eltwise_95_t_in_0_ptr_const_f32, eltwise_95_t_weight_0_ptr_const_f32, eltwise_95_t_weight_1_ptr_const_f32, (ai_u32)(28800), (ai_size)(96));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(95, 1, {(stai_ptr) eltwise_95_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_95 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_97 */
  {
    
  forward_lite_eltwise_eltwise_97(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_97 */
  /* LITE_KERNEL_SECTION BEGIN nl_98 */
  {
      ai_handle nl_98_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_98_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 360960);
  
  _STAI_REATT8_EVENT_NODE_START_CB(98, 1, {(stai_ptr) nl_98_t_in_0_ptr_const_handle});
    
  forward_lite_nl_gelu_if32of32(nl_98_t_out_0_ptr_handle, nl_98_t_in_0_ptr_const_handle, nl_98_t_in_0_shape_ch_h_prod_const_s32, (ai_handle)(nl_98_l_params_const_f32));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(98, 1, {(stai_ptr) nl_98_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_98 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_99 */
  {
    
  forward_lite_eltwise_eltwise_99(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_99 */
  /* LITE_KERNEL_SECTION BEGIN pad_100 */
  {
    
  forward_lite_pad_pad_100(net_ctx);
  }
  /* LITE_KERNEL_SECTION END pad_100 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_103 */
  {
      const ai_float* conv2d_103_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 245760);
    ai_float* conv2d_103_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15360);
    const ai_u8* conv2d_103_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3756576);
    const ai_u8* conv2d_103_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3867168);
  
  _STAI_REATT8_EVENT_NODE_START_CB(106, 1, {(stai_ptr) conv2d_103_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_103_t_in_0_ptr_const_f32, conv2d_103_t_out_0_ptr_f32, conv2d_103_t_weight_0_ptr_const_u8, conv2d_103_t_weight_1_ptr_const_u8, conv2d_103_t_in_0_shape_ch_const_u32, conv2d_103_t_out_0_shape_ch_const_u32, conv2d_103_t_in_0_shape_w_const_u32, conv2d_103_t_in_0_shape_h_const_u32, conv2d_103_t_out_0_shape_w_const_u32, conv2d_103_t_out_0_shape_h_const_u32, conv2d_103_t_weight_0_shape_w_const_u32, conv2d_103_t_weight_0_shape_h_const_u32, conv2d_103_l_pad_W_0_const_s32, conv2d_103_l_pad_H_0_const_s32, conv2d_103_l_stride_1_const_u16, conv2d_103_l_stride_0_const_u16, 65, 1, conv2d_103_l_dilation_W_const_u16, conv2d_103_l_dilation_H_const_u16, conv2d_103_v_n_groups_const_size);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(106, 1, {(stai_ptr) conv2d_103_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_103 */
  /* LITE_KERNEL_SECTION BEGIN reduce_107 */
  {
    
  forward_lite_reduce_reduce_107(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_107 */
  /* LITE_KERNEL_SECTION BEGIN reduce_107_Mul */
  {
      ai_float* reduce_107_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 246960);
    const ai_float* reduce_107_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 245760);
    const ai_float* reduce_107_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310364);
    const ai_float* reduce_107_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(107, 1, {(stai_ptr) reduce_107_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_107_Mul_t_out_0_ptr_f32, reduce_107_Mul_t_in_0_ptr_const_f32, reduce_107_Mul_t_weight_0_ptr_const_f32, reduce_107_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(107, 1, {(stai_ptr) reduce_107_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_107_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_109 */
  {
    
  forward_lite_eltwise_eltwise_109(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_109 */
  /* LITE_KERNEL_SECTION BEGIN reduce_110 */
  {
    
  forward_lite_reduce_reduce_110(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_110 */
  /* LITE_KERNEL_SECTION BEGIN reduce_110_Mul */
  {
      ai_float* reduce_110_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 248160);
    const ai_float* reduce_110_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 245760);
    const ai_float* reduce_110_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310364);
    const ai_float* reduce_110_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(111, 1, {(stai_ptr) reduce_110_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_110_Mul_t_out_0_ptr_f32, reduce_110_Mul_t_in_0_ptr_const_f32, reduce_110_Mul_t_weight_0_ptr_const_f32, reduce_110_Mul_t_weight_1_ptr_const_f32, (ai_u32)(300), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(111, 1, {(stai_ptr) reduce_110_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_110_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_112 */
  {
      ai_handle nl_112_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 245760);
    const ai_handle nl_112_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 248160);
  
  _STAI_REATT8_EVENT_NODE_START_CB(112, 1, {(stai_ptr) nl_112_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_112_t_out_0_ptr_handle, nl_112_t_in_0_ptr_const_handle, nl_112_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(112, 1, {(stai_ptr) nl_112_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_112 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_113 */
  {
    
  forward_lite_eltwise_eltwise_113(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_113 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_116 */
  {
    
  forward_lite_eltwise_eltwise_116(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_116 */
  /* LITE_KERNEL_SECTION BEGIN nl_108 */
  {
      ai_handle nl_108_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 245760);
    const ai_handle nl_108_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 246960);
  
  _STAI_REATT8_EVENT_NODE_START_CB(108, 1, {(stai_ptr) nl_108_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_108_t_out_0_ptr_handle, nl_108_t_in_0_ptr_const_handle, nl_108_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(108, 1, {(stai_ptr) nl_108_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_108 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_114 */
  {
    
  forward_lite_eltwise_eltwise_114(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_114 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_115 */
  {
      ai_float* eltwise_115_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 248160);
    const ai_float* eltwise_115_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 248160);
    const ai_float* eltwise_115_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3310368);
    const ai_float* eltwise_115_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3867552);
  
  _STAI_REATT8_EVENT_NODE_START_CB(115, 1, {(stai_ptr) eltwise_115_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_115_t_out_0_ptr_f32, eltwise_115_t_in_0_ptr_const_f32, eltwise_115_t_weight_0_ptr_const_f32, eltwise_115_t_weight_1_ptr_const_f32, (ai_u32)(28800), (ai_size)(96));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(115, 1, {(stai_ptr) eltwise_115_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_115 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_117 */
  {
    
  forward_lite_eltwise_eltwise_117(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_117 */
  /* LITE_KERNEL_SECTION BEGIN nl_118 */
  {
      ai_handle nl_118_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 245760);
    const ai_handle nl_118_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
  
  _STAI_REATT8_EVENT_NODE_START_CB(118, 1, {(stai_ptr) nl_118_t_in_0_ptr_const_handle});
    
  forward_lite_nl_gelu_if32of32(nl_118_t_out_0_ptr_handle, nl_118_t_in_0_ptr_const_handle, nl_118_t_in_0_shape_ch_h_prod_const_s32, (ai_handle)(nl_118_l_params_const_f32));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(118, 1, {(stai_ptr) nl_118_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_118 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_119 */
  {
    
  forward_lite_eltwise_eltwise_119(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_119 */
  /* LITE_KERNEL_SECTION BEGIN gemm_128 */
  {
    
  forward_lite_dense_gemm_128(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_128 */
  /* LITE_KERNEL_SECTION BEGIN nl_131 */
  {
      ai_handle nl_131_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 131760);
    const ai_handle nl_131_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 130560);
  
  _STAI_REATT8_EVENT_NODE_START_CB(131, 1, {(stai_ptr) nl_131_t_in_0_ptr_const_handle});
    
  forward_lite_nl_softmax_if32of32(nl_131_t_out_0_ptr_handle, nl_131_t_in_0_ptr_const_handle, nl_131_t_in_0_shape_ch_h_prod_const_s32, 1, 300);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(131, 1, {(stai_ptr) nl_131_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_131 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_133 */
  {
    
  forward_lite_eltwise_eltwise_133(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_133 */
  /* LITE_KERNEL_SECTION BEGIN reduce_134 */
  {
    
  forward_lite_reduce_reduce_134(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_134 */
  /* LITE_KERNEL_SECTION BEGIN tile_137 */
  {
    
  forward_lite_tile_tile_137(net_ctx);
  }
  /* LITE_KERNEL_SECTION END tile_137 */
  /* LITE_KERNEL_SECTION BEGIN concat_421 */
  {
    
  forward_lite_concat_concat_421(net_ctx);
  }
  /* LITE_KERNEL_SECTION END concat_421 */
  /* LITE_KERNEL_SECTION BEGIN reduce_422 */
  {
    
  forward_lite_reduce_reduce_422(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_422 */
  /* LITE_KERNEL_SECTION BEGIN reduce_422_Mul */
  {
      ai_float* reduce_422_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 120);
    const ai_float* reduce_422_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_float* reduce_422_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3868320);
    const ai_float* reduce_422_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29604);
  
  _STAI_REATT8_EVENT_NODE_START_CB(422, 1, {(stai_ptr) reduce_422_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_422_Mul_t_out_0_ptr_f32, reduce_422_Mul_t_in_0_ptr_const_f32, reduce_422_Mul_t_weight_0_ptr_const_f32, reduce_422_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(422, 1, {(stai_ptr) reduce_422_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_422_Mul */
  /* LITE_KERNEL_SECTION BEGIN eltwise_424 */
  {
    
  forward_lite_eltwise_eltwise_424(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_424 */
  /* LITE_KERNEL_SECTION BEGIN reduce_425 */
  {
    
  forward_lite_reduce_reduce_425(net_ctx);
  }
  /* LITE_KERNEL_SECTION END reduce_425 */
  /* LITE_KERNEL_SECTION BEGIN reduce_425_Mul */
  {
      ai_float* reduce_425_Mul_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 240);
    const ai_float* reduce_425_Mul_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_float* reduce_425_Mul_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3868320);
    const ai_float* reduce_425_Mul_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 29608);
  
  _STAI_REATT8_EVENT_NODE_START_CB(426, 1, {(stai_ptr) reduce_425_Mul_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(reduce_425_Mul_t_out_0_ptr_f32, reduce_425_Mul_t_in_0_ptr_const_f32, reduce_425_Mul_t_weight_0_ptr_const_f32, reduce_425_Mul_t_weight_1_ptr_const_f32, (ai_u32)(30), (ai_size)(1));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(426, 1, {(stai_ptr) reduce_425_Mul_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END reduce_425_Mul */
  /* LITE_KERNEL_SECTION BEGIN nl_427 */
  {
      ai_handle nl_427_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 0);
    const ai_handle nl_427_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 240);
  
  _STAI_REATT8_EVENT_NODE_START_CB(427, 1, {(stai_ptr) nl_427_t_in_0_ptr_const_handle});
    
  forward_lite_nl_rsqrt_if32of32(nl_427_t_out_0_ptr_handle, nl_427_t_in_0_ptr_const_handle, nl_427_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(427, 1, {(stai_ptr) nl_427_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_427 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_428 */
  {
    
  forward_lite_eltwise_eltwise_428(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_428 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_431 */
  {
    
  forward_lite_eltwise_eltwise_431(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_431 */
  /* LITE_KERNEL_SECTION BEGIN nl_423 */
  {
      ai_handle nl_423_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 0);
    const ai_handle nl_423_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 120);
  
  _STAI_REATT8_EVENT_NODE_START_CB(423, 1, {(stai_ptr) nl_423_t_in_0_ptr_const_handle});
    
  forward_lite_nl_neg_if32of32(nl_423_t_out_0_ptr_handle, nl_423_t_in_0_ptr_const_handle, nl_423_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(423, 1, {(stai_ptr) nl_423_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_423 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_429 */
  {
    
  forward_lite_eltwise_eltwise_429(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_429 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_430 */
  {
      ai_float* eltwise_430_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_float* eltwise_430_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 27120);
    const ai_float* eltwise_430_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3868324);
    const ai_float* eltwise_430_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 3869220);
  
  _STAI_REATT8_EVENT_NODE_START_CB(430, 1, {(stai_ptr) eltwise_430_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(eltwise_430_t_out_0_ptr_f32, eltwise_430_t_in_0_ptr_const_f32, eltwise_430_t_weight_0_ptr_const_f32, eltwise_430_t_weight_1_ptr_const_f32, (ai_u32)(6720), (ai_size)(224));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(430, 1, {(stai_ptr) eltwise_430_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END eltwise_430 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_432 */
  {
    
  forward_lite_eltwise_eltwise_432(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_432 */
  /* LITE_KERNEL_SECTION BEGIN gemm_441 */
  {
    
  forward_lite_dense_gemm_441(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_441 */
  /* LITE_KERNEL_SECTION BEGIN nl_444 */
  {
      ai_handle nl_444_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_444_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 0);
  
  _STAI_REATT8_EVENT_NODE_START_CB(444, 1, {(stai_ptr) nl_444_t_in_0_ptr_const_handle});
    
  forward_lite_nl_gelu_if32of32(nl_444_t_out_0_ptr_handle, nl_444_t_in_0_ptr_const_handle, nl_444_t_in_0_shape_ch_h_prod_const_s32, (ai_handle)(nl_444_l_params_const_f32));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(444, 1, {(stai_ptr) nl_444_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_444 */
  /* LITE_KERNEL_SECTION BEGIN gemm_453 */
  {
    
  forward_lite_dense_gemm_453(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_453 */
  /* LITE_KERNEL_SECTION BEGIN nl_456 */
  {
      ai_handle nl_456_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 15360);
    const ai_handle nl_456_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 0);
  
  _STAI_REATT8_EVENT_NODE_START_CB(456, 1, {(stai_ptr) nl_456_t_in_0_ptr_const_handle});
    
  forward_lite_nl_gelu_if32of32(nl_456_t_out_0_ptr_handle, nl_456_t_in_0_ptr_const_handle, nl_456_t_in_0_shape_ch_h_prod_const_s32, (ai_handle)(nl_456_l_params_const_f32));
    
  _STAI_REATT8_EVENT_NODE_STOP_CB(456, 1, {(stai_ptr) nl_456_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END nl_456 */
  /* LITE_KERNEL_SECTION BEGIN gemm_465 */
  {
    
  forward_lite_dense_gemm_465(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_465 */
  return net_ctx->_return_code;
}

/*****************************************************************************/
/*  Getters APIs Section  */
STAI_API_ENTRY
stai_size stai_reatt8_get_context_size()
{
  return (stai_size)STAI_REATT8_CONTEXT_SIZE;
}

#if defined(HAVE_REATT8_INFO)
STAI_API_ENTRY
stai_return_code stai_reatt8_get_info(
  stai_network* network,
  stai_network_info* info)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, info==NULL, STAI_ERROR_NETWORK_INVALID_INFO, net_ctx->_return_code)

  // Copy of network info struct
  *info = g_reatt8_info;

  return STAI_SUCCESS;
}
#endif


STAI_API_ENTRY
stai_return_code stai_reatt8_get_activations(
  stai_network* network, stai_ptr* activations, stai_size* n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, !n_activations, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_activations = STAI_REATT8_ACTIVATIONS_NUM;
for (stai_size idx=0; activations && (idx<STAI_REATT8_ACTIVATIONS_NUM); idx++) {
    // get address of the activations buffers
    activations[idx] = net_ctx->_activations[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reatt8_get_weights(
  stai_network* network, stai_ptr* weights, stai_size* n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_weights, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_weights = STAI_REATT8_WEIGHTS_NUM;
for (stai_size idx=0; weights && (idx<STAI_REATT8_WEIGHTS_NUM); idx++) {
    // get address of the weights buffers
    weights[idx] = net_ctx->_weights[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reatt8_get_inputs(
  stai_network* network, stai_ptr* inputs, stai_size* n_inputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_inputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_inputs = STAI_REATT8_IN_NUM;
  for (stai_size idx=0; inputs && (idx<STAI_REATT8_IN_NUM); idx++) {
    inputs[idx] = net_ctx->_inputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reatt8_get_outputs(
  stai_network* network, stai_ptr* outputs, stai_size* n_outputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_outputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_outputs = STAI_REATT8_OUT_NUM;
  for (stai_size idx=0; outputs && (idx<STAI_REATT8_OUT_NUM); idx++) {
    outputs[idx] = net_ctx->_outputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reatt8_get_error(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /* return 1st generated error or STAI_SUCCESS if no errors so far */
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reatt8_get_states(
  stai_network* network, stai_ptr* states, stai_size* n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_states, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  /* get the number of internals states (supporting multi-heap also for internal states) */
  *n_states = STAI_REATT8_STATES_NUM;

  STAI_UNUSED(states)
return net_ctx->_return_code;
}


/*****************************************************************************/
/*  Setters APIs Section  */

STAI_API_ENTRY
stai_return_code stai_reatt8_set_activations(
  stai_network* network,
  const stai_ptr* activations,
  const stai_size n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _activations_alignment[] = STAI_REATT8_ACTIVATIONS_ALIGNMENTS;
  STAI_PRINT("  [stai_reatt8_set_activations] network(%p) activations[%d]: %p\n\n", net_ctx, n_activations, activations)
  _STAI_SET_ERROR(net_ctx, !activations,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_activations!=STAI_REATT8_ACTIVATIONS_NUM,
                  STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_NUM, net_ctx->_return_code)

  for (stai_size idx=0; activations && idx<STAI_REATT8_ACTIVATIONS_NUM; idx++) {
    STAI_PRINT("  activation[%d]: %p\n", idx, activations[idx])
    _STAI_SET_ERROR(net_ctx, activations[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)activations[idx]) & (_activations_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_activations[idx] = activations[idx];
  }_stai_reatt8_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reatt8_set_weights(
  stai_network* network,
  const stai_ptr* weights,
  const stai_size n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _weights_alignment[] = STAI_REATT8_WEIGHTS_ALIGNMENTS;
  _STAI_SET_ERROR(net_ctx, !weights,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_weights!=STAI_REATT8_WEIGHTS_NUM,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_NUM, net_ctx->_return_code)
  for (stai_size idx=0; weights && idx<STAI_REATT8_WEIGHTS_NUM; idx++) {
    STAI_PRINT("  weight[%d]: %p\n", idx, weights[idx])
    _STAI_SET_ERROR(net_ctx, weights[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)weights[idx]) & (_weights_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_weights[idx] = weights[idx];
  }_stai_reatt8_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reatt8_set_inputs(
  stai_network* network,
  const stai_ptr* inputs,
  const stai_size n_inputs)
{
  const uintptr_t _inputs_alignment[] = STAI_REATT8_IN_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !inputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_inputs!=STAI_REATT8_IN_NUM,
                  STAI_ERROR_NETWORK_INVALID_IN_NUM, net_ctx->_return_code)

  for (stai_size idx=0; inputs && idx<STAI_REATT8_IN_NUM; idx++) {
    STAI_PRINT("  input[%d]: %p\n", idx, inputs[idx])
    _STAI_SET_ERROR(net_ctx, inputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)inputs[idx]) & (_inputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_inputs[idx] = inputs[idx];
  }

  _stai_reatt8_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reatt8_set_outputs(
  stai_network* network,
  const stai_ptr* outputs,
  const stai_size n_outputs)
{
  const uintptr_t _outputs_alignment[] = STAI_REATT8_OUT_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !outputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_outputs!=STAI_REATT8_OUT_NUM,
                  STAI_ERROR_NETWORK_INVALID_OUT_NUM, net_ctx->_return_code)

  for (stai_size idx=0; outputs && idx<n_outputs; idx++) {
    STAI_PRINT("  output[%d]: %p\n", idx, outputs[idx])
    _STAI_SET_ERROR(net_ctx, outputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)outputs[idx]) & (_outputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_outputs[idx] = outputs[idx];
  }

  _stai_reatt8_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_reatt8_set_states(
  stai_network* network,
  const stai_ptr* states,
  const stai_size n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  STAI_UNUSED(states)
  STAI_UNUSED(n_states)
_stai_reatt8_check(net_ctx);
  return net_ctx->_return_code;
}

STAI_API_ENTRY
stai_return_code stai_reatt8_set_callback(
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
#undef _STAI_REATT8_EVENT_NODE_START_CB
#undef _STAI_REATT8_EVENT_NODE_STOP_CB
#undef _STAI_REATT8_MODEL_SIGNATURE
#undef _STAI_REATT8_DATETIME
#undef _STAI_REATT8_COMPILE_DATETIME

