import { assert } from "./assert.js";

const bind = Function.prototype.bind;

function boundChain(n, proxied) {
    let f = function () {};
    for (let i = 0; i < n; i++) {
        f = bind.call(proxied ? new Proxy(f, {}) : f);
        delete f.name; // "bound bound ..." names would grow quadratically
    }
    return f;
}

function proxyChain(n) {
    let f = function () {};
    f.prototype = null;
    for (let i = 0; i < n; i++)
        f = new Proxy(f, {});
    return f;
}

function construct(newTarget) {
    return Object.getPrototypeOf(Reflect.construct(Object, [], newTarget));
}

assert(construct(boundChain(3, false)), Object.prototype);
assert(construct(boundChain(3, true)), Object.prototype);
assert(construct(proxyChain(2)), Object.prototype);

assert(construct(boundChain(300000, false)), Object.prototype);
assert(construct(boundChain(100000, true)), Object.prototype);

const revocable = Proxy.revocable(function () {}, {});
const throughRevoked = bind.call(revocable.proxy);
revocable.revoke();
let error;
try {
    construct(throughRevoked);
} catch (e) {
    error = e;
}
assert(error instanceof TypeError, true);
