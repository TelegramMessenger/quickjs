#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#include "quickjs-libc.h"
#include "cutils.h"

static JSRuntime *new_runtime(void)
{
    JSRuntime *rt = JS_NewRuntime();

    if (rt)
        JS_SetDumpFlags(rt, JS_ABORT_ON_LEAKS);
    return rt;
}

static JSValue eval(JSContext *ctx, const char *code)
{
    return JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_GLOBAL);
}

static JSValue cfunc_callback(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    return JS_ThrowTypeError(ctx, "from cfunc");
}

static JSValue cfuncdata_callback(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv,
                                  int magic, JSValueConst *func_data)
{
    return JS_ThrowTypeError(ctx, "from cfuncdata");
}

static JSValue cclosure_callback(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv,
                                 int magic, void *func_data)
{
  return JS_ThrowTypeError(ctx, "from cclosure");
}

static bool closure_finalized = false;

static void cclosure_opaque_finalize(void *opaque)
{
    if ((intptr_t)opaque == 12)
        closure_finalized = true;
}

static void cfunctions(void)
{
    uint32_t length;
    const char *s;
    JSValue ret, stack;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue cfunc = JS_NewCFunction(ctx, cfunc_callback, "cfunc", 42);
    JSValue cfuncdata =
        JS_NewCFunctionData2(ctx, cfuncdata_callback, "cfuncdata",
                             /*length*/1337, /*magic*/0, /*data_len*/0, NULL);
    JSValue cclosure =
        JS_NewCClosure(ctx, cclosure_callback, "cclosure", cclosure_opaque_finalize,
                       /*length*/0xC0DE, /*magic*/11, /*opaque*/(void*)12);
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "cfunc", cfunc);
    JS_SetPropertyStr(ctx, global, "cfuncdata", cfuncdata);
    JS_SetPropertyStr(ctx, global, "cclosure", cclosure);
    JS_FreeValue(ctx, global);

    ret = eval(ctx, "cfunc.name");
    assert(!JS_IsException(ret));
    assert(JS_IsString(ret));
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "cfunc"));
    JS_FreeCString(ctx, s);
    ret = eval(ctx, "cfunc.length");
    assert(!JS_IsException(ret));
    assert(JS_IsNumber(ret));
    assert(0 == JS_ToUint32(ctx, &length, ret));
    assert(length == 42);

    ret = eval(ctx, "cfuncdata.name");
    assert(!JS_IsException(ret));
    assert(JS_IsString(ret));
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "cfuncdata"));
    JS_FreeCString(ctx, s);
    ret = eval(ctx, "cfuncdata.length");
    assert(!JS_IsException(ret));
    assert(JS_IsNumber(ret));
    assert(0 == JS_ToUint32(ctx, &length, ret));
    assert(length == 1337);

    ret = eval(ctx, "cclosure.name");
    assert(!JS_IsException(ret));
    assert(JS_IsString(ret));
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "cclosure"));
    JS_FreeCString(ctx, s);
    ret = eval(ctx, "cclosure.length");
    assert(!JS_IsException(ret));
    assert(JS_IsNumber(ret));
    assert(0 == JS_ToUint32(ctx, &length, ret));
    assert(length == 0xC0DE);

    ret = eval(ctx, "cfunc()");
    assert(JS_IsException(ret));
    ret = JS_GetException(ctx);
    assert(JS_IsError(ret));
    stack = JS_GetPropertyStr(ctx, ret, "stack");
    assert(JS_IsString(stack));
    s = JS_ToCString(ctx, stack);
    JS_FreeValue(ctx, stack);
    assert(s);
    assert(!strcmp(s, "    at cfunc (native)\n    at <eval> (<input>:1:1)\n"));
    JS_FreeCString(ctx, s);
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "TypeError: from cfunc"));
    JS_FreeCString(ctx, s);

    ret = eval(ctx, "cfuncdata()");
    assert(JS_IsException(ret));
    ret = JS_GetException(ctx);
    assert(JS_IsError(ret));
    stack = JS_GetPropertyStr(ctx, ret, "stack");
    assert(JS_IsString(stack));
    s = JS_ToCString(ctx, stack);
    JS_FreeValue(ctx, stack);
    assert(s);
    assert(!strcmp(s, "    at cfuncdata (native)\n    at <eval> (<input>:1:1)\n"));
    JS_FreeCString(ctx, s);
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "TypeError: from cfuncdata"));
    JS_FreeCString(ctx, s);

    ret = eval(ctx, "cclosure()");
    assert(JS_IsException(ret));
    ret = JS_GetException(ctx);
    assert(JS_IsError(ret));
    stack = JS_GetPropertyStr(ctx, ret, "stack");
    assert(JS_IsString(stack));
    s = JS_ToCString(ctx, stack);
    JS_FreeValue(ctx, stack);
    assert(s);
    assert(!strcmp(s, "    at cclosure (native)\n    at <eval> (<input>:1:1)\n"));
    JS_FreeCString(ctx, s);
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "TypeError: from cclosure"));
    JS_FreeCString(ctx, s);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    assert(closure_finalized);
}

#define MAX_TIME 10

static int timeout_interrupt_handler(JSContext *ctx, void *opaque)
{
    int *time = (int *)opaque;
    return (*time)++ > MAX_TIME;
}

