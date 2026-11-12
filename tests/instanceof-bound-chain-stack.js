import { assert } from "./assert.js";

const bind = Function.prototype.bind;

function chain(n) {
    let f = function () {};
    for (let i = 0; i < n; i++) {
        f = bind.call(f);
        delete f.name; // "bound bound ..." names would grow quadratically
        Object.setPrototypeOf(f, null); // no inherited Symbol.hasInstance
    }
    return f;
}

function check(n, value) {
    const f = chain(n);
    let result, error;
    try {
        result = value instanceof f;
    } catch (e) {
        error = e;
    }
    if (error !== undefined)
        assert(error instanceof RangeError, true,
               "only a stack overflow may be thrown, got " + error);
    else
        assert(result, false);
    return error === undefined;
}

assert(check(1, {}), true);
assert(check(100, {}), true);
assert(check(100, 1), true);

function Base() {}
let bound = bind.call(Base);
Object.setPrototypeOf(bound, null);
bound = bind.call(bound);
Object.setPrototypeOf(bound, null);
assert(new Base() instanceof bound, true);
assert({} instanceof bound, false);

check(200000, {});
