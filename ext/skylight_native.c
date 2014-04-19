#include <skylight.h>
#include <skylight_native.h>
#include <ruby.h>

#ifdef HAVE_RUBY_ENCODING_H
#include <ruby/encoding.h>
#endif

/*
 *
 * ===== Error messages =====
 *
 */

static const char* rb_hello_is_freed_err =
  "You can't do anything with a Hello once it's been serialized";

static const char* rb_trace_is_freed_err =
  "You can't do anything with a Trace once it's been serialized or moved into a Batch";

static const char* rb_batch_is_freed_err =
  "You can't do anything with a Batch once it's been serialized";

static const char* rb_error_is_freed_err =
  "You can't do anything with a Error once it's been serialized";

/*
 *
 * ===== Ruby helpers =====
 *
 */

#define TO_S(VAL) \
  RSTRING_PTR(rb_funcall(VAL, rb_intern("to_s"), 0))

#define CHECK_TYPE(VAL, T)                        \
  do {                                            \
    if (TYPE(VAL) != T) {                         \
      rb_raise(rb_eArgError, "expected " #VAL " to be " #T " but was '%s' (%s [%i])", \
                TO_S(VAL), rb_obj_classname(VAL), TYPE(VAL)); \
      return Qnil;                                \
    }                                             \
  } while(0)                                      \

#define CHECK_NUMERIC(VAL)                        \
  do {                                            \
    if (TYPE(VAL) != T_BIGNUM &&                  \
        TYPE(VAL) != T_FIXNUM) {                  \
      rb_raise(rb_eArgError, "expected " #VAL " to be numeric but was '%s' (%s [%i])", \
                TO_S(VAL), rb_obj_classname(VAL), TYPE(VAL)); \
      return Qnil;                                \
    }                                             \
  } while(0)                                      \

#define My_Struct(name, type, msg)                \
  Get_Struct(name, self, type, msg);              \

#define Transfer_My_Struct(name, type, msg)       \
  My_Struct(name, type, msg);                     \
  DATA_PTR(self) = NULL;                          \

#define Transfer_Struct(name, obj, type, msg)     \
  Get_Struct(name, obj, type, msg);               \
  DATA_PTR(obj) = NULL;                           \

#define Get_Struct(name, obj, type, msg)          \
  type name;                                      \
  Data_Get_Struct(obj, type, name);               \
  if (name == NULL) {                             \
    rb_raise(rb_eRuntimeError, "%s", msg);        \
  }                                               \

#define VEC2STR(vector)                               \
  ({                                                  \
    rust_buf_t* v = (vector);                         \
    VALUE ret = rb_str_new((char *)v->data, v->fill); \
    ret;                                              \
  })

#define BUF2STR(buf)                    \
  ({                                    \
    sk_buf_t b = (buf);                 \
    rb_str_from_buf_len(b.data, b.len); \
  })

#define SERIALIZE(MSG)                                                  \
  ({                                                                    \
    VALUE ret;                                                          \
    size_t size;                                                        \
    sk_serializer_t* serializer;                                        \
                                                                        \
    if (!MSG) {                                                         \
      ret = Qnil;                                                       \
    }                                                                   \
    else {                                                              \
      serializer = sk_ ## MSG ## _get_serializer(MSG);                  \
                                                                        \
      if (!serializer) {                                                \
        ret = Qnil;                                                     \
      }                                                                 \
      else {                                                            \
        size = sk_serializer_get_serialized_size(serializer);           \
        ret = rb_buf_new(size);                                         \
                                                                        \
        if (sk_ ## MSG ## _serialize(MSG, serializer, STR2BUF(ret))) {  \
          ret = Qnil;                                                   \
        }                                                               \
                                                                        \
        sk_serializer_free(serializer);                                 \
      }                                                                 \
    }                                                                   \
                                                                        \
    sk_ ## MSG ## _free(MSG);                                           \
    ret;                                                                \
  })

static inline VALUE
rb_str_from_buf_len(void* data, uintptr_t len) {
  if (data == NULL) {
    return Qnil;
  }
  else {
    VALUE ret = rb_str_new(data, len);
    rb_enc_associate(ret, rb_utf8_encoding());
    return ret;
  }
}

static inline VALUE
rb_buf_new(long len) {
  return rb_str_new(NULL, len);
}

/**
 * Ruby types defined here
 */

static VALUE rb_mSkylight;
static VALUE rb_mUtil;
static VALUE rb_cClock;
static VALUE rb_cHello;
static VALUE rb_cError;
static VALUE rb_cTrace;
static VALUE rb_cBatch;

