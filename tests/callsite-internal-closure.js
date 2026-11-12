import { assert } from "./assert.js";

function collect(seen) {
    return (_, frames) => {
        for (const f of frames) {
            if (f.isNative())
                seen.push(f.getFunction());
        }
        return "";
    };
}

function callAll(seen, arg) {
    for (const f of seen) {
        if (typeof f === "function")
            f(arg);
    }
}

{
    const seen = [];
    let steps = 0;
    async function* gen() {
        await 0;
        new Error().stack;
        callAll(seen, 1);
        steps++;
        try {
            await Promise.reject(1);
        } catch {
            new Error().stack;
            callAll(seen, 2);
        }
        steps++;
        yield 3;
    }
    Error.prepareStackTrace = collect(seen);
    const it = gen();
    const r = await it.next();
    Error.prepareStackTrace = undefined;
    assert(r.value, 3);
    assert(r.done, false);
    assert(steps, 2);
    assert(seen.length, 2);
    assert(seen.every((f) => f === undefined), true);
}

{
    const seen = [];
    let armed = false;
    async function* empty() {}
    const it = empty();
    await it.next();
    Object.defineProperty(Object.prototype, "then", {
        configurable: true,
        get() {
            if (armed) {
                armed = false;
                new Error().stack;
            }
            return undefined;
        },
    });
    Error.prepareStackTrace = collect(seen);
    const p = it.return(1);
    armed = true;
    const r = await p;
    Error.prepareStackTrace = undefined;
    delete Object.prototype.then;
    assert(r.value, 1);
    assert(r.done, true);
    assert(armed, false);
    callAll(seen, 5);
    assert(seen.length, 1);
    assert(seen[0], undefined);
}

{
    Error.prepareStackTrace = (_, frames) => frames;
    const frames = [1].map(function m() { return new Error().stack; })[0];
    Error.prepareStackTrace = undefined;
    const native = frames.find((f) => f.isNative());
    assert(native.getFunction(), Array.prototype.map);
}
