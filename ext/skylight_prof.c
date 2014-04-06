#include <ruby.h>

// Only include these files if CPU profiling is enabled
#ifdef HAVE_CPU_PROFILING
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <ruby/debug.h>
#include <vm_core.h>
#include <iseq.h>
#include <ruby/debug.h>
#endif

#include <skylight.h>
#include <skylight_native.h>

#define FRAME_BUF_SIZE 256

static VALUE rb_mSkylight;

#ifdef HAVE_CPU_PROFILING

static rb_vm_t* rb_curr_vm;
static VALUE rb_cInstrumenter;
static VALUE rb_instrumenter;
static VALUE sym_sample_stacks;
static VALUE frame_buf[FRAME_BUF_SIZE];

#endif

// Whether or not we can do CPU profiling
static VALUE
cpu_profiling_supported(VALUE klass) {
#ifdef HAVE_CPU_PROFILING
  return Qtrue;
#else
  return Qfalse;
#endif
}

#ifdef HAVE_CPU_PROFILING

inline static int
calc_lineno(const rb_iseq_t* iseq, const VALUE* pc) {
  return rb_iseq_line_no(iseq, pc - iseq->iseq_encoded);
}

/*
 * Mostly copied from MRI 2.1 source
 */
static int
rb_thread_profile_frames(rb_thread_t* th, int start, int limit, VALUE *buff, int* lines) {
  int i;
  rb_control_frame_t* cfp = th->cfp, *end_cfp = RUBY_VM_END_CONTROL_FRAME(th);

  for (i = 0; i < limit && cfp != end_cfp;) {
    if (cfp->iseq && cfp->pc) {
      if (start > 0) {
        start--;
        continue;
      }

      /* record frame info */
      buff[i] = cfp->iseq->self;

      if (lines) {
        lines[i] = calc_lineno(cfp->iseq, cfp->pc);
      }

      ++i;
    }

    cfp = RUBY_VM_PREVIOUS_CONTROL_FRAME(cfp);
  }

  return i;
}


static void
cpu_profiler_job_handler(void* data) {
  // It might be possible for the instrumenter singleton to be unset, so check
  if (rb_instrumenter != Qnil) {
    rb_funcall(rb_instrumenter, sym_sample_stacks, 0);
  }
}

static void
cpu_profiler_sig_handler(int sig, siginfo_t* info, void* ctx) {
  // rb_thread_t* th = rb_curr_vm->running_thread;

  // Process the handler in ruby
  rb_postponed_job_register_one(0, cpu_profiler_job_handler, 0);
}

static int
register_signal_handler() {
  struct sigaction sa;

  sa.sa_sigaction = cpu_profiler_sig_handler;
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  return sigaction(SIGALRM, &sa, NULL);
}

static int
start_interval_timer() {
  struct itimerval timer;

  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 10000; // 10 ms
  timer.it_value = timer.it_interval;

  return setitimer(ITIMER_REAL, &timer, 0);
}

static VALUE
start_cpu_profiler(VALUE self, VALUE rb_thread) {
  int ret;

  rb_curr_vm = ((rb_thread_t*) DATA_PTR(rb_thread))->vm;

  if (rb_instrumenter != Qnil) {
    return Qfalse;
  }

  // Set the singleton instrumenter instance
  rb_instrumenter = self;

  ret = register_signal_handler();
  if (ret) {
    return Qfalse;
  }

  if (start_interval_timer()) {
    return Qfalse;
  }

  return Qtrue;
}

static VALUE
stop_cpu_profiler(VALUE self) {
  struct itimerval timer;

  rb_instrumenter = Qnil;

  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  timer.it_value = timer.it_interval;

  if (setitimer(ITIMER_REAL, &timer, 0)) {
    return Qfalse;
  }

  return Qtrue;
}

/*
 *
 * ===== CPU PROFILING IMPLEMENTATION =====
 *
 */

