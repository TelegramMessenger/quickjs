import { assert } from "./assert.js";

// Past the closure variable limit get_closure_var() fails; its -1 must fail the compile, not be emitted as index 65535.
var n = 32768;

function nested(inner_body) {
    return "(function outer(" + "a,".repeat(n - 1) + "a) { if (0) eval('');" +
        " return function mid(" + "b,".repeat(n - 1) + "b) { if (0) eval('');" +
        "  return function inner() { " + inner_body + " }; }; })";
}

function compile_and_run(src) {
    try {
        return (0, eval)(src)()()();
    } catch (e) {
        return e;
    }
}

var free_variable = compile_and_run(nested(
    "var t = typeof x; if (0) eval(''); return t;"));
assert(free_variable instanceof SyntaxError, true,
       "a lookup through the eval variable objects must be rejected, got " +
       free_variable);

var with_scope = compile_and_run(nested(
    "with ({}) { var t = typeof x; } if (0) eval(''); return t;"));
assert(with_scope instanceof SyntaxError, true,
       "a lookup through a with scope must be rejected, got " + with_scope);

var no_lookup = compile_and_run(nested("if (0) eval(''); return 1;"));
assert(no_lookup instanceof SyntaxError, true,
       "an eval environment past the limit must be rejected, got " + no_lookup);

n = 4;
assert(compile_and_run(nested(
    "var t = typeof x; if (0) eval(''); return t;")), "undefined",
    "the same shape within the limit must still compile and run");
assert(compile_and_run(nested(
    "var t = typeof a + typeof b; if (0) eval(''); return t;")),
    "undefinedundefined",
    "captured parameters within the limit must still resolve");
