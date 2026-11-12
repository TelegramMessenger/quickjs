import { assert } from "./assert.js";

const blocks = n => "{;}".repeat(n);

// The last block that still has a 16-bit index works.
assert(Function(blocks(65000) + "{ let x = 1; { let y = 2; return (() => x + y)(); } }")(), 3);

function rejected(body) {
    try {
        Function(body);
    } catch (e) {
        return e instanceof SyntaxError;
    }
    return false;
}

assert(rejected(blocks(65536) + "{ let x = 1; return x; }"), true);
assert(rejected(blocks(65536) + "{ class C { #p = 1; static t(o) { return o.#p; } } return C.t(new C); }"), true);
assert(rejected(blocks(70000)), true);
// class expressions in a default parameter push the body scope past the limit
assert(rejected("return function (a = [" + "class {},".repeat(33000) + "]) { function f() {} return f; }"), true);
