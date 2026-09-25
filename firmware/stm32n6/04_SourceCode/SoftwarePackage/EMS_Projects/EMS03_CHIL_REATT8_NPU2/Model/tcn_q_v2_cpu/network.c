/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-09-03T10:20:49+0800
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
#include "network.h"
#include "network_details.h"
#include "network_data.h"
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
#ifndef _STAI_NETWORK_EVENT_NODE_START_CB
  #define _STAI_NETWORK_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
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
  #define _STAI_NETWORK_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_NETWORK_EVENT_NODE_START_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_START_CB */

#ifdef STAI_EVENT_NODE_STOP_CB
#ifndef _STAI_NETWORK_EVENT_NODE_STOP_CB
  #define _STAI_NETWORK_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
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
  #define _STAI_NETWORK_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_NETWORK_EVENT_NODE_STOP_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_STOP_CB */


/*****************************************************************************/
#define _STAI_NETWORK_MODEL_SIGNATURE     "0xdcd11116e320a955dca7f01ca2886d59"
#define _STAI_NETWORK_DATETIME            "2026-09-03T10:20:49+0800"
#define _STAI_NETWORK_COMPILE_DATETIME    __DATE__ " " __TIME__

#define _STAI_CONTEXT_ALIGNMENT        STAI_NETWORK_CONTEXT_ALIGNMENT

/*****************************************************************************/
#define g_network_activations_1     (NULL)




#if defined(HAVE_NETWORK_INFO)
/*****************************************************************************/
static const stai_network_info g_network_info = {
  .model_signature = _STAI_NETWORK_MODEL_SIGNATURE,
  .c_compile_datetime = _STAI_NETWORK_COMPILE_DATETIME,
  .c_model_name = STAI_NETWORK_MODEL_NAME,
  .c_model_datetime = _STAI_NETWORK_DATETIME,
  .c_model_signature = 0x0,
  .runtime_version = STAI_INIT_VERSION(12, 0, 1),
  .tool_version = STAI_INIT_VERSION(4, 0, 1),
  .api_version = STAI_INIT_VERSION(1, 0, 0),
  .n_macc = STAI_NETWORK_MACC_NUM,
  .n_nodes = STAI_NETWORK_NODES_NUM,
  .flags = STAI_NETWORK_FLAGS,
  .n_inputs = STAI_NETWORK_IN_NUM,
  .n_outputs = STAI_NETWORK_OUT_NUM,
  .n_activations = STAI_NETWORK_ACTIVATIONS_NUM,
  .n_weights = STAI_NETWORK_WEIGHTS_NUM,
  .n_states = STAI_NETWORK_STATES_NUM,
  .inputs = (stai_tensor[STAI_NETWORK_IN_NUM]) {
    STAI_INIT_TENSOR(
      STAI_NETWORK_IN_1_NAME,
      STAI_NETWORK_IN_1_FLAGS,
      STAI_NETWORK_IN_1_FORMAT,
      STAI_NETWORK_IN_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 4, 1, 1, 300, 1),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    STAI_INIT_TENSOR(
      STAI_NETWORK_IN_2_NAME,
      STAI_NETWORK_IN_2_FLAGS,
      STAI_NETWORK_IN_2_FORMAT,
      STAI_NETWORK_IN_2_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 2, 1, 2),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
    .outputs = (stai_tensor[STAI_NETWORK_OUT_NUM]) {
    STAI_INIT_TENSOR(
      STAI_NETWORK_OUT_1_NAME,
      STAI_NETWORK_OUT_1_FLAGS,
      STAI_NETWORK_OUT_1_FORMAT,
      STAI_NETWORK_OUT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 2, 1, 3),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .activations = (stai_tensor[STAI_NETWORK_ACTIVATIONS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_ACTIVATION_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_ACTIVATION_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 194356),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_NETWORK_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 611328),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },

  .states = NULL
};
#endif

#define _STAI_CONTEXT_ACQUIRE(_net_ctx, _net_handle) \
  _stai_network_context* _net_ctx = (_stai_network_context*)(_net_handle); \
  STAI_ASSERT(_net_ctx != NULL) \
  _STAI_SET_ERROR(_net_ctx, _net_ctx->_magic != STAI_MAGIC, \
                  STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE, _net_ctx->_return_code)


/*****************************************************************************/
static
void _stai_network_check(_stai_network_context* net_ctx)
{
  stai_size idx;

// Check activations status
  for (idx=0; idx<STAI_NETWORK_ACTIVATIONS_NUM; idx++) {
    if (net_ctx->_activations[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_ACTIVATIONS_NUM) ? STAI_FLAG_ACTIVATIONS : STAI_FLAG_NONE;
// Check inputs status
  for (idx=0; idx<STAI_NETWORK_IN_NUM; idx++) {
    if (net_ctx->_inputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_IN_NUM) ? STAI_FLAG_INPUTS : STAI_FLAG_NONE;

  // Check outputs status
  for (idx=0; idx<STAI_NETWORK_OUT_NUM; idx++) {
    if (net_ctx->_outputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_OUT_NUM) ? STAI_FLAG_OUTPUTS : STAI_FLAG_NONE;

// Check weights status
  for (idx=0; idx<STAI_NETWORK_WEIGHTS_NUM; idx++) {
    if (net_ctx->_weights[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_WEIGHTS_NUM) ? STAI_FLAG_WEIGHTS : STAI_FLAG_NONE;
STAI_PRINT("  [_stai_network_check] flags: 0x%08x\n", net_ctx->_flags)
}


/*****************************************************************************/
STAI_API_ENTRY
stai_return_code stai_network_init(
  stai_network* network)
{
  /* Memory where to store internal context is provided by applications as a raw byte buffer */
  _stai_network_context* net_ctx = (_stai_network_context*)(network);
  net_ctx->_return_code = STAI_SUCCESS;
  STAI_PRINT("[Entering Network Init] network(%p) context_size(%d)\n", net_ctx, (int32_t)sizeof(_stai_network_context))

  _STAI_SET_ERROR(net_ctx, STAI_NETWORK_CONTEXT_SIZE != sizeof(_stai_network_context),
                 STAI_ERROR_NETWORK_INVALID_CONTEXT_SIZE, net_ctx->_return_code)

  {
    const _stai_network_context _network_context = {
      ._magic = STAI_MAGIC,
      ._signature = STAI_NETWORK_MODEL_SIGNATURE,
      ._flags = STAI_NETWORK_FLAGS,
      ._return_code = STAI_SUCCESS,
      ._callback = NULL,
      ._callback_cookie = NULL,
      ._activations = {
      (stai_ptr)g_network_activations_1
      },
      ._weights = {
      (stai_ptr)g_network_weights_array
      },
      ._inputs = {
    NULL,NULL},
      ._outputs = {
    NULL},
    };

    // Deep copy of internal context to opaque buffer provided by app
    *net_ctx = _network_context;

    _stai_network_check(net_ctx);
  }

  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_deinit(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /*  Reset flags to initial state  */
  net_ctx->_flags = STAI_NETWORK_FLAGS;
  return net_ctx->_return_code;
}

/*****************************************************************************/



/* Int quant #0 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conversion_1_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05349510535597801f),
    AI_PACK_INTQ_ZP(-19)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_43_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.027194013819098473f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_43_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 8,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.006180114112794399f, 0.007221273612231016f, 0.0032316341530531645f, 0.0037314442452043295f, 0.004394275136291981f, 0.004253636579960585f, 0.004510869737714529f, 0.0041366154327988625f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conv2d_6_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03341173753142357f),
    AI_PACK_INTQ_ZP(-107)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conv2d_5_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.031269706785678864f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(eltwise_7_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.030721191316843033f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conv2d_11_0_1_eltwise_12_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04149854928255081f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(eltwise_12_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04149854928255081f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conv2d_16_0_1_eltwise_17_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04149854928255081f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(eltwise_17_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04149854928255081f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conv2d_21_0_1_eltwise_22_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.062476057559251785f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #11 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(eltwise_22_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.062476057559251785f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #12 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conv2d_26_0_1_eltwise_27_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06406153738498688f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #13 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(eltwise_27_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06406153738498688f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #14 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conv2d_31_0_1_eltwise_32_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.07169536501169205f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #15 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(eltwise_32_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.07169536501169205f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #16 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conv2d_36_0_1_eltwise_37_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.07169536501169205f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #17 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(eltwise_37_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.027194013819098473f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #18 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(slice_42_gather_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.027194013819098473f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #19 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(concat_44_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.027194013819098473f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #20 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_45_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.019172484055161476f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #21 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_45_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 64,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0018804536666721106f, 0.007907225750386715f, 0.0038897874765098095f, 0.0017939119134098291f, 0.0026874816976487637f, 0.003412490477785468f, 0.0021075611002743244f, 0.0017973720096051693f, 0.0019691763445734978f, 0.0034387214109301567f, 0.001721368171274662f, 0.002032086020335555f, 0.001997019164264202f, 0.0030254959128797054f, 0.0075494409538805485f, 0.0019303770968690515f, 0.005544832441955805f, 0.002963840030133724f, 0.001848953659646213f, 0.0029770121909677982f, 0.00211571273393929f, 0.0049874610267579556f, 0.0029081027023494244f, 0.0020648327190428972f, 0.002508506877347827f, 0.0020116488449275494f, 0.0030960123986005783f, 0.001969671342521906f, 0.0028675859794020653f, 0.003561110934242606f, 0.001916574197821319f, 0.002451414940878749f, 0.001955329440534115f, 0.005060858093202114f, 0.0030539187137037516f, 0.004753186833113432f, 0.0022388987708836794f, 0.003944575786590576f, 0.003941381350159645f, 0.00239559100009501f, 0.002327975118532777f, 0.002670065499842167f, 0.0018789301393553615f, 0.002157603856176138f, 0.0019889932591468096f, 0.0020554910879582167f, 0.002780524780973792f, 0.0018628978868946433f, 0.004767667967826128f, 0.002005447167903185f, 0.0017621717415750027f, 0.004580573178827763f, 0.0020781296771019697f, 0.0030445170123130083f, 0.005073487292975187f, 0.001869745901785791f, 0.003490005386993289f, 0.0033276083413511515f, 0.0027453508228063583f, 0.002654244191944599f, 0.00179776968434453f, 0.00541273457929492f, 0.002407558960840106f, 0.001639964641071856f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #22 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_46_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.016138950362801552f),
    AI_PACK_INTQ_ZP(-28)))

/* Int quant #23 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_46_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 3,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.002901462372392416f, 0.002713096560910344f, 0.006440502125769854f),
    AI_PACK_INTQ_ZP(0, 0, 0)))



/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  conversion_1_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  gemm_43_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 8, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  gemm_43_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 16, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  gemm_43_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 8, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  gemm_43_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 48, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_6_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_5_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_7_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_11_0_1_eltwise_12_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_12_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_16_0_1_eltwise_17_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_17_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#12 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_21_0_1_eltwise_22_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#13 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_22_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#14 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_26_0_1_eltwise_27_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#15 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_27_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_31_0_1_eltwise_32_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_32_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  conv2d_36_0_1_eltwise_37_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  eltwise_37_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 19200, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  slice_42_gather_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 64, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  slice_42_gather_0_placeholder_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#22 */
AI_ARRAY_OBJ_DECLARE(
  concat_44_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 72, AI_STATIC)

/* Array#23 */
AI_ARRAY_OBJ_DECLARE(
  gemm_45_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 64, AI_STATIC)

/* Array#24 */
AI_ARRAY_OBJ_DECLARE(
  gemm_45_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#25 */
AI_ARRAY_OBJ_DECLARE(
  gemm_45_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 64, AI_STATIC)

/* Array#26 */
AI_ARRAY_OBJ_DECLARE(
  gemm_45_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 384, AI_STATIC)

/* Array#27 */
AI_ARRAY_OBJ_DECLARE(
  gemm_46_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3, AI_STATIC)

/* Array#28 */
AI_ARRAY_OBJ_DECLARE(
  gemm_46_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 192, AI_STATIC)

/* Array#29 */
AI_ARRAY_OBJ_DECLARE(
  gemm_46_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 3, AI_STATIC)

/* Array#30 */
AI_ARRAY_OBJ_DECLARE(
  gemm_46_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 18, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  conversion_1_output, AI_STATIC,
  62, 0x1,
  AI_SHAPE_INIT(4, 1, 2, 1, 1), AI_STRIDE_INIT(4, 1, 1, 2, 2),
  1, &conversion_1_output_array, &conversion_1_output_array_intq)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  gemm_43_bias, AI_STATIC,
  72, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 1), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &gemm_43_bias_array, NULL)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  gemm_43_output, AI_STATIC,
  73, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 1, 1), AI_STRIDE_INIT(4, 1, 1, 8, 8),
  1, &gemm_43_output_array, &gemm_43_output_array_intq)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  gemm_43_scratch0, AI_STATIC,
  74, 0x0,
  AI_SHAPE_INIT(4, 1, 48, 1, 1), AI_STRIDE_INIT(4, 2, 2, 96, 96),
  1, &gemm_43_scratch0_array, NULL)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  gemm_43_weights, AI_STATIC,
  75, 0x1,
  AI_SHAPE_INIT(4, 2, 8, 1, 1), AI_STRIDE_INIT(4, 1, 2, 16, 16),
  1, &gemm_43_weights_array, &gemm_43_weights_array_intq)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_5_output, AI_STATIC,
  50, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &conv2d_5_output_array, &conv2d_5_output_array_intq)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_6_output, AI_STATIC,
  54, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &conv2d_6_output_array, &conv2d_6_output_array_intq)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_7_output, AI_STATIC,
  71, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &eltwise_7_output_array, &eltwise_7_output_array_intq)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_11_0_1_eltwise_12_conversion_output, AI_STATIC,
  1, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &conv2d_11_0_1_eltwise_12_conversion_output_array, &conv2d_11_0_1_eltwise_12_conversion_output_array_intq)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_12_output, AI_STATIC,
  64, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &eltwise_12_output_array, &eltwise_12_output_array_intq)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_16_0_1_eltwise_17_conversion_output, AI_STATIC,
  9, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &conv2d_16_0_1_eltwise_17_conversion_output_array, &conv2d_16_0_1_eltwise_17_conversion_output_array_intq)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_17_output, AI_STATIC,
  65, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &eltwise_17_output_array, &eltwise_17_output_array_intq)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_21_0_1_eltwise_22_conversion_output, AI_STATIC,
  17, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &conv2d_21_0_1_eltwise_22_conversion_output_array, &conv2d_21_0_1_eltwise_22_conversion_output_array_intq)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_22_output, AI_STATIC,
  66, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &eltwise_22_output_array, &eltwise_22_output_array_intq)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_26_0_1_eltwise_27_conversion_output, AI_STATIC,
  25, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &conv2d_26_0_1_eltwise_27_conversion_output_array, &conv2d_26_0_1_eltwise_27_conversion_output_array_intq)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_27_output, AI_STATIC,
  67, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &eltwise_27_output_array, &eltwise_27_output_array_intq)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_31_0_1_eltwise_32_conversion_output, AI_STATIC,
  33, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &conv2d_31_0_1_eltwise_32_conversion_output_array, &conv2d_31_0_1_eltwise_32_conversion_output_array_intq)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_32_output, AI_STATIC,
  68, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &eltwise_32_output_array, &eltwise_32_output_array_intq)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  conv2d_36_0_1_eltwise_37_conversion_output, AI_STATIC,
  41, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &conv2d_36_0_1_eltwise_37_conversion_output_array, &conv2d_36_0_1_eltwise_37_conversion_output_array_intq)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_37_output, AI_STATIC,
  69, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 300, 1), AI_STRIDE_INIT(4, 1, 1, 64, 19200),
  1, &eltwise_37_output_array, &eltwise_37_output_array_intq)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  eltwise_37_output0, AI_STATIC,
  70, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 300), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &eltwise_37_output_array, &eltwise_37_output_array_intq)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  slice_42_gather_0_output, AI_STATIC,
  112, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &slice_42_gather_0_output_array, &slice_42_gather_0_output_array_intq)

