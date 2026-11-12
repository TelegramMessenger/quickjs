import { assert } from "./assert.js";

function isLeaked(use) {
    let o = {};
    const ref = new WeakRef(o);
    Error.prepareStackTrace = o;
    use();
    Error.prepareStackTrace = undefined;
    o = null;
    return ref.deref() !== undefined;
}

assert(isLeaked(() => {}), false);
assert(isLeaked(() => { try { null.x; } catch {} }), false);
assert(isLeaked(() => { new Error("x"); }), false);
assert(isLeaked(() => { Error.captureStackTrace({}); }), false);
assert(isLeaked(() => {
    const limit = Error.stackTraceLimit;
    Error.stackTraceLimit = 0;
    for (let i = 0; i < 100; i++)
        Error.captureStackTrace({});
    Error.stackTraceLimit = limit;
}), false);

{
    let calls = 0;
    let f = () => { calls++; return "trace"; };
    const ref = new WeakRef(f);
    Error.prepareStackTrace = f;
    assert(new Error("x").stack, "trace");
    Error.prepareStackTrace = undefined;
    f = null;
    assert(calls, 1);
    assert(ref.deref(), undefined);
}
