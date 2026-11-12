import { assert } from "./assert.js";

Error.prepareStackTrace = (e, sites) => sites;
const self = new Error().stack[0].getFunction();
Error.prepareStackTrace = undefined;

if (globalThis.reentered) {
    let o = {};
    let x = 0x1234;
    globalThis.ref = new WeakRef(o);
    globalThis.get = () => x;
    globalThis.set = (v) => { x = v; };
    throw 1;
}
globalThis.reentered = true;

function deep(n) {
    return n > 0 ? deep(n - 1) + 1 : 0;
}

if (typeof self === "function") {
    let thrown;
    try {
        self();
    } catch (e) {
        thrown = e;
    }
    assert(thrown, 1);
    assert(ref.deref(), undefined);
    deep(2);
    assert(get(), 0x1234);
    set(5);
    deep(2);
    assert(get(), 5);
}