/* Tensor #22 */
AI_TENSOR_OBJ_DECLARE(
  slice_42_gather_0_placeholder, AI_STATIC,
  113, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &slice_42_gather_0_placeholder_array, NULL)

/* Tensor #23 */
AI_TENSOR_OBJ_DECLARE(
  concat_44_output, AI_STATIC,
  0, 0x1,
  AI_SHAPE_INIT(4, 1, 72, 1, 1), AI_STRIDE_INIT(4, 1, 1, 72, 72),
  1, &concat_44_output_array, &concat_44_output_array_intq)

/* Tensor #24 */
AI_TENSOR_OBJ_DECLARE(
  gemm_45_bias, AI_STATIC,
  76, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &gemm_45_bias_array, NULL)

/* Tensor #25 */
AI_TENSOR_OBJ_DECLARE(
  gemm_45_output, AI_STATIC,
  77, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &gemm_45_output_array, &gemm_45_output_array_intq)

/* Tensor #26 */
AI_TENSOR_OBJ_DECLARE(
  gemm_45_scratch0, AI_STATIC,
  78, 0x0,
  AI_SHAPE_INIT(4, 1, 384, 1, 1), AI_STRIDE_INIT(4, 2, 2, 768, 768),
  1, &gemm_45_scratch0_array, NULL)

/* Tensor #27 */
AI_TENSOR_OBJ_DECLARE(
  gemm_45_weights, AI_STATIC,
  79, 0x1,
  AI_SHAPE_INIT(4, 72, 64, 1, 1), AI_STRIDE_INIT(4, 1, 72, 4608, 4608),
  1, &gemm_45_weights_array, &gemm_45_weights_array_intq)

/* Tensor #28 */
AI_TENSOR_OBJ_DECLARE(
  gemm_46_bias, AI_STATIC,
  80, 0x0,
  AI_SHAPE_INIT(4, 1, 3, 1, 1), AI_STRIDE_INIT(4, 4, 4, 12, 12),
  1, &gemm_46_bias_array, NULL)

/* Tensor #29 */
AI_TENSOR_OBJ_DECLARE(
  gemm_46_output, AI_STATIC,
  81, 0x1,
  AI_SHAPE_INIT(4, 1, 3, 1, 1), AI_STRIDE_INIT(4, 1, 1, 3, 3),
  1, &gemm_46_output_array, &gemm_46_output_array_intq)

/* Tensor #30 */
AI_TENSOR_OBJ_DECLARE(
  gemm_46_scratch0, AI_STATIC,
  82, 0x0,
  AI_SHAPE_INIT(4, 1, 18, 1, 1), AI_STRIDE_INIT(4, 2, 2, 36, 36),
  1, &gemm_46_scratch0_array, NULL)

/* Tensor #31 */
AI_TENSOR_OBJ_DECLARE(
  gemm_46_weights, AI_STATIC,
  83, 0x1,
  AI_SHAPE_INIT(4, 64, 3, 1, 1), AI_STRIDE_INIT(4, 1, 64, 192, 192),
  1, &gemm_46_weights_array, &gemm_46_weights_array_intq)


AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_43_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conversion_1_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_43_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_43_weights, &gemm_43_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_43_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  gemm_43_layer, 43,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &gemm_43_chain,
  NULL, &gemm_43_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_7_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &conv2d_6_output, &conv2d_5_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_7_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_7_layer, 7,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &eltwise_7_chain,
  NULL, &eltwise_7_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_12_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_7_output, &conv2d_11_0_1_eltwise_12_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_12_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_12_layer, 12,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &eltwise_12_chain,
  NULL, &eltwise_12_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_17_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_12_output, &conv2d_16_0_1_eltwise_17_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_17_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_17_layer, 17,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &eltwise_17_chain,
  NULL, &eltwise_17_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_22_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_17_output, &conv2d_21_0_1_eltwise_22_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_22_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_22_layer, 22,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &eltwise_22_chain,
  NULL, &eltwise_22_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_27_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_22_output, &conv2d_26_0_1_eltwise_27_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_27_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_27_layer, 27,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &eltwise_27_chain,
  NULL, &eltwise_27_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_32_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_27_output, &conv2d_31_0_1_eltwise_32_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_32_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_32_layer, 32,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &eltwise_32_chain,
  NULL, &eltwise_32_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  eltwise_37_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_32_output, &conv2d_36_0_1_eltwise_37_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &eltwise_37_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  eltwise_37_layer, 37,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &eltwise_37_chain,
  NULL, &eltwise_37_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  slice_42_gather_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &eltwise_37_output0, &slice_42_gather_0_placeholder),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &slice_42_gather_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  slice_42_gather_0_layer, 42,
  GATHER_TYPE, 0x0, NULL,
  gather, forward_gather,
  &slice_42_gather_0_chain,
  NULL, &slice_42_gather_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_HEIGHT, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  concat_44_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &slice_42_gather_0_output, &gemm_43_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &concat_44_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  concat_44_layer, 44,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &concat_44_chain,
  NULL, &concat_44_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_45_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &concat_44_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_45_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_45_weights, &gemm_45_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_45_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  gemm_45_layer, 45,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &gemm_45_chain,
  NULL, &gemm_45_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_46_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_45_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_46_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_46_weights, &gemm_46_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_46_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  gemm_46_layer, 46,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &gemm_46_chain,
  NULL, &gemm_46_layer, AI_STATIC, 
)
/**  Hybrid layers declarations section  *************************************/
void forward_lite_dense_integer_SSSA_ch_gemm_43(_stai_network_context* net_ctx)
{
  conversion_1_output_array.data = AI_PTR(net_ctx->_activations[0] + 96008);
  conversion_1_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96008);
  gemm_43_weights_array.data = AI_PTR(net_ctx->_weights[0] + 4);
  gemm_43_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 4);
  gemm_43_bias_array.data = AI_PTR(net_ctx->_weights[0] + 20);
  gemm_43_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 20);
  gemm_43_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 96012);
  gemm_43_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 96012);
  gemm_43_output_array.data = AI_PTR(net_ctx->_activations[0] + 96000);
  gemm_43_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96000);
  _STAI_NETWORK_EVENT_NODE_START_CB(43, 1, { conversion_1_output.data->data});
  forward_dense_integer_SSSA_ch(&gemm_43_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(43, 1, { gemm_43_output.data->data});
}
void forward_lite_eltwise_integer_INT8_eltwise_7(_stai_network_context* net_ctx)
{
  conv2d_6_output_array.data = AI_PTR(net_ctx->_activations[0] + 96308);
  conv2d_6_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96308);
  conv2d_5_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  conv2d_5_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_7_output_array.data = AI_PTR(net_ctx->_activations[0] + 96308);
  eltwise_7_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96308);
  _STAI_NETWORK_EVENT_NODE_START_CB(7, 2, { conv2d_6_output.data->data,conv2d_5_output.data->data});
  forward_eltwise_integer_INT8(&eltwise_7_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(7, 1, { eltwise_7_output.data->data});
}
void forward_lite_eltwise_integer_INT8_eltwise_12(_stai_network_context* net_ctx)
{
  eltwise_7_output_array.data = AI_PTR(net_ctx->_activations[0] + 96308);
  eltwise_7_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96308);
  conv2d_11_0_1_eltwise_12_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 19200);
  conv2d_11_0_1_eltwise_12_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19200);
  eltwise_12_output_array.data = AI_PTR(net_ctx->_activations[0] + 96308);
  eltwise_12_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96308);
  _STAI_NETWORK_EVENT_NODE_START_CB(12, 2, { eltwise_7_output.data->data,conv2d_11_0_1_eltwise_12_conversion_output.data->data});
  forward_eltwise_integer_INT8(&eltwise_12_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(12, 1, { eltwise_12_output.data->data});
}
void forward_lite_eltwise_integer_INT8_eltwise_17(_stai_network_context* net_ctx)
{
  eltwise_12_output_array.data = AI_PTR(net_ctx->_activations[0] + 96308);
  eltwise_12_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96308);
  conv2d_16_0_1_eltwise_17_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 18944);
  conv2d_16_0_1_eltwise_17_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 18944);
  eltwise_17_output_array.data = AI_PTR(net_ctx->_activations[0] + 175156);
  eltwise_17_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 175156);
  _STAI_NETWORK_EVENT_NODE_START_CB(17, 2, { eltwise_12_output.data->data,conv2d_16_0_1_eltwise_17_conversion_output.data->data});
  forward_eltwise_integer_INT8(&eltwise_17_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(17, 1, { eltwise_17_output.data->data});
}
void forward_lite_eltwise_integer_INT8_eltwise_22(_stai_network_context* net_ctx)
{
  eltwise_17_output_array.data = AI_PTR(net_ctx->_activations[0] + 175156);
  eltwise_17_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 175156);
  conv2d_21_0_1_eltwise_22_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 15104);
  conv2d_21_0_1_eltwise_22_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 15104);
  eltwise_22_output_array.data = AI_PTR(net_ctx->_activations[0] + 96008);
  eltwise_22_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96008);
  _STAI_NETWORK_EVENT_NODE_START_CB(22, 2, { eltwise_17_output.data->data,conv2d_21_0_1_eltwise_22_conversion_output.data->data});
  forward_eltwise_integer_INT8(&eltwise_22_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(22, 1, { eltwise_22_output.data->data});
}
void forward_lite_eltwise_integer_INT8_eltwise_27(_stai_network_context* net_ctx)
{
  eltwise_22_output_array.data = AI_PTR(net_ctx->_activations[0] + 96008);
  eltwise_22_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96008);
  conv2d_26_0_1_eltwise_27_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 11008);
  conv2d_26_0_1_eltwise_27_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 11008);
  eltwise_27_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_27_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  _STAI_NETWORK_EVENT_NODE_START_CB(27, 2, { eltwise_22_output.data->data,conv2d_26_0_1_eltwise_27_conversion_output.data->data});
  forward_eltwise_integer_INT8(&eltwise_27_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(27, 1, { eltwise_27_output.data->data});
}
void forward_lite_eltwise_integer_INT8_eltwise_32(_stai_network_context* net_ctx)
{
  eltwise_27_output_array.data = AI_PTR(net_ctx->_activations[0] + 76800);
  eltwise_27_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76800);
  conv2d_31_0_1_eltwise_32_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 96008);
  conv2d_31_0_1_eltwise_32_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96008);
  eltwise_32_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  eltwise_32_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(32, 2, { eltwise_27_output.data->data,conv2d_31_0_1_eltwise_32_conversion_output.data->data});
  forward_eltwise_integer_INT8(&eltwise_32_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(32, 1, { eltwise_32_output.data->data});
}
void forward_lite_eltwise_integer_INT8_eltwise_37(_stai_network_context* net_ctx)
{
  eltwise_32_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  eltwise_32_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  conv2d_36_0_1_eltwise_37_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 96008);
  conv2d_36_0_1_eltwise_37_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96008);
  eltwise_37_output_array.data = AI_PTR(net_ctx->_activations[0] + 19200);
  eltwise_37_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19200);
  _STAI_NETWORK_EVENT_NODE_START_CB(37, 2, { eltwise_32_output.data->data,conv2d_36_0_1_eltwise_37_conversion_output.data->data});
  forward_eltwise_integer_INT8(&eltwise_37_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(37, 1, { eltwise_37_output.data->data});
}
void forward_lite_gather_slice_42_gather_0(_stai_network_context* net_ctx)
{
  eltwise_37_output_array.data = AI_PTR(net_ctx->_activations[0] + 19200);
  eltwise_37_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19200);
  slice_42_gather_0_placeholder_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  slice_42_gather_0_placeholder_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  slice_42_gather_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  slice_42_gather_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(42, 2, { eltwise_37_output0.data->data,slice_42_gather_0_placeholder.data->data});
  forward_gather(&slice_42_gather_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(42, 1, { slice_42_gather_0_output.data->data});
}
void forward_lite_concat_concat_44(_stai_network_context* net_ctx)
{
  slice_42_gather_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  slice_42_gather_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  gemm_43_output_array.data = AI_PTR(net_ctx->_activations[0] + 96000);
  gemm_43_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96000);
  concat_44_output_array.data = AI_PTR(net_ctx->_activations[0] + 64);
  concat_44_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64);
  _STAI_NETWORK_EVENT_NODE_START_CB(44, 2, { slice_42_gather_0_output.data->data,gemm_43_output.data->data});
  forward_concat(&concat_44_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(44, 1, { concat_44_output.data->data});
}
void forward_lite_dense_integer_SSSA_ch_gemm_45(_stai_network_context* net_ctx)
{
  concat_44_output_array.data = AI_PTR(net_ctx->_activations[0] + 64);
  concat_44_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64);
  gemm_45_weights_array.data = AI_PTR(net_ctx->_weights[0] + 606260);
  gemm_45_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 606260);
  gemm_45_bias_array.data = AI_PTR(net_ctx->_weights[0] + 610868);
  gemm_45_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 610868);
  gemm_45_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 136);
  gemm_45_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 136);
  gemm_45_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  gemm_45_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(45, 1, { concat_44_output.data->data});
  forward_dense_integer_SSSA_ch(&gemm_45_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(45, 1, { gemm_45_output.data->data});
}
void forward_lite_dense_integer_SSSA_ch_gemm_46(_stai_network_context* net_ctx)
{
  gemm_45_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  gemm_45_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  gemm_46_weights_array.data = AI_PTR(net_ctx->_weights[0] + 611124);
  gemm_46_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 611124);
  gemm_46_bias_array.data = AI_PTR(net_ctx->_weights[0] + 611316);
  gemm_46_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 611316);
  gemm_46_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 64);
  gemm_46_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 64);
  gemm_46_output_array.data = AI_PTR(net_ctx->_activations[0] + 100);
  gemm_46_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 100);
  _STAI_NETWORK_EVENT_NODE_START_CB(46, 1, { gemm_45_output.data->data});
  forward_dense_integer_SSSA_ch(&gemm_46_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(46, 1, { gemm_46_output.data->data});
}

