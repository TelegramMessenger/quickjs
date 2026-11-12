import { assert } from "./assert.js";

// `with` needs sloppy code, hence the Function constructor.
var call = new Function("o", "with (o) { var r = f?.(1, 2); } return r;");
assert(call({ f: null }), undefined);
assert(call({ f: undefined }), undefined);
assert(call({ f(a, b) { return this.k + a + b; }, k: 10 }), 13);

var chain = new Function("o", "with (o) { return [f?.(), g?.(3)?.x, typeof h?.()]; }");
var r = chain({ f() { return 1; }, g(v) { return { x: v }; }, h: null });
assert(r[0], 1);
assert(r[1], 3);
assert(r[2], "undefined");

var outer = new Function("o", "var f = null; with (o) { for (var i = 0; i < 3; i++) f?.(); } return i;");
assert(outer({}), 3);

var ev = new Function("o", "with (o) { return eval?.('1 + 1'); }");
assert(ev({}), 2);
