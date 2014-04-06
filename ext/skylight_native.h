#ifndef __SKYLIGHT_NATIVE_H__
#define __SKYLIGHT_NATIVE_H__

#include <ruby.h>

#define STR2BUF(str)                  \
  ({                                  \
    sk_buf_t buf;                     \
    VALUE rb_str = (str);             \
                                      \
    if (rb_str == Qnil) {             \
      buf.data = 0;                   \
      buf.len = 0;                    \
    }                                 \
    else {                            \
      buf.data = RSTRING_PTR(rb_str); \
      buf.len = RSTRING_LEN(rb_str);  \
    }                                 \
                                      \
    buf;                              \
  })

#define DEBUG(...)                    \
  do {                                \
    if (false) {                      \
      fprintf(stderr, __VA_ARGS__);   \
    }                                 \
  } while (0)

void sk_cpu_prof_start_span(sk_trace_t* trace);
void sk_cpu_prof_stop_span(sk_trace_t* trace);
void sk_cpu_prof_sample_stack(sk_trace_t* trace, VALUE rb_thread);

// Initializes CPU profiling
void Init_skylight_prof(void);

#endif
