import { assert } from "./assert.js";

// add_closure_variables() builds the environment a direct eval sees by
// combining the calling function's arguments, locals and inherited closure
// variables into one JSClosureVar array. Each of those sets is individually
// bounded below JS_MAX_LOCAL_VARS, but their sum was stored into the 16-bit
// JSFunctionBytecode.closure_var_count field with no range check. A combined
// count above JS_MAX_LOCAL_VARS (65535) wrapped the field to a tiny value, so
// js_closure2() allocated only that many var_ref slots while the eval bytecode
// kept indexing the full environment -- OP_get_var_ref then read past the
// allocation (heap-buffer-overflow in JS_CallInternal), a deterministic crash.
// Non-strict duplicate formals let a ~200 byte source reach the boundary.
// add_closure_variables() now rejects an environment larger than the field can
// hold with a SyntaxError. Run under ASan.
//
// The reported payload: an inner function of 32768 arguments, called from an
// outer of 32768 arguments, whose direct eval combines
//   32768 (inner args) + 4 (implicit locals) + 32769 (the 32768 inherited
//   outer args plus the synthesized function binding) = 65541 entries.
// The inner() call is made at the top level so its large argument frame does
// not sit below another call frame. With the fix the oversized environment is
// rejected while the eval is compiled (one frame), so a SyntaxError is thrown.
// Without the fix the field truncates to 5 and the engine instead reads out of
// bounds while executing the eval bytecode -- a heap-buffer-overflow under
// ASan, or, when the surrounding stack is tight, the distinct RangeError of
// entering the eval frame; neither is a SyntaxError, so this test fails on the
// unpatched engine.
var n = 32768;
var inner = Function("a,".repeat(n - 1) + "a",
    "return function(" + "a,".repeat(1000) + "z," + "a,".repeat(n - 1002) + "a" +
    "){return eval('z')}")();
var threw = null;
try {
    inner();
} catch (e) {
    threw = e;
}
assert(threw instanceof SyntaxError, true,
       "the oversized direct-eval environment must be rejected with a " +
       "SyntaxError, got " + threw);

// A direct-eval environment within the limit must keep working: a single
// argument plus a direct eval that reads it back.
assert((function (z) { return eval("z"); })(42), 42,
       "direct eval within the closure limit must still resolve z");
