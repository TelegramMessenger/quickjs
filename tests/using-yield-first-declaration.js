import { assert } from "./assert.js";

var log = [];

function res(name) {
    return { [Symbol.dispose]() { log.push("dispose " + name); } };
}

function closable() {
    return {
        [Symbol.iterator]() {
            return {
                next() { return { value: 1, done: false }; },
                return() { log.push("closed"); return {}; },
            };
        },
    };
}

// Each generator is suspended in the first `using` statement of a scope, then left with return(), throw() or next().
function leave(gen, how) {
    var g = gen(), r;
    log = [];
    g.next();
    try {
        r = how == "throw" ? g.throw("thrown") : how == "return" ? g.return(7) : g.next(res("b"));
        r = r.done + " " + r.value;
    } catch (e) {
        r = "caught " + e;
    }
    return r + ": " + log.join();
}

function* block() { { using a = res("a"), b = yield 1; log.push("body"); } }
assert(leave(block, "return"), "true 7: dispose a");
assert(leave(block, "throw"), "caught thrown: dispose a");
assert(leave(block, "next"), "true undefined: body,dispose b,dispose a");

function* body() { using a = res("a"), b = yield 1; log.push("body"); }
assert(leave(body, "return"), "true 7: dispose a");
assert(leave(body, "next"), "true undefined: body,dispose b,dispose a");

function* for_head() { for (using a = res("a"), b = yield 1; ; ) { log.push("body"); break; } }
assert(leave(for_head, "return"), "true 7: dispose a");
assert(leave(for_head, "next"), "true undefined: body,dispose b,dispose a");

function* in_for_of() { for (var x of closable()) { using a = yield 1; } }
assert(leave(in_for_of, "return"), "true 7: closed");
assert(leave(in_for_of, "throw"), "caught thrown: closed");

function* in_try() { try { { using a = yield 1; } } finally { log.push("finally"); } }
assert(leave(in_try, "return"), "true 7: finally");
assert(leave(in_try, "throw"), "caught thrown: finally");

function* nested() {
    try {
        for (var x of closable()) { using a = res("a"), b = yield 1; }
    } finally {
        log.push("finally");
    }
}
assert(leave(nested, "return"), "true 7: dispose a,closed,finally");

function* later() { { using a = res("a"); using b = yield 1; log.push("body"); } }
assert(leave(later, "return"), "true 7: dispose a");