// This func assumes that frame_buf has been populated
static void
trace_track_sample(sk_trace_t* trace, int depth, sk_bool timed) {
  int i, j, last;
  sk_frame_t frame;

  if (depth == 0)
    return;

  last = depth-1;

  // Skip Skylight#sample_stack methods
  for (j = 1; j < depth; ++j) {
    rb_iseq_t* iseq = (rb_iseq_t*) DATA_PTR(frame_buf[j]);

    if (iseq->type == ISEQ_TYPE_METHOD) {
      break;
    }
  }

  // There are no frames to sample
  if (j >= last)
    return;

  // Explicitly drop the top frame to leave out the #sample_stacks method
  for (i = last; i > j; --i) {
    rb_iseq_t* iseq = (rb_iseq_t*) DATA_PTR(frame_buf[i]);

    if (i < last && iseq->type != ISEQ_TYPE_METHOD)
      continue;

    DEBUG("[RUBY_NATIVE] sk_trace_register_stack_frame_(under?)\n");
    frame = (i == last ?
      sk_trace_register_stack_frame(trace, frame_buf[i], timed):
      sk_trace_register_stack_frame_under(trace, frame, frame_buf[i], timed));

    if (!sk_trace_stack_frame_has_details(trace, frame)) {
      DEBUG("[RUBY_NATIVE] getting stack frame details\n");

      sk_buf_t label = STR2BUF(rb_profile_frame_full_label(frame_buf[i]));
      sk_buf_t path = STR2BUF(rb_profile_frame_absolute_path(frame_buf[i]));

      DEBUG("[RUBY_NATIVE] setting stack frame details\n");
      sk_trace_stack_frame_set_details(trace, frame, label, path);
    }
  }

  sk_trace_register_stack_frame_end(trace, frame);
}
#endif

void
sk_cpu_prof_start_span(sk_trace_t* trace) {
#ifdef HAVE_CPU_PROFILING
  if (rb_instrumenter != Qnil) {
    DEBUG("[RUBY_NATIVE] sk_cpu_prof_start_span(...)\n");
    int depth = rb_profile_frames(0, FRAME_BUF_SIZE, frame_buf, NULL);
    trace_track_sample(trace, depth, SK_FALSE);
    DEBUG("[RUBY_NATIVE DONE] sk_cpu_prof_start_span(...)\n");
  }
#endif
}

void
sk_cpu_prof_stop_span(sk_trace_t* trace) {
#ifdef HAVE_CPU_PROFILING
  if (rb_instrumenter != Qnil) {
    DEBUG("[RUBY_NATIVE] sk_cpu_prof_stop_span(...)\n");
    int depth = rb_profile_frames(0, FRAME_BUF_SIZE, frame_buf, NULL);
    trace_track_sample(trace, depth, SK_FALSE);
    DEBUG("[RUBY_NATIVE DONE] sk_cpu_prof_stop_span(...)\n");
  }
#endif
}

void
sk_cpu_prof_sample_stack(sk_trace_t* trace, VALUE rb_thread) {
#ifdef HAVE_CPU_PROFILING
  rb_thread_t* th;

  // TODO: Add a check
  th = (rb_thread_t*) DATA_PTR(rb_thread);

  // TODO: Check if a thread is killed

  int depth = rb_thread_profile_frames(th, 0, FRAME_BUF_SIZE, frame_buf, NULL);
  trace_track_sample(trace, depth, SK_TRUE);
#endif
}

void Init_skylight_prof(void) {

  rb_mSkylight = rb_define_module("Skylight");
  rb_define_singleton_method(rb_mSkylight, "cpu_profiling_supported?", cpu_profiling_supported, 0);

#ifdef HAVE_CPU_PROFILING

  rb_curr_vm = NULL;

  rb_cInstrumenter = rb_define_class_under(rb_mSkylight, "Instrumenter", rb_cObject);
  rb_define_method(rb_cInstrumenter, "native_start_cpu_profiler", start_cpu_profiler, 1);
  rb_define_method(rb_cInstrumenter, "native_stop_cpu_profiler", stop_cpu_profiler, 0);

  rb_instrumenter = Qnil;
  sym_sample_stacks = rb_intern("sample_stacks");

#endif
}