static void sync_call(void)
{
    static const char code[] =
"(function() { \
    try { \
        while (true) {} \
    } catch (e) {} \
})();";

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    int time = 0;
    JS_SetInterruptHandler(rt, timeout_interrupt_handler, &time);
    JSValue ret = eval(ctx, code);
    assert(time > MAX_TIME);
    assert(JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    assert(JS_HasException(ctx));
    JSValue e = JS_GetException(ctx);
    assert(JS_IsUncatchableError(e));
    JS_FreeValue(ctx, e);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void async_call(void)
{
    static const char code[] =
"(async function() { \
    const loop = async () => { \
        await Promise.resolve(); \
        while (true) {} \
    }; \
    await loop().catch(() => {}); \
})();";

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    int time = 0;
    JS_SetInterruptHandler(rt, timeout_interrupt_handler, &time);
    JSValue ret = eval(ctx, code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    assert(JS_IsJobPending(rt));
    int r = 0;
    while (JS_IsJobPending(rt)) {
        r = JS_ExecutePendingJob(rt, &ctx);
    }
    assert(time > MAX_TIME);
    assert(r == -1);
    assert(JS_HasException(ctx));
    JSValue e = JS_GetException(ctx);
    assert(JS_IsUncatchableError(e));
    JS_FreeValue(ctx, e);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void std_eval_interrupt_handler(void)
{
    static const char code[] =
        "import * as std from 'std'; std.evalScript('for(;;){}')";
    JSRuntime *rt = new_runtime();
    js_std_init_handlers(rt);
    JSContext *ctx = JS_NewContext(rt);
    js_init_module_std(ctx, "std");
    int time = 0;
    JS_SetInterruptHandler(rt, timeout_interrupt_handler, &time);
    JSValue ret =
        JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
    ret = js_std_await(ctx, ret);
    assert(time > MAX_TIME);
    assert(JS_IsException(ret));
    ret = JS_GetException(ctx);
    assert(JS_IsError(ret));
    // uncatchable "interrupted" exception is turned into a regular exception
    assert(!JS_IsUncatchableError(ret));
    const char *str = JS_ToCString(ctx, ret);
    assert(str != NULL);
    assert(strstr(str, "InternalError: interrupted"));
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, ret);
    uintptr_t interrupt_handler = js_std_cmd(/*GetInterruptHandler*/5, rt);
    uintptr_t interrupt_opaque = js_std_cmd(/*GetInterruptOpaque*/6, rt);
    assert(interrupt_handler == (uintptr_t)timeout_interrupt_handler);
    assert(interrupt_opaque == (uintptr_t)&time);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue save_value(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv)
{
    assert(argc == 1);
    JSValue *p = (JSValue *)JS_GetContextOpaque(ctx);
    *p = JS_DupValue(ctx, argv[0]);
    return JS_UNDEFINED;
}

static void async_call_stack_overflow(void)
{
    static const char code[] =
"(async function() { \
    const f = () => f(); \
    try { \
        await Promise.resolve(); \
        f(); \
    } catch (e) { \
        save_value(e); \
    } \
})();";

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue value = JS_UNDEFINED;
    JS_SetContextOpaque(ctx, &value);
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "save_value", JS_NewCFunction(ctx, save_value, "save_value", 1));
    JS_FreeValue(ctx, global);
    JSValue ret = eval(ctx, code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    assert(JS_IsJobPending(rt));
    int r = 0;
    while (JS_IsJobPending(rt)) {
        r = JS_ExecutePendingJob(rt, &ctx);
    }
    assert(r == 1);
    assert(!JS_HasException(ctx));
    assert(JS_IsError(value)); // stack overflow should be caught
    JS_FreeValue(ctx, value);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

// https://github.com/quickjs-ng/quickjs/issues/914
static void raw_context_global_var(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContextRaw(rt);
    JS_AddIntrinsicEval(ctx);
    {
        JSValue ret = eval(ctx, "globalThis");
        assert(JS_IsException(ret));
        JS_FreeValue(ctx, ret);
    }
    {
        JSValue ret = eval(ctx, "var x = 42");
        assert(JS_IsUndefined(ret));
        JS_FreeValue(ctx, ret);
    }
    {
        JSValue ret = eval(ctx, "function f() {}");
        assert(JS_IsUndefined(ret));
        JS_FreeValue(ctx, ret);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void is_array(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    {
        JSValue ret = eval(ctx, "[]");
        assert(!JS_IsException(ret));
        assert(JS_IsArray(ret));
        JS_FreeValue(ctx, ret);
    }
    {
        JSValue ret = eval(ctx, "new Proxy([], {})");
        assert(!JS_IsException(ret));
        assert(!JS_IsArray(ret));
        assert(JS_IsProxy(ret));
        JSValue handler = JS_GetProxyHandler(ctx, ret);
        JSValue target = JS_GetProxyTarget(ctx, ret);
        assert(!JS_IsException(handler));
        assert(!JS_IsException(target));
        assert(!JS_IsProxy(handler));
        assert(!JS_IsProxy(target));
        assert(JS_IsObject(handler));
        assert(JS_IsArray(target));
        JS_FreeValue(ctx, handler);
        JS_FreeValue(ctx, target);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static int loader_calls;

static JSModuleDef *loader(JSContext *ctx, const char *name, void *opaque)
{
    loader_calls++;
    assert(!strcmp(name, "b"));
    static const char code[] = "export function f(x){}";
    JSValue ret = JS_Eval(ctx, code, strlen(code), "b",
                          JS_EVAL_TYPE_MODULE|JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(ret));
    JSModuleDef *m = JS_VALUE_GET_PTR(ret);
    assert(m);
    JS_FreeValue(ctx, ret);
    return m;
}

static void module_serde(void)
{
    JSRuntime *rt = new_runtime();
    //JS_SetDumpFlags(rt, JS_DUMP_MODULE_RESOLVE);
    JS_SetModuleLoaderFunc(rt, NULL, loader, NULL);
    JSContext *ctx = JS_NewContext(rt);
    static const char code[] = "import {f} from 'b'; f()";
    assert(loader_calls == 0);
    JSValue mod = JS_Eval(ctx, code, strlen(code), "a",
                          JS_EVAL_TYPE_MODULE|JS_EVAL_FLAG_COMPILE_ONLY);
    assert(loader_calls == 1);
    assert(!JS_IsException(mod));
    assert(JS_IsModule(mod));
    size_t len = 0;
    uint8_t *buf = JS_WriteObject(ctx, &len, mod,
                                  JS_WRITE_OBJ_BYTECODE|JS_WRITE_OBJ_REFERENCE);
    assert(buf);
    assert(len > 0);
    JS_FreeValue(ctx, mod);
    assert(loader_calls == 1);
    mod = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
    js_free(ctx, buf);
    assert(loader_calls == 1); // 'b' is returned from cache
    assert(!JS_IsException(mod));
    JSValue ret = JS_EvalFunction(ctx, mod);
    assert(!JS_IsException(ret));
    assert(JS_IsPromise(ret));
    JSValue result = JS_PromiseResult(ctx, ret);
    assert(!JS_IsException(result));
    assert(JS_IsUndefined(result));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

struct rejection_counts {
    int reject_count;
    int handle_count;
};

static void rejection_counter(JSContext *ctx, JSValueConst promise,
                              JSValueConst reason, bool is_handled, void *opaque)
{
    struct rejection_counts *c = opaque;
    if (is_handled)
        c->handle_count++;
    else
        c->reject_count++;
}

// A synchronous module that throws at top level must surface exactly one unhandled rejection
static void module_unhandled_rejection(void)
{
    struct rejection_counts c = {0, 0};
    JSRuntime *rt = new_runtime();
    JS_SetHostPromiseRejectionTracker(rt, rejection_counter, &c);
    JSContext *ctx = JS_NewContext(rt);

    static const char code[] = "throw new Error('Nuke the entire site from orbit. It\\'s the only way to be sure.')";
    JSValue v = JS_Eval(ctx, code, strlen(code), "<m>", JS_EVAL_TYPE_MODULE);
    JS_FreeValue(ctx, v);

    JSContext *c1;
    while (JS_ExecutePendingJob(rt, &c1) > 0)
        ;

    // net unhandled rejections == (2 rejects - 1 handled)
    assert(c.reject_count == 2);
    assert(c.handle_count == 1);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void promise_mark_as_handled(void)
{
    struct rejection_counts c = {0, 0};
    JSRuntime *rt = new_runtime();
    JS_SetHostPromiseRejectionTracker(rt, rejection_counter, &c);
    JSContext *ctx = JS_NewContext(rt);
    JSContext *c1;

    // marking an already-rejected promise notifies the tracker exactly once
    static const char code[] = "Promise.reject('kaboom')";
    JSValue promise = JS_Eval(ctx, code, strlen(code), "<t>", JS_EVAL_TYPE_GLOBAL);
    assert(JS_IsPromise(promise));
    while (JS_ExecutePendingJob(rt, &c1) > 0)
        ;
    assert(c.reject_count == 1);
    assert(c.handle_count == 0);
    JS_PromiseMarkAsHandled(ctx, promise);
    assert(c.handle_count == 1);
    JS_PromiseMarkAsHandled(ctx, promise);
    assert(c.handle_count == 1);
    JS_FreeValue(ctx, promise);

    // marking a pending promise suppresses the report when it later rejects
    JSValue resolving_funcs[2];
    JSValue promise2 = JS_NewPromiseCapability(ctx, resolving_funcs);
    JS_PromiseMarkAsHandled(ctx, promise2);
    JSValue reason = JS_NewString(ctx, "unseen");
    JSValue ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1,
                          (JSValueConst *)&reason);
    while (JS_ExecutePendingJob(rt, &c1) > 0)
        ;
    assert(c.reject_count == 1);
    assert(c.handle_count == 1);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, reason);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise2);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void promise_then(void)
{
    const char *s;
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSContext *c1;
    JSValue got = JS_UNDEFINED;
    JS_SetContextOpaque(ctx, &got);

    // the result promise is intrinsic: neither an overridden then() nor
    // Symbol.species is consulted
    static const char code[] =
        "class P extends Promise {"
        "  static get [Symbol.species]() { throw new Error('species'); }"
        "  then() { throw new Error('then'); }"
        "}"
        "P.resolve('ok')";
    JSValue promise = eval(ctx, code);
    assert(JS_IsPromise(promise));
    JSValue on_fulfilled = JS_NewCFunction(ctx, save_value, "onFulfilled", 1);
    JSValue result = JS_PromiseThen(ctx, promise, on_fulfilled, JS_UNDEFINED);
    assert(JS_IsPromise(result));
    while (JS_ExecutePendingJob(rt, &c1) > 0)
        ;
    s = JS_ToCString(ctx, got);
    assert(s);
    assert(!strcmp(s, "ok"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, got);
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, on_fulfilled);
    JS_FreeValue(ctx, promise);

    // rejection handler receives the reason when the promise rejects later
    JSValue resolving_funcs[2];
    JSValue promise2 = JS_NewPromiseCapability(ctx, resolving_funcs);
    JSValue on_rejected = JS_NewCFunction(ctx, save_value, "onRejected", 1);
    JSValue result2 = JS_PromiseThen(ctx, promise2, JS_UNDEFINED, on_rejected);
    assert(JS_IsPromise(result2));
    got = JS_UNDEFINED;
    JSValue reason = JS_NewString(ctx, "boom");
    JSValue ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1,
                          (JSValueConst *)&reason);
    while (JS_ExecutePendingJob(rt, &c1) > 0)
        ;
    s = JS_ToCString(ctx, got);
    assert(s);
    assert(!strcmp(s, "boom"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, got);
    assert(JS_PromiseState(ctx, result2) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, reason);
    JS_FreeValue(ctx, result2);
    JS_FreeValue(ctx, on_rejected);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise2);

    // a non-promise argument is a TypeError
    JSValue bad = JS_PromiseThen(ctx, JS_UNDEFINED, JS_UNDEFINED, JS_UNDEFINED);
    assert(JS_IsException(bad));
    JSValue exc = JS_GetException(ctx);
    JS_FreeValue(ctx, exc);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void runtime_cstring_free(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    // string -> cstring + JS_FreeCStringRT
    {
        JSValue ret = eval(ctx, "\"testStringPleaseIgnore\"");
        assert(JS_IsString(ret));
        const char *s = JS_ToCString(ctx, ret);
        assert(s);
        assert(strcmp(s, "testStringPleaseIgnore") == 0);
        JS_FreeCStringRT(rt, s);
        JS_FreeValue(ctx, ret);
    }
    // string -> cstring + JS_FreeCStringRT, destroying the source value first
    {
        JSValue ret = eval(ctx, "\"testStringPleaseIgnore\"");
        assert(JS_IsString(ret));
        const char *s = JS_ToCString(ctx, ret);
        assert(s);
        JS_FreeValue(ctx, ret);
        assert(strcmp(s, "testStringPleaseIgnore") == 0);
        JS_FreeCStringRT(rt, s);
    }
    // number -> cstring + JS_FreeCStringRT
    {
        JSValue ret = eval(ctx, "123987");
        assert(JS_IsNumber(ret));
        const char *s = JS_ToCString(ctx, ret);
        assert(s);
        assert(strcmp(s, "123987") == 0);
        JS_FreeCStringRT(rt, s);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void utf16_string(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    {
        JSValue v = JS_NewStringUTF16(ctx, NULL, 0);
        assert(!JS_IsException(v));
        const char *s = JS_ToCString(ctx, v);
        assert(s);
        assert(!strcmp(s, ""));
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewStringUTF16(ctx, (uint16_t[]){'o','k'}, 2);
        assert(!JS_IsException(v));
        const char *s = JS_ToCString(ctx, v);
        assert(s);
        assert(!strcmp(s, "ok"));
        JS_FreeCString(ctx, s);
        size_t n;
        const uint16_t *u = JS_ToCStringLenUTF16(ctx, &n, v);
        assert(u);
        assert(n == 2);
        assert(u[0] == 'o');
        assert(u[1] == 'k');
        JS_FreeCStringUTF16(ctx, u);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewStringUTF16(ctx, (uint16_t[]){0xD800}, 1);
        assert(!JS_IsException(v));
        const char *s = JS_ToCString(ctx, v);
        assert(s);
        // questionable but surrogates don't map to UTF-8 without WTF-8
        assert(!strcmp(s, "\xED\xA0\x80"));
        JS_FreeCString(ctx, s);
        size_t n;
        const uint16_t *u = JS_ToCStringLenUTF16(ctx, &n, v);
        assert(u);
        assert(n == 1);
        assert(u[0] == 0xD800);
        JS_FreeCStringUTF16(ctx, u);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewStringLen(ctx, "ok", 2); // ascii -> ucs
        assert(!JS_IsException(v));
        size_t n;
        const uint16_t *u = JS_ToCStringLenUTF16(ctx, &n, v);
        assert(u);
        assert(n == 2);
        assert(u[0] == 'o');
        assert(u[1] == 'k');
        JS_FreeCStringUTF16(ctx, u);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewStringUTF16(ctx, NULL, (size_t)INT_MAX + 1);
        assert(JS_IsException(v));
        JSValue e = JS_GetException(ctx);
        assert(JS_IsError(e));
        const char *s = JS_ToCString(ctx, e);
        assert(s);
        assert(strstr(s, "invalid string length") != NULL);
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, e);
    }
    {
        // create wide char slice string, see JS_STRING_SLICE_LEN_MAX
        JSValue v = eval(ctx, "`\\uD800\\uDC00`.repeat(8192).slice(0, -1)");
        assert(!JS_IsException(v));
        size_t n;
        const uint16_t *u = JS_ToCStringLenUTF16(ctx, &n, v);
        assert(u);
        assert(n == 2*8192-1);
        assert(u[0] == 0xD800);
        assert(u[1] == 0xDC00);
        JS_FreeCStringUTF16(ctx, u);
        JS_FreeValue(ctx, v);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void weak_map_gc_check(void)
{
    static const char init_code[] =
"const map = new WeakMap(); \
function addItem() { \
    const k = { \
        text: 'a', \
    }; \
    map.set(k, {k}); \
}";
    static const char test_code[] = "addItem()";

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue ret = eval(ctx, init_code);
    assert(!JS_IsException(ret));

    JSValue ret_test = eval(ctx, test_code);
    assert(!JS_IsException(ret_test));
    JS_RunGC(rt);
    JSMemoryUsage memory_usage;
    JS_ComputeMemoryUsage(rt, &memory_usage);

    for (int i = 0; i < 3; i++) {
        JSValue ret_test2 = eval(ctx, test_code);
        assert(!JS_IsException(ret_test2));
        JS_RunGC(rt);
        JSMemoryUsage memory_usage2;
        JS_ComputeMemoryUsage(rt, &memory_usage2);

        assert(memory_usage.memory_used_count == memory_usage2.memory_used_count);
        assert(memory_usage.memory_used_size == memory_usage2.memory_used_size);
        JS_FreeValue(ctx, ret_test2);
    }

    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, ret_test);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

struct {
    int hook_type_call_count[4];
} promise_hook_state;

static void promise_hook_cb(JSContext *ctx, JSPromiseHookType type,
                            JSValueConst promise, JSValueConst parent_promise,
                            void *opaque)
{
    assert(type == JS_PROMISE_HOOK_INIT ||
           type == JS_PROMISE_HOOK_BEFORE ||
           type == JS_PROMISE_HOOK_AFTER ||
           type == JS_PROMISE_HOOK_RESOLVE);
    promise_hook_state.hook_type_call_count[type]++;
    assert(opaque == (void *)&promise_hook_state);
    if (!JS_IsUndefined(parent_promise)) {
        JSValue global_object = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, global_object, "actual",
                          JS_DupValue(ctx, parent_promise));
        JS_FreeValue(ctx, global_object);
    }
}

static void promise_hook(void)
{
    int *cc = promise_hook_state.hook_type_call_count;
    JSContext *unused;
    JSRuntime *rt = new_runtime();
    //JS_SetDumpFlags(rt, JS_DUMP_PROMISE);
    JS_SetPromiseHook(rt, promise_hook_cb, &promise_hook_state);
    JSContext *ctx = JS_NewContext(rt);
    JSValue global_object = JS_GetGlobalObject(ctx);
    {
        // empty module; creates an outer and inner module promise;
        // JS_Eval returns the outer promise
        JSValue ret = JS_Eval(ctx, "", 0, "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret));
        JS_FreeValue(ctx, ret);
        assert(2 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(2 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with unresolved promise; the outer and inner module promises
        // are resolved but not the user's promise
        static const char code[] = "new Promise(() => {})";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(2 == cc[JS_PROMISE_HOOK_RESOLVE]); // outer and inner module promise
        assert(!JS_IsJobPending(rt));
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with resolved promise
        static const char code[] = "new Promise((resolve,reject) => resolve())";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(3 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with rejected promise
        static const char code[] = "new Promise((resolve,reject) => reject())";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(2 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with promise chain
        static const char code[] =
            "globalThis.count = 0;"
            "globalThis.actual = undefined;" // set by promise_hook_cb
            "globalThis.expected = new Promise(resolve => resolve());"
            "expected.then(_ => count++)";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(4 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(3 == cc[JS_PROMISE_HOOK_RESOLVE]);
        JSValue v = JS_GetPropertyStr(ctx, global_object, "count");
        assert(!JS_IsException(v));
        int32_t count;
        assert(0 == JS_ToInt32(ctx, &count, v));
        assert(0 == count);
        JS_FreeValue(ctx, v);
        assert(JS_IsJobPending(rt));
        assert(1 == JS_ExecutePendingJob(rt, &unused));
        assert(!JS_HasException(ctx));
        assert(4 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(4 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
        v = JS_GetPropertyStr(ctx, global_object, "count");
        assert(!JS_IsException(v));
        assert(0 == JS_ToInt32(ctx, &count, v));
        assert(1 == count);
        JS_FreeValue(ctx, v);
        JSValue actual = JS_GetPropertyStr(ctx, global_object, "actual");
        JSValue expected = JS_GetPropertyStr(ctx, global_object, "expected");
        assert(!JS_IsException(actual));
        assert(!JS_IsException(expected));
        assert(JS_IsSameValue(ctx, actual, expected));
        JS_FreeValue(ctx, actual);
        JS_FreeValue(ctx, expected);
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with thenable; fires before and after hooks
        static const char code[] =
            "new Promise(resolve => resolve({then(resolve){ resolve() }}))";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(2 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(JS_IsJobPending(rt));
        assert(1 == JS_ExecutePendingJob(rt, &unused));
        assert(!JS_HasException(ctx));
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(1 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(1 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(3 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
    }
    JS_FreeValue(ctx, global_object);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void dump_memory_usage(void)
{
    JSMemoryUsage stats;

    JSRuntime *rt = NULL;
    JSContext *ctx = NULL;

    rt = new_runtime();
    ctx = JS_NewContext(rt);

    //JS_SetDumpFlags(rt, JS_DUMP_PROMISE);

    static const char code[] =
    "globalThis.count = 0;"
    "globalThis.actual = undefined;" // set by promise_hook_cb
    "globalThis.expected = new Promise(resolve => resolve());"
    "expected.then(_ => count++)";

    JSValue evalVal = JS_Eval(ctx, code, strlen(code), "<input>", 0);
    JS_FreeValue(ctx, evalVal);

    FILE *temp = tmpfile();
    assert(temp != NULL);
    JS_ComputeMemoryUsage(rt, &stats);
    JS_DumpMemoryUsage(temp, &stats, rt);
    // JS_DumpMemoryUsage(stdout, &stats, rt);
    fclose(temp);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void new_errors(void)
{
    typedef struct {
        const char name[16];
        JSValue (*func)(JSContext *, const char *, ...);
    } Entry;
    static const Entry entries[] = {
        {"Error",           JS_NewPlainError},
        {"InternalError",   JS_NewInternalError},
        {"RangeError",      JS_NewRangeError},
        {"ReferenceError",  JS_NewReferenceError},
        {"SyntaxError",     JS_NewSyntaxError},
        {"TypeError",       JS_NewTypeError},
    };
    const Entry *e;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    for (e = entries; e < endof(entries); e++) {
        JSValue obj = (*e->func)(ctx, "the %s", "needle");
        assert(!JS_IsException(obj));
        assert(JS_IsObject(obj));
        assert(JS_IsError(obj));
        const char *haystack = JS_ToCString(ctx, obj);
        char needle[256];
        snprintf(needle, sizeof(needle), "%s: the needle", e->name);
        assert(strstr(haystack, needle));
        JS_FreeCString(ctx, haystack);
        JSValue stack = JS_GetPropertyStr(ctx, obj, "stack");
        assert(!JS_IsException(stack));
        assert(JS_IsString(stack));
        JS_FreeValue(ctx, stack);
        JS_FreeValue(ctx, obj);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

// JS_NewContext() already installs DOMException by way of
// JS_AddIntrinsicAToB(), so a host that also calls JS_AddIntrinsicDOMException()
// explicitly installs it twice. The second install must release the prototype
// the first one left in ctx->class_proto[] instead of overwriting the slot;
// new_runtime() aborts at JS_FreeRuntime() if it does not.
static void dom_exception_added_twice(void)
{
    JSValue ret;
    const char *s;
    int i;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    // the implicit install left a working DOMException behind
    ret = eval(ctx, "DOMException.prototype === "
                    "Object.getPrototypeOf(new DOMException)");
    assert(JS_ToBool(ctx, ret) == true);
    JS_FreeValue(ctx, ret);

    // installing it again, repeatedly, must not leak the previous prototype
    for (i = 0; i < 3; i++)
        assert(JS_AddIntrinsicDOMException(ctx) == 0);

    // and the class prototype still matches the global constructor, i.e. the
    // slot tracks the newest install rather than a freed or stale object
    ret = eval(ctx, "DOMException.prototype === "
                    "Object.getPrototypeOf(new DOMException)");
    assert(JS_ToBool(ctx, ret) == true);
    JS_FreeValue(ctx, ret);

    ret = eval(ctx, "new DOMException('m', 'InvalidCharacterError').name");
    assert(!JS_IsException(ret));
    s = JS_ToCString(ctx, ret);
    assert(s);
    assert(!strcmp(s, "InvalidCharacterError"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, ret);

    // the internally thrown DOMExceptions pick up the current prototype too
    ret = eval(ctx, "try { atob('a') } catch (e) { "
                    "  e instanceof DOMException && e.name } ");
    assert(!JS_IsException(ret));
    s = JS_ToCString(ctx, ret);
    assert(s);
    assert(!strcmp(s, "InvalidCharacterError"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, ret);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void backtrace_oom_callsite_array(void)
{
    static const char setup_code[] =
        "Error.prepareStackTrace = (e, frames) => frames;\n"
        "globalThis.f = () => new Error();\n";
    JSValue global_object, func, ret;
    JSMemoryUsage stats;
    uint32_t headroom;
    JSRuntime *rt;
    JSContext *ctx;

    rt = new_runtime();
    ctx = JS_NewContext(rt);
    global_object = JS_GetGlobalObject(ctx);

    ret = eval(ctx, setup_code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);

    func = JS_GetPropertyStr(ctx, global_object, "f");
    assert(JS_IsFunction(ctx, func));

    for (headroom = 0; headroom < 2048; headroom++) {
        JS_ComputeMemoryUsage(rt, &stats);
        JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + headroom);
        ret = JS_Call(ctx, func, JS_UNDEFINED, 0, NULL);
        JS_SetMemoryLimit(rt, 0);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, JS_GetException(ctx));
    }

    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, global_object);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void large_allocation_accounting(void)
{
    static const size_t block_size = 1024 * 1024;
    JSMemoryUsage before, after;
    JSValue ret, exception;
    JSRuntime *rt;
    JSContext *ctx;
    const char *str;

    rt = new_runtime();
    ctx = JS_NewContext(rt);

    /* the shapes JS_NewContext leaves behind are freed by the first GC inside
       eval(); collect them first so that the delta is the block alone */
    JS_RunGC(rt);
    JS_ComputeMemoryUsage(rt, &before);
    ret = eval(ctx, "globalThis.a = new Uint8Array(1024 * 1024)");
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    JS_ComputeMemoryUsage(rt, &after);
    assert(after.malloc_size - before.malloc_size >= (int64_t)block_size);

    JS_ComputeMemoryUsage(rt, &before);
    JS_SetMemoryLimit(rt, (size_t)before.malloc_size + 4 * block_size);
    ret = eval(ctx, "globalThis.a = [];\n"
                    "for (let i = 0; i < 64; i++) {\n"
                    "    // OOM exception should be uncatchable\n"
                    "    try {\n"
                    "        a.push(new Uint8Array(1024 * 1024));"
                    "    } catch (e) {\n"
                    "        throw 'unexpected exception';\n"
                    "    }\n"
                    "}");
    assert(JS_IsException(ret));
    JS_SetMemoryLimit(rt, 0);
    JS_ComputeMemoryUsage(rt, &after);
    assert(after.malloc_size - before.malloc_size < (int64_t)(8 * block_size));

    exception = JS_GetException(ctx);
    assert(JS_IsError(exception));
    str = JS_ToCString(ctx, exception);
    assert(str);
    assert(!strcmp(str, "InternalError: out of memory"));
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, exception);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void backtrace_oom_current_exception(void)
{
    static const char setup_code[] =
        "globalThis.f = function() { missing; };\n"
        "Object.defineProperty(f, 'name', { value: 'x'.repeat(2 * 1024 * 1024) });";
    JSMemoryUsage stats;
    JSValue ret, exception;
    JSRuntime *rt;
    JSContext *ctx;

    rt = new_runtime();
    ctx = JS_NewContext(rt);

    ret = eval(ctx, setup_code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);

    JS_ComputeMemoryUsage(rt, &stats);
    JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + 128 * 1024);

    ret = eval(ctx, "f()");
    assert(JS_IsException(ret));
    assert(JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    JS_SetMemoryLimit(rt, 0);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void proxy_own_keys_huge_length(void)
{
    static const char setup_code[] =
        "globalThis.p = new Proxy({}, {\n"
        "    ownKeys() { return { length: 0x20000000 }; },\n"
        "});";
    JSMemoryUsage stats;
    JSValue ret, exception;
    JSRuntime *rt;
    JSContext *ctx;
    const char *str;

    rt = new_runtime();
    ctx = JS_NewContext(rt);

    ret = eval(ctx, setup_code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);

    JS_ComputeMemoryUsage(rt, &stats);
    JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + 128 * 1024);

    ret = eval(ctx, "Object.keys(p)");
    assert(JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    exception = JS_GetException(ctx);
    str = JS_ToCString(ctx, exception);
    assert(str);
    /* the trap result has no index 0, so the first key must be rejected */
    assert(!strcmp(str, "TypeError: proxy: properties must be strings or symbols"));
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, exception);
    JS_SetMemoryLimit(rt, 0);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static int gop_get_own_property(JSContext *ctx, JSPropertyDescriptor *desc,
                                JSValueConst obj, JSAtom prop)
{
    const char *name;
    int found;

    found = 0;
    name = JS_AtomToCString(ctx, prop);
    if (!name)
        return -1;
    if (!strcmp(name, "answer")) {
        found = 1;
        *desc = (JSPropertyDescriptor){
            .value = JS_NewInt32(ctx, 42),
            .flags = JS_PROP_C_W_E | JS_PROP_HAS_VALUE,
        };
    }
    JS_FreeCString(ctx, name);
    return found;
}

static void global_object_prototype(void)
{
    static const char code[] = "answer";
    JSValue global_object, proto, ret;
    JSClassID class_id;
    JSRuntime *rt;
    JSContext *ctx;
    int32_t answer;
    int res;

    {
        rt = new_runtime();
        ctx = JS_NewContext(rt);
        proto = JS_NewObject(ctx);
        assert(JS_IsObject(proto));
        JSCFunctionListEntry prop = JS_PROP_INT32_DEF("answer", 42, JS_PROP_C_W_E);
        JS_SetPropertyFunctionList(ctx, proto, &prop, 1);
        global_object = JS_GetGlobalObject(ctx);
        res = JS_SetPrototype(ctx, global_object, proto);
        assert(res == true);
        JS_FreeValue(ctx, global_object);
        JS_FreeValue(ctx, proto);
        ret = eval(ctx, code);
        assert(!JS_IsException(ret));
        assert(JS_IsNumber(ret));
        res = JS_ToInt32(ctx, &answer, ret);
        assert(res == 0);
        assert(answer == 42);
        JS_FreeValue(ctx, ret);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
    {
        JSClassExoticMethods exotic = (JSClassExoticMethods){
            .get_own_property = gop_get_own_property,
        };
        JSClassDef def = (JSClassDef){
            .class_name = "Global Object",
            .exotic = &exotic,
        };
        rt = new_runtime();
        class_id = 0;
        JS_NewClassID(rt, &class_id);
        res = JS_NewClass(rt, class_id, &def);
        assert(res == 0);
        ctx = JS_NewContext(rt);
        proto = JS_NewObjectClass(ctx, class_id);
        assert(JS_IsObject(proto));
        global_object = JS_GetGlobalObject(ctx);
        res = JS_SetPrototype(ctx, global_object, proto);
        assert(res == true);
        JS_FreeValue(ctx, global_object);
        JS_FreeValue(ctx, proto);
        ret = eval(ctx, code);
        assert(!JS_IsException(ret));
        assert(JS_IsNumber(ret));
        res = JS_ToInt32(ctx, &answer, ret);
        assert(res == 0);
        assert(answer == 42);
        JS_FreeValue(ctx, ret);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
}

// https://github.com/quickjs-ng/quickjs/issues/1178
static void slice_string_tocstring(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue ret = eval(ctx, "'.'.repeat(16384).slice(1, -1)");
    assert(!JS_IsException(ret));
    assert(JS_IsString(ret));
    const char *str = JS_ToCString(ctx, ret);
    assert(strlen(str) == 16382);
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void immutable_array_buffer(void)
{
    JSValue obj, ret;
    bool immutable;
    char buf[96];
    int i, v;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    for (i = 0; i < 2; i++) {
        obj = JS_NewObject(ctx);
        immutable = (i == 0);
        assert(-1 == JS_IsImmutableArrayBuffer(JS_NULL));
        assert(-1 == JS_IsImmutableArrayBuffer(JS_UNDEFINED));
        assert(-1 == JS_IsImmutableArrayBuffer(obj));
        assert(-1 == JS_SetImmutableArrayBuffer(JS_NULL, immutable));
        assert(-1 == JS_SetImmutableArrayBuffer(JS_UNDEFINED, immutable));
        assert(-1 == JS_SetImmutableArrayBuffer(obj, immutable));
        JS_FreeValue(ctx, obj);
    }
    obj = eval(ctx, "globalThis.ab = new ArrayBuffer(1)");
    assert(!JS_IsException(obj));
    assert(JS_IsArrayBuffer(obj));
    assert(!JS_IsImmutableArrayBuffer(obj));
    for (i = 1; i <= 3; i++) {
        immutable = (i == 2);
        if (i > 1)
            JS_SetImmutableArrayBuffer(obj, immutable);
        assert(immutable == JS_IsImmutableArrayBuffer(obj));
        snprintf(buf, sizeof(buf),
                 "var ta = new Uint8Array(ab); ta[0] = %d; ta[0]", i);
        ret = eval(ctx, buf);
        assert(!JS_IsException(ret));
        assert(JS_IsNumber(ret));
        assert(0 == JS_ToInt32(ctx, &v, ret));
        JS_FreeValue(ctx, ret);
        if (immutable) {
            assert(v != i);
        } else {
            assert(v == i);
        }
    }
    JS_FreeValue(ctx, obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void *sab_test_alloc(void *opaque, size_t size)
{
    return malloc(size);
}

static void sab_test_free(void *opaque, void *ptr)
{
    free(ptr);
}

static void shared_array_buffer_growth(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue ret, exception;

    ret = eval(ctx, "new SharedArrayBuffer(16)");
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);

    ret = eval(ctx,
               "const sab = new SharedArrayBuffer(16, { maxByteLength: 16 });"
               "sab.grow(16);"
               "sab.byteLength === 16 && sab.maxByteLength === 16");
    assert(!JS_IsException(ret));
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    ret = eval(ctx, "new SharedArrayBuffer(16, { maxByteLength: 16384 })");
    assert(JS_IsException(ret));
    assert(JS_HasException(ctx));
    exception = JS_GetException(ctx);
    assert(JS_IsError(exception));
    JS_FreeValue(ctx, exception);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    JSSharedArrayBufferFunctions funcs = {
        .sab_alloc = sab_test_alloc,
        .sab_free = sab_test_free,
        .sab_dup = NULL,
        .sab_opaque = NULL,
    };

    rt = new_runtime();
    JS_SetSharedArrayBufferFunctions(rt, &funcs);
    ctx = JS_NewContext(rt);
    ret = eval(ctx,
               "const sab = new SharedArrayBuffer(16, { maxByteLength: 16384 });"
               "const u8 = new Uint8Array(sab);"
               "sab.grow(16384);"
               "u8[1024] === 0 && u8.byteLength === 16384");
    assert(!JS_IsException(ret));
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void get_uint8array(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue val;
    uint8_t *p;
    size_t size;
    uint8_t buf[3] = { 1, 2, 3 };

    val = eval(ctx, "new Uint8Array(0)");
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 0);
    JS_FreeValue(ctx, val);

    val = JS_NewUint8Array(ctx, NULL, 0, NULL, NULL, false);
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 0);
    JS_FreeValue(ctx, val);

    val = JS_NewUint8ArrayCopy(ctx, NULL, 0);
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 0);
    JS_FreeValue(ctx, val);

    val = JS_NewUint8ArrayCopy(ctx, buf, sizeof(buf));
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 3);
    assert(p[0] == 1 && p[1] == 2 && p[2] == 3);
    JS_FreeValue(ctx, val);

    val = eval(ctx, "new Uint8Array([4, 5, 6])");
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 3);
    assert(p[0] == 4 && p[1] == 5 && p[2] == 6);
    JS_FreeValue(ctx, val);

    val = eval(ctx, "new Int32Array(4)");
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p == NULL);
    assert(size == 0);
    JS_FreeValue(ctx, val);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void new_symbol(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue sym, ret;

    /* Local symbol with NULL description -> Symbol() */
    sym = JS_NewSymbol(ctx, NULL, false);
    assert(!JS_IsException(sym));
    assert(JS_IsSymbol(sym));
    JS_SetPropertyStr(ctx, global, "sym_local_null", sym);

    ret = eval(ctx, "typeof sym_local_null === 'symbol' && sym_local_null.description === undefined && Symbol.keyFor(sym_local_null) === undefined");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    /* Global symbol with NULL description -> Symbol.for() -> Symbol.for('undefined') */
    sym = JS_NewSymbol(ctx, NULL, true);
    assert(!JS_IsException(sym));
    assert(JS_IsSymbol(sym));
    JS_SetPropertyStr(ctx, global, "sym_global_null", sym);

    ret = eval(ctx, "typeof sym_global_null === 'symbol' && sym_global_null.description === 'undefined' && Symbol.keyFor(sym_global_null) === 'undefined'");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    /* Local symbol with description -> Symbol('test_local') */
    sym = JS_NewSymbol(ctx, "test_local", false);
    assert(!JS_IsException(sym));
    assert(JS_IsSymbol(sym));
    JS_SetPropertyStr(ctx, global, "sym_local_str", sym);

    ret = eval(ctx, "sym_local_str.description === 'test_local' && Symbol.keyFor(sym_local_str) === undefined");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    /* Global symbol with description -> Symbol.for('test_global') */
    sym = JS_NewSymbol(ctx, "test_global", true);
    assert(!JS_IsException(sym));
    assert(JS_IsSymbol(sym));
    JS_SetPropertyStr(ctx, global, "sym_global_str", sym);

    ret = eval(ctx, "sym_global_str.description === 'test_global' && Symbol.keyFor(sym_global_str) === 'test_global'");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void bulk_free_macros(void) {
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue val0 = JS_NewObject(ctx);
    JSValue val1 = JS_NewArray(ctx);
    JSValue val2 = JS_NewDate(ctx, 0.0);
    
    JSMemoryUsage mem_usage;
    JS_ComputeMemoryUsage(rt, &mem_usage);
    int obj_count = mem_usage.obj_count;

    JS_FreeValues(ctx, val0, val1);
    JS_FreeValuesRT(rt, val2);

    // silly atoms to ensure qjs doesn't find built-ins that match
    JSAtom atom0 = JS_NewAtom(ctx, "ALL!!");
    JSAtom atom1 = JS_NewAtom(ctx, "YOUR!!");
    JSAtom atom2 = JS_NewAtom(ctx, "ATOMS!!");
    JSAtom atom3 = JS_NewAtom(ctx, "ARE!!");
    JSAtom atom4 = JS_NewAtom(ctx, "BELONG!!");
    JSAtom atom5 = JS_NewAtom(ctx, "TO US!!");

    JS_ComputeMemoryUsage(rt, &mem_usage);
    assert((obj_count - 3) == mem_usage.obj_count);
    int atom_count = mem_usage.atom_count;

    JS_FreeAtoms(ctx, atom1, atom3, atom4);
    JS_FreeAtomsRT(rt, atom0, atom2, atom5);

    JS_ComputeMemoryUsage(rt, &mem_usage);
    assert((atom_count - 6) == mem_usage.atom_count);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static int detach_free_count;

static void *detach_realloc_func(JSRuntime *rt, void *opaque, void *ptr,
                                 size_t size)
{
    if (size == 0) {
        detach_free_count++;
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

static void detach_array_buffer_free_once(void)
{
    JSValue obj;
    uint8_t *buf;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    detach_free_count = 0;
    buf = malloc(8);
    obj = JS_NewArrayBuffer(ctx, buf, 8, /*max_len*/0, detach_realloc_func,
                            NULL, false);
    assert(JS_IsArrayBuffer(obj));

    /* detaching releases the backing store exactly once */
    JS_DetachArrayBuffer(ctx, obj);
    assert(detach_free_count == 1);

    /* finalizing the detached buffer must not release it a second time */
    JS_FreeValue(ctx, obj);
    assert(detach_free_count == 1);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void *realloc_array_buffer_data(JSRuntime *rt, void *opaque, void *ptr,
                                       size_t size)
{
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

static void typed_array_sort_index_overflow(void)
{
    const size_t len = 0x40000010; /* 2**30 + 16 */
    JSMemoryUsage stats;
    JSValue global, buffer, ret;
    JSRuntime *rt;
    JSContext *ctx;
    uint8_t *buf;

    /* the data is never read, but keep it real memory so a regression
       corrupts the heap in a way ASan can see rather than segfaulting */
    buf = calloc(1, len);
    rt = new_runtime();
    ctx = JS_NewContext(rt);

    buffer = JS_NewArrayBuffer(ctx, buf, len, /*max_len*/0,
                               realloc_array_buffer_data, NULL, false);
    assert(JS_IsArrayBuffer(buffer));
    global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "buf", buffer);
    JS_FreeValue(ctx, global);

    /* Cap memory so that the correctly computed 4 GB index array also
       fails to allocate on 64 bit platforms; the undersized allocation a
       32 bit platform computes fits well within the cap. */
    JS_ComputeMemoryUsage(rt, &stats);
    JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + 1024 * 1024);

    ret = eval(ctx, "new Uint8Array(buf).sort((a, b) => a - b)");
    assert(JS_IsException(ret));
    assert(JS_HasException(ctx));
    JS_FreeValue(ctx, ret);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static int transfer_free_count;
static int transfer_realloc_count;

static void *transfer_realloc_func(JSRuntime *rt, void *opaque, void *ptr,
                                   size_t size)
{
    if (size == 0) {
        transfer_free_count++;
        free(ptr);
        return NULL;
    }
    transfer_realloc_count++;
    return realloc(ptr, size);
}

/* a realloc function that can only free, never resize */
static void *failing_realloc_func(JSRuntime *rt, void *opaque, void *ptr,
                                  size_t size)
{
    if (size == 0) {
        transfer_free_count++;
        free(ptr);
    }
    return NULL;
}

static JSValue make_external_ab(JSContext *ctx, uint8_t **pbuf, size_t len,
                                size_t max_len,
                                JSReallocArrayBufferDataFunc *realloc_func)
{
    uint8_t *buf = malloc(max_len > len ? max_len : len);
    memset(buf, 0xAB, len);
    JSValue obj = JS_NewArrayBuffer(ctx, buf, len, max_len, realloc_func, NULL,
                                    /*is_shared*/false);
    assert(JS_IsArrayBuffer(obj));
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "ab", JS_DupValue(ctx, obj));
    JS_FreeValue(ctx, global);
    *pbuf = buf;
    return obj;
}

static void transfer_external_array_buffer(void)
{
    JSValue obj, ret, exc, det;
    const char *s;
    uint8_t *buf, *p;
    size_t i, size;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    // same-length transfer succeeds and reuses the backing store; each variant
    // detaches the source, so use a fresh buffer per iteration
    {
        static const char *const exprs[] = {
            "ab.transfer()", "ab.transfer(8)", "ab.transferToFixedLength()",
            "ab.transferToFixedLength(8)", "ab.transferToImmutable()",
        };
        for (i = 0; i < countof(exprs); i++) {
            transfer_free_count = 0;
            transfer_realloc_count = 0;
            obj = make_external_ab(ctx, &buf, 8, /*max_len*/0,
                                   transfer_realloc_func);

            ret = eval(ctx, exprs[i]);
            assert(!JS_IsException(ret));
            assert(JS_IsArrayBuffer(ret));
            assert(transfer_realloc_count == 0);
            p = JS_GetArrayBuffer(ctx, &size, ret);
            assert(p == buf);
            assert(size == 8);
            assert(p[0] == 0xAB && p[7] == 0xAB);
            det = eval(ctx, "ab.detached");
            assert(JS_IsBool(det));
            assert(JS_VALUE_GET_BOOL(det));
            JS_FreeValue(ctx, det);
            assert(transfer_free_count == 0);
            JS_FreeValue(ctx, ret);
            assert(transfer_free_count == 1);

            ret = eval(ctx, "globalThis.ab = undefined");
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, obj);
            assert(transfer_free_count == 1);
        }
    }

    // a length-changing transfer resizes the external store through the
    // realloc function and hands it over to the new ArrayBuffer
    {
        static const struct {
            const char *code;
            size_t len;
        } cases[] = {
            { "ab.transfer(4)", 4 },
            { "ab.transfer(16)", 16 },
            { "ab.transferToFixedLength(4)", 4 },
            { "ab.transferToImmutable(16)", 16 },
        };
        for (i = 0; i < countof(cases); i++) {
            transfer_free_count = 0;
            transfer_realloc_count = 0;
            obj = make_external_ab(ctx, &buf, 8, /*max_len*/0,
                                   transfer_realloc_func);

            ret = eval(ctx, cases[i].code);
            assert(!JS_IsException(ret));
            assert(JS_IsArrayBuffer(ret));
            assert(transfer_realloc_count == 1);
            assert(transfer_free_count == 0);
            p = JS_GetArrayBuffer(ctx, &size, ret);
            assert(size == cases[i].len);
            assert(p[0] == 0xAB && p[3] == 0xAB);
            if (cases[i].len > 8)
                assert(p[8] == 0 && p[15] == 0); // the grown tail is zeroed
            det = eval(ctx, "ab.detached");
            assert(JS_IsBool(det));
            assert(JS_VALUE_GET_BOOL(det));
            JS_FreeValue(ctx, det);

            // the source handed the store over; releasing the new AB frees it
            JS_FreeValue(ctx, ret);
            assert(transfer_free_count == 1);

            ret = eval(ctx, "globalThis.ab = undefined");
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, obj);
            assert(transfer_free_count == 1);
        }
    }

    // a transfer that fails to realloc throws and leaves the source alone
    {
        transfer_free_count = 0;
        obj = make_external_ab(ctx, &buf, 8, /*max_len*/0,
                               failing_realloc_func);

        ret = eval(ctx, "ab.transfer(16)");
        assert(JS_IsException(ret));
        JS_FreeValue(ctx, ret);
        exc = JS_GetException(ctx);
        assert(JS_IsError(exc));
        s = JS_ToCString(ctx, exc);
        assert(s);
        assert(strstr(s, "out of memory"));
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, exc);

        det = eval(ctx, "ab.detached");
        assert(JS_IsBool(det));
        assert(!JS_VALUE_GET_BOOL(det));
        JS_FreeValue(ctx, det);
        p = JS_GetArrayBuffer(ctx, &size, obj);
        assert(p == buf && size == 8 && p[0] == 0xAB);
        assert(transfer_free_count == 0);

        ret = eval(ctx, "globalThis.ab = undefined");
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, obj);
        assert(transfer_free_count == 1);
    }

    // transfer(0) changes the length but doesn't realloc: it detaches the
    // source (freeing the external store) and yields a fresh empty AB
    {
        transfer_free_count = 0;
        transfer_realloc_count = 0;
        obj = make_external_ab(ctx, &buf, 8, /*max_len*/0,
                               transfer_realloc_func);

        ret = eval(ctx, "ab.transfer(0)");
        assert(!JS_IsException(ret));
        assert(JS_IsArrayBuffer(ret));
        p = JS_GetArrayBuffer(ctx, &size, ret);
        assert(size == 0);
        assert(transfer_free_count == 1);
        JS_FreeValue(ctx, ret);

        det = eval(ctx, "ab.detached");
        assert(JS_IsBool(det));
        assert(JS_VALUE_GET_BOOL(det));
        JS_FreeValue(ctx, det);

        ret = eval(ctx, "globalThis.ab = undefined");
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, obj);
        assert(transfer_free_count == 1);
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void resize_external_array_buffer(void)
{
    JSValue obj, global, ret, exc, det;
    const char *s;
    uint8_t *buf, *p;
    size_t size;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    transfer_free_count = 0;
    transfer_realloc_count = 0;
    obj = make_external_ab(ctx, &buf, 8, /*max_len*/16, transfer_realloc_func);

    ret = eval(ctx, "ab.resizable && ab.maxByteLength === 16");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    // growing reallocs through the callback and zeroes the new tail
    ret = eval(ctx, "ab.resize(16), ab.byteLength");
    assert(!JS_IsException(ret));
    assert(JS_VALUE_GET_INT(ret) == 16);
    JS_FreeValue(ctx, ret);
    assert(transfer_realloc_count == 1);
    p = JS_GetArrayBuffer(ctx, &size, obj);
    assert(size == 16);
    assert(p[0] == 0xAB && p[7] == 0xAB);
    assert(p[8] == 0 && p[15] == 0);

    // and so does shrinking
    ret = eval(ctx, "ab.resize(4), ab.byteLength");
    assert(!JS_IsException(ret));
    assert(JS_VALUE_GET_INT(ret) == 4);
    JS_FreeValue(ctx, ret);
    assert(transfer_realloc_count == 2);
    p = JS_GetArrayBuffer(ctx, &size, obj);
    assert(size == 4);
    assert(p[0] == 0xAB && p[3] == 0xAB);

    // resizing past maxByteLength still throws, without calling the callback
    ret = eval(ctx, "ab.resize(17)");
    assert(JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    exc = JS_GetException(ctx);
    s = JS_ToCString(ctx, exc);
    assert(s);
    assert(strstr(s, "invalid array buffer length"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, exc);
    assert(transfer_realloc_count == 2);

    // a resizable external AB carries its maxByteLength across a transfer
    ret = eval(ctx, "ab.transfer(8)");
    assert(!JS_IsException(ret));
    assert(transfer_realloc_count == 3);
    det = eval(ctx, "ab.detached");
    assert(JS_IsBool(det));
    assert(JS_VALUE_GET_BOOL(det));
    JS_FreeValue(ctx, det);
    global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "ab2", ret);
    JS_FreeValue(ctx, global);
    ret = eval(ctx, "ab2.resizable && ab2.maxByteLength === 16");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);
    ret = eval(ctx, "ab2.resize(16), ab2.byteLength");
    assert(!JS_IsException(ret));
    assert(JS_VALUE_GET_INT(ret) == 16);
    JS_FreeValue(ctx, ret);
    assert(transfer_realloc_count == 4);

    assert(transfer_free_count == 0);
    ret = eval(ctx, "globalThis.ab = globalThis.ab2 = undefined");
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, obj);
    assert(transfer_free_count == 1);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

/* hands the buffer over to the runtime allocator, the way quickjs manages the
   memory of ArrayBuffers it allocates itself */
static void *runtime_realloc_func(JSRuntime *rt, void *opaque, void *ptr,
                                  size_t size)
{
    return js_realloc_rt(rt, ptr, size);
}

static void transfer_default_managed_array_buffer(void)
{
    JSValue obj, global, ret, det;
    uint8_t *buf, *p;
    size_t size;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    buf = js_malloc(ctx, 8);
    memset(buf, 0xAB, 8);
    obj = JS_NewArrayBuffer(ctx, buf, 8, /*max_len*/0,
                            runtime_realloc_func, NULL, false);
    assert(JS_IsArrayBuffer(obj));
    global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "ab", JS_DupValue(ctx, obj));
    JS_FreeValue(ctx, global);

    ret = eval(ctx, "ab.transfer(16)");
    assert(!JS_IsException(ret));
    assert(JS_IsArrayBuffer(ret));
    p = JS_GetArrayBuffer(ctx, &size, ret);
    assert(size == 16);
    assert(p[0] == 0xAB && p[7] == 0xAB);
    assert(p[8] == 0 && p[15] == 0);
    det = eval(ctx, "ab.detached");
    assert(JS_IsBool(det));
    assert(JS_VALUE_GET_BOOL(det));
    JS_FreeValue(ctx, det);
    JS_FreeValue(ctx, ret);
    ret = eval(ctx, "globalThis.ab = undefined");
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, obj);

    buf = js_malloc(ctx, 8);
    memset(buf, 0xCD, 8);
    obj = JS_NewArrayBuffer(ctx, buf, 8, /*max_len*/0,
                            runtime_realloc_func, NULL, false);
    global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "ab", JS_DupValue(ctx, obj));
    JS_FreeValue(ctx, global);

    ret = eval(ctx, "ab.transferToFixedLength(4)");
    assert(!JS_IsException(ret));
    p = JS_GetArrayBuffer(ctx, &size, ret);
    assert(size == 4);
    assert(p[0] == 0xCD && p[3] == 0xCD);
    JS_FreeValue(ctx, ret);
    ret = eval(ctx, "globalThis.ab = undefined");
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, obj);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void get_class_name(void)
{
    static const struct {
        const char *code;
        const char *name;
    } builtins[] = {
        { "({})",               "Object" },
        { "[]",                 "Array" },
        { "new Error()",        "Error" },
        { "new Date()",         "Date" },
        { "/re/",               "RegExp" },
        { "new Map()",          "Map" },
        { "new ArrayBuffer(0)", "ArrayBuffer" },
        { "new Uint8Array(0)",  "Uint8Array" },
        { "(function(){})",     "Function" },
    };
    JSClassDef def = (JSClassDef){ .class_name = "MyClass" };
    JSClassID class_id, unregistered_class_id;
    JSAtom atom, expected;
    JSRuntime *rt;
    JSContext *ctx;
    const char *s;
    JSValue obj;
    size_t i;

    rt = new_runtime();
    class_id = 0;
    JS_NewClassID(rt, &class_id);
    assert(0 == JS_NewClass(rt, class_id, &def));

    /* handed out by JS_NewClassID() but never passed to JS_NewClass() */
    unregistered_class_id = 0;
    JS_NewClassID(rt, &unregistered_class_id);

    ctx = JS_NewContext(rt);

    /* the name of a class registered from C, not its class id */
    expected = JS_NewAtom(ctx, "MyClass");
    atom = JS_GetClassName(rt, class_id);
    assert(atom == expected);
    s = JS_AtomToCString(ctx, atom);
    assert(s);
    assert(!strcmp(s, "MyClass"));
    JS_FreeCString(ctx, s);
    JS_FreeAtom(ctx, atom);
    JS_FreeAtom(ctx, expected);

    /* the names of the built-in classes */
    for (i = 0; i < countof(builtins); i++) {
        obj = eval(ctx, builtins[i].code);
        assert(!JS_IsException(obj));
        atom = JS_GetClassName(rt, JS_GetClassID(obj));
        assert(atom != JS_ATOM_NULL);
        s = JS_AtomToCString(ctx, atom);
        assert(s);
        assert(!strcmp(s, builtins[i].name));
        JS_FreeCString(ctx, s);
        JS_FreeAtom(ctx, atom);
        JS_FreeValue(ctx, obj);
    }

    /* class ids without a registered class have no name */
    assert(JS_ATOM_NULL == JS_GetClassName(rt, JS_INVALID_CLASS_ID));
    assert(JS_ATOM_NULL == JS_GetClassName(rt, unregistered_class_id));
    assert(JS_ATOM_NULL == JS_GetClassName(rt, class_id + 4096));

    /* the caller owns the returned atom; the class keeps its own reference,
       so releasing it repeatedly doesn't free the name out from under it */
    for (i = 0; i < 64; i++)
        JS_FreeAtom(ctx, JS_GetClassName(rt, class_id));
    atom = JS_GetClassName(rt, class_id);
    s = JS_AtomToCString(ctx, atom);
    assert(s);
    assert(!strcmp(s, "MyClass"));
    JS_FreeCString(ctx, s);
    JS_FreeAtom(ctx, atom);

    /* every registered class has a name, and it is a name rather than a
       number: returning the class id instead produced a valid-looking atom,
       either an unrelated predefined one for a small id or the id spelled out
       in decimal for a large one, so check the whole table rather than the
       handful of classes spelled out above */
    {
        JSClassID id;
        int registered = 0;

        for (id = 1; id < class_id + 16; id++) {
            char decimal[32];
            if (!JS_IsRegisteredClass(rt, id))
                continue;
            registered++;
            atom = JS_GetClassName(rt, id);
            assert(atom != JS_ATOM_NULL);
            s = JS_AtomToCString(ctx, atom);
            assert(s);
            // a few internal classes are deliberately unnamed, but none is
            // named after a number
            snprintf(decimal, sizeof(decimal), "%u", (unsigned)id);
            assert(strcmp(s, decimal));
            assert(s[0] < '0' || s[0] > '9');
            JS_FreeCString(ctx, s);
            JS_FreeAtom(ctx, atom);
        }
        /* the built-ins alone are far more than this */
        assert(registered > 20);
    }

    /* two classes sharing a name share the interned atom, and the name does
       not depend on which context asks */
    {
        JSClassDef def2 = (JSClassDef){ .class_name = "MyClass" };
        JSClassID class_id2 = 0;
        JSContext *ctx2;
        JSAtom a1, a2;

        JS_NewClassID(rt, &class_id2);
        assert(0 == JS_NewClass(rt, class_id2, &def2));
        assert(class_id2 != class_id);

        a1 = JS_GetClassName(rt, class_id);
        a2 = JS_GetClassName(rt, class_id2);
        assert(a1 == a2);
        JS_FreeAtom(ctx, a1);
        JS_FreeAtom(ctx, a2);

        ctx2 = JS_NewContext(rt);
        a1 = JS_GetClassName(rt, class_id);
        a2 = JS_GetClassName(rt, class_id);
        assert(a1 == a2);
        JS_FreeAtom(ctx2, a1);
        JS_FreeAtom(ctx2, a2);
        JS_FreeContext(ctx2);
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

void object_from(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue t0 = JS_NewObject(ctx);
    assert(!JS_IsException(t0));
    assert(JS_IsObject(t0));
    JSAtom prop = JS_NewAtomLen(ctx, "prop", 4);
    assert(prop != JS_ATOM_NULL);
    JSValue val = JS_NULL;
    JSValue t1 = JS_NewObjectFrom(ctx, 1, &prop, &val);
    assert(!JS_IsException(t1));
    assert(JS_IsObject(t1));
    uint32_t old_shape_hash_count = js_std_cmd(/*GetShapeHashCount*/4, rt);
    JSValue t2 = JS_NewObjectFrom(ctx, 1, &prop, &val);
    assert(!JS_IsException(t2));
    assert(JS_IsObject(t2));
    uint32_t new_shape_hash_count = js_std_cmd(/*GetShapeHashCount*/4, rt);
    assert(old_shape_hash_count == new_shape_hash_count);
    JS_FreeAtom(ctx, prop);
    JS_FreeValue(ctx, t2);
    JS_FreeValue(ctx, t1);
    JS_FreeValue(ctx, t0);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

void add_intrinsic_bigint(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContextRaw(rt);
    JS_AddIntrinsicBaseObjects(ctx);
    JS_AddIntrinsicBigInt(ctx);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

void new_typed_array(void)
{
    JSValueConst argv[4];
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    uint8_t *buf = js_malloc(ctx, 8);
    JSValue ab = JS_NewArrayBuffer(ctx, buf, 8, /*max_len*/0, NULL, NULL, false);
    assert(JS_IsArrayBuffer(ab));
    for (int argc = 0; argc < (int)countof(argv); argc++) {
        JSValue ret = JS_NewTypedArray(ctx, argc, argv, JS_TYPED_ARRAY_UINT8);
        assert(!JS_IsException(ret));
        assert(JS_IsObject(ret));
        JS_FreeValue(ctx, ret);
        argv[argc] = JS_UNDEFINED;
        argv[0] = ab;
    }
    JS_FreeValue(ctx, ab);
    js_free(ctx, buf);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

void private_symbols(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue pub = JS_NewSymbol(ctx, "public", /*is_global*/false);
    JSValue priv = JS_NewPrivateSymbol(ctx, "private");
    assert(JS_IsSymbol(pub));
    assert(JS_IsSymbol(priv));
    JSValue obj = JS_NewObject(ctx);
    assert(JS_IsObject(obj));
    assert(true == JS_SetPropertyValue(ctx, obj, pub, JS_TRUE, JS_PROP_C_W_E));
    assert(true == JS_SetPropertyValue(ctx, obj, priv, JS_FALSE, JS_PROP_C_W_E));
    JSValue global_object = JS_GetGlobalObject(ctx);
    assert(true == JS_SetPropertyStr(ctx, global_object, "o", JS_DupValue(ctx, obj)));
    JS_FreeValue(ctx, global_object);
    JSValue result = eval(ctx, "Object.getOwnPropertySymbols(o)");
    assert(JS_IsArray(result));
    int64_t length = -1;
    assert(0 == JS_GetLength(ctx, result, &length));
    assert(length == 1);
    JSValue item = JS_GetPropertyUint32(ctx, result, 0);
    assert(JS_IsSymbol(item));
    assert(JS_IsSameValue(ctx, item, pub));
    JS_FreeValue(ctx, item);
    JS_FreeValue(ctx, result);
    result = JS_GetPropertyValue(ctx, obj, JS_DupValue(ctx, pub));
    assert(JS_IsBool(result));
    assert(JS_IsSameValue(ctx, result, JS_TRUE));
    result = JS_GetPropertyValue(ctx, obj, JS_DupValue(ctx, priv));
    assert(JS_IsBool(result));
    assert(JS_IsSameValue(ctx, result, JS_FALSE));
    JS_FreeValue(ctx, obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

// https://github.com/quickjs-ng/quickjs/issues/1181
// JS_NewContext() must fail cleanly, not crash or leak, whichever of its
// allocations runs out of memory.
static void new_context_low_memory(void)
{
    JSMemoryUsage stats;
    size_t base, headroom;
    JSRuntime *rt;
    JSContext *ctx;
    int all, ok;

    all = ok = 0;
    rt = new_runtime();
    JS_ComputeMemoryUsage(rt, &stats);
    base = (size_t)stats.malloc_size;
    // Small steps first, so that each of the first allocations gets its turn
    // to fail, then bigger ones until a few contexts fit.
    for (headroom = 0; ok < 4 && headroom < 4 * 1024 * 1024;
         headroom += headroom < 8192 ? 8 : 1024) {
        JS_SetMemoryLimit(rt, base + headroom);
        ctx = JS_NewContext(rt); // expected to fail, not to crash
        if (ctx) {
            JS_FreeContext(ctx);
            ok++;
        }
        all++;
    }
    JS_SetMemoryLimit(rt, 0);
    JS_FreeRuntime(rt);
    assert(ok > 0);     // expect some successes...
    assert(ok < all);   // ...but not all
}

// running out of memory in JSON.stringify must not leak the array element
// that was being converted
static void json_stringify_oom(void)
{
    static const char setup_code[] =
        "globalThis.f = () => JSON.stringify([{}]);\n"
        "f();\n"; // JSON.stringify is set up on first access, do it now
    JSValue global_object, func, ret;
    JSMemoryUsage stats;
    uint32_t headroom;
    JSRuntime *rt;
    JSContext *ctx;

    rt = new_runtime();
    ctx = JS_NewContext(rt);
    global_object = JS_GetGlobalObject(ctx);

    ret = eval(ctx, setup_code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);

    func = JS_GetPropertyStr(ctx, global_object, "f");
    assert(JS_IsFunction(ctx, func));

    for (headroom = 0; headroom < 2048; headroom++) {
        JS_ComputeMemoryUsage(rt, &stats);
        JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + headroom);
        ret = JS_Call(ctx, func, JS_UNDEFINED, 0, NULL);
        JS_SetMemoryLimit(rt, 0);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, JS_GetException(ctx));
    }

    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, global_object);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

/* The CPU budget of an embedder ran out: from now on the handler says stop
   at every poll. */
static int stop_interrupt_handler(JSContext *ctx, void *opaque)
{
    int *polls = (int *)opaque;
    return ++*polls > 50;
}

/* The uncatchable exception raised by the interrupt handler must reach the
   embedder through every construct that turns an exception into a value on
   the way out: a Promise executor, Promise.try, an async generator, the
   Promise.all/race iterator, a thenable getter, using and DisposableStack
   disposers, an iterator return() during a throw, Error.prepareStackTrace
   and Error.stackTraceLimit. Otherwise the script goes on and eval() returns
   the trailing 1. Each construct runs at the top level and inside a job. */
static void uncatchable_interrupt_sites(void)
{
    static const char *const sites[] = {
        "try { for (;;) {} } catch (e) {}",
        "try { for (;;) {} } finally {}",
        "new Promise(() => { for (;;) {} })",
        "Promise.try(() => { for (;;) {} })",
        "(async function* () { for (;;) {} })().next()",
        "Promise.all({ [Symbol.iterator]() { for (;;) {} } })",
        "Promise.race({ [Symbol.iterator]() { for (;;) {} } })",
        "Promise.resolve({ get then() { for (;;) {} } })",
        "try { { using x = { [Symbol.dispose]() { for (;;) {} } };"
        " throw new Error('body'); } } catch (e) {}",
        "try { const s = new DisposableStack();"
        " s.defer(() => { throw new Error('a'); });"
        " s.defer(() => { for (;;) {} }); s.dispose(); } catch (e) {}",
        "try { for (const x of { [Symbol.iterator]() { return {"
        " next() { return { value: 1, done: false }; },"
        " return() { for (;;) {} } }; } }) throw new Error('body'); }"
        " catch (e) {}",
        "try { Array.from({ [Symbol.iterator]() { return {"
        " next() { return { value: 1, done: false }; },"
        " return() { for (;;) {} } }; } }, () => { throw new Error('body'); });"
        " } catch (e) {}",
        "Error.prepareStackTrace = () => { for (;;) {} };"
        " try { null.x; } catch (e) {}",
        "Error.stackTraceLimit = { valueOf() { for (;;) {} } };"
        " try { null.x; } catch (e) {}",
    };
    static const char *const modes[] = {
        "%s; 1",
        "Promise.resolve().then(() => { %s; }); 1",
    };
    char code[1024];
    size_t i, j;

    for (i = 0; i < sizeof(sites) / sizeof(sites[0]); i++) {
        for (j = 0; j < sizeof(modes) / sizeof(modes[0]); j++) {
            JSRuntime *rt = new_runtime();
            JSContext *ctx = JS_NewContext(rt);
            JSValue ret, e;
            const char *str;
            int polls = 0, r;

            snprintf(code, sizeof(code), modes[j], sites[i]);
            JS_SetInterruptHandler(rt, stop_interrupt_handler, &polls);
            ret = eval(ctx, code);
            if (j == 1) {
                /* the construct runs in a job */
                assert(!JS_IsException(ret));
                JS_FreeValue(ctx, ret);
                r = 1;
                while (r > 0)
                    r = JS_ExecutePendingJob(rt, &ctx);
                assert(r == -1);
            } else {
                assert(JS_IsException(ret));
            }
            assert(polls > 50);
            assert(JS_HasException(ctx));
            e = JS_GetException(ctx);
            assert(JS_IsError(e));
            assert(JS_IsUncatchableError(e));
            str = JS_ToCString(ctx, e);
            assert(str != NULL);
            assert(strstr(str, "InternalError: interrupted"));
            JS_FreeCString(ctx, str);
            JS_FreeValue(ctx, e);
            /* the embedder took the exception: the runtime is usable again */
            JS_SetInterruptHandler(rt, NULL, NULL);
            ret = eval(ctx, "1 + 1");
            assert(JS_VALUE_GET_TAG(ret) == JS_TAG_INT);
            assert(JS_VALUE_GET_INT(ret) == 2);
            JS_FreeValue(ctx, ret);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
        }
    }
}

/* At the memory limit the error object of the interrupt could not be
   allocated and JS_NULL was thrown instead, which a plain catch block
   catches. The embedder must get an uncatchable InternalError. */
static void uncatchable_interrupt_at_memory_limit(void)
{
    static const char setup_code[] =
        "globalThis.keep = [];"
        " for (let i = 0; i < 64; i++) keep.push(new Uint8Array(16384));"
        " globalThis.f = function() { try { for (;;) {} } catch (e) {} return 1; };";
    JSValue global_object, func, ret, e;
    JSMemoryUsage stats;
    uint32_t headroom;
    const char *str;

    for (headroom = 0; headroom < 2048; headroom += 128) {
        JSRuntime *rt = new_runtime();
        JSContext *ctx = JS_NewContext(rt);
        int polls = 0;

        ret = eval(ctx, setup_code);
        assert(!JS_IsException(ret));
        JS_FreeValue(ctx, ret);
        global_object = JS_GetGlobalObject(ctx);
        func = JS_GetPropertyStr(ctx, global_object, "f");
        assert(JS_IsFunction(ctx, func));
        /* the function is compiled; only the call runs at the limit */
        JS_ComputeMemoryUsage(rt, &stats);
        JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + headroom);
        JS_SetInterruptHandler(rt, stop_interrupt_handler, &polls);
        ret = JS_Call(ctx, func, JS_UNDEFINED, 0, NULL);
        assert(JS_IsException(ret));
        assert(polls > 50);
        e = JS_GetException(ctx);
        assert(JS_IsError(e));
        assert(JS_IsUncatchableError(e));
        JS_SetMemoryLimit(rt, 0);
        str = JS_ToCString(ctx, e);
        assert(str != NULL);
        assert(strstr(str, "InternalError: interrupted"));
        JS_FreeCString(ctx, str);
        JS_FreeValue(ctx, e);
        JS_FreeValue(ctx, func);
        JS_FreeValue(ctx, global_object);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
}

/* Running out of memory is uncatchable too: no catch block runs, and the
   embedder gets an InternalError object even when the error object itself
   was the allocation that failed. The limits sweep the point at which the
   allocation fails so that both the object and its property array hit it. */
static void uncatchable_out_of_memory(void)
{
    static const char *const codes[] = {
        "var caught = 0; try { const a = [];"
        " for (;;) a.push({ a: 1, b: 2, c: 3 }); } catch (e) { caught = 1; }"
        " caught",
        "var caught = 0; try { const a = [];"
        " for (;;) a.push(new Array(1000).fill(1)); } catch (e) { caught = 1; }"
        " caught",
    };
    size_t i, limit;

    for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        for (limit = 4000000; limit < 4000000 + 40 * 997; limit += 997) {
            JSRuntime *rt = new_runtime();
            JSContext *ctx;
            JSValue ret, e;
            const char *str;

            JS_SetMemoryLimit(rt, limit);
            ctx = JS_NewContext(rt);
            ret = eval(ctx, codes[i]);
            assert(JS_IsException(ret));
            e = JS_GetException(ctx);
            assert(JS_IsError(e));
            assert(JS_IsUncatchableError(e));
            JS_SetMemoryLimit(rt, 0);
            str = JS_ToCString(ctx, e);
            assert(str != NULL);
            assert(strstr(str, "InternalError: out of memory"));
            JS_FreeCString(ctx, str);
            JS_FreeValue(ctx, e);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
        }
    }
}

/* A push_scope() allocation failure must fail the whole compilation cleanly.
   push_scope() returns -1 when the parser cannot grow fd->scopes; every caller
   has to propagate that, otherwise scope_level walks negative and a later
   fd->scopes[scope_level] access becomes an out-of-bounds read/write. We compile
   a scope-heavy program under a sweep of memory ceilings so that some ceiling
   lands inside the parser's scope allocations, and assert the parser either
   compiles it or reports a clean error object, never crashing. The out-of-bounds
   access itself is caught by ASan (the suite's CI configuration): on a fixed
   engine the sweep stays clean, on a regressed one the scopes[] underflow is a
   heap-buffer-overflow. This asserts clean failure; it does not try to observe a
   corruption.

   Note: this uses a plain JS_NewRuntime() rather than new_runtime(), i.e. it does
   not enable JS_ABORT_ON_LEAKS. QuickJS's parser has a separate, pre-existing
   atom-cleanup gap on the compile-time OOM path (the identifier atoms of block-
   and catch-scoped variables are not all freed when a parse aborts for OOM); that
   is a benign leak, orthogonal to the memory corruption guarded here, and would
   otherwise mask this test. */
static void parser_scope_out_of_memory(void)
{
    static char src[1 << 16];
    size_t len = 0;
    int unit = 0;
    uint32_t headroom;

    /* A breadth of scope-pushing constructs: block, if, for, for-of, switch,
       try/catch, with, function body, class body / private-field / method /
       static-init scopes, and a derived class whose default constructor is
       synthesized at parse time. Sequential units also grow the top-level
       function's scope array, so a ceiling in the sweep lands on a push_scope()
       reallocation. Nesting stays shallow so the only failure under the ceiling
       is out of memory, never a stack overflow. */
    for (unit = 0; unit < 48; unit++) {
        int n = snprintf(src + len, sizeof(src) - len,
            "{let a=0;if(a){let b=1;}for(let i=0;i<1;i++){let c=i;}"
            "for(const z of[0]){let q=z;}try{let d=2;}catch(e){let f=3;}"
            "switch(a){case 0:{let g=4;}}with({}){let w=0;}}\n"
            "function fn%d(){let h=0;{let k=1;}return h;}\n"
            "class C%d{#p=0;m(){let x=0;}static{let y=1;}}\n"
            "class D%d extends C%d{}\n",
            unit, unit, unit, unit);
        assert(n > 0 && (size_t)n < sizeof(src) - len);
        len += (size_t)n;
    }

    /* Given enough memory the program must compile: proves the sweep is not
       vacuously failing on an invalid source. */
    {
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx = JS_NewContext(rt);
        JSValue ret = JS_Eval(ctx, src, len, "<scope>",
                              JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
        assert(!JS_IsException(ret));
        JS_FreeValue(ctx, ret);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }

    /* Sweep the ceiling from "fails on an early parser allocation" up to
       "compiles fully". Every outcome must be clean. */
    for (headroom = 0; headroom < 4u * 1024 * 1024; headroom += 48 * 1024) {
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx = JS_NewContext(rt);
        JSMemoryUsage stats;
        JSValue ret;

        JS_ComputeMemoryUsage(rt, &stats);
        JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + headroom);
        ret = JS_Eval(ctx, src, len, "<scope>",
                      JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
        if (JS_IsException(ret)) {
            JSValue e = JS_GetException(ctx);
            const char *str;
            assert(JS_IsError(e));        /* a real error object, not corruption */
            JS_SetMemoryLimit(rt, 0);     /* lift so stringifying can allocate */
            str = JS_ToCString(ctx, e);
            assert(str != NULL);
            assert(strstr(str, "out of memory") != NULL);
            JS_FreeCString(ctx, str);
            JS_FreeValue(ctx, e);
        } else {
            JS_SetMemoryLimit(rt, 0);
        }
        JS_FreeValue(ctx, ret);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);               /* ABORT_ON_LEAKS catches corruption */
    }
}

/* `await` is a keyword token outside async functions too (in modules, in class
   static blocks), and js_parse_for_in_of() accepted an `await using` head
   there without asking whether the function could await. Its disposal emits
   OP_await into a normal function, whose frame then left the interpreter
   through the generator exit and skipped close_var_refs(). The parser must
   refuse the declaration wherever `await` is refused, and still accept it in
   async functions and at a module's top level. */
static int compiles(const char *src, int flags)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue ret = JS_Eval(ctx, src, strlen(src), "<await-using>",
                          flags | JS_EVAL_FLAG_COMPILE_ONLY);
    int ok = !JS_IsException(ret);
    if (!ok) {
        JSValue e = JS_GetException(ctx);
        const char *str = JS_ToCString(ctx, e);
        assert(str != NULL);
        assert(strstr(str, "SyntaxError") != NULL);
        JS_FreeCString(ctx, str);
        JS_FreeValue(ctx, e);
    }
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return ok;
}

static void await_using_outside_async(void)
{
    const int script = JS_EVAL_TYPE_GLOBAL, module = JS_EVAL_TYPE_MODULE;

    assert(!compiles("class C { static { for (await using x of []) {} } }", script));
    assert(!compiles("class C { static { for (await using x in {}) {} } }", script));
    assert(!compiles("function f() { for (await using x of []) {} }", module));
    assert(!compiles("class C { static { for (await using x of []) {} } }", module));
    assert(!compiles("export function f() { for (await using [a] of []) {} }", module));

    assert(compiles("async function f() { for (await using x of []) {} }", script));
    assert(compiles("(async () => { for (await using x of []) {} })", script));
    assert(compiles("async function* g() { for (await using x of []) {} }", script));
    assert(compiles("for (await using x of []) {}", module));
    assert(compiles("function f() { for (using x of []) {} }", script));
}

/* The sibling of parser_scope_out_of_memory for the label array. new_label()
   returns -1 when the parser cannot grow fd->label_slots; the for-in/of and
   C-style for headers and js_parse_destructuring_element use the returned
   label as an array index (a relocation loop from label_cont, or
   label_slots[label_parse]) before the poisoned-bytecode error is noticed, so
   an ignored -1 becomes an out-of-bounds access at label_slots[-1]. This is a
   distinct array from the scopes[] underflow above and is not covered by the
   scope_level<0 backstop. A label-heavy program under the same ceiling sweep
   lands a ceiling on a new_label() reallocation; ASan catches the underflow on
   a regressed engine, and the parser must otherwise report a clean error.
   Plain JS_NewRuntime() for the same pre-existing-atom-leak reason noted above. */
static void parser_label_out_of_memory(void)
{
    static char src[1 << 16];
    size_t len = 0;
    int unit = 0;
    uint32_t headroom;

    /* Each unit mints labels in the three headers that index by a new_label()
       result: a for-of (js_parse_for_in_of), a C-style for, and an array
       destructuring target (js_parse_destructuring_element). Sequential units
       grow label_slots so a ceiling in the sweep lands on a new_label(). */
    while (len + 128 < sizeof(src)) {
        int n = snprintf(src + len, sizeof(src) - len,
            "for(const z%d of[0]){let q=z%d;}"
            "for(let i%d=0;i%d<1;i%d++){let c=i%d;}"
            "let[a%d,b%d]=[0,1];\n",
            unit, unit, unit, unit, unit, unit, unit, unit);
        if (n <= 0 || (size_t)n >= sizeof(src) - len)
            break;
        len += (size_t)n;
        unit++;
    }

    {
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx = JS_NewContext(rt);
        JSValue ret = JS_Eval(ctx, src, len, "<label>",
                              JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
        assert(!JS_IsException(ret));
        JS_FreeValue(ctx, ret);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }

    int oom_seen = 0;
    for (headroom = 0; headroom < 4u * 1024 * 1024; headroom += 48 * 1024) {
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx = JS_NewContext(rt);
        JSMemoryUsage stats;
        JSValue ret;

        JS_ComputeMemoryUsage(rt, &stats);
        JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + headroom);
        ret = JS_Eval(ctx, src, len, "<label>",
                      JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
        if (JS_IsException(ret)) {
            JSValue e = JS_GetException(ctx);
            const char *str;
            /* The parse must fail cleanly, never with a wild access (ASan
               guards that). Under a ceiling a failed label allocation surfaces
               as out of memory, or, when it poisons parsing first, as a plain
               SyntaxError or an uninitialized completion value; all are clean. */
            JS_SetMemoryLimit(rt, 0);
            str = JS_ToCString(ctx, e);
            if (str != NULL && strstr(str, "out of memory") != NULL)
                oom_seen = 1;
            JS_FreeCString(ctx, str);
            JS_FreeValue(ctx, e);
        } else {
            JS_SetMemoryLimit(rt, 0);
        }
        JS_FreeValue(ctx, ret);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
    /* The sweep must have reached the memory limit mid-parse at least once. */
    assert(oom_seen);
}

typedef struct {
    size_t size;
    size_t reserved;
} AllocFailureHeader;

static struct {
    bool armed;
    long requests;
    long fail_at;
} alloc_failure;

static bool alloc_failure_hits(void)
{
    return alloc_failure.armed &&
        ++alloc_failure.requests == alloc_failure.fail_at;
}

static void *alloc_failure_malloc(void *opaque, size_t size)
{
    AllocFailureHeader *h;

    if (alloc_failure_hits())
        return NULL;
    h = malloc(sizeof(*h) + size);
    if (!h)
        return NULL;
    h->size = size;
    return h + 1;
}

static void *alloc_failure_calloc(void *opaque, size_t count, size_t size)
{
    void *ptr = alloc_failure_malloc(opaque, count * size);

    if (ptr)
        memset(ptr, 0, count * size);
    return ptr;
}

static void alloc_failure_free(void *opaque, void *ptr)
{
    if (ptr)
        free((AllocFailureHeader *)ptr - 1);
}

static void *alloc_failure_realloc(void *opaque, void *ptr, size_t size)
{
    AllocFailureHeader *h = ptr ? (AllocFailureHeader *)ptr - 1 : NULL;

    if ((!h || size > h->size) && alloc_failure_hits())
        return NULL;
    h = realloc(h, sizeof(*h) + size);
    if (!h)
        return NULL;
    h->size = size;
    return h + 1;
}

static size_t alloc_failure_usable_size(const void *ptr)
{
    return ptr ? ((const AllocFailureHeader *)ptr - 1)->size : 0;
}

static long compile_with_alloc_failure(const char *src, int eval_type,
                                       long fail_at)
{
    static const JSMallocFunctions mf = {
        alloc_failure_calloc,
        alloc_failure_malloc,
        alloc_failure_free,
        alloc_failure_realloc,
        alloc_failure_usable_size,
    };
    JSRuntime *rt = JS_NewRuntime2(&mf, NULL);
    JSContext *ctx = JS_NewContext(rt);
    JSValue ret;
    long requests;

    alloc_failure.requests = 0;
    alloc_failure.fail_at = fail_at;
    alloc_failure.armed = true;
    ret = JS_Eval(ctx, src, strlen(src), "<alloc>",
                  eval_type | JS_EVAL_FLAG_COMPILE_ONLY);
    alloc_failure.armed = false;
    requests = alloc_failure.requests;
    if (!fail_at)
        assert(!JS_IsException(ret));
    if (!JS_IsException(ret) && eval_type == JS_EVAL_TYPE_GLOBAL)
        ret = JS_EvalFunction(ctx, ret);
    if (JS_IsException(ret))
        JS_FreeValue(ctx, JS_GetException(ctx));
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return requests;
}

static void slice_atom_releases_parent(void)
{
    static const char code[] =
        "for (var i = 0; i < 100; i++) {\n"
        "    var p = String(i).padStart(7, '0') + 'x'.repeat(1000000);\n"
        "    var s = p.substring(0, 500000);\n"
        "    var o = {};\n"
        "    o[s] = 1;\n"
        "    Symbol(p.substring(1, 500000));\n"
        "}\n";
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue ret;

    JS_SetMemoryLimit(rt, 16 << 20);
    ret = eval(ctx, code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void array_length_oom(void)
{
    static const char setup_code[] =
        "globalThis.kept = [];\n"
        "globalThis.f = () => {\n"
        "    for (let i = 0; i < 4; i++) {\n"
        "        const C = class extends Array {};\n"
        "        kept.push(Object.create(C.prototype), new C());\n"
        "    }\n"
        "};\n"
        "f();\n";
    JSValue global_object, func, ret;
    JSMemoryUsage stats;
    uint32_t headroom;
    JSRuntime *rt;
    JSContext *ctx;

    rt = new_runtime();
    ctx = JS_NewContext(rt);
    global_object = JS_GetGlobalObject(ctx);

    ret = eval(ctx, setup_code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);

    func = JS_GetPropertyStr(ctx, global_object, "f");
    assert(JS_IsFunction(ctx, func));

    for (headroom = 0; headroom < 16384; headroom += 8) {
        JS_RunGC(rt);
        JS_ComputeMemoryUsage(rt, &stats);
        JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + headroom);
        ret = JS_Call(ctx, func, JS_UNDEFINED, 0, NULL);
        JS_SetMemoryLimit(rt, 0);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, JS_GetException(ctx));
    }

    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, global_object);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static long eval_with_alloc_failure(const char *src, long fail_at)
{
    static const JSMallocFunctions mf = {
        alloc_failure_calloc,
        alloc_failure_malloc,
        alloc_failure_free,
        alloc_failure_realloc,
        alloc_failure_usable_size,
    };
    JSRuntime *rt = JS_NewRuntime2(&mf, NULL);
    JSContext *ctx = JS_NewContext(rt);
    JSValue ret, e;
    const char *str;
    long requests;

    alloc_failure.requests = 0;
    alloc_failure.fail_at = fail_at;
    alloc_failure.armed = true;
    ret = JS_Eval(ctx, src, strlen(src), "<alloc>", JS_EVAL_TYPE_GLOBAL);
    alloc_failure.armed = false;
    requests = alloc_failure.requests;
    if (JS_IsException(ret)) {
        assert(fail_at);
        e = JS_GetException(ctx);
        assert(JS_IsError(e));
        assert(JS_IsUncatchableError(e));
        JS_FreeValue(ctx, e);
    } else {
        str = JS_ToCString(ctx, ret);
        assert(str);
        assert(!strcmp(str, "none"));
        JS_FreeCString(ctx, str);
    }
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return requests;
}

static void atom_allocation_failure(void)
{
    static const char script[] =
        "var caught = 'none';\n"
        "try {\n"
        "    var keep = [];\n"
        "    for (var i = 0; i < 4000; i++) {\n"
        "        var o = {};\n"
        "        o['k' + i] = i;\n"
        "        keep.push(o);\n"
        "    }\n"
        "} catch (e) {\n"
        "    caught = typeof e;\n"
        "}\n"
        "caught;\n";
    enum { count = 3000 };
    char *src, *p;
    long n, requests;
    int i;

    requests = eval_with_alloc_failure(script, 0);
    for (n = 1; n <= requests; n++)
        eval_with_alloc_failure(script, n);

    src = malloc(count * 24 + 64);
    assert(src);
    p = src;
    p += sprintf(p, "var ");
    for (i = 0; i < count; i++)
        p += sprintf(p, "v%d,", i);
    p += sprintf(p, "v;\nvar a = [");
    for (i = 0; i < count; i++)
        p += sprintf(p, "'s%d',", i);
    sprintf(p, "];\n'none';\n");
    requests = eval_with_alloc_failure(src, 0);
    for (n = 1; n <= requests; n++)
        eval_with_alloc_failure(src, n);
    free(src);
}

static void parser_allocation_failure(void)
{
    static const char script[] =
        "var out = [];\n"
        "function unit(a) {\n"
        "  let b = a + 1, j = 0;\n"
        "  { let c = a * 2; { const d = c + b; out.push(d); } }\n"
        "  for (let i = 0; i < 2; i++) { let g = i; out.push(() => g + b); }\n"
        "  for (const k in {x: 1}) { out.push(k); }\n"
        "  for (const [p, q = 3] of [[1]]) { out.push(p + q); }\n"
        "  outer: for (let x = 0; x < 2; x++) {\n"
        "    for (let y = 0; y < 2; y++) { if (y) continue outer; out.push(x); }\n"
        "  }\n"
        "  switch (a) { default: out.push(9); case 0: { let s = 0; out.push(s); }\n"
        "    break; case 1: out.push(1); }\n"
        "  try { throw new Error('e'); } catch ({message}) { out.push(message); }\n"
        "  finally { out.push(3); }\n"
        "  with ({w: 7}) { out.push(w); }\n"
        "  class Base { #p = 1; static s = 2; f = 3; get p() { return this.#p; }\n"
        "    static { Base.t = 5; } m() { return super.toString; } }\n"
        "  class Derived extends Base { m(...r) { return r.length + this.p; } }\n"
        "  out.push(new Derived().m(1, 2), Base.t);\n"
        "  const o = { a, ['k' + a]: 1, get g() { return 1; }, ...{sp: 1} };\n"
        "  let { a: d0, ...rest } = o; [d0, j] = [j, d0];\n"
        "  out.push(`t${a}u${b}`, /(?<y>\\d+)-(?<m>\\d+)/.exec('1-2').groups.m,\n"
        "           o?.g?.x, a ?? b, 2 ** 3, rest.sp);\n"
        "  { using r = { [Symbol.dispose]() { out.push('d'); } }; }\n"
        "  out.push(eval('let ev = a + 1; ev'), 1.5, 2.5, 'str',\n"
        "           function () { return a; }());\n"
        "  function* gen() { yield* [1, 2]; }\n"
        "  async function af() { await 0; for await (const v of []) {} }\n"
        "  out.push([...gen()].length, typeof af);\n"
        "  return out.length;\n"
        "}\n"
        "unit(0) + unit(1);\n";
    static const char module[] =
        "export const c = 1;\n"
        "export function f(x = 1, ...r) { return x + r.length; }\n"
        "export default class { static #p = 1; static p() { return this.#p; } }\n"
        "const t = await 0;\n"
        "for (const z of [t]) { let q = z; }\n"
        "export { t as u };\n";
    long n, requests;

    requests = compile_with_alloc_failure(script, JS_EVAL_TYPE_GLOBAL, 0);
    assert(requests > 0);
    for (n = 1; n <= requests; n++)
        compile_with_alloc_failure(script, JS_EVAL_TYPE_GLOBAL, n);
    requests = compile_with_alloc_failure(module, JS_EVAL_TYPE_MODULE, 0);
    assert(requests > 0);
    for (n = 1; n <= requests; n++)
        compile_with_alloc_failure(module, JS_EVAL_TYPE_MODULE, n);
}

static JSRuntime *alloc_failure_gc_rt;

static void *alloc_failure_gc(void *ptr)
{
    if (!ptr && alloc_failure.armed && alloc_failure_gc_rt)
        JS_SetGCThreshold(alloc_failure_gc_rt, 0);
    return ptr;
}

static void *alloc_failure_gc_calloc(void *opaque, size_t count, size_t size)
{
    return alloc_failure_gc(alloc_failure_calloc(opaque, count, size));
}

static void *alloc_failure_gc_malloc(void *opaque, size_t size)
{
    return alloc_failure_gc(alloc_failure_malloc(opaque, size));
}

static void *alloc_failure_gc_realloc(void *opaque, void *ptr, size_t size)
{
    void *ret = alloc_failure_realloc(opaque, ptr, size);

    return size ? alloc_failure_gc(ret) : ret;
}

static long run_with_alloc_failure(const char *src, long fail_at, bool collect)
{
    static const JSMallocFunctions mf = {
        alloc_failure_gc_calloc,
        alloc_failure_gc_malloc,
        alloc_failure_free,
        alloc_failure_gc_realloc,
        alloc_failure_usable_size,
    };
    JSRuntime *rt = JS_NewRuntime2(&mf, NULL);
    JSContext *ctx = JS_NewContext(rt);
    JSContext *job_ctx;
    JSValue ret;
    long requests;
    int r;

    alloc_failure.requests = 0;
    alloc_failure.fail_at = fail_at;
    alloc_failure_gc_rt = collect ? rt : NULL;
    alloc_failure.armed = true;
    ret = JS_Eval(ctx, src, strlen(src), "<alloc>", JS_EVAL_TYPE_GLOBAL);
    if (!fail_at)
        assert(!JS_IsException(ret));
    if (JS_IsException(ret))
        JS_FreeValue(ctx, JS_GetException(ctx));
    JS_FreeValue(ctx, ret);
    while ((r = JS_ExecutePendingJob(rt, &job_ctx)) != 0) {
        if (r < 0)
            JS_FreeValue(job_ctx, JS_GetException(job_ctx));
    }
    alloc_failure.armed = false;
    alloc_failure_gc_rt = NULL;
    requests = alloc_failure.requests;
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return requests;
}

static void run_alloc_failure_sweep(const char *src, bool collect)
{
    long n, requests;

    requests = run_with_alloc_failure(src, 0, collect);
    assert(requests > 0);
    for (n = 1; n <= requests; n++)
        run_with_alloc_failure(src, n, collect);
}

static void shape_resize_allocation_failure(void)
{
    run_alloc_failure_sweep("var o = {}; for (var i = 0; i < 100; i++) o[i] = i;",
                            true);
}

static void out_of_memory_exception_runs_no_gc(void)
{
    static const char setup_code[] =
        "var cycle = {};\n"
        "cycle.self = cycle;\n"
        "globalThis.probe = new WeakRef(cycle);\n"
        "cycle = null;\n"
        "globalThis.alive = () => probe.deref() !== undefined;\n"
        "globalThis.registry = new FinalizationRegistry(() => {});\n"
        "globalThis.target = {};\n"
        "registry.register(target, 1);\n"
        "globalThis.grow = (n) => { 'x'.repeat(n); };\n"
        "globalThis.drop = () => { target = null; };\n"
        "grow(1);\n";
    static const char *const names[] = { "grow", "drop" };
    JSValue global_object, alive, func, size, ret;
    JSRuntime *rt;
    JSContext *ctx;
    int i;

    rt = new_runtime();
    JS_SetGCThreshold(rt, (size_t)-1);
    ctx = JS_NewContext(rt);
    ret = eval(ctx, setup_code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    global_object = JS_GetGlobalObject(ctx);
    alive = JS_GetPropertyStr(ctx, global_object, "alive");
    size = JS_NewInt32(ctx, 100000);
    /* an unreachable cycle is still there after an allocation failed at the GC threshold */
    for (i = 0; i < (int)countof(names); i++) {
        func = JS_GetPropertyStr(ctx, global_object, names[i]);
        assert(JS_IsFunction(ctx, func));
        JS_SetGCThreshold(rt, 0);
        JS_SetMemoryLimit(rt, 1);
        ret = JS_Call(ctx, func, JS_UNDEFINED, 1, (JSValueConst *)&size);
        JS_SetMemoryLimit(rt, 0);
        JS_SetGCThreshold(rt, (size_t)-1);
        if (i == 0)
            assert(JS_IsException(ret));
        if (JS_HasException(ctx))
            JS_FreeValue(ctx, JS_GetException(ctx));
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, func);
        ret = JS_Call(ctx, alive, JS_UNDEFINED, 0, NULL);
        assert(JS_ToBool(ctx, ret) == 1);
    }
    JS_FreeValue(ctx, alive);
    JS_FreeValue(ctx, global_object);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static struct {
    JSContext *ctx;
    int depth;
    int calls;
    bool nested;
} finalizer_alloc;

static void finalizer_alloc_finalizer(JSRuntime *rt, JSValueConst val)
{
    finalizer_alloc.calls++;
    if (finalizer_alloc.depth++)
        finalizer_alloc.nested = true;
    else if (finalizer_alloc.ctx)
        JS_FreeValue(finalizer_alloc.ctx, JS_NewObject(finalizer_alloc.ctx));
    finalizer_alloc.depth--;
}

static void finalizer_allocation_runs_no_gc(void)
{
    JSClassDef def = (JSClassDef){
        .class_name = "FinalizerAlloc",
        .finalizer = finalizer_alloc_finalizer,
    };
    JSClassID class_id = 0;
    JSValue obj[3];
    JSRuntime *rt;
    JSContext *ctx;
    int i;

    rt = new_runtime();
    ctx = JS_NewContext(rt);
    JS_NewClassID(rt, &class_id);
    assert(0 == JS_NewClass(rt, class_id, &def));
    JS_SetGCThreshold(rt, (size_t)-1);
    for (i = 0; i < 3; i++) {
        obj[i] = JS_NewObjectClass(ctx, class_id);
        assert(JS_IsObject(obj[i]));
    }
    for (i = 1; i < 3; i++) {
        JS_SetPropertyStr(ctx, obj[i], "self", JS_DupValue(ctx, obj[i]));
        JS_FreeValue(ctx, obj[i]);
    }
    memset(&finalizer_alloc, 0, sizeof(finalizer_alloc));
    finalizer_alloc.ctx = ctx;
    JS_SetGCThreshold(rt, 0);
    /* the object its finalizer allocates must not collect the two cycles from inside the free */
    JS_FreeValue(ctx, obj[0]);
    assert(finalizer_alloc.calls == 1 && !finalizer_alloc.nested);
    JS_RunGC(rt);
    assert(finalizer_alloc.calls == 3 && !finalizer_alloc.nested);
    finalizer_alloc.ctx = NULL;
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void finalization_registry_job_oom(void)
{
    static const char setup_code[] =
        "globalThis.calls = 0;\n"
        "globalThis.registry = new FinalizationRegistry(() => { calls++; });\n"
        "globalThis.targets = [{}, {}];\n"
        "registry.register(targets[0], 0);\n"
        "registry.register(targets[1], 1);\n"
        "globalThis.drop = (i) => { targets[i] = null; };\n";
    JSValue global_object, func, arg, ret;
    JSContext *ctx, *job_ctx;
    JSRuntime *rt;

    rt = new_runtime();
    ctx = JS_NewContext(rt);
    ret = eval(ctx, setup_code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    global_object = JS_GetGlobalObject(ctx);
    func = JS_GetPropertyStr(ctx, global_object, "drop");
    assert(JS_IsFunction(ctx, func));

    /* no memory for the cleanup job of the first target: nothing is thrown or queued */
    arg = JS_NewInt32(ctx, 0);
    JS_SetMemoryLimit(rt, 1);
    ret = JS_Call(ctx, func, JS_UNDEFINED, 1, (JSValueConst *)&arg);
    JS_SetMemoryLimit(rt, 0);
    assert(JS_IsUndefined(ret));
    assert(!JS_HasException(ctx));
    assert(!JS_IsJobPending(rt));

    arg = JS_NewInt32(ctx, 1);
    ret = JS_Call(ctx, func, JS_UNDEFINED, 1, (JSValueConst *)&arg);
    assert(JS_IsUndefined(ret));
    assert(JS_ExecutePendingJob(rt, &job_ctx) == 1);
    assert(!JS_IsJobPending(rt));
    ret = JS_GetPropertyStr(ctx, global_object, "calls");
    assert(JS_VALUE_GET_TAG(ret) == JS_TAG_INT && JS_VALUE_GET_INT(ret) == 1);

    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, global_object);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void module_value_outlives_context(void)
{
    static const char code[] = "export const x = 1;\n";
    JSMemoryUsage before, after;
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue mod, ret;

    mod = JS_Eval(ctx, code, strlen(code), "m",
                  JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(JS_IsModule(mod));
    ret = JS_EvalFunction(ctx, JS_DupValue(ctx, mod));
    assert(JS_IsPromise(ret));
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_RunGC(rt);
    /* the context is gone: the last reference releases the module definition */
    JS_ComputeMemoryUsage(rt, &before);
    JS_FreeValueRT(rt, mod);
    JS_ComputeMemoryUsage(rt, &after);
    assert(after.malloc_count == before.malloc_count - 1);
    JS_FreeRuntime(rt);
}

static void module_pending_await(void)
{
    static const char code[] =
        "globalThis.self = import('pending');\n"
        "globalThis.never = new Promise(() => {});\n"
        "await globalThis.never;\n";
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSContext *job_ctx;
    JSValue ret;

    ret = JS_Eval(ctx, code, strlen(code), "pending", JS_EVAL_TYPE_MODULE);
    assert(JS_IsPromise(ret));
    assert(JS_PROMISE_PENDING == JS_PromiseState(ctx, ret));
    JS_FreeValue(ctx, ret);
    while (JS_ExecutePendingJob(rt, &job_ctx) > 0)
        ;
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static long live_blocks;

static void *live_blocks_calloc(void *opaque, size_t count, size_t size)
{
    void *ptr = alloc_failure_calloc(opaque, count, size);

    if (ptr)
        live_blocks++;
    return ptr;
}

static void *live_blocks_malloc(void *opaque, size_t size)
{
    void *ptr = alloc_failure_malloc(opaque, size);

    if (ptr)
        live_blocks++;
    return ptr;
}

static void live_blocks_free(void *opaque, void *ptr)
{
    if (ptr)
        live_blocks--;
    alloc_failure_free(opaque, ptr);
}

static void *live_blocks_realloc(void *opaque, void *ptr, size_t size)
{
    void *ret = alloc_failure_realloc(opaque, ptr, size);

    if (!ptr && ret)
        live_blocks++;
    return ret;
}

static const JSMallocFunctions live_blocks_mf = {
    live_blocks_calloc,
    live_blocks_malloc,
    live_blocks_free,
    live_blocks_realloc,
    alloc_failure_usable_size,
};

static void weak_map_symbol_key_teardown(void)
{
    static const char *const codes[] = {
        "var big = new Uint8Array(1 << 16);\n"
        "var s = Symbol('key');\n"
        "var wm = new WeakMap();\n"
        "wm.set(s, {});\n",

        "var big = new Uint8Array(1 << 16);\n"
        "var wm = new WeakMap();\n"
        "wm.set(Symbol.iterator, [big]);\n",

        "var big = new Uint8Array(1 << 16);\n"
        "var wm = new WeakMap();\n"
        "(function () { var s = Symbol('own'); wm.set(s, { s, big }); })();\n",

        "var big = new Uint8Array(1 << 16);\n"
        "var s = Symbol('all');\n"
        "var wm = new WeakMap(), ws = new WeakSet(), ref = new WeakRef(s);\n"
        "var fr = new FinalizationRegistry(function () {});\n"
        "wm.set(s, { ref, fr, wm, ws }); ws.add(s);\n"
        "fr.register(s, { wm }, s); fr.register(s, ref);\n"
        "wm.set(wm, s); wm.set(Symbol('gone'), big);\n",
    };
    size_t i;

    /* the value of an entry keyed by a live symbol must not keep the heap after JS_FreeRuntime() */
    for (i = 0; i < countof(codes); i++) {
        JSRuntime *rt = JS_NewRuntime2(&live_blocks_mf, NULL);
        JSContext *ctx = JS_NewContext(rt);
        JSValue ret = eval(ctx, codes[i]);

        assert(!JS_IsException(ret));
        JS_FreeValue(ctx, ret);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(live_blocks == 0);
    }
}

static bool call_at_memory_limit(const char *setup, const char *arg,
                                 size_t headroom)
{
    JSRuntime *rt = JS_NewRuntime2(&live_blocks_mf, NULL);
    JSContext *ctx = JS_NewContext(rt);
    JSValue global_object, func, argv, ret;
    JSMemoryUsage stats;
    bool failed;

    ret = eval(ctx, setup);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    global_object = JS_GetGlobalObject(ctx);
    func = JS_GetPropertyStr(ctx, global_object, "f");
    assert(JS_IsFunction(ctx, func));
    argv = JS_NewString(ctx, arg);
    JS_RunGC(rt);
    JS_ComputeMemoryUsage(rt, &stats);
    JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + headroom);
    ret = JS_Call(ctx, func, JS_UNDEFINED, 1, (JSValueConst *)&argv);
    JS_SetMemoryLimit(rt, 0);
    failed = JS_IsException(ret) || JS_HasException(ctx);
    if (failed)
        JS_FreeValue(ctx, JS_GetException(ctx));
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, argv);
    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, global_object);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    /* a reference leaked by the failed call keeps the realm, and with it every large block */
    assert(live_blocks == 0);
    return failed;
}

static void lazy_builtin_out_of_memory(void)
{
    static const char setup[] =
        "function f() { Array.fromAsync; Iterator.zip; Iterator.zipKeyed; }\n";
    size_t headroom;
    bool loaded = false, failed = false;

    for (headroom = 0; headroom < 48 * 1024; headroom += 128) {
        if (call_at_memory_limit(setup, "", headroom))
            failed = true;
        else
            loaded = true;
    }
    assert(failed && loaded);
}

static void resolver_allocation_failure(void)
{
    static const char *const uses[] = {
        "arguments.length", "typeof f", "this",
        "(() => arguments.length)()", "(() => typeof f)()", "(() => this)()",
    };
    char src[2048];
    size_t len;
    long n, requests;
    int i, j;

    /* the binding the resolver adds is the 107th variable: the hash table of the variables doubles */
    for (j = 0; j < (int)countof(uses); j++) {
        len = snprintf(src, sizeof(src), "(function f() { var v0");
        for (i = 1; i < 106; i++)
            len += snprintf(src + len, sizeof(src) - len, ", v%d", i);
        len += snprintf(src + len, sizeof(src) - len, "; return [%s", uses[j]);
        for (i = 0; i < 106; i++)
            len += snprintf(src + len, sizeof(src) - len, ", v%d", i);
        snprintf(src + len, sizeof(src) - len, "]; })");
        requests = compile_with_alloc_failure(src, JS_EVAL_TYPE_GLOBAL, 0);
        assert(requests > 0);
        for (n = 1; n <= requests; n++)
            compile_with_alloc_failure(src, JS_EVAL_TYPE_GLOBAL, n);
    }
}

static void regexp_allocation_failure(void)
{
    char src[2048], filler[400];
    long n, requests;

    /* in the first four patterns \1 or the class is the last byte its byte code buffer has room for */
    memset(filler, 'a', sizeof(filler));
    snprintf(src, sizeof(src),
             "[/(.)%.218s\\1*/, /(.)%.218s[a-c]+/, /(.)%.331s.\\1{2,3}/,"
             " /(.)%.331s.[^a-c]?/i, /(.)\\1*/, /(?<n>.)\\k<n>+/, /(.)[a-c]*/,"
             " /.[a-c]*/y, /(?<=(.)\\1*)/, /(.)[\\q{ab|c}]*/v].length;",
             filler, filler, filler, filler);
    requests = compile_with_alloc_failure(src, JS_EVAL_TYPE_GLOBAL, 0);
    assert(requests > 0);
    for (n = 1; n <= requests; n++)
        compile_with_alloc_failure(src, JS_EVAL_TYPE_GLOBAL, n);
}

static int count_interrupt_handler(JSContext *ctx, void *opaque)
{
    int *polls = (int *)opaque;
    ++*polls;
    return 0;
}

static void with_branch_polls_interrupts(void)
{
    /* the OP_with_put_var of `var x` is the only backward branch of this loop, which ends when a[j] runs out */
    static const char setup[] =
        "var a = [];"
        "for (var i = 0; i < 100000; i++) a[i] = { v: i };"
        "function f() {"
        "    return eval('var j = 0;"
        " try { for (;;) var x = a[j++].v; } catch (e) {} j');"
        "}";
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue ret;
    int polls = 0;

    ret = eval(ctx, setup);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    JS_SetInterruptHandler(rt, count_interrupt_handler, &polls);
    ret = eval(ctx, "f()");
    JS_SetInterruptHandler(rt, NULL, NULL);
    assert(JS_VALUE_GET_TAG(ret) == JS_TAG_INT);
    assert(JS_VALUE_GET_INT(ret) == 100001);
    /* 100000 iterations, the handler is called every 10000 polls */
    assert(polls >= 5);
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void normalize_out_of_memory(void)
{
    static const char code[] =
        "var s = 'e'.repeat(200000), caught = 'none';"
        " try { s.normalize('NFD'); } catch (e) { caught = typeof e; }"
        " caught";
    bool oom_seen = false, done_seen = false;
    size_t limit;

    /* somewhere in this range the UTF-32 copy of the string fits and the work buffer does not */
    for (limit = 1200000; limit < 4800000; limit += 40000) {
        JSRuntime *rt = new_runtime();
        JSContext *ctx;
        JSValue ret;

        JS_SetMemoryLimit(rt, limit);
        ctx = JS_NewContext(rt);
        ret = eval(ctx, code);
        JS_SetMemoryLimit(rt, 0);
        if (JS_IsException(ret)) {
            JSValue e = JS_GetException(ctx);

            assert(JS_IsError(e));
            assert(JS_IsUncatchableError(e));
            JS_FreeValue(ctx, e);
            oom_seen = true;
        } else {
            const char *str = JS_ToCString(ctx, ret);

            assert(str != NULL);
            assert(!strcmp(str, "none"));
            JS_FreeCString(ctx, str);
            done_seen = true;
        }
        JS_FreeValue(ctx, ret);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
    assert(oom_seen && done_seen);
}

static JSValue return_exception(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    return JS_EXCEPTION;
}

static void exception_without_value(void)
{
    static const char *const code[] = {
        "try { fail(); } catch (e) { caught = typeof e; }",
        "try { Reflect.apply(fail, null, []); } catch (e) { caught = typeof e; }",
        "Promise.resolve().then(fail).catch(e => { caught = typeof e; });",
        "(async () => { fail(); })().catch(e => { caught = typeof e; });",
    };
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt), *job_ctx;
    JSValue global, ret, e;
    const char *str;
    int i, r;

    global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "fail",
                      JS_NewCFunction(ctx, return_exception, "fail", 0));
    ret = eval(ctx, "var caught = 'none';");
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    for (i = 0; i < (int)countof(code); i++) {
        ret = eval(ctx, code[i]);
        r = JS_IsException(ret) ? -1 : 0;
        JS_FreeValue(ctx, ret);
        while (r == 0 && JS_IsJobPending(rt))
            r = JS_ExecutePendingJob(rt, &job_ctx) < 0 ? -1 : 0;
        /* the function threw nothing: the script is stopped instead of catching the "no exception" marker */
        assert(r < 0);
        e = JS_GetException(ctx);
        assert(JS_IsError(e));
        assert(JS_IsUncatchableError(e));
        JS_FreeValue(ctx, e);
        while (JS_IsJobPending(rt)) {
            if (JS_ExecutePendingJob(rt, &job_ctx) < 0)
                JS_FreeValue(job_ctx, JS_GetException(job_ctx));
        }
    }
    ret = JS_GetPropertyStr(ctx, global, "caught");
    str = JS_ToCString(ctx, ret);
    assert(str != NULL);
    assert(!strcmp(str, "none"));
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

/* calls go() with each growing allocation failing in turn, then asks check() whether script sees the exception marker as a value */
static void stored_exception_value(const char *setup)
{
    static const JSMallocFunctions mf = {
        alloc_failure_calloc,
        alloc_failure_malloc,
        alloc_failure_free,
        alloc_failure_realloc,
        alloc_failure_usable_size,
    };
    JSRuntime *rt = JS_NewRuntime2(&mf, NULL);
    JSContext *ctx = JS_NewContext(rt);
    JSValue global, go, check, ret;
    long n, requests;

    alloc_failure.armed = false;
    ret = JS_Eval(ctx, setup, strlen(setup), "<setup>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    global = JS_GetGlobalObject(ctx);
    go = JS_GetPropertyStr(ctx, global, "go");
    check = JS_GetPropertyStr(ctx, global, "check");
    assert(JS_IsFunction(ctx, go) && JS_IsFunction(ctx, check));
    requests = 1;
    for (n = 0; n <= requests; n++) {
        alloc_failure.requests = 0;
        alloc_failure.fail_at = n;
        alloc_failure.armed = true;
        ret = JS_Call(ctx, go, JS_UNDEFINED, 0, NULL);
        alloc_failure.armed = false;
        if (n == 0) {
            assert(!JS_IsException(ret));
            requests = alloc_failure.requests;
        }
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, JS_GetException(ctx));
        ret = JS_Call(ctx, check, JS_UNDEFINED, 0, NULL);
        assert(JS_IsBool(ret) && !JS_VALUE_GET_BOOL(ret));
    }
    JS_FreeValue(ctx, check);
    JS_FreeValue(ctx, go);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void own_property_value_allocation_failure(void)
{
    stored_exception_value(
        "var S = new String('abc'), D = null;\n"
        "function go() { D = null; D = Object.getOwnPropertyDescriptor(S, 1); }\n"
        "function check() { return D !== null && typeof D.value == 'unknown'; }\n");
    stored_exception_value(
        "var B = new BigUint64Array([2n ** 63n]), D = null;\n"
        "function go() { D = null; D = Reflect.getOwnPropertyDescriptor(B, 0); }\n"
        "function check() { return D !== null && typeof D.value == 'unknown'; }\n");
}

static void function_name_allocation_failure(void)
{
    stored_exception_value(
        "var F = null;\n"
        "function go() { F = null; F = { 12: function () {} }[12]; }\n"
        "function check() { return F !== null && typeof F.name == 'unknown'; }\n");
    stored_exception_value(
        "var F = null;\n"
        "function go() { F = null; F = { 14: class {} }[14]; }\n"
        "function check() { return F !== null && typeof F.name == 'unknown'; }\n");
    stored_exception_value(
        "var F = null;\n"
        "function go() { F = null; F = { 16() {} }[16]; }\n"
        "function check() { return F !== null && typeof F.name == 'unknown'; }\n");
}

static void exception_value_not_stored(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue obj = JS_NewObject(ctx), arr = JS_NewArray(ctx), ret;
    JSAtom atom = JS_NewAtom(ctx, "c");
    JSPropertyEnum *tab;
    uint32_t len;

    /* what a caller does when it passes on a failed JS_New*() call without looking at it */
    assert(JS_SetPropertyStr(ctx, obj, "a", JS_EXCEPTION) < 0);
    assert(JS_DefinePropertyValueStr(ctx, obj, "b", JS_EXCEPTION,
                                     JS_PROP_C_W_E) < 0);
    assert(JS_SetProperty(ctx, obj, atom, JS_EXCEPTION) < 0);
    assert(JS_DefinePropertyValue(ctx, obj, atom, JS_EXCEPTION,
                                  JS_PROP_C_W_E) < 0);
    assert(JS_SetPropertyUint32(ctx, arr, 0, JS_EXCEPTION) < 0);
    assert(JS_SetPropertyInt64(ctx, arr, 0, JS_EXCEPTION) < 0);
    assert(JS_DefinePropertyValueUint32(ctx, arr, 0, JS_EXCEPTION,
                                        JS_PROP_C_W_E) < 0);
    assert(JS_SetPropertyUint32(ctx, obj, 7, JS_EXCEPTION) < 0);
    JS_FreeAtom(ctx, atom);
    ret = JS_GetPropertyStr(ctx, arr, "length");
    assert(JS_VALUE_GET_TAG(ret) == JS_TAG_INT && JS_VALUE_GET_INT(ret) == 0);
    assert(JS_GetOwnPropertyNames(ctx, &tab, &len, obj, JS_GPN_STRING_MASK) == 0);
    assert(len == 0);
    JS_FreePropertyEnum(ctx, tab, len);
    JS_FreeValue(ctx, arr);
    JS_FreeValue(ctx, obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(void)
{
    cfunctions();
    sync_call();
    async_call();
    async_call_stack_overflow();
    raw_context_global_var();
    is_array();
    module_serde();
    module_unhandled_rejection();
    promise_mark_as_handled();
    promise_then();
    runtime_cstring_free();
    utf16_string();
    weak_map_gc_check();
    promise_hook();
    dump_memory_usage();
    new_errors();
    dom_exception_added_twice();
    large_allocation_accounting();
    backtrace_oom_current_exception();
    backtrace_oom_callsite_array();
    proxy_own_keys_huge_length();
    global_object_prototype();
    slice_string_tocstring();
    immutable_array_buffer();
    shared_array_buffer_growth();
    get_uint8array();
    new_symbol();
    bulk_free_macros();
    detach_array_buffer_free_once();
    typed_array_sort_index_overflow();
    transfer_external_array_buffer();
    resize_external_array_buffer();
    transfer_default_managed_array_buffer();
    get_class_name();
    object_from();
    add_intrinsic_bigint();
    new_typed_array();
    std_eval_interrupt_handler();
    slice_atom_releases_parent();
    array_length_oom();
    atom_allocation_failure();
    uncatchable_interrupt_sites();
    uncatchable_interrupt_at_memory_limit();
    uncatchable_out_of_memory();
    shape_resize_allocation_failure();
    out_of_memory_exception_runs_no_gc();
    finalizer_allocation_runs_no_gc();
    finalization_registry_job_oom();
    module_value_outlives_context();
    module_pending_await();
    weak_map_symbol_key_teardown();
    lazy_builtin_out_of_memory();
    parser_scope_out_of_memory();
    parser_label_out_of_memory();
    await_using_outside_async();
    parser_allocation_failure();
    resolver_allocation_failure();
    regexp_allocation_failure();
    with_branch_polls_interrupts();
    normalize_out_of_memory();
    exception_without_value();
    own_property_value_allocation_failure();
    function_name_allocation_failure();
    exception_value_not_stored();
    private_symbols();
    new_context_low_memory();
    json_stringify_oom();
    return 0;
}