/*****************************************************************************/


static const ai_u32 conversion_1_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2;
static const ai_float conversion_1_t_out_0_fmt_scale_const_f32 = 0.05349510535597801f;
static const ai_i8 conversion_1_t_out_0_fmt_zero_const_s8 = -19;


static const ai_u32 conversion_0_t_out_0_shape_h_w_ch_d_prod_const_u32 = 300;
static const ai_float conversion_0_t_out_0_fmt_scale_const_f32 = 0.012579751200973988f;
static const ai_i8 conversion_0_t_out_0_fmt_zero_const_s8 = -22;

static const ai_u16 conv2d_6_t_in_0_shape_w_const_u16 = 300;
static const ai_u16 conv2d_6_t_in_0_shape_h_const_u16 = 1;
static const ai_u16 conv2d_6_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_6_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_6_t_in_0_shape_ch_const_u16 = 1;
static const ai_u16 conv2d_6_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 conv2d_6_t_in_0_fmt_zero_const_s8 = -22;
static const ai_i8 conv2d_6_t_out_0_fmt_zero_const_s8 = -107;
static const ai_float conv2d_6_t_in_0_fmt_scale_const_f32 = 0.012579751200973988f;
static const ai_float conv2d_6_t_out_0_fmt_scale_const_f32 = 0.03341173753142357f;
static const ai_float conv2d_6_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.001797228236682713f, 2.5451572582824156e-05f, 0.00024340952222701162f, 0.00131616136059165f, 2.253688506925755e-07f, 0.0019997551571577787f, 0.0020435273181647062f, 9.288552973885089e-05f, 0.0020427170675247908f, 0.0014842926757410169f, 7.130607264116406e-05f, 0.0012825476005673409f, 0.0003931044484488666f, 0.002018793486058712f, 0.0003108655218966305f, 0.001200878294184804f, 0.0013368240324780345f, 0.0001887075777631253f, 0.0005726626259274781f, 0.00011936483497265726f, 7.111360173439607e-05f, 0.0012222708901390433f, 0.0017953179776668549f, 0.0014124993467703462f, 0.0019398242002353072f, 0.001454244600608945f, 0.0015812527853995562f, 0.0006625863024964929f, 0.0012924147304147482f, 0.0017546038143336773f, 0.001678220578469336f, 0.0014031233731657267f, 0.002204571384936571f, 0.0005530276685021818f, 0.002195003442466259f, 0.0015089379157871008f, 0.001052616978995502f, 0.0006814090884290636f, 0.000930320646148175f, 0.0009631876600906253f, 0.0005505313165485859f, 0.002080982318148017f, 0.0016074617160484195f, 0.0021163267083466053f, 0.0010291149374097586f, 0.00011757648462662473f, 4.5375898480415344e-05f, 0.0008655866258777678f, 0.0013657348463311791f, 0.0015708130085840821f, 0.002318238839507103f, 0.0007368872757069767f, 0.0010277978144586086f, 0.0005971427308395505f, 0.0004166933649685234f, 0.0014457618817687035f, 0.00014203289174474776f, 0.0018939947476610541f, 0.0012758388184010983f, 0.0003684713738039136f, 0.002181417541578412f, 1.813026756281033e-05f, 0.0008988152840174735f, 0.00034482296905480325f);
static const ai_layer_format_type conv2d_6_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_i8 pad_2_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-22);
static const ai_i16 pad_2_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_2_t_in_0_shape_h_const_u32 = 1;

static const ai_u16 conv2d_3_t_in_0_shape_w_const_u16 = 302;
static const ai_u16 conv2d_3_t_in_0_shape_h_const_u16 = 1;
static const ai_u16 conv2d_3_t_in_0_shape_ch_const_u16 = 1;
static const ai_u16 conv2d_3_t_out_0_shape_ch_const_u16 = 64;
static const ai_u16 conv2d_3_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 conv2d_3_t_weight_0_shape_h_const_u16 = 1;
static const ai_u16 conv2d_3_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_3_l_stride_0_const_u16 = 1;
static const ai_i32 conv2d_3_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_3_l_pad_H_0_const_s32 = 0;
static const ai_i8 conv2d_3_t_in_0_fmt_zero_const_s8 = -22;
static const ai_i8 conv2d_3_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float conv2d_3_t_in_0_fmt_scale_const_f32 = 0.012579751200973988f;
static const ai_float conv2d_3_t_out_0_fmt_scale_const_f32 = 0.026113374158740044f;
static const ai_float conv2d_3_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.009823376312851906f, 0.0055273957550525665f, 0.006407277192920446f, 0.011162472888827324f, 0.007988782599568367f, 0.007643309887498617f, 0.00997073482722044f, 0.009490877389907837f, 0.006559113506227732f, 0.012023583985865116f, 0.009581870399415493f, 0.01276970375329256f, 0.006444656755775213f, 0.009637346491217613f, 0.010736140422523022f, 0.013760359957814217f, 0.009189346805214882f, 0.008896582759916782f, 0.010587251745164394f, 0.0065420521423220634f, 0.003036010777577758f, 0.011699887923896313f, 0.012075772508978844f, 0.008068344555795193f, 0.005271521396934986f, 0.008367081172764301f, 0.013096961192786694f, 0.007501014042645693f, 0.011315219104290009f, 0.012349088676273823f, 0.0035697384737432003f, 0.010482716374099255f, 0.009767767041921616f, 0.007853521034121513f, 0.007826747372746468f, 0.013934100046753883f, 0.006459246389567852f, 0.003354125190526247f, 0.009339078329503536f, 0.008143494836986065f, 0.001995699480175972f, 0.00458244513720274f, 0.002485485514625907f, 0.005199138540774584f, 0.008530506864190102f, 0.013473894447088242f, 0.006380925420671701f, 0.008826109580695629f, 0.010333940386772156f, 0.011901087127625942f, 0.007609182503074408f, 0.011984487995505333f, 0.011615946888923645f, 0.002729451283812523f, 0.006017705425620079f, 0.005318791139870882f, 0.00240108510479331f, 0.0078553082421422f, 0.009624467231333256f, 0.01313965953886509f, 0.006641831714659929f, 0.010138648562133312f, 0.01315376814454794f, 0.009113505482673645f);
static const ai_layer_format_type conv2d_3_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 conv2d_3_t_out_0_shape_w_const_u16 = 300;
static const ai_u16 conv2d_3_t_out_0_shape_h_const_u16 = 1;

static const ai_i8 pad_4_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_4_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_4_t_in_0_shape_h_const_u32 = 1;

static const ai_u16 conv2d_5_t_in_0_shape_w_const_u16 = 302;
static const ai_u16 conv2d_5_t_in_0_shape_h_const_u16 = 1;
static const ai_u16 conv2d_5_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 conv2d_5_t_out_0_shape_ch_const_u16 = 64;
static const ai_u16 conv2d_5_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 conv2d_5_t_weight_0_shape_h_const_u16 = 1;
static const ai_u16 conv2d_5_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_5_l_stride_0_const_u16 = 1;
static const ai_i8 conv2d_5_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 conv2d_5_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float conv2d_5_t_in_0_fmt_scale_const_f32 = 0.026113374158740044f;
static const ai_float conv2d_5_t_out_0_fmt_scale_const_f32 = 0.031269706785678864f;
static const ai_float conv2d_5_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0020405868999660015f, 0.002978422911837697f, 0.001848169369623065f, 0.004213296342641115f, 0.0035099126398563385f, 0.0017963299760594964f, 0.0027505955658853054f, 0.003195876022800803f, 0.0055932155810296535f, 0.0019693851936608553f, 0.0021296809427440166f, 0.0018155679572373629f, 0.0070702144876122475f, 0.0025093350559473038f, 0.0067324270494282246f, 0.00202733906917274f, 0.00694996165111661f, 0.0022564807441085577f, 0.002272149780765176f, 0.006101103499531746f, 0.00540568120777607f, 0.001990220043808222f, 0.0018985859351232648f, 0.0019031737465411425f, 0.0017982125282287598f, 0.002045457251369953f, 0.004317826125770807f, 0.0023856665939092636f, 0.002330954186618328f, 0.0036287240218371153f, 0.002100998070091009f, 0.004290314391255379f, 0.004466387443244457f, 0.0032733273692429066f, 0.0022757607512176037f, 0.002587617142125964f, 0.0035340175963938236f, 0.003974722232669592f, 0.004283794201910496f, 0.00208904966711998f, 0.0038868847768753767f, 0.0020409459248185158f, 0.0018865970196202397f, 0.0040958537720143795f, 0.003755982732400298f, 0.003653505817055702f, 0.00745648005977273f, 0.005949941463768482f, 0.0018243666272610426f, 0.001805260544642806f, 0.001962898997589946f, 0.0020162821747362614f, 0.005778100341558456f, 0.0020013193134218454f, 0.002804266521707177f, 0.001995058497413993f, 0.002177563263103366f, 0.004061378072947264f, 0.002172946697100997f, 0.007869450375437737f, 0.0040174443274736404f, 0.002203197916969657f, 0.001869885833002627f, 0.005617119837552309f);
static const ai_layer_format_type conv2d_5_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 conv2d_5_t_out_0_shape_w_const_u16 = 300;
static const ai_u16 conv2d_5_t_out_0_shape_h_const_u16 = 1;


static const ai_i8 pad_8_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_8_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_8_t_in_0_shape_h_const_u32 = 1;

static const ai_u32 pad_8_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19456;
static const ai_float pad_8_0_conversion_t_in_0_fmt_scale_const_f32 = 0.030721191316843033f;
static const ai_i8 pad_8_0_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_u32 conv2d_9_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_9_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_9_t_in_0_shape_w_const_u32 = 304;
static const ai_u32 conv2d_9_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_9_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_9_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_9_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_9_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_9_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_9_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_9_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_9_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_9_l_dilation_W_const_u16 = 2;
static const ai_u16 conv2d_9_l_dilation_H_const_u16 = 1;
static const ai_size conv2d_9_v_n_groups_const_size = 1;

static const ai_u32 conv2d_9_0_0_pad_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19200;
static const ai_float conv2d_9_0_0_pad_10_conversion_t_out_0_fmt_scale_const_f32 = 0.02891252376139164f;
static const ai_i8 conv2d_9_0_0_pad_10_conversion_t_out_0_fmt_zero_const_s8 = -128;

static const ai_i8 pad_10_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_10_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_10_t_in_0_shape_h_const_u32 = 1;

static const ai_u32 pad_10_0_0_conv2d_11_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19456;
static const ai_float pad_10_0_0_conv2d_11_conversion_t_in_0_fmt_scale_const_f32 = 0.02891252376139164f;
static const ai_i8 pad_10_0_0_conv2d_11_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_u32 conv2d_11_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_11_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_11_t_in_0_shape_w_const_u32 = 304;
static const ai_u32 conv2d_11_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_11_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_11_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_11_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_11_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_11_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_11_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_11_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_11_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_11_l_dilation_W_const_u16 = 2;
static const ai_u16 conv2d_11_l_dilation_H_const_u16 = 1;
static const ai_size conv2d_11_v_n_groups_const_size = 1;

static const ai_u32 conv2d_11_0_1_eltwise_12_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19200;
static const ai_float conv2d_11_0_1_eltwise_12_conversion_t_out_0_fmt_scale_const_f32 = 0.04149854928255081f;
static const ai_i8 conv2d_11_0_1_eltwise_12_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_i8 pad_13_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_13_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_13_t_in_0_shape_h_const_u32 = 1;