/**
 * class Skylight::Util::Clock
 */

static VALUE
clock_high_res_time(VALUE self) {
  uint64_t time = sk_high_res_time();
  return ULL2NUM(time);
}

/**
 * class Skylight::Hello
 */

static VALUE
hello_new(VALUE klass, VALUE version, VALUE config) {
  sk_hello_t* hello;

  CHECK_TYPE(version, T_STRING);
  CHECK_TYPE(config, T_FIXNUM);

  hello = sk_hello_new(STR2BUF(version), FIX2INT(config));

  if (hello == NULL) {
    return Qnil;
  }

  return Data_Wrap_Struct(rb_cHello, NULL, sk_hello_free, hello);
}

static VALUE
hello_load(VALUE self, VALUE protobuf) {
  sk_hello_t* hello;

  CHECK_TYPE(protobuf, T_STRING);

  hello = sk_hello_load(STR2BUF(protobuf));

  if (hello == NULL) {
    return Qnil;
  }

  return Data_Wrap_Struct(rb_cHello, NULL, sk_hello_free, hello);
}


static VALUE
hello_get_version(VALUE self) {
  My_Struct(hello, sk_hello_t*, rb_hello_is_freed_err);
  return BUF2STR(sk_hello_get_version(hello));
}

static VALUE
hello_cmd_length(VALUE self) {
  My_Struct(hello, sk_hello_t*, rb_hello_is_freed_err);
  return UINT2NUM(sk_hello_cmd_length(hello));
}

static VALUE
hello_add_cmd_part(VALUE self, VALUE val) {
  CHECK_TYPE(val, T_STRING);

  My_Struct(hello, sk_hello_t*, rb_hello_is_freed_err);
  sk_hello_cmd_add(hello, STR2BUF(val));

  return Qnil;
}

static VALUE
hello_cmd_get(VALUE self, VALUE idx) {
  CHECK_TYPE(idx, T_FIXNUM);

  My_Struct(hello, sk_hello_t*, rb_hello_is_freed_err);
  return BUF2STR(sk_hello_get_cmd(hello, FIX2INT(idx)));
}

static VALUE
hello_serialize(VALUE self) {
  Transfer_My_Struct(hello, sk_hello_t*, rb_hello_is_freed_err);
  return SERIALIZE(hello);
}

/**
 * Skylight::Trace
 */

static VALUE
trace_new(VALUE self, VALUE started_at, VALUE uuid) {
  sk_trace_t* trace;

  CHECK_NUMERIC(started_at);
  CHECK_TYPE(uuid, T_STRING);

  trace = sk_trace_new(NUM2ULL(started_at), STR2BUF(uuid));

  if (trace == NULL) {
    return Qnil;
  }

  return Data_Wrap_Struct(rb_cTrace, NULL, sk_trace_free, trace);
}

static VALUE
trace_name_from_serialized(VALUE self, VALUE data) {

  VALUE ret;
  rust_buf_t* name;

  CHECK_TYPE(data, T_STRING);

  name = sk_trace_name_from_serialized_into_new_buffer(STR2BUF(data));

  if (name == NULL) {
    return Qnil;
  }

  ret = VEC2STR(name);
  sk_free_buf(name);
  return ret;
}

static VALUE
trace_get_started_at(VALUE self) {
  My_Struct(trace, sk_trace_t*, rb_trace_is_freed_err);
  return ULL2NUM(sk_trace_get_started_at(trace));
}

static VALUE
trace_set_name(VALUE self, VALUE name) {
  CHECK_TYPE(name, T_STRING);

  My_Struct(trace, sk_trace_t*, rb_trace_is_freed_err);
  sk_trace_set_name(trace, STR2BUF(name));

  return Qnil;
}

static VALUE
trace_get_name(VALUE self) {
  My_Struct(trace, sk_trace_t*, rb_trace_is_freed_err);
  return BUF2STR(sk_trace_get_name(trace));
}

static VALUE
trace_get_uuid(VALUE self) {
  My_Struct(trace, sk_trace_t*, rb_trace_is_freed_err);
  return BUF2STR(sk_trace_get_uuid(trace));
}

static VALUE
trace_start_span(VALUE self, VALUE time, VALUE category) {
  CHECK_NUMERIC(time);
  CHECK_TYPE(category, T_STRING);

  My_Struct(trace, sk_trace_t*, rb_trace_is_freed_err);

  sk_cpu_prof_start_span(trace);
  return UINT2NUM(sk_trace_span_start(trace, NUM2ULL(time), STR2BUF(category)));
}

