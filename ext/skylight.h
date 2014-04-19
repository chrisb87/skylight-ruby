#ifndef __SKYLIGHT_H__
#define __SKYLIGHT_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define SK_FALSE 0
#define SK_TRUE 1

typedef uint8_t sk_bool;

// TODO: remove rust_str
typedef struct {
  size_t fill;    // in bytes; if zero, heapified
  size_t alloc;   // in bytes
  uint8_t data[0];
} rust_buf_t;

typedef struct {
  void* data;
  uintptr_t len;
} sk_buf_t;

// Private, only used as a token
typedef struct {
  uintptr_t priv_a;
  uintptr_t priv_b;
  uint8_t priv_c;
} sk_frame_t;

/*
// Used as a token when registering the full stack
pub struct StackFrame {
  span: uint,

  // The number of open probes traversed by the stack
  probes_seen: uint,

  // Tracks whether or not the frames that have been registered are in scope.
  // u8 is used to make C integration easier
  in_scope: u8
}
 */

/*
 *
 * ===== Types =====
 *
 */

typedef void sk_hello_t;
typedef void sk_error_t;
typedef void sk_trace_t;
typedef void sk_batch_t;
typedef void sk_serializer_t;

/*
 *
 * ===== Util =====
 *
 */

void sk_free_buf(rust_buf_t*);

/*
 *
 * ===== High resolution timer =====
 *
 */

uint64_t sk_high_res_time(void);

/*
 *
 * ===== Hello =====
 *
 */

sk_hello_t* sk_hello_new(sk_buf_t, uint32_t);

sk_hello_t* sk_hello_load(sk_buf_t);

void sk_hello_free(sk_hello_t*);

void sk_hello_cmd_add(sk_hello_t*, sk_buf_t);

sk_buf_t sk_hello_get_version(sk_hello_t*);

uint32_t sk_hello_cmd_length(sk_hello_t*);

sk_buf_t sk_hello_get_cmd(sk_hello_t*, uint32_t);

sk_serializer_t* sk_hello_get_serializer(sk_hello_t*);

int sk_hello_serialize(sk_hello_t*, sk_serializer_t*, sk_buf_t);

/*
 *
 * ===== Trace =====
 *
 */

sk_trace_t* sk_trace_new(uint64_t start, sk_buf_t uuid);

void sk_trace_free(sk_trace_t*);

uint64_t sk_trace_get_started_at(sk_trace_t*);

void sk_trace_set_name(sk_trace_t*, sk_buf_t);

sk_buf_t sk_trace_get_name(sk_trace_t*);

sk_buf_t sk_trace_get_uuid(sk_trace_t*);

uint32_t sk_trace_span_start(sk_trace_t*, uint64_t, sk_buf_t);

void sk_trace_span_set_title(sk_trace_t*, uint32_t, sk_buf_t);

void sk_trace_span_set_description(sk_trace_t*, uint32_t, sk_buf_t);

void sk_trace_span_stop(sk_trace_t*, uint32_t, uint64_t);

rust_buf_t* sk_trace_name_from_serialized_into_new_buffer(sk_buf_t);

sk_serializer_t* sk_trace_get_serializer(sk_trace_t*);

int sk_trace_serialize(sk_trace_t*, sk_serializer_t*, sk_buf_t);

/*
 *
 * ===== CPU Profiling =====
 *
 */

void sk_trace_add_stack_frame_filter(sk_trace_t*, sk_buf_t);

sk_frame_t sk_trace_register_stack_frame(sk_trace_t*, uintptr_t, sk_bool);

sk_frame_t sk_trace_register_stack_frame_under(sk_trace_t*, sk_frame_t, uintptr_t, sk_bool);

void sk_trace_register_stack_frame_end(sk_trace_t*, sk_frame_t);

sk_bool sk_trace_stack_frame_has_details(sk_trace_t*, sk_frame_t);

void sk_trace_stack_frame_set_details(sk_trace_t*, sk_frame_t, sk_buf_t, sk_buf_t);

/*
 *
 * ===== Batch =====
 *
 */

sk_batch_t* sk_batch_new(uint32_t, sk_buf_t);

void sk_batch_free(sk_batch_t*);

void sk_batch_set_endpoint_count(sk_batch_t*, sk_buf_t, uint64_t);

void sk_batch_move_in(sk_batch_t*, sk_buf_t);

sk_serializer_t* sk_batch_get_serializer(sk_batch_t*);

int sk_batch_serialize(sk_batch_t*, sk_serializer_t*, sk_buf_t);

/*
 *
 * ===== Error =====
 *
 */

sk_error_t* sk_error_new(sk_buf_t, sk_buf_t);

void sk_error_free(sk_error_t*);

sk_error_t* sk_error_load(sk_buf_t);

sk_buf_t sk_error_get_group(sk_error_t*);

sk_buf_t sk_error_get_description(sk_error_t*);

sk_buf_t sk_error_get_details(sk_error_t*);

void sk_error_set_details(sk_error_t*, sk_buf_t);

sk_serializer_t* sk_error_get_serializer(sk_error_t*);

int sk_error_serialize(sk_error_t*, sk_serializer_t*, sk_buf_t);

/*
 *
 * ===== Serializer =====
 *
 */

void sk_serializer_free(sk_serializer_t*);

size_t sk_serializer_get_serialized_size(sk_serializer_t*);

#endif