static const ai_u32 pad_13_0_0_conv2d_14_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19712;
static const ai_float pad_13_0_0_conv2d_14_conversion_t_in_0_fmt_scale_const_f32 = 0.04149854928255081f;
static const ai_i8 pad_13_0_0_conv2d_14_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_u32 conv2d_14_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_14_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_14_t_in_0_shape_w_const_u32 = 308;
static const ai_u32 conv2d_14_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_14_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_14_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_14_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_14_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_14_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_14_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_14_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_14_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_14_l_dilation_W_const_u16 = 4;
static const ai_u16 conv2d_14_l_dilation_H_const_u16 = 1;
static const ai_size conv2d_14_v_n_groups_const_size = 1;

static const ai_u32 conv2d_14_0_0_pad_15_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19200;
static const ai_float conv2d_14_0_0_pad_15_conversion_t_out_0_fmt_scale_const_f32 = 0.040445659309625626f;
static const ai_i8 conv2d_14_0_0_pad_15_conversion_t_out_0_fmt_zero_const_s8 = -128;

static const ai_i8 pad_15_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_15_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_15_t_in_0_shape_h_const_u32 = 1;

static const ai_u32 pad_15_0_0_conv2d_16_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19712;
static const ai_float pad_15_0_0_conv2d_16_conversion_t_in_0_fmt_scale_const_f32 = 0.040445659309625626f;
static const ai_i8 pad_15_0_0_conv2d_16_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_u32 conv2d_16_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_16_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_16_t_in_0_shape_w_const_u32 = 308;
static const ai_u32 conv2d_16_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_16_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_16_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_16_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_16_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_16_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_16_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_16_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_16_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_16_l_dilation_W_const_u16 = 4;
static const ai_u16 conv2d_16_l_dilation_H_const_u16 = 1;
static const ai_size conv2d_16_v_n_groups_const_size = 1;

static const ai_u32 conv2d_16_0_1_eltwise_17_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19200;
static const ai_float conv2d_16_0_1_eltwise_17_conversion_t_out_0_fmt_scale_const_f32 = 0.04149854928255081f;
static const ai_i8 conv2d_16_0_1_eltwise_17_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_i8 pad_18_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_18_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_18_t_in_0_shape_h_const_u32 = 1;

static const ai_u32 pad_18_0_0_conv2d_19_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 20224;
static const ai_float pad_18_0_0_conv2d_19_conversion_t_in_0_fmt_scale_const_f32 = 0.04149854928255081f;
static const ai_i8 pad_18_0_0_conv2d_19_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_u32 conv2d_19_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_19_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_19_t_in_0_shape_w_const_u32 = 316;
static const ai_u32 conv2d_19_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_19_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_19_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_19_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_19_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_19_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_19_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_19_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_19_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_19_l_dilation_W_const_u16 = 8;
static const ai_u16 conv2d_19_l_dilation_H_const_u16 = 1;
static const ai_size conv2d_19_v_n_groups_const_size = 1;

static const ai_u32 conv2d_19_0_0_pad_20_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19200;
static const ai_float conv2d_19_0_0_pad_20_conversion_t_out_0_fmt_scale_const_f32 = 0.0626107007265091f;
static const ai_i8 conv2d_19_0_0_pad_20_conversion_t_out_0_fmt_zero_const_s8 = -128;

static const ai_i8 pad_20_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_20_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_20_t_in_0_shape_h_const_u32 = 1;

static const ai_u32 pad_20_0_0_conv2d_21_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 20224;
static const ai_float pad_20_0_0_conv2d_21_conversion_t_in_0_fmt_scale_const_f32 = 0.0626107007265091f;
static const ai_i8 pad_20_0_0_conv2d_21_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_u32 conv2d_21_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_21_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_21_t_in_0_shape_w_const_u32 = 316;
static const ai_u32 conv2d_21_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_21_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_21_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_21_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_21_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_21_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_21_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_21_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_21_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_21_l_dilation_W_const_u16 = 8;
static const ai_u16 conv2d_21_l_dilation_H_const_u16 = 1;
static const ai_size conv2d_21_v_n_groups_const_size = 1;

static const ai_u32 conv2d_21_0_1_eltwise_22_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19200;
static const ai_float conv2d_21_0_1_eltwise_22_conversion_t_out_0_fmt_scale_const_f32 = 0.062476057559251785f;
static const ai_i8 conv2d_21_0_1_eltwise_22_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_i8 pad_23_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_23_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_23_t_in_0_shape_h_const_u32 = 1;

static const ai_u32 pad_23_0_0_conv2d_24_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 21248;
static const ai_float pad_23_0_0_conv2d_24_conversion_t_in_0_fmt_scale_const_f32 = 0.062476057559251785f;
static const ai_i8 pad_23_0_0_conv2d_24_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_u32 conv2d_24_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_24_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_24_t_in_0_shape_w_const_u32 = 332;
static const ai_u32 conv2d_24_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_24_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_24_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_24_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_24_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_24_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_24_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_24_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_24_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_24_l_dilation_W_const_u16 = 16;
static const ai_u16 conv2d_24_l_dilation_H_const_u16 = 1;
static const ai_size conv2d_24_v_n_groups_const_size = 1;

static const ai_u32 conv2d_24_0_0_pad_25_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19200;
static const ai_float conv2d_24_0_0_pad_25_conversion_t_out_0_fmt_scale_const_f32 = 0.05941449850797653f;
static const ai_i8 conv2d_24_0_0_pad_25_conversion_t_out_0_fmt_zero_const_s8 = -128;

static const ai_i8 pad_25_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_25_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_25_t_in_0_shape_h_const_u32 = 1;

static const ai_u32 pad_25_0_0_conv2d_26_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 21248;
static const ai_float pad_25_0_0_conv2d_26_conversion_t_in_0_fmt_scale_const_f32 = 0.05941449850797653f;
static const ai_i8 pad_25_0_0_conv2d_26_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_u32 conv2d_26_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_26_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_26_t_in_0_shape_w_const_u32 = 332;
static const ai_u32 conv2d_26_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_26_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_26_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_26_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_26_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_26_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_26_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_26_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_26_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_26_l_dilation_W_const_u16 = 16;
static const ai_u16 conv2d_26_l_dilation_H_const_u16 = 1;
static const ai_size conv2d_26_v_n_groups_const_size = 1;

static const ai_u32 conv2d_26_0_1_eltwise_27_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19200;
static const ai_float conv2d_26_0_1_eltwise_27_conversion_t_out_0_fmt_scale_const_f32 = 0.06406153738498688f;
static const ai_i8 conv2d_26_0_1_eltwise_27_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_i8 pad_28_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_28_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_28_t_in_0_shape_h_const_u32 = 1;

static const ai_u32 pad_28_0_0_conv2d_29_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 23296;
static const ai_float pad_28_0_0_conv2d_29_conversion_t_in_0_fmt_scale_const_f32 = 0.06406153738498688f;
static const ai_i8 pad_28_0_0_conv2d_29_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_u32 conv2d_29_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_29_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_29_t_in_0_shape_w_const_u32 = 364;
static const ai_u32 conv2d_29_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_29_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_29_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_29_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_29_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_29_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_29_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_29_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_29_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_29_l_dilation_W_const_u16 = 32;
static const ai_u16 conv2d_29_l_dilation_H_const_u16 = 1;
static const ai_size conv2d_29_v_n_groups_const_size = 1;

static const ai_u32 conv2d_29_0_0_pad_30_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19200;
static const ai_float conv2d_29_0_0_pad_30_conversion_t_out_0_fmt_scale_const_f32 = 0.08398376405239105f;
static const ai_i8 conv2d_29_0_0_pad_30_conversion_t_out_0_fmt_zero_const_s8 = -128;

static const ai_i8 pad_30_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_30_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_30_t_in_0_shape_h_const_u32 = 1;

static const ai_u32 pad_30_0_0_conv2d_31_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 23296;
static const ai_float pad_30_0_0_conv2d_31_conversion_t_in_0_fmt_scale_const_f32 = 0.08398376405239105f;
static const ai_i8 pad_30_0_0_conv2d_31_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_u32 conv2d_31_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_31_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_31_t_in_0_shape_w_const_u32 = 364;
static const ai_u32 conv2d_31_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_31_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_31_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_31_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_31_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_31_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_31_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_31_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_31_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_31_l_dilation_W_const_u16 = 32;
static const ai_u16 conv2d_31_l_dilation_H_const_u16 = 1;
static const ai_size conv2d_31_v_n_groups_const_size = 1;

static const ai_u32 conv2d_31_0_1_eltwise_32_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19200;
static const ai_float conv2d_31_0_1_eltwise_32_conversion_t_out_0_fmt_scale_const_f32 = 0.07169536501169205f;
static const ai_i8 conv2d_31_0_1_eltwise_32_conversion_t_out_0_fmt_zero_const_s8 = -128;


static const ai_i8 pad_33_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_33_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_33_t_in_0_shape_h_const_u32 = 1;

static const ai_u32 pad_33_0_0_conv2d_34_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 23296;
static const ai_float pad_33_0_0_conv2d_34_conversion_t_in_0_fmt_scale_const_f32 = 0.07169536501169205f;
static const ai_i8 pad_33_0_0_conv2d_34_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_u32 conv2d_34_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_34_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_34_t_in_0_shape_w_const_u32 = 364;
static const ai_u32 conv2d_34_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_34_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_34_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_34_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_34_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_34_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_34_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_34_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_34_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_34_l_dilation_W_const_u16 = 32;
static const ai_u16 conv2d_34_l_dilation_H_const_u16 = 1;
static const ai_size conv2d_34_v_n_groups_const_size = 1;

static const ai_u32 conv2d_34_0_0_pad_35_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19200;
static const ai_float conv2d_34_0_0_pad_35_conversion_t_out_0_fmt_scale_const_f32 = 0.11800151318311691f;
static const ai_i8 conv2d_34_0_0_pad_35_conversion_t_out_0_fmt_zero_const_s8 = -128;

static const ai_i8 pad_35_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 pad_35_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 pad_35_t_in_0_shape_h_const_u32 = 1;

static const ai_u32 pad_35_0_0_conv2d_36_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 23296;
static const ai_float pad_35_0_0_conv2d_36_conversion_t_in_0_fmt_scale_const_f32 = 0.11800151318311691f;
static const ai_i8 pad_35_0_0_conv2d_36_conversion_t_in_0_fmt_zero_const_s8 = -128;

static const ai_u32 conv2d_36_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_36_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 conv2d_36_t_in_0_shape_w_const_u32 = 364;
static const ai_u32 conv2d_36_t_in_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_36_t_out_0_shape_w_const_u32 = 300;
static const ai_u32 conv2d_36_t_out_0_shape_h_const_u32 = 1;
static const ai_u32 conv2d_36_t_weight_0_shape_w_const_u32 = 3;
static const ai_u32 conv2d_36_t_weight_0_shape_h_const_u32 = 1;
static const ai_i32 conv2d_36_l_pad_W_0_const_s32 = 0;
static const ai_i32 conv2d_36_l_pad_H_0_const_s32 = 0;
static const ai_u16 conv2d_36_l_stride_1_const_u16 = 1;
static const ai_u16 conv2d_36_l_stride_0_const_u16 = 1;
static const ai_u16 conv2d_36_l_dilation_W_const_u16 = 32;
static const ai_u16 conv2d_36_l_dilation_H_const_u16 = 1;
static const ai_size conv2d_36_v_n_groups_const_size = 1;

static const ai_u32 conv2d_36_0_1_eltwise_37_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 19200;
static const ai_float conv2d_36_0_1_eltwise_37_conversion_t_out_0_fmt_scale_const_f32 = 0.07169536501169205f;
static const ai_i8 conv2d_36_0_1_eltwise_37_conversion_t_out_0_fmt_zero_const_s8 = -128;






