import { assert } from "./assert.js";

// A non-strict function with a simple parameter list and a reference to
// `arguments` gets a *mapped* arguments object: resolve_labels() captures every
// parameter into the per-frame var_ref array so arguments[i] and the parameter
// stay aliased (js_build_mapped_arguments() -> get_var_ref()). Each capture
// bumps JSFunctionDef.var_ref_count, which js_create_function() stores into the
// 16-bit JSFunctionBytecode.var_ref_count field with no range check.
//
// The parameter count alone is bounded below JS_MAX_LOCAL_VARS (65535), but an
// independently captured local -- here `x`, closed over by the arrow -- adds one
// more var_ref. With the maximum 65535 parameters the total reaches 65536 and
// the field wraps to 0, so JS_CallInternal() allocates a zero-length sf->var_refs
// array while js_build_mapped_arguments() still indexes one slot per parameter.
// get_var_ref() then reads past the allocation (dynamic-stack-buffer-overflow in
// get_var_ref under ASan, a deterministic crash on a large enough JS stack). A
// ~130 KB source of non-strict duplicate formals reaches the boundary.
//
// js_create_function() now rejects a captured-variable total above the field's
// range with a SyntaxError, thrown while the function is compiled -- before any
// call frame is built, so the rejection does not need the large runtime stack
// the out-of-bounds access does.
//
// The reported payload: 65535 formals (all captured for the mapped arguments
// object) + 1 captured local = 65536 var_refs. With the fix the oversized
// function is rejected while it is compiled, so Function() throws a SyntaxError.
// Without the fix the field truncates to 0 and, when the function is later
// called, the engine reads out of bounds while building the mapped arguments
// object -- a dynamic-stack-buffer-overflow under ASan on the sandbox's 6 MiB JS
// stack, or the distinct RangeError of entering the oversized call frame under a
// smaller stack; neither is the SyntaxError the fix now raises at compile time,
// so this test fails on the unpatched engine (no SyntaxError from Function()).
var threw = null;
try {
    Function("a,".repeat(65534) + "a", // 65535 formals
             "var x;(()=>x);return arguments");
} catch (e) {
    threw = e;
}
assert(threw instanceof SyntaxError, true,
       "the oversized captured-variable set must be rejected with a " +
       "SyntaxError, got " + threw);

// A captured-variable set within the field's range must still compile. The
// maximum 65535 parameters captured for a mapped arguments object, with no extra
// captured local, is exactly at the boundary (var_ref_count 65535, the field's
// highest value) and must be accepted. Only compilation is exercised here:
// calling such a function needs a multi-megabyte JS stack, which the fix does not
// change -- the point is that the fix rejects 65536 without over-rejecting 65535.
assert(typeof Function("a,".repeat(65534) + "a", "return arguments"), "function",
       "a mapped arguments object at the captured-variable limit must compile");

// An ordinary mapped arguments object with a captured local keeps aliasing its
// parameters: writing a parameter is visible through arguments[] and vice versa.
var f = Function("a", "var x; (() => x); arguments[0] = 7; return a;");
assert(f.call(null, 1), 7, "mapped arguments must still alias the parameter");