static VALUE
trace_stop_span(VALUE self, VALUE idx, VALUE time) {
  CHECK_NUMERIC(time);
  CHECK_TYPE(idx, T_FIXNUM);

  My_Struct(trace, sk_trace_t*, rb_trace_is_freed_err);

  sk_cpu_prof_stop_span(trace);
  sk_trace_span_stop(trace, FIX2UINT(idx), NUM2ULL(time));

  return Qnil;
}

static VALUE
trace_span_set_title(VALUE self, VALUE idx, VALUE val) {
  CHECK_TYPE(idx, T_FIXNUM);
  CHECK_TYPE(val, T_STRING);

  My_Struct(trace, sk_trace_t*, rb_trace_is_freed_err);
  sk_trace_span_set_title(trace, FIX2UINT(idx), STR2BUF(val));

  return Qnil;
}

static VALUE
trace_span_set_description(VALUE self, VALUE idx, VALUE val) {
  CHECK_TYPE(idx, T_FIXNUM);
  CHECK_TYPE(val, T_STRING);

  My_Struct(trace, sk_trace_t*, rb_trace_is_freed_err);
  sk_trace_span_set_description(trace, FIX2UINT(idx), STR2BUF(val));

  return Qnil;
}

static VALUE
trace_sample_stack(VALUE self, VALUE th) {
  // TODO: check type of thread
  My_Struct(trace, sk_trace_t*, rb_trace_is_freed_err);
  sk_cpu_prof_sample_stack(trace, th);

  return Qnil;
}

static VALUE
trace_serialize(VALUE self) {
  Transfer_My_Struct(trace, sk_trace_t*, rb_trace_is_freed_err);
  return SERIALIZE(trace);
}

static VALUE
trace_add_stack_frame_filter(VALUE self, VALUE filter) {
  // Safety first, always
  if (filter == Qnil)
    return Qnil;

  CHECK_TYPE(filter, T_STRING);

  My_Struct(trace, sk_trace_t*, rb_trace_is_freed_err);
  sk_trace_add_stack_frame_filter(trace, STR2BUF(filter));

  return Qnil;
}

/**
 * class Skylight::Batch
 */

static VALUE
batch_new(VALUE self, VALUE timestamp, VALUE hostname) {
  sk_batch_t* batch;

  CHECK_NUMERIC(timestamp);
  CHECK_TYPE(hostname, T_STRING);

  // TODO: Safe check
  batch = sk_batch_new((uint32_t) NUM2ULONG(timestamp), STR2BUF(hostname));

  if (batch == NULL) {
    return Qnil;
  }

  return Data_Wrap_Struct(rb_cBatch, NULL, sk_batch_free, batch);
}

static VALUE
batch_set_endpoint_count(VALUE self, VALUE endpoint, VALUE count) {

  CHECK_TYPE(endpoint, T_STRING);
  CHECK_NUMERIC(count);

  My_Struct(batch, sk_batch_t*, rb_batch_is_freed_err);
  sk_batch_set_endpoint_count(batch, STR2BUF(endpoint), NUM2ULL(count));

  return Qnil;
}

static VALUE
batch_move_in(VALUE self, VALUE trace) {
  CHECK_TYPE(trace, T_STRING);

  My_Struct(batch, sk_batch_t*, rb_batch_is_freed_err);
  sk_batch_move_in(batch, STR2BUF(trace));

  return Qnil;
}

static VALUE
batch_serialize(VALUE self) {
  Transfer_My_Struct(batch, sk_batch_t*, rb_batch_is_freed_err);
  return SERIALIZE(batch);
}

/**
 * class Skylight::Error
 */

static VALUE
error_new(VALUE self, VALUE group, VALUE desc) {
  sk_error_t* error;

  CHECK_TYPE(group, T_STRING);
  CHECK_TYPE(desc, T_STRING);

  error = sk_error_new(STR2BUF(group), STR2BUF(desc));

  if (error == NULL) {
    return Qnil;
  }

  return Data_Wrap_Struct(rb_cError, NULL, sk_error_free, error);
}

static VALUE
error_load(VALUE self, VALUE protobuf) {
  sk_error_t* error;

  CHECK_TYPE(protobuf, T_STRING);

  error = sk_error_load(STR2BUF(protobuf));

  if (error == NULL) {
    return Qnil;
  }

  return Data_Wrap_Struct(rb_cError, NULL, sk_error_free, error);
}


