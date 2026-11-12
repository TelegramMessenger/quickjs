import * as std from "qjs:std";
import { assert } from "./assert.js";

const GeneratorFunction = (function* () {}).constructor;
const AsyncGeneratorFunction = (async function* () {}).constructor;

globalThis.leaked = null;
let ref;

function alive(what) {
    std.gc();
    assert(ref.deref() !== undefined, true, what + ": frame value collected");
}

(function () {
    function* gen(a = (globalThis.leaked = () => this)) { yield 1; }
    const o = { tag: "this" };
    ref = new WeakRef(o);
    o.g = gen.call(o);
})();
alive("this in a parameter initialiser");
assert(leaked().tag, "this");
assert(leaked().g.next().value, 1);

(function () {
    async function* gen(a = (globalThis.leaked = () => this)) { yield 1; }
    const o = { tag: "async this" };
    ref = new WeakRef(o);
    o.g = gen.call(o);
})();
alive("this in a parameter initialiser of an async generator");
assert(leaked().tag, "async this");
assert((await leaked().g.next()).value, 1);

(function () {
    function* gen(a = (globalThis.leaked = () => arguments)) {}
    const g = gen(undefined, 2);
    const args = leaked();
    ref = new WeakRef(args);
    args.g = g;
})();
alive("arguments in a parameter initialiser");
assert(leaked().length, 2);

(function () {
    const gen = function* self(o, a = (globalThis.leaked = () => self)) {};
    const o = { tag: "name" };
    ref = new WeakRef(o);
    o.g = gen(o);
})();
alive("the function name in a parameter initialiser");
assert(typeof leaked(), "function");

for (const ctor of [GeneratorFunction, AsyncGeneratorFunction]) {
    (function () {
        const gen = ctor("a", `
            arguments;
            globalThis.leaked = (v) => (v ? (a = v) : a);
            yield;
        `);
        const g = gen(0);
        g.next();
        const o = { g, tag: "mapped" };
        ref = new WeakRef(o);
        leaked(o);
    })();
    alive("a mapped argument shared with a closure of the body");
    assert(leaked().tag, "mapped");
}

(function () {
    const gen = GeneratorFunction("a", `
        globalThis.leaked = arguments;
        yield;
    `);
    const g = gen(0);
    g.next();
    const o = { g, tag: "arguments" };
    ref = new WeakRef(o);
    leaked[0] = o;
})();
alive("the mapped arguments object");
assert(leaked[0].tag, "arguments");
assert(leaked[0].g.next().done, true);

(function () {
    const gen = GeneratorFunction("self", `
        globalThis.leaked = arguments;
        self = yield 1;
        yield 2;
    `);
    const g = gen(0);
    ref = new WeakRef(g);
    g.next();
    g.next(g);
})();
alive("a generator held by its own frame");
assert(leaked[0].next().done, true);

(function () {
    function* gen(a = (globalThis.leaked = () => a)) { yield a; }
    const g = gen();
    globalThis.leaked = null;
    std.gc();
    assert(g.next().done, false);
})();

(function () {
    const o = { tag: "throw" };
    function* gen(a = (globalThis.leaked = () => this), b = (() => { throw o; })()) {}
    let thrown;
    try {
        gen.call(o);
    } catch (e) {
        thrown = e;
    }
    assert(thrown, o);
})();
std.gc();
assert(leaked().tag, "throw");

(function () {
    function* gen(a = (globalThis.leaked = () => this)) {}
    const o = {};
    ref = new WeakRef(o);
    o.g = gen.call(o);
})();
globalThis.leaked = null;
std.gc();
assert(ref.deref(), undefined);
