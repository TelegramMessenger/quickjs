// https://github.com/quickjs-ng/quickjs/issues/1908
import { assert } from "./assert.js";

var outer = 42, seen;

var f = ({} = eval("1")) => 1;
assert(f(), 1);
assert(f({}), 1);

class C { m([] = eval("[]")) { return 2; } }
assert(new C().m(), 2);

function g({} = eval("1")) { return 3; }
assert(g(), 3);

var h = (...[{} = eval("1")]) => 4;
assert(h(), 4);

var k = ({} = eval(...["1"])) => 5;
assert(k(), 5);

var r = ({} = eval("seen = outer + 1")) => seen;
assert(r(), 43);

function t({} = eval("seen = this.v + new.target.length")) {}
t.prototype.v = 10;
new t();
assert(seen, 10);

var n = (a, {} = eval("a")) => a;
assert(n(6), 6);

var names = [];
for (var i = 0; i < 65534; i++)
    names.push("_" + i.toString(36));

function compileEvalInLastScope(count) {
    try {
        return typeof (0, eval)("(() => { 'use strict'; var " +
                                names.slice(0, count).join(",") +
                                "; { let z = 7; return eval('z'); } })");
    } catch (e) {
        return e;
    }
}

assert(compileEvalInLastScope(65533), "function");
assert(compileEvalInLastScope(65534) instanceof SyntaxError, true,
       "an eval whose scope starts at the last local slot must be rejected");