static VALUE
error_get_group(VALUE self) {
  My_Struct(error, sk_error_t*, rb_error_is_freed_err);
  return BUF2STR(sk_error_get_group(error));
}

static VALUE
error_get_description(VALUE self) {
  My_Struct(error, sk_error_t*, rb_error_is_freed_err);
  return BUF2STR(sk_error_get_description(error));
}

static VALUE
error_get_details(VALUE self) {
  My_Struct(error, sk_error_t*, rb_error_is_freed_err);
  return BUF2STR(sk_error_get_details(error));
}

static VALUE
error_set_details(VALUE self, VALUE details) {
  CHECK_TYPE(details, T_STRING);

  My_Struct(error, sk_error_t*, rb_error_is_freed_err);
  sk_error_set_details(error, STR2BUF(details));

  return Qnil;
}

static VALUE
error_serialize(VALUE self) {
  Transfer_My_Struct(error, sk_error_t*, rb_batch_is_freed_err);
  return SERIALIZE(error);
}

void Init_skylight_native() {
  rb_mSkylight = rb_define_module("Skylight");
  rb_mUtil  = rb_define_module_under(rb_mSkylight, "Util");

  rb_cClock = rb_define_class_under(rb_mUtil, "Clock", rb_cObject);
  rb_define_method(rb_cClock, "native_hrtime", clock_high_res_time, 0);

  rb_cHello = rb_define_class_under(rb_mSkylight, "Hello", rb_cObject);
  rb_define_singleton_method(rb_cHello, "native_new", hello_new, 2);
  rb_define_singleton_method(rb_cHello, "native_load", hello_load, 1);
  rb_define_method(rb_cHello, "native_get_version", hello_get_version, 0);
  rb_define_method(rb_cHello, "native_cmd_length", hello_cmd_length, 0);
  rb_define_method(rb_cHello, "native_add_cmd_part", hello_add_cmd_part, 1);
  rb_define_method(rb_cHello, "native_cmd_get", hello_cmd_get, 1);
  rb_define_method(rb_cHello, "native_serialize", hello_serialize, 0);

  rb_cError = rb_define_class_under(rb_mSkylight, "Error", rb_cObject);
  rb_define_singleton_method(rb_cError, "native_new", error_new, 2);
  rb_define_singleton_method(rb_cError, "native_load", error_load, 1);
  rb_define_method(rb_cError, "native_get_group", error_get_group, 0);
  rb_define_method(rb_cError, "native_get_description", error_get_description, 0);
  rb_define_method(rb_cError, "native_get_details", error_get_details, 0);
  rb_define_method(rb_cError, "native_set_details", error_set_details, 1);
  rb_define_method(rb_cError, "native_serialize", error_serialize, 0);

  rb_cTrace = rb_define_class_under(rb_mSkylight, "Trace", rb_cObject);
  rb_define_singleton_method(rb_cTrace, "native_new", trace_new, 2);
  rb_define_singleton_method(rb_cTrace, "native_name_from_serialized", trace_name_from_serialized, 1);
  rb_define_method(rb_cTrace, "native_get_started_at", trace_get_started_at, 0);
  rb_define_method(rb_cTrace, "native_get_name", trace_get_name, 0);
  rb_define_method(rb_cTrace, "native_set_name", trace_set_name, 1);
  rb_define_method(rb_cTrace, "native_get_uuid", trace_get_uuid, 0);
  rb_define_method(rb_cTrace, "native_serialize", trace_serialize, 0);
  rb_define_method(rb_cTrace, "native_start_span", trace_start_span, 2);
  rb_define_method(rb_cTrace, "native_stop_span", trace_stop_span, 2);
  rb_define_method(rb_cTrace, "native_span_set_title", trace_span_set_title, 2);
  rb_define_method(rb_cTrace, "native_span_set_description", trace_span_set_description, 2);
  rb_define_method(rb_cTrace, "native_add_stack_frame_filter", trace_add_stack_frame_filter, 1);
  rb_define_method(rb_cTrace, "native_sample_stack", trace_sample_stack, 1);

  rb_cBatch = rb_define_class_under(rb_mSkylight, "Batch", rb_cObject);
  rb_define_singleton_method(rb_cBatch, "native_new", batch_new, 2);
  rb_define_method(rb_cBatch, "native_move_in", batch_move_in, 1);
  rb_define_method(rb_cBatch, "native_set_endpoint_count", batch_set_endpoint_count, 2);
  rb_define_method(rb_cBatch, "native_serialize", batch_serialize, 0);

  Init_skylight_prof();
}
