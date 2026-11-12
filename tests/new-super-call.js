import { assert } from "./assert.js";

function is_syntax_error(src) {
    try {
        (0, eval)(src);
    } catch (e) {
        return e instanceof SyntaxError;
    }
    return false;
}

// This shape passed the stack size check: OP_ret then used the thrown value as its return address.
assert(is_syntax_error(
    "class A extends Object { constructor() { super();" +
    " try { throw 1; } finally { switch (Object) { case new super(): } } } }"));
assert(is_syntax_error(
    "class A extends Object { constructor() { new super(); } }"));
assert(is_syntax_error(
    "class A extends Object { constructor() { super(); new super(1, 2); } }"));
assert(is_syntax_error(
    "class A extends Object { constructor() { super(); new super; } }"));
assert(is_syntax_error(
    "class A extends Object { constructor() { (() => new super())(); } }"));
assert(is_syntax_error(
    "class A extends Object { constructor(a = new super()) { } }"));

class K {
    constructor(v) { this.v = v; }
}
class B {}
B.prototype.K = K;
class C extends B {
    constructor() {
        super();
        this.a = new super.K(1);
        this.b = new super["K"](2);
        this.c = new super.K;
    }
}
var c = new C();
assert(c.a.v, 1);
assert(c.b.v, 2);
assert(c.c instanceof K);
