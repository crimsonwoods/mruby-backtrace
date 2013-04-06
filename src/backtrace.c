#include "mruby.h"
#include "mruby/proc.h"
#include "mruby/irep.h"
#include "mruby/variable.h"
#if !defined(DISABLE_VM_BACKTRACE)
#  include <libunwind.h>
#  if defined(__x86_64__)
#    include <libunwind-x86_64.h>
#  elif defined(__x86__)
#    include <libunwind-x86.h>
#  elif defined(__arm__)
#    include <libunwind-arm.h>
#  else
#    error "mruby-backtrace is NOT supported this platform."
#  endif
#endif

#define PROC_NAME_MAX 512
#define BT_OUT_FP stdout

#if !defined(DISABLE_VM_BACKTRACE)
static mrb_value
mrb_bt_put_vm(mrb_state *mrb, mrb_value self)
{
#if !defined(DISABLE_STDIO)
  unw_context_t unw_ctx;
  unw_cursor_t unw_cur;
  int ret;
  unw_word_t ip, sp, off;
  unw_proc_info_t pi;
  char buf[PROC_NAME_MAX];
  int n = 0;

  unw_getcontext(&unw_ctx);
  ret = unw_init_local(&unw_cur, &unw_ctx);
  if (0 > ret) {
    return;
  }

  fprintf(BT_OUT_FP, "---- [backtrace] ---------------\n");
  do {
    if ((ret = unw_get_reg(&unw_cur, UNW_REG_IP, &ip)) < 0 ||
        (ret = unw_get_reg(&unw_cur, UNW_REG_SP, &sp)) < 0) {
      break;
    }
    buf[0] = '\0';
    if ((ret = unw_get_proc_name(&unw_cur, buf, sizeof(buf), &off)) == 0) {
      fprintf(BT_OUT_FP, "%p <%s + 0x%lx>\n", (void*)ip, buf, off);
    } else {
      break;
    }
    if ((ret = unw_get_proc_info(&unw_cur, &pi)) < 0) {
      break;
    }
    ret = unw_step(&unw_cur);
    if (ret < 0) {
      unw_get_reg(&unw_cur, UNW_REG_IP, &ip);
    }
    if (++n > 64) {
      break;
    }
  } while (ret > 0);
  fprintf(BT_OUT_FP, "--------------------------------\n");
#endif
  return mrb_nil_value();
}
#endif

static mrb_value
mrb_bt_put_rb(mrb_state *mrb, mrb_value self)
{
#if !defined(DISABLE_STDIO)
  mrb_callinfo *ci = mrb->ci;
  mrb_irep *irep;
  mrb_code *pc;
  char const *filename;
  int line = -1;
  char separator;
  char const *cname;
  char const *method;

  fprintf(BT_OUT_FP, "backtrace:\n");
  do {
    if (MRB_PROC_CFUNC_P(ci->proc)) {
      continue;
    }

    filename = "(unknown)";
    line = -1;

    irep = ci->proc->body.irep;

    if (irep->filename) {
      filename = irep->filename;
    }

    if (irep->lines) {
      pc = (ci + 1)->pc;
      if (irep->iseq <= pc && pc < irep->iseq + irep->ilen) {
        line = irep->lines[pc - irep->iseq - 1];
      }
    }

    if (ci->target_class == ci->proc->target_class) {
      separator = '.';
    } else {
      separator = '#';
    }

    cname = mrb_class_name(mrb, ci->proc->target_class);
    method = mrb_sym2name(mrb, ci->mid);

    if (method) {
      if (cname) {
        fprintf(BT_OUT_FP, "\t%s:%d:in %s%c%s\n",
          filename, line,
          cname, separator, method);
      } else {
        fprintf(BT_OUT_FP, "\t%s:%d:in %s\n",
          filename, line,
          method);
      }
    } else {
      fprintf(BT_OUT_FP, "\t%s:%d\n", filename, line);
    }
  } while (--ci >= mrb->cibase);
#endif
  return mrb_nil_value();
}

void
mrb_mruby_backtrace_gem_init(mrb_state* mrb)
{
  struct RClass *mod;
  mod = mrb_define_class(mrb, "Backtrace", NULL);
#if !defined(DISABLE_VM_BACKTRACE)
  mrb_define_class_method(mrb, mod, "put_vm", mrb_bt_put_vm, ARGS_NONE());
#endif
  mrb_define_class_method(mrb, mod, "put_rb", mrb_bt_put_rb, ARGS_NONE());
}

void
mrb_mruby_backtrace_gem_final(mrb_state* mrb)
{
}

