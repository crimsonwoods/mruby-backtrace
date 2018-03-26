#include "mruby.h"
#include "mruby/proc.h"
#include "mruby/irep.h"
#include "mruby/variable.h"
#include "mruby/numeric.h"
#include "mruby/string.h"
#include "mruby/array.h"
#if !defined(DISABLE_VM_BACKTRACE)
#  include <libunwind.h>
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
    return mrb_nil_value();
  }

  fprintf(BT_OUT_FP, "---- [backtrace] ---------------\n");
  do {
    if ((ret = unw_get_reg(&unw_cur, UNW_REG_IP, &ip)) < 0 ||
        (ret = unw_get_reg(&unw_cur, UNW_REG_SP, &sp)) < 0) {
      break;
    }
    buf[0] = '\0';
    if ((ret = unw_get_proc_name(&unw_cur, buf, sizeof(buf), &off)) == 0) {
#if defined(__x86_64__)
      fprintf(BT_OUT_FP, "%p <%s + 0x%lx>\n", (void*)ip, buf, off);
#else
      fprintf(BT_OUT_FP, "%p <%s + 0x%x>\n", (void*)ip, buf, off);
#endif
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
callinfo_to_string(mrb_state *mrb, mrb_callinfo *ci)
{
  mrb_value str;
  mrb_irep *irep;
  mrb_code *pc;
  char const *filename;
  int line;
  char separator;
  char const *cname;
  char const *method;

  if (MRB_PROC_CFUNC_P(ci->proc)) {
    return mrb_nil_value();
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

#ifdef MRB_PROC_TARGET_CLASS
  if (ci->target_class == MRB_PROC_TARGET_CLASS(ci->proc)) {
#else
  if (ci->target_class == ci->proc->target_class) {
#endif
    separator = '.';
  } else {
    separator = '#';
  }

#ifdef MRB_PROC_TARGET_CLASS
  cname = mrb_class_name(mrb, MRB_PROC_TARGET_CLASS(ci->proc));
#else
  cname = mrb_class_name(mrb, ci->proc->target_class);
#endif
  method = mrb_sym2name(mrb, ci->mid);

  mrb_value line_no = mrb_fixnum_to_str(mrb, mrb_fixnum_value(line), 10);
  if (method) {
    if (cname) {
      str = mrb_str_new(mrb, "\t", 1);
      str = mrb_str_cat_cstr(mrb, str, filename);
      str = mrb_str_cat_cstr(mrb, str, ":");
      str = mrb_str_cat_cstr(mrb, str, RSTRING_PTR(line_no));
      str = mrb_str_cat_cstr(mrb, str, ":in ");
      str = mrb_str_cat_cstr(mrb, str, cname);
      str = mrb_str_buf_cat(mrb, str, &separator, 1);
      str = mrb_str_cat_cstr(mrb, str, method);
    } else {
      str = mrb_str_new(mrb, "\t", 1);
      str = mrb_str_cat_cstr(mrb, str, filename);
      str = mrb_str_cat_cstr(mrb, str, ":");
      str = mrb_str_cat_cstr(mrb, str, RSTRING_PTR(line_no));
      str = mrb_str_cat_cstr(mrb, str, ":in ");
      str = mrb_str_cat_cstr(mrb, str, method);
    }
  } else {
    str = mrb_str_new(mrb, "\t", 1);
    str = mrb_str_cat_cstr(mrb, str, filename);
    str = mrb_str_cat_cstr(mrb, str, ":");
    str = mrb_str_cat_cstr(mrb, str, RSTRING_PTR(line_no));
  }
  return str;
}

static mrb_callinfo*
get_backtrace_point(mrb_state* mrb)
{
  if (NULL == mrb->c->ci) {
    return NULL;
  }
  if (mrb->c->ci == mrb->c->cibase) {
    return NULL;
  }
  return mrb->c->ci - 1;
}

static mrb_callinfo*
next_backtrace_point(mrb_state* mrb, mrb_callinfo *info)
{
  if ((NULL == info) || (info < mrb->c->cibase) || (info > mrb->c->ciend)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid backtrace point is given.");
  }
  do {
    if (info == mrb->c->cibase) {
      /* the end of call-stack */
      return NULL;
    }
    --info;
  } while (MRB_PROC_CFUNC_P(info->proc));

  return info;
}

static mrb_value
make_backtrace_item(mrb_state* mrb, mrb_callinfo *info)
{
  return callinfo_to_string(mrb, info);
}

static mrb_value
mrb_bt_put_rb(mrb_state *mrb, mrb_value self)
{
#if !defined(DISABLE_STDIO)
  mrb_callinfo *bp = get_backtrace_point(mrb);
  fprintf(BT_OUT_FP, "backtrace:\n");
  do {
    mrb_value str = callinfo_to_string(mrb, bp);
    fprintf(BT_OUT_FP, "%s\n", RSTRING_PTR(str));
    bp = next_backtrace_point(mrb, bp);
  } while (NULL != bp);
#endif
  return mrb_nil_value();
}

static mrb_value
mrb_bt_backtrace(mrb_state *mrb, mrb_value self)
{
  mrb_value ary;
  mrb_callinfo *bp = get_backtrace_point(mrb);
  if (NULL == bp) {
    return mrb_nil_value();
  }
  ary = mrb_ary_new(mrb);
  do {
    mrb_value item = make_backtrace_item(mrb, bp);
    mrb_ary_push(mrb, ary, item);
    bp = next_backtrace_point(mrb, bp);
  } while(NULL != bp);
  return ary;
}

void
mrb_mruby_backtrace_gem_init(mrb_state* mrb)
{
  struct RClass *mod;
  mod = mrb_define_module(mrb, "Backtrace");
#if !defined(DISABLE_VM_BACKTRACE)
  mrb_define_module_function(mrb, mod, "put_vm", mrb_bt_put_vm, MRB_ARGS_NONE());
#endif
  mrb_define_module_function(mrb, mod, "put_rb", mrb_bt_put_rb, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, mod, "backtrace", mrb_bt_backtrace, MRB_ARGS_NONE());
}

void
mrb_mruby_backtrace_gem_final(mrb_state* mrb)
{
}

