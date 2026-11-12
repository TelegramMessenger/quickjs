import { assert } from "./assert.js";

function isLeaked(install) {
    let o = {};
    const ref = new WeakRef(o);
    install(o);
    o = null;
    return ref.deref() !== undefined;
}

const saved = Error.stackTraceLimit;

assert(isLeaked((o) => {
    Error.stackTraceLimit = o;
    Error.stackTraceLimit = 10;
}), false);

assert(isLeaked((o) => {
    for (let i = 0; i < 100; i++)
        Error.stackTraceLimit = o;
    Error.stackTraceLimit = 10;
}), false);

assert(isLeaked((o) => { Error.stackTraceLimit = o; }), true);
Error.stackTraceLimit = saved;

{
    let calls = 0;
    Error.stackTraceLimit = {
        valueOf() {
            calls++;
            Error.stackTraceLimit = 3;
            return 2;
        },
    };
    let e;
    try { null.x; } catch (err) { e = err; }
    assert(calls, 1);
    assert(typeof e.stack, "string");
    assert(Error.stackTraceLimit, 3);
}
Error.stackTraceLimit = saved;