static const ai_u32 conversion_47_t_out_0_shape_h_w_ch_d_prod_const_u32 = 3;
static const ai_float conversion_47_t_in_0_fmt_scale_const_f32 = 0.016138950362801552f;
static const ai_i8 conversion_47_t_in_0_fmt_zero_const_s8 = -28;
STAI_API_ENTRY
stai_return_code stai_network_run(
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


  /* LITE_KERNEL_SECTION BEGIN conversion_1 */
  {
      const ai_float* conversion_1_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_inputs[1] + 0);
    ai_i8* conversion_1_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 96008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(1, 1, {(stai_ptr) conversion_1_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conversion_1_t_in_0_ptr_const_f32, conversion_1_t_out_0_ptr_s8, conversion_1_t_out_0_shape_h_w_ch_d_prod_const_u32, conversion_1_t_out_0_fmt_scale_const_f32, conversion_1_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(1, 1, {(stai_ptr) conversion_1_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conversion_1 */
  /* LITE_KERNEL_SECTION BEGIN gemm_43 */
  {
    
  forward_lite_dense_integer_SSSA_ch_gemm_43(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_43 */
  /* LITE_KERNEL_SECTION BEGIN conversion_0 */
  {
      const ai_float* conversion_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_inputs[0] + 0);
    ai_i8* conversion_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 96008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(0, 1, {(stai_ptr) conversion_0_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conversion_0_t_in_0_ptr_const_f32, conversion_0_t_out_0_ptr_s8, conversion_0_t_out_0_shape_h_w_ch_d_prod_const_u32, conversion_0_t_out_0_fmt_scale_const_f32, conversion_0_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(0, 1, {(stai_ptr) conversion_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conversion_0 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_6 */
  {
      const ai_i8* conv2d_6_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 96008);
    const ai_i8* conv2d_6_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 52);
    const ai_i32* conv2d_6_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 116);
    ai_i8* conv2d_6_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 96308);
    ai_i16* conv2d_6_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 95488);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(6, 1, {(stai_ptr) conv2d_6_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(conv2d_6_t_in_0_ptr_const_s8, conv2d_6_t_in_0_shape_w_const_u16, conv2d_6_t_in_0_shape_h_const_u16, conv2d_6_l_stride_1_const_u16, conv2d_6_l_stride_0_const_u16, conv2d_6_t_in_0_shape_ch_const_u16, conv2d_6_t_weight_0_ptr_const_s8, conv2d_6_t_out_0_shape_ch_const_u16, conv2d_6_t_weight_1_ptr_const_s32, conv2d_6_t_in_0_fmt_zero_const_s8, conv2d_6_t_out_0_fmt_zero_const_s8, conv2d_6_t_in_0_fmt_scale_const_f32, conv2d_6_t_out_0_fmt_scale_const_f32, conv2d_6_t_weight_0_fmt_scale_const_f32, conv2d_6_l_out_ch_format_const_layer_format_type, conv2d_6_t_out_0_ptr_s8, 1, 512, conv2d_6_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(6, 1, {(stai_ptr) conv2d_6_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_6 */
  /* LITE_KERNEL_SECTION BEGIN pad_2 */
  {
      const ai_ptr pad_2_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 96008);
    ai_ptr pad_2_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 115508);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(2, 1, {(stai_ptr) pad_2_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_2_t_in_0_ptr_const_ptr, pad_2_t_out_0_ptr_ptr, (ai_handle)(pad_2_v_pad_constant_value_const_s8), pad_2_t_in_0_fmt_bitsize_const_s16, pad_2_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(300), (ai_i32)(0), (ai_i32)(0), (ai_i32)(2), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(2, 1, {(stai_ptr) pad_2_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_2 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_3 */
  {
      const ai_i8* conv2d_3_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 115508);
    const ai_i8* conv2d_3_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 372);
    const ai_i32* conv2d_3_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 564);
    ai_i8* conv2d_3_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 116388);
    ai_i16* conv2d_3_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 115812);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(3, 1, {(stai_ptr) conv2d_3_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(conv2d_3_t_in_0_ptr_const_s8, conv2d_3_t_in_0_shape_w_const_u16, conv2d_3_t_in_0_shape_h_const_u16, conv2d_3_t_in_0_shape_ch_const_u16, conv2d_3_t_weight_0_ptr_const_s8, conv2d_3_t_out_0_shape_ch_const_u16, conv2d_3_t_weight_0_shape_w_const_u16, conv2d_3_t_weight_0_shape_h_const_u16, conv2d_3_l_stride_1_const_u16, conv2d_3_l_stride_0_const_u16, conv2d_3_l_pad_W_0_const_s32, conv2d_3_l_pad_H_0_const_s32, conv2d_3_t_weight_1_ptr_const_s32, conv2d_3_t_in_0_fmt_zero_const_s8, conv2d_3_t_out_0_fmt_zero_const_s8, conv2d_3_t_in_0_fmt_scale_const_f32, conv2d_3_t_out_0_fmt_scale_const_f32, conv2d_3_t_weight_0_fmt_scale_const_f32, conv2d_3_l_out_ch_format_const_layer_format_type, conv2d_3_t_out_0_ptr_s8, conv2d_3_t_out_0_shape_w_const_u16, conv2d_3_t_out_0_shape_h_const_u16, 1, 576, conv2d_3_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(3, 1, {(stai_ptr) conv2d_3_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_3 */
  /* LITE_KERNEL_SECTION BEGIN pad_4 */
  {
      const ai_ptr pad_4_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 116388);
    ai_ptr pad_4_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 116260);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(4, 1, {(stai_ptr) pad_4_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_4_t_in_0_ptr_const_ptr, pad_4_t_out_0_ptr_ptr, (ai_handle)(pad_4_v_pad_constant_value_const_s8), pad_4_t_in_0_fmt_bitsize_const_s16, pad_4_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(128), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(4, 1, {(stai_ptr) pad_4_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_4 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_5 */
  {
      const ai_i8* conv2d_5_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 116260);
    const ai_i8* conv2d_5_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 820);
    const ai_i32* conv2d_5_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 13108);
    ai_i8* conv2d_5_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 76800);
    ai_i16* conv2d_5_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 135588);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(5, 1, {(stai_ptr) conv2d_5_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_sssa8_ch(conv2d_5_t_in_0_ptr_const_s8, conv2d_5_t_in_0_shape_w_const_u16, conv2d_5_t_in_0_shape_h_const_u16, conv2d_5_t_in_0_shape_ch_const_u16, conv2d_5_t_weight_0_ptr_const_s8, conv2d_5_t_out_0_shape_ch_const_u16, conv2d_5_t_weight_0_shape_w_const_u16, conv2d_5_t_weight_0_shape_h_const_u16, conv2d_5_l_stride_1_const_u16, conv2d_5_l_stride_0_const_u16, conv2d_5_t_weight_1_ptr_const_s32, conv2d_5_t_in_0_fmt_zero_const_s8, conv2d_5_t_out_0_fmt_zero_const_s8, conv2d_5_t_in_0_fmt_scale_const_f32, conv2d_5_t_out_0_fmt_scale_const_f32, conv2d_5_t_weight_0_fmt_scale_const_f32, conv2d_5_l_out_ch_format_const_layer_format_type, conv2d_5_t_out_0_ptr_s8, conv2d_5_t_out_0_shape_w_const_u16, conv2d_5_t_out_0_shape_h_const_u16, 1, 1, 1280, conv2d_5_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(5, 1, {(stai_ptr) conv2d_5_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_5 */
  /* LITE_KERNEL_SECTION BEGIN eltwise_7 */
  {
    
  forward_lite_eltwise_integer_INT8_eltwise_7(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_7 */
  /* LITE_KERNEL_SECTION BEGIN pad_8 */
  {
      const ai_ptr pad_8_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 96308);
    ai_ptr pad_8_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 76544);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(8, 1, {(stai_ptr) pad_8_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_8_t_in_0_ptr_const_ptr, pad_8_t_out_0_ptr_ptr, (ai_handle)(pad_8_v_pad_constant_value_const_s8), pad_8_t_in_0_fmt_bitsize_const_s16, pad_8_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(256), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(8, 1, {(stai_ptr) pad_8_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_8 */
  /* LITE_KERNEL_SECTION BEGIN pad_8_0_conversion */
  {
      const ai_i8* pad_8_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 76544);
    ai_float* pad_8_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 115508);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(8, 1, {(stai_ptr) pad_8_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(pad_8_0_conversion_t_in_0_ptr_const_s8, pad_8_0_conversion_t_out_0_ptr_f32, pad_8_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, pad_8_0_conversion_t_in_0_fmt_scale_const_f32, pad_8_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(8, 1, {(stai_ptr) pad_8_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END pad_8_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN conv2d_9 */
  {
      const ai_float* conv2d_9_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 115508);
    ai_float* conv2d_9_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
    const ai_u8* conv2d_9_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 13364);
    const ai_u8* conv2d_9_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 62516);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(9, 1, {(stai_ptr) conv2d_9_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_9_t_in_0_ptr_const_f32, conv2d_9_t_out_0_ptr_f32, conv2d_9_t_weight_0_ptr_const_u8, conv2d_9_t_weight_1_ptr_const_u8, conv2d_9_t_in_0_shape_ch_const_u32, conv2d_9_t_out_0_shape_ch_const_u32, conv2d_9_t_in_0_shape_w_const_u32, conv2d_9_t_in_0_shape_h_const_u32, conv2d_9_t_out_0_shape_w_const_u32, conv2d_9_t_out_0_shape_h_const_u32, conv2d_9_t_weight_0_shape_w_const_u32, conv2d_9_t_weight_0_shape_h_const_u32, conv2d_9_l_pad_W_0_const_s32, conv2d_9_l_pad_H_0_const_s32, conv2d_9_l_stride_1_const_u16, conv2d_9_l_stride_0_const_u16, 1, 5, conv2d_9_l_dilation_W_const_u16, conv2d_9_l_dilation_H_const_u16, conv2d_9_v_n_groups_const_size);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(9, 1, {(stai_ptr) conv2d_9_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_9 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_9_0_0_pad_10_conversion */
  {
      const ai_float* conv2d_9_0_0_pad_10_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
    ai_i8* conv2d_9_0_0_pad_10_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 19200);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(9, 1, {(stai_ptr) conv2d_9_0_0_pad_10_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conv2d_9_0_0_pad_10_conversion_t_in_0_ptr_const_f32, conv2d_9_0_0_pad_10_conversion_t_out_0_ptr_s8, conv2d_9_0_0_pad_10_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, conv2d_9_0_0_pad_10_conversion_t_out_0_fmt_scale_const_f32, conv2d_9_0_0_pad_10_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(9, 1, {(stai_ptr) conv2d_9_0_0_pad_10_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_9_0_0_pad_10_conversion */
  /* LITE_KERNEL_SECTION BEGIN pad_10 */
  {
      const ai_ptr pad_10_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 19200);
    ai_ptr pad_10_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 18944);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(10, 1, {(stai_ptr) pad_10_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_10_t_in_0_ptr_const_ptr, pad_10_t_out_0_ptr_ptr, (ai_handle)(pad_10_v_pad_constant_value_const_s8), pad_10_t_in_0_fmt_bitsize_const_s16, pad_10_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(256), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(10, 1, {(stai_ptr) pad_10_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_10 */
  /* LITE_KERNEL_SECTION BEGIN pad_10_0_0_conv2d_11_conversion */
  {
      const ai_i8* pad_10_0_0_conv2d_11_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 18944);
    ai_float* pad_10_0_0_conv2d_11_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 115508);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(10, 1, {(stai_ptr) pad_10_0_0_conv2d_11_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(pad_10_0_0_conv2d_11_conversion_t_in_0_ptr_const_s8, pad_10_0_0_conv2d_11_conversion_t_out_0_ptr_f32, pad_10_0_0_conv2d_11_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, pad_10_0_0_conv2d_11_conversion_t_in_0_fmt_scale_const_f32, pad_10_0_0_conv2d_11_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(10, 1, {(stai_ptr) pad_10_0_0_conv2d_11_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END pad_10_0_0_conv2d_11_conversion */
  /* LITE_KERNEL_SECTION BEGIN conv2d_11 */
  {
      const ai_float* conv2d_11_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 115508);
    ai_float* conv2d_11_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
    const ai_u8* conv2d_11_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 62772);
    const ai_u8* conv2d_11_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 111924);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(11, 1, {(stai_ptr) conv2d_11_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_11_t_in_0_ptr_const_f32, conv2d_11_t_out_0_ptr_f32, conv2d_11_t_weight_0_ptr_const_u8, conv2d_11_t_weight_1_ptr_const_u8, conv2d_11_t_in_0_shape_ch_const_u32, conv2d_11_t_out_0_shape_ch_const_u32, conv2d_11_t_in_0_shape_w_const_u32, conv2d_11_t_in_0_shape_h_const_u32, conv2d_11_t_out_0_shape_w_const_u32, conv2d_11_t_out_0_shape_h_const_u32, conv2d_11_t_weight_0_shape_w_const_u32, conv2d_11_t_weight_0_shape_h_const_u32, conv2d_11_l_pad_W_0_const_s32, conv2d_11_l_pad_H_0_const_s32, conv2d_11_l_stride_1_const_u16, conv2d_11_l_stride_0_const_u16, 1, 5, conv2d_11_l_dilation_W_const_u16, conv2d_11_l_dilation_H_const_u16, conv2d_11_v_n_groups_const_size);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(11, 1, {(stai_ptr) conv2d_11_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_11 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_11_0_1_eltwise_12_conversion */
  {
      const ai_float* conv2d_11_0_1_eltwise_12_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
    ai_i8* conv2d_11_0_1_eltwise_12_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 19200);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(11, 1, {(stai_ptr) conv2d_11_0_1_eltwise_12_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conv2d_11_0_1_eltwise_12_conversion_t_in_0_ptr_const_f32, conv2d_11_0_1_eltwise_12_conversion_t_out_0_ptr_s8, conv2d_11_0_1_eltwise_12_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, conv2d_11_0_1_eltwise_12_conversion_t_out_0_fmt_scale_const_f32, conv2d_11_0_1_eltwise_12_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(11, 1, {(stai_ptr) conv2d_11_0_1_eltwise_12_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_11_0_1_eltwise_12_conversion */
  /* LITE_KERNEL_SECTION BEGIN eltwise_12 */
  {
    
  forward_lite_eltwise_integer_INT8_eltwise_12(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_12 */
  /* LITE_KERNEL_SECTION BEGIN pad_13 */
  {
      const ai_ptr pad_13_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 96308);
    ai_ptr pad_13_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 18944);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(13, 1, {(stai_ptr) pad_13_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_13_t_in_0_ptr_const_ptr, pad_13_t_out_0_ptr_ptr, (ai_handle)(pad_13_v_pad_constant_value_const_s8), pad_13_t_in_0_fmt_bitsize_const_s16, pad_13_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(512), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(13, 1, {(stai_ptr) pad_13_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_13 */
  /* LITE_KERNEL_SECTION BEGIN pad_13_0_0_conv2d_14_conversion */
  {
      const ai_i8* pad_13_0_0_conv2d_14_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 18944);
    ai_float* pad_13_0_0_conv2d_14_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 115508);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(13, 1, {(stai_ptr) pad_13_0_0_conv2d_14_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(pad_13_0_0_conv2d_14_conversion_t_in_0_ptr_const_s8, pad_13_0_0_conv2d_14_conversion_t_out_0_ptr_f32, pad_13_0_0_conv2d_14_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, pad_13_0_0_conv2d_14_conversion_t_in_0_fmt_scale_const_f32, pad_13_0_0_conv2d_14_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(13, 1, {(stai_ptr) pad_13_0_0_conv2d_14_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END pad_13_0_0_conv2d_14_conversion */
  /* LITE_KERNEL_SECTION BEGIN conv2d_14 */
  {
      const ai_float* conv2d_14_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 115508);
    ai_float* conv2d_14_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 18944);
    const ai_u8* conv2d_14_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 112180);
    const ai_u8* conv2d_14_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 161332);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(14, 1, {(stai_ptr) conv2d_14_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_14_t_in_0_ptr_const_f32, conv2d_14_t_out_0_ptr_f32, conv2d_14_t_weight_0_ptr_const_u8, conv2d_14_t_weight_1_ptr_const_u8, conv2d_14_t_in_0_shape_ch_const_u32, conv2d_14_t_out_0_shape_ch_const_u32, conv2d_14_t_in_0_shape_w_const_u32, conv2d_14_t_in_0_shape_h_const_u32, conv2d_14_t_out_0_shape_w_const_u32, conv2d_14_t_out_0_shape_h_const_u32, conv2d_14_t_weight_0_shape_w_const_u32, conv2d_14_t_weight_0_shape_h_const_u32, conv2d_14_l_pad_W_0_const_s32, conv2d_14_l_pad_H_0_const_s32, conv2d_14_l_stride_1_const_u16, conv2d_14_l_stride_0_const_u16, 1, 9, conv2d_14_l_dilation_W_const_u16, conv2d_14_l_dilation_H_const_u16, conv2d_14_v_n_groups_const_size);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(14, 1, {(stai_ptr) conv2d_14_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_14 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_14_0_0_pad_15_conversion */
  {
      const ai_float* conv2d_14_0_0_pad_15_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 18944);
    ai_i8* conv2d_14_0_0_pad_15_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 175156);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(14, 1, {(stai_ptr) conv2d_14_0_0_pad_15_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conv2d_14_0_0_pad_15_conversion_t_in_0_ptr_const_f32, conv2d_14_0_0_pad_15_conversion_t_out_0_ptr_s8, conv2d_14_0_0_pad_15_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, conv2d_14_0_0_pad_15_conversion_t_out_0_fmt_scale_const_f32, conv2d_14_0_0_pad_15_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(14, 1, {(stai_ptr) conv2d_14_0_0_pad_15_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_14_0_0_pad_15_conversion */
  /* LITE_KERNEL_SECTION BEGIN pad_15 */
  {
      const ai_ptr pad_15_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 175156);
    ai_ptr pad_15_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 18944);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(15, 1, {(stai_ptr) pad_15_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_15_t_in_0_ptr_const_ptr, pad_15_t_out_0_ptr_ptr, (ai_handle)(pad_15_v_pad_constant_value_const_s8), pad_15_t_in_0_fmt_bitsize_const_s16, pad_15_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(512), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(15, 1, {(stai_ptr) pad_15_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_15 */
  /* LITE_KERNEL_SECTION BEGIN pad_15_0_0_conv2d_16_conversion */
  {
      const ai_i8* pad_15_0_0_conv2d_16_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 18944);
    ai_float* pad_15_0_0_conv2d_16_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 115508);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(15, 1, {(stai_ptr) pad_15_0_0_conv2d_16_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(pad_15_0_0_conv2d_16_conversion_t_in_0_ptr_const_s8, pad_15_0_0_conv2d_16_conversion_t_out_0_ptr_f32, pad_15_0_0_conv2d_16_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, pad_15_0_0_conv2d_16_conversion_t_in_0_fmt_scale_const_f32, pad_15_0_0_conv2d_16_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(15, 1, {(stai_ptr) pad_15_0_0_conv2d_16_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END pad_15_0_0_conv2d_16_conversion */
  /* LITE_KERNEL_SECTION BEGIN conv2d_16 */
  {
      const ai_float* conv2d_16_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 115508);
    ai_float* conv2d_16_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 18944);
    const ai_u8* conv2d_16_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 161588);
    const ai_u8* conv2d_16_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 210740);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(16, 1, {(stai_ptr) conv2d_16_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_16_t_in_0_ptr_const_f32, conv2d_16_t_out_0_ptr_f32, conv2d_16_t_weight_0_ptr_const_u8, conv2d_16_t_weight_1_ptr_const_u8, conv2d_16_t_in_0_shape_ch_const_u32, conv2d_16_t_out_0_shape_ch_const_u32, conv2d_16_t_in_0_shape_w_const_u32, conv2d_16_t_in_0_shape_h_const_u32, conv2d_16_t_out_0_shape_w_const_u32, conv2d_16_t_out_0_shape_h_const_u32, conv2d_16_t_weight_0_shape_w_const_u32, conv2d_16_t_weight_0_shape_h_const_u32, conv2d_16_l_pad_W_0_const_s32, conv2d_16_l_pad_H_0_const_s32, conv2d_16_l_stride_1_const_u16, conv2d_16_l_stride_0_const_u16, 1, 9, conv2d_16_l_dilation_W_const_u16, conv2d_16_l_dilation_H_const_u16, conv2d_16_v_n_groups_const_size);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(16, 1, {(stai_ptr) conv2d_16_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_16 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_16_0_1_eltwise_17_conversion */
  {
      const ai_float* conv2d_16_0_1_eltwise_17_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 18944);
    ai_i8* conv2d_16_0_1_eltwise_17_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 18944);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(16, 1, {(stai_ptr) conv2d_16_0_1_eltwise_17_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conv2d_16_0_1_eltwise_17_conversion_t_in_0_ptr_const_f32, conv2d_16_0_1_eltwise_17_conversion_t_out_0_ptr_s8, conv2d_16_0_1_eltwise_17_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, conv2d_16_0_1_eltwise_17_conversion_t_out_0_fmt_scale_const_f32, conv2d_16_0_1_eltwise_17_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(16, 1, {(stai_ptr) conv2d_16_0_1_eltwise_17_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_16_0_1_eltwise_17_conversion */
  /* LITE_KERNEL_SECTION BEGIN eltwise_17 */
  {
    
  forward_lite_eltwise_integer_INT8_eltwise_17(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_17 */
  /* LITE_KERNEL_SECTION BEGIN pad_18 */
  {
      const ai_ptr pad_18_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 175156);
    ai_ptr pad_18_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 154932);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(18, 1, {(stai_ptr) pad_18_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_18_t_in_0_ptr_const_ptr, pad_18_t_out_0_ptr_ptr, (ai_handle)(pad_18_v_pad_constant_value_const_s8), pad_18_t_in_0_fmt_bitsize_const_s16, pad_18_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(1024), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(18, 1, {(stai_ptr) pad_18_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_18 */
  /* LITE_KERNEL_SECTION BEGIN pad_18_0_0_conv2d_19_conversion */
  {
      const ai_i8* pad_18_0_0_conv2d_19_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 154932);
    ai_float* pad_18_0_0_conv2d_19_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15104);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(18, 1, {(stai_ptr) pad_18_0_0_conv2d_19_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(pad_18_0_0_conv2d_19_conversion_t_in_0_ptr_const_s8, pad_18_0_0_conv2d_19_conversion_t_out_0_ptr_f32, pad_18_0_0_conv2d_19_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, pad_18_0_0_conv2d_19_conversion_t_in_0_fmt_scale_const_f32, pad_18_0_0_conv2d_19_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(18, 1, {(stai_ptr) pad_18_0_0_conv2d_19_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END pad_18_0_0_conv2d_19_conversion */
  /* LITE_KERNEL_SECTION BEGIN conv2d_19 */
  {
      const ai_float* conv2d_19_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15104);
    ai_float* conv2d_19_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 96008);
    const ai_u8* conv2d_19_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 210996);
    const ai_u8* conv2d_19_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 260148);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(19, 1, {(stai_ptr) conv2d_19_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_19_t_in_0_ptr_const_f32, conv2d_19_t_out_0_ptr_f32, conv2d_19_t_weight_0_ptr_const_u8, conv2d_19_t_weight_1_ptr_const_u8, conv2d_19_t_in_0_shape_ch_const_u32, conv2d_19_t_out_0_shape_ch_const_u32, conv2d_19_t_in_0_shape_w_const_u32, conv2d_19_t_in_0_shape_h_const_u32, conv2d_19_t_out_0_shape_w_const_u32, conv2d_19_t_out_0_shape_h_const_u32, conv2d_19_t_weight_0_shape_w_const_u32, conv2d_19_t_weight_0_shape_h_const_u32, conv2d_19_l_pad_W_0_const_s32, conv2d_19_l_pad_H_0_const_s32, conv2d_19_l_stride_1_const_u16, conv2d_19_l_stride_0_const_u16, 1, 17, conv2d_19_l_dilation_W_const_u16, conv2d_19_l_dilation_H_const_u16, conv2d_19_v_n_groups_const_size);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(19, 1, {(stai_ptr) conv2d_19_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_19 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_19_0_0_pad_20_conversion */
  {
      const ai_float* conv2d_19_0_0_pad_20_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 96008);
    ai_i8* conv2d_19_0_0_pad_20_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 15104);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(19, 1, {(stai_ptr) conv2d_19_0_0_pad_20_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conv2d_19_0_0_pad_20_conversion_t_in_0_ptr_const_f32, conv2d_19_0_0_pad_20_conversion_t_out_0_ptr_s8, conv2d_19_0_0_pad_20_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, conv2d_19_0_0_pad_20_conversion_t_out_0_fmt_scale_const_f32, conv2d_19_0_0_pad_20_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(19, 1, {(stai_ptr) conv2d_19_0_0_pad_20_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_19_0_0_pad_20_conversion */
  /* LITE_KERNEL_SECTION BEGIN pad_20 */
  {
      const ai_ptr pad_20_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 15104);
    ai_ptr pad_20_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 75776);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(20, 1, {(stai_ptr) pad_20_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_20_t_in_0_ptr_const_ptr, pad_20_t_out_0_ptr_ptr, (ai_handle)(pad_20_v_pad_constant_value_const_s8), pad_20_t_in_0_fmt_bitsize_const_s16, pad_20_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(1024), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(20, 1, {(stai_ptr) pad_20_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_20 */
  /* LITE_KERNEL_SECTION BEGIN pad_20_0_0_conv2d_21_conversion */
  {
      const ai_i8* pad_20_0_0_conv2d_21_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 75776);
    ai_float* pad_20_0_0_conv2d_21_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 15104);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(20, 1, {(stai_ptr) pad_20_0_0_conv2d_21_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(pad_20_0_0_conv2d_21_conversion_t_in_0_ptr_const_s8, pad_20_0_0_conv2d_21_conversion_t_out_0_ptr_f32, pad_20_0_0_conv2d_21_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, pad_20_0_0_conv2d_21_conversion_t_in_0_fmt_scale_const_f32, pad_20_0_0_conv2d_21_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(20, 1, {(stai_ptr) pad_20_0_0_conv2d_21_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END pad_20_0_0_conv2d_21_conversion */
  /* LITE_KERNEL_SECTION BEGIN conv2d_21 */
  {
      const ai_float* conv2d_21_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 15104);
    ai_float* conv2d_21_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 96008);
    const ai_u8* conv2d_21_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 260404);
    const ai_u8* conv2d_21_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 309556);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(21, 1, {(stai_ptr) conv2d_21_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_21_t_in_0_ptr_const_f32, conv2d_21_t_out_0_ptr_f32, conv2d_21_t_weight_0_ptr_const_u8, conv2d_21_t_weight_1_ptr_const_u8, conv2d_21_t_in_0_shape_ch_const_u32, conv2d_21_t_out_0_shape_ch_const_u32, conv2d_21_t_in_0_shape_w_const_u32, conv2d_21_t_in_0_shape_h_const_u32, conv2d_21_t_out_0_shape_w_const_u32, conv2d_21_t_out_0_shape_h_const_u32, conv2d_21_t_weight_0_shape_w_const_u32, conv2d_21_t_weight_0_shape_h_const_u32, conv2d_21_l_pad_W_0_const_s32, conv2d_21_l_pad_H_0_const_s32, conv2d_21_l_stride_1_const_u16, conv2d_21_l_stride_0_const_u16, 1, 17, conv2d_21_l_dilation_W_const_u16, conv2d_21_l_dilation_H_const_u16, conv2d_21_v_n_groups_const_size);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(21, 1, {(stai_ptr) conv2d_21_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_21 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_21_0_1_eltwise_22_conversion */
  {
      const ai_float* conv2d_21_0_1_eltwise_22_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 96008);
    ai_i8* conv2d_21_0_1_eltwise_22_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 15104);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(21, 1, {(stai_ptr) conv2d_21_0_1_eltwise_22_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conv2d_21_0_1_eltwise_22_conversion_t_in_0_ptr_const_f32, conv2d_21_0_1_eltwise_22_conversion_t_out_0_ptr_s8, conv2d_21_0_1_eltwise_22_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, conv2d_21_0_1_eltwise_22_conversion_t_out_0_fmt_scale_const_f32, conv2d_21_0_1_eltwise_22_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(21, 1, {(stai_ptr) conv2d_21_0_1_eltwise_22_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_21_0_1_eltwise_22_conversion */
  /* LITE_KERNEL_SECTION BEGIN eltwise_22 */
  {
    
  forward_lite_eltwise_integer_INT8_eltwise_22(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_22 */
  /* LITE_KERNEL_SECTION BEGIN pad_23 */
  {
      const ai_ptr pad_23_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 96008);
    ai_ptr pad_23_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 173108);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(23, 1, {(stai_ptr) pad_23_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_23_t_in_0_ptr_const_ptr, pad_23_t_out_0_ptr_ptr, (ai_handle)(pad_23_v_pad_constant_value_const_s8), pad_23_t_in_0_fmt_bitsize_const_s16, pad_23_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(2048), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(23, 1, {(stai_ptr) pad_23_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_23 */
  /* LITE_KERNEL_SECTION BEGIN pad_23_0_0_conv2d_24_conversion */
  {
      const ai_i8* pad_23_0_0_conv2d_24_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 173108);
    ai_float* pad_23_0_0_conv2d_24_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 11008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(23, 1, {(stai_ptr) pad_23_0_0_conv2d_24_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(pad_23_0_0_conv2d_24_conversion_t_in_0_ptr_const_s8, pad_23_0_0_conv2d_24_conversion_t_out_0_ptr_f32, pad_23_0_0_conv2d_24_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, pad_23_0_0_conv2d_24_conversion_t_in_0_fmt_scale_const_f32, pad_23_0_0_conv2d_24_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(23, 1, {(stai_ptr) pad_23_0_0_conv2d_24_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END pad_23_0_0_conv2d_24_conversion */
  /* LITE_KERNEL_SECTION BEGIN conv2d_24 */
  {
      const ai_float* conv2d_24_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 11008);
    ai_float* conv2d_24_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 115208);
    const ai_u8* conv2d_24_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 309812);
    const ai_u8* conv2d_24_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 358964);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(24, 1, {(stai_ptr) conv2d_24_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_24_t_in_0_ptr_const_f32, conv2d_24_t_out_0_ptr_f32, conv2d_24_t_weight_0_ptr_const_u8, conv2d_24_t_weight_1_ptr_const_u8, conv2d_24_t_in_0_shape_ch_const_u32, conv2d_24_t_out_0_shape_ch_const_u32, conv2d_24_t_in_0_shape_w_const_u32, conv2d_24_t_in_0_shape_h_const_u32, conv2d_24_t_out_0_shape_w_const_u32, conv2d_24_t_out_0_shape_h_const_u32, conv2d_24_t_weight_0_shape_w_const_u32, conv2d_24_t_weight_0_shape_h_const_u32, conv2d_24_l_pad_W_0_const_s32, conv2d_24_l_pad_H_0_const_s32, conv2d_24_l_stride_1_const_u16, conv2d_24_l_stride_0_const_u16, 1, 33, conv2d_24_l_dilation_W_const_u16, conv2d_24_l_dilation_H_const_u16, conv2d_24_v_n_groups_const_size);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(24, 1, {(stai_ptr) conv2d_24_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_24 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_24_0_0_pad_25_conversion */
  {
      const ai_float* conv2d_24_0_0_pad_25_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 115208);
    ai_i8* conv2d_24_0_0_pad_25_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 11008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(24, 1, {(stai_ptr) conv2d_24_0_0_pad_25_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conv2d_24_0_0_pad_25_conversion_t_in_0_ptr_const_f32, conv2d_24_0_0_pad_25_conversion_t_out_0_ptr_s8, conv2d_24_0_0_pad_25_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, conv2d_24_0_0_pad_25_conversion_t_out_0_fmt_scale_const_f32, conv2d_24_0_0_pad_25_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(24, 1, {(stai_ptr) conv2d_24_0_0_pad_25_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_24_0_0_pad_25_conversion */
  /* LITE_KERNEL_SECTION BEGIN pad_25 */
  {
      const ai_ptr pad_25_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 11008);
    ai_ptr pad_25_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 74752);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(25, 1, {(stai_ptr) pad_25_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_25_t_in_0_ptr_const_ptr, pad_25_t_out_0_ptr_ptr, (ai_handle)(pad_25_v_pad_constant_value_const_s8), pad_25_t_in_0_fmt_bitsize_const_s16, pad_25_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(2048), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(25, 1, {(stai_ptr) pad_25_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_25 */
  /* LITE_KERNEL_SECTION BEGIN pad_25_0_0_conv2d_26_conversion */
  {
      const ai_i8* pad_25_0_0_conv2d_26_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 74752);
    ai_float* pad_25_0_0_conv2d_26_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 11008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(25, 1, {(stai_ptr) pad_25_0_0_conv2d_26_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(pad_25_0_0_conv2d_26_conversion_t_in_0_ptr_const_s8, pad_25_0_0_conv2d_26_conversion_t_out_0_ptr_f32, pad_25_0_0_conv2d_26_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, pad_25_0_0_conv2d_26_conversion_t_in_0_fmt_scale_const_f32, pad_25_0_0_conv2d_26_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(25, 1, {(stai_ptr) pad_25_0_0_conv2d_26_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END pad_25_0_0_conv2d_26_conversion */
  /* LITE_KERNEL_SECTION BEGIN conv2d_26 */
  {
      const ai_float* conv2d_26_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 11008);
    ai_float* conv2d_26_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 115208);
    const ai_u8* conv2d_26_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 359220);
    const ai_u8* conv2d_26_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 408372);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(26, 1, {(stai_ptr) conv2d_26_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_26_t_in_0_ptr_const_f32, conv2d_26_t_out_0_ptr_f32, conv2d_26_t_weight_0_ptr_const_u8, conv2d_26_t_weight_1_ptr_const_u8, conv2d_26_t_in_0_shape_ch_const_u32, conv2d_26_t_out_0_shape_ch_const_u32, conv2d_26_t_in_0_shape_w_const_u32, conv2d_26_t_in_0_shape_h_const_u32, conv2d_26_t_out_0_shape_w_const_u32, conv2d_26_t_out_0_shape_h_const_u32, conv2d_26_t_weight_0_shape_w_const_u32, conv2d_26_t_weight_0_shape_h_const_u32, conv2d_26_l_pad_W_0_const_s32, conv2d_26_l_pad_H_0_const_s32, conv2d_26_l_stride_1_const_u16, conv2d_26_l_stride_0_const_u16, 1, 33, conv2d_26_l_dilation_W_const_u16, conv2d_26_l_dilation_H_const_u16, conv2d_26_v_n_groups_const_size);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(26, 1, {(stai_ptr) conv2d_26_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_26 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_26_0_1_eltwise_27_conversion */
  {
      const ai_float* conv2d_26_0_1_eltwise_27_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 115208);
    ai_i8* conv2d_26_0_1_eltwise_27_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 11008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(26, 1, {(stai_ptr) conv2d_26_0_1_eltwise_27_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conv2d_26_0_1_eltwise_27_conversion_t_in_0_ptr_const_f32, conv2d_26_0_1_eltwise_27_conversion_t_out_0_ptr_s8, conv2d_26_0_1_eltwise_27_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, conv2d_26_0_1_eltwise_27_conversion_t_out_0_fmt_scale_const_f32, conv2d_26_0_1_eltwise_27_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(26, 1, {(stai_ptr) conv2d_26_0_1_eltwise_27_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_26_0_1_eltwise_27_conversion */
  /* LITE_KERNEL_SECTION BEGIN eltwise_27 */
  {
    
  forward_lite_eltwise_integer_INT8_eltwise_27(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_27 */
  /* LITE_KERNEL_SECTION BEGIN pad_28 */
  {
      const ai_ptr pad_28_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 76800);
    ai_ptr pad_28_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 171060);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(28, 1, {(stai_ptr) pad_28_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_28_t_in_0_ptr_const_ptr, pad_28_t_out_0_ptr_ptr, (ai_handle)(pad_28_v_pad_constant_value_const_s8), pad_28_t_in_0_fmt_bitsize_const_s16, pad_28_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(4096), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(28, 1, {(stai_ptr) pad_28_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_28 */
  /* LITE_KERNEL_SECTION BEGIN pad_28_0_0_conv2d_29_conversion */
  {
      const ai_i8* pad_28_0_0_conv2d_29_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 171060);
    ai_float* pad_28_0_0_conv2d_29_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 101172);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(28, 1, {(stai_ptr) pad_28_0_0_conv2d_29_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(pad_28_0_0_conv2d_29_conversion_t_in_0_ptr_const_s8, pad_28_0_0_conv2d_29_conversion_t_out_0_ptr_f32, pad_28_0_0_conv2d_29_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, pad_28_0_0_conv2d_29_conversion_t_in_0_fmt_scale_const_f32, pad_28_0_0_conv2d_29_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(28, 1, {(stai_ptr) pad_28_0_0_conv2d_29_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END pad_28_0_0_conv2d_29_conversion */
  /* LITE_KERNEL_SECTION BEGIN conv2d_29 */
  {
      const ai_float* conv2d_29_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 101172);
    ai_float* conv2d_29_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_u8* conv2d_29_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 408628);
    const ai_u8* conv2d_29_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 457780);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(29, 1, {(stai_ptr) conv2d_29_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_29_t_in_0_ptr_const_f32, conv2d_29_t_out_0_ptr_f32, conv2d_29_t_weight_0_ptr_const_u8, conv2d_29_t_weight_1_ptr_const_u8, conv2d_29_t_in_0_shape_ch_const_u32, conv2d_29_t_out_0_shape_ch_const_u32, conv2d_29_t_in_0_shape_w_const_u32, conv2d_29_t_in_0_shape_h_const_u32, conv2d_29_t_out_0_shape_w_const_u32, conv2d_29_t_out_0_shape_h_const_u32, conv2d_29_t_weight_0_shape_w_const_u32, conv2d_29_t_weight_0_shape_h_const_u32, conv2d_29_l_pad_W_0_const_s32, conv2d_29_l_pad_H_0_const_s32, conv2d_29_l_stride_1_const_u16, conv2d_29_l_stride_0_const_u16, 1, 65, conv2d_29_l_dilation_W_const_u16, conv2d_29_l_dilation_H_const_u16, conv2d_29_v_n_groups_const_size);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(29, 1, {(stai_ptr) conv2d_29_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_29 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_29_0_0_pad_30_conversion */
  {
      const ai_float* conv2d_29_0_0_pad_30_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_i8* conv2d_29_0_0_pad_30_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 96008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(29, 1, {(stai_ptr) conv2d_29_0_0_pad_30_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conv2d_29_0_0_pad_30_conversion_t_in_0_ptr_const_f32, conv2d_29_0_0_pad_30_conversion_t_out_0_ptr_s8, conv2d_29_0_0_pad_30_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, conv2d_29_0_0_pad_30_conversion_t_out_0_fmt_scale_const_f32, conv2d_29_0_0_pad_30_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(29, 1, {(stai_ptr) conv2d_29_0_0_pad_30_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_29_0_0_pad_30_conversion */
  /* LITE_KERNEL_SECTION BEGIN pad_30 */
  {
      const ai_ptr pad_30_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 96008);
    ai_ptr pad_30_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(30, 1, {(stai_ptr) pad_30_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_30_t_in_0_ptr_const_ptr, pad_30_t_out_0_ptr_ptr, (ai_handle)(pad_30_v_pad_constant_value_const_s8), pad_30_t_in_0_fmt_bitsize_const_s16, pad_30_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(4096), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(30, 1, {(stai_ptr) pad_30_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_30 */
  /* LITE_KERNEL_SECTION BEGIN pad_30_0_0_conv2d_31_conversion */
  {
      const ai_i8* pad_30_0_0_conv2d_31_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_float* pad_30_0_0_conv2d_31_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 96008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(30, 1, {(stai_ptr) pad_30_0_0_conv2d_31_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(pad_30_0_0_conv2d_31_conversion_t_in_0_ptr_const_s8, pad_30_0_0_conv2d_31_conversion_t_out_0_ptr_f32, pad_30_0_0_conv2d_31_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, pad_30_0_0_conv2d_31_conversion_t_in_0_fmt_scale_const_f32, pad_30_0_0_conv2d_31_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(30, 1, {(stai_ptr) pad_30_0_0_conv2d_31_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END pad_30_0_0_conv2d_31_conversion */
  /* LITE_KERNEL_SECTION BEGIN conv2d_31 */
  {
      const ai_float* conv2d_31_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 96008);
    ai_float* conv2d_31_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_u8* conv2d_31_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 458036);
    const ai_u8* conv2d_31_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 507188);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(31, 1, {(stai_ptr) conv2d_31_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_31_t_in_0_ptr_const_f32, conv2d_31_t_out_0_ptr_f32, conv2d_31_t_weight_0_ptr_const_u8, conv2d_31_t_weight_1_ptr_const_u8, conv2d_31_t_in_0_shape_ch_const_u32, conv2d_31_t_out_0_shape_ch_const_u32, conv2d_31_t_in_0_shape_w_const_u32, conv2d_31_t_in_0_shape_h_const_u32, conv2d_31_t_out_0_shape_w_const_u32, conv2d_31_t_out_0_shape_h_const_u32, conv2d_31_t_weight_0_shape_w_const_u32, conv2d_31_t_weight_0_shape_h_const_u32, conv2d_31_l_pad_W_0_const_s32, conv2d_31_l_pad_H_0_const_s32, conv2d_31_l_stride_1_const_u16, conv2d_31_l_stride_0_const_u16, 1, 65, conv2d_31_l_dilation_W_const_u16, conv2d_31_l_dilation_H_const_u16, conv2d_31_v_n_groups_const_size);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(31, 1, {(stai_ptr) conv2d_31_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_31 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_31_0_1_eltwise_32_conversion */
  {
      const ai_float* conv2d_31_0_1_eltwise_32_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_i8* conv2d_31_0_1_eltwise_32_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 96008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(31, 1, {(stai_ptr) conv2d_31_0_1_eltwise_32_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conv2d_31_0_1_eltwise_32_conversion_t_in_0_ptr_const_f32, conv2d_31_0_1_eltwise_32_conversion_t_out_0_ptr_s8, conv2d_31_0_1_eltwise_32_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, conv2d_31_0_1_eltwise_32_conversion_t_out_0_fmt_scale_const_f32, conv2d_31_0_1_eltwise_32_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(31, 1, {(stai_ptr) conv2d_31_0_1_eltwise_32_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_31_0_1_eltwise_32_conversion */
  /* LITE_KERNEL_SECTION BEGIN eltwise_32 */
  {
    
  forward_lite_eltwise_integer_INT8_eltwise_32(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_32 */
  /* LITE_KERNEL_SECTION BEGIN pad_33 */
  {
      const ai_ptr pad_33_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 0);
    ai_ptr pad_33_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 19200);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(33, 1, {(stai_ptr) pad_33_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_33_t_in_0_ptr_const_ptr, pad_33_t_out_0_ptr_ptr, (ai_handle)(pad_33_v_pad_constant_value_const_s8), pad_33_t_in_0_fmt_bitsize_const_s16, pad_33_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(4096), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(33, 1, {(stai_ptr) pad_33_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_33 */
  /* LITE_KERNEL_SECTION BEGIN pad_33_0_0_conv2d_34_conversion */
  {
      const ai_i8* pad_33_0_0_conv2d_34_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 19200);
    ai_float* pad_33_0_0_conv2d_34_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 96008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(33, 1, {(stai_ptr) pad_33_0_0_conv2d_34_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(pad_33_0_0_conv2d_34_conversion_t_in_0_ptr_const_s8, pad_33_0_0_conv2d_34_conversion_t_out_0_ptr_f32, pad_33_0_0_conv2d_34_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, pad_33_0_0_conv2d_34_conversion_t_in_0_fmt_scale_const_f32, pad_33_0_0_conv2d_34_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(33, 1, {(stai_ptr) pad_33_0_0_conv2d_34_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END pad_33_0_0_conv2d_34_conversion */
  /* LITE_KERNEL_SECTION BEGIN conv2d_34 */
  {
      const ai_float* conv2d_34_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 96008);
    ai_float* conv2d_34_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
    const ai_u8* conv2d_34_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 507444);
    const ai_u8* conv2d_34_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 556596);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(34, 1, {(stai_ptr) conv2d_34_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_34_t_in_0_ptr_const_f32, conv2d_34_t_out_0_ptr_f32, conv2d_34_t_weight_0_ptr_const_u8, conv2d_34_t_weight_1_ptr_const_u8, conv2d_34_t_in_0_shape_ch_const_u32, conv2d_34_t_out_0_shape_ch_const_u32, conv2d_34_t_in_0_shape_w_const_u32, conv2d_34_t_in_0_shape_h_const_u32, conv2d_34_t_out_0_shape_w_const_u32, conv2d_34_t_out_0_shape_h_const_u32, conv2d_34_t_weight_0_shape_w_const_u32, conv2d_34_t_weight_0_shape_h_const_u32, conv2d_34_l_pad_W_0_const_s32, conv2d_34_l_pad_H_0_const_s32, conv2d_34_l_stride_1_const_u16, conv2d_34_l_stride_0_const_u16, 1, 65, conv2d_34_l_dilation_W_const_u16, conv2d_34_l_dilation_H_const_u16, conv2d_34_v_n_groups_const_size);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(34, 1, {(stai_ptr) conv2d_34_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_34 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_34_0_0_pad_35_conversion */
  {
      const ai_float* conv2d_34_0_0_pad_35_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
    ai_i8* conv2d_34_0_0_pad_35_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 96008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(34, 1, {(stai_ptr) conv2d_34_0_0_pad_35_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conv2d_34_0_0_pad_35_conversion_t_in_0_ptr_const_f32, conv2d_34_0_0_pad_35_conversion_t_out_0_ptr_s8, conv2d_34_0_0_pad_35_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, conv2d_34_0_0_pad_35_conversion_t_out_0_fmt_scale_const_f32, conv2d_34_0_0_pad_35_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(34, 1, {(stai_ptr) conv2d_34_0_0_pad_35_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_34_0_0_pad_35_conversion */
  /* LITE_KERNEL_SECTION BEGIN pad_35 */
  {
      const ai_ptr pad_35_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 96008);
    ai_ptr pad_35_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 19200);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(35, 1, {(stai_ptr) pad_35_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(pad_35_t_in_0_ptr_const_ptr, pad_35_t_out_0_ptr_ptr, (ai_handle)(pad_35_v_pad_constant_value_const_s8), pad_35_t_in_0_fmt_bitsize_const_s16, pad_35_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(19200), (ai_i32)(0), (ai_i32)(0), (ai_i32)(4096), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(35, 1, {(stai_ptr) pad_35_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END pad_35 */
  /* LITE_KERNEL_SECTION BEGIN pad_35_0_0_conv2d_36_conversion */
  {
      const ai_i8* pad_35_0_0_conv2d_36_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 19200);
    ai_float* pad_35_0_0_conv2d_36_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 96008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(35, 1, {(stai_ptr) pad_35_0_0_conv2d_36_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(pad_35_0_0_conv2d_36_conversion_t_in_0_ptr_const_s8, pad_35_0_0_conv2d_36_conversion_t_out_0_ptr_f32, pad_35_0_0_conv2d_36_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, pad_35_0_0_conv2d_36_conversion_t_in_0_fmt_scale_const_f32, pad_35_0_0_conv2d_36_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(35, 1, {(stai_ptr) pad_35_0_0_conv2d_36_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END pad_35_0_0_conv2d_36_conversion */
  /* LITE_KERNEL_SECTION BEGIN conv2d_36 */
  {
      const ai_float* conv2d_36_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 96008);
    ai_float* conv2d_36_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
    const ai_u8* conv2d_36_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 556852);
    const ai_u8* conv2d_36_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 606004);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(36, 1, {(stai_ptr) conv2d_36_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32_group(conv2d_36_t_in_0_ptr_const_f32, conv2d_36_t_out_0_ptr_f32, conv2d_36_t_weight_0_ptr_const_u8, conv2d_36_t_weight_1_ptr_const_u8, conv2d_36_t_in_0_shape_ch_const_u32, conv2d_36_t_out_0_shape_ch_const_u32, conv2d_36_t_in_0_shape_w_const_u32, conv2d_36_t_in_0_shape_h_const_u32, conv2d_36_t_out_0_shape_w_const_u32, conv2d_36_t_out_0_shape_h_const_u32, conv2d_36_t_weight_0_shape_w_const_u32, conv2d_36_t_weight_0_shape_h_const_u32, conv2d_36_l_pad_W_0_const_s32, conv2d_36_l_pad_H_0_const_s32, conv2d_36_l_stride_1_const_u16, conv2d_36_l_stride_0_const_u16, 1, 65, conv2d_36_l_dilation_W_const_u16, conv2d_36_l_dilation_H_const_u16, conv2d_36_v_n_groups_const_size);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(36, 1, {(stai_ptr) conv2d_36_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conv2d_36 */
  /* LITE_KERNEL_SECTION BEGIN conv2d_36_0_1_eltwise_37_conversion */
  {
      const ai_float* conv2d_36_0_1_eltwise_37_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
    ai_i8* conv2d_36_0_1_eltwise_37_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 96008);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(36, 1, {(stai_ptr) conv2d_36_0_1_eltwise_37_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(conv2d_36_0_1_eltwise_37_conversion_t_in_0_ptr_const_f32, conv2d_36_0_1_eltwise_37_conversion_t_out_0_ptr_s8, conv2d_36_0_1_eltwise_37_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, conv2d_36_0_1_eltwise_37_conversion_t_out_0_fmt_scale_const_f32, conv2d_36_0_1_eltwise_37_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(36, 1, {(stai_ptr) conv2d_36_0_1_eltwise_37_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END conv2d_36_0_1_eltwise_37_conversion */
  /* LITE_KERNEL_SECTION BEGIN eltwise_37 */
  {
    
  forward_lite_eltwise_integer_INT8_eltwise_37(net_ctx);
  }
  /* LITE_KERNEL_SECTION END eltwise_37 */
  /* LITE_KERNEL_SECTION BEGIN slice_42_gather_0 */
  {
    
  forward_lite_gather_slice_42_gather_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END slice_42_gather_0 */
  /* LITE_KERNEL_SECTION BEGIN concat_44 */
  {
    
  forward_lite_concat_concat_44(net_ctx);
  }
  /* LITE_KERNEL_SECTION END concat_44 */
  /* LITE_KERNEL_SECTION BEGIN gemm_45 */
  {
    
  forward_lite_dense_integer_SSSA_ch_gemm_45(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_45 */
  /* LITE_KERNEL_SECTION BEGIN gemm_46 */
  {
    
  forward_lite_dense_integer_SSSA_ch_gemm_46(net_ctx);
  }
  /* LITE_KERNEL_SECTION END gemm_46 */
  /* LITE_KERNEL_SECTION BEGIN conversion_47 */
  {
      const ai_i8* conversion_47_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 100);
    ai_float* conversion_47_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_outputs[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(47, 1, {(stai_ptr) conversion_47_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(conversion_47_t_in_0_ptr_const_s8, conversion_47_t_out_0_ptr_f32, conversion_47_t_out_0_shape_h_w_ch_d_prod_const_u32, conversion_47_t_in_0_fmt_scale_const_f32, conversion_47_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(47, 1, {(stai_ptr) conversion_47_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END conversion_47 */
  return net_ctx->_return_code;
}

/*****************************************************************************/
/*  Getters APIs Section  */
STAI_API_ENTRY
stai_size stai_network_get_context_size()
{
  return (stai_size)STAI_NETWORK_CONTEXT_SIZE;
}

#if defined(HAVE_NETWORK_INFO)
STAI_API_ENTRY
stai_return_code stai_network_get_info(
  stai_network* network,
  stai_network_info* info)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, info==NULL, STAI_ERROR_NETWORK_INVALID_INFO, net_ctx->_return_code)

  // Copy of network info struct
  *info = g_network_info;

  return STAI_SUCCESS;
}
#endif


STAI_API_ENTRY
stai_return_code stai_network_get_activations(
  stai_network* network, stai_ptr* activations, stai_size* n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, !n_activations, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_activations = STAI_NETWORK_ACTIVATIONS_NUM;
for (stai_size idx=0; activations && (idx<STAI_NETWORK_ACTIVATIONS_NUM); idx++) {
    // get address of the activations buffers
    activations[idx] = net_ctx->_activations[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_weights(
  stai_network* network, stai_ptr* weights, stai_size* n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_weights, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_weights = STAI_NETWORK_WEIGHTS_NUM;
for (stai_size idx=0; weights && (idx<STAI_NETWORK_WEIGHTS_NUM); idx++) {
    // get address of the weights buffers
    weights[idx] = net_ctx->_weights[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_inputs(
  stai_network* network, stai_ptr* inputs, stai_size* n_inputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_inputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_inputs = STAI_NETWORK_IN_NUM;
  for (stai_size idx=0; inputs && (idx<STAI_NETWORK_IN_NUM); idx++) {
    inputs[idx] = net_ctx->_inputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_outputs(
  stai_network* network, stai_ptr* outputs, stai_size* n_outputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_outputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_outputs = STAI_NETWORK_OUT_NUM;
  for (stai_size idx=0; outputs && (idx<STAI_NETWORK_OUT_NUM); idx++) {
    outputs[idx] = net_ctx->_outputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_error(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /* return 1st generated error or STAI_SUCCESS if no errors so far */
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_states(
  stai_network* network, stai_ptr* states, stai_size* n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_states, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  /* get the number of internals states (supporting multi-heap also for internal states) */
  *n_states = STAI_NETWORK_STATES_NUM;

  STAI_UNUSED(states)
return net_ctx->_return_code;
}


/*****************************************************************************/
/*  Setters APIs Section  */

STAI_API_ENTRY
stai_return_code stai_network_set_activations(
  stai_network* network,
  const stai_ptr* activations,
  const stai_size n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _activations_alignment[] = STAI_NETWORK_ACTIVATIONS_ALIGNMENTS;
  STAI_PRINT("  [stai_network_set_activations] network(%p) activations[%d]: %p\n\n", net_ctx, n_activations, activations)
  _STAI_SET_ERROR(net_ctx, !activations,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_activations!=STAI_NETWORK_ACTIVATIONS_NUM,
                  STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_NUM, net_ctx->_return_code)

  for (stai_size idx=0; activations && idx<STAI_NETWORK_ACTIVATIONS_NUM; idx++) {
    STAI_PRINT("  activation[%d]: %p\n", idx, activations[idx])
    _STAI_SET_ERROR(net_ctx, activations[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)activations[idx]) & (_activations_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_activations[idx] = activations[idx];
  }_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_weights(
  stai_network* network,
  const stai_ptr* weights,
  const stai_size n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _weights_alignment[] = STAI_NETWORK_WEIGHTS_ALIGNMENTS;
  _STAI_SET_ERROR(net_ctx, !weights,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_weights!=STAI_NETWORK_WEIGHTS_NUM,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_NUM, net_ctx->_return_code)
  for (stai_size idx=0; weights && idx<STAI_NETWORK_WEIGHTS_NUM; idx++) {
    STAI_PRINT("  weight[%d]: %p\n", idx, weights[idx])
    _STAI_SET_ERROR(net_ctx, weights[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)weights[idx]) & (_weights_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_weights[idx] = weights[idx];
  }_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_inputs(
  stai_network* network,
  const stai_ptr* inputs,
  const stai_size n_inputs)
{
  const uintptr_t _inputs_alignment[] = STAI_NETWORK_IN_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !inputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_inputs!=STAI_NETWORK_IN_NUM,
                  STAI_ERROR_NETWORK_INVALID_IN_NUM, net_ctx->_return_code)

  for (stai_size idx=0; inputs && idx<STAI_NETWORK_IN_NUM; idx++) {
    STAI_PRINT("  input[%d]: %p\n", idx, inputs[idx])
    _STAI_SET_ERROR(net_ctx, inputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)inputs[idx]) & (_inputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_inputs[idx] = inputs[idx];
  }

  _stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_outputs(
  stai_network* network,
  const stai_ptr* outputs,
  const stai_size n_outputs)
{
  const uintptr_t _outputs_alignment[] = STAI_NETWORK_OUT_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !outputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_outputs!=STAI_NETWORK_OUT_NUM,
                  STAI_ERROR_NETWORK_INVALID_OUT_NUM, net_ctx->_return_code)

  for (stai_size idx=0; outputs && idx<n_outputs; idx++) {
    STAI_PRINT("  output[%d]: %p\n", idx, outputs[idx])
    _STAI_SET_ERROR(net_ctx, outputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)outputs[idx]) & (_outputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_outputs[idx] = outputs[idx];
  }

  _stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_states(
  stai_network* network,
  const stai_ptr* states,
  const stai_size n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  STAI_UNUSED(states)
  STAI_UNUSED(n_states)
_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}

STAI_API_ENTRY
stai_return_code stai_network_set_callback(
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
#undef _STAI_NETWORK_EVENT_NODE_START_CB
#undef _STAI_NETWORK_EVENT_NODE_STOP_CB
#undef _STAI_NETWORK_MODEL_SIGNATURE
#undef _STAI_NETWORK_DATETIME
#undef _STAI_NETWORK_COMPILE_DATETIME

