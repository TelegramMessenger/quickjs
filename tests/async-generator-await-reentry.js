import { assert } from "./assert.js";

let armed = null;
Object.defineProperty(Object.prototype, "then", {
    configurable: true,
    get() {
        const f = armed;
        armed = null;
        if (f)
            f();
        return undefined;
    },
});

async function ticks() {
    for (let i = 0; i < 10; i++)
        await null;
}

{
    const log = [];
    let release;
    const pending = new Promise((resolve) => { release = resolve; });
    async function* gen() {
        const x = yield 1;
        log.push("yield:" + x);
        const y = await pending;
        log.push("await:" + y);
    }
    const it = gen();
    armed = () => { it.next("A"); };
    it.next();
    await ticks();
    assert(log.join(), "yield:A");
    release("B");
    await ticks();
    assert(log.join(), "yield:A,await:B");
}

{
    const log = [];
    const pending = new Promise(() => {});
    let caught = "none";
    async function* gen() {
        try { yield 1; } catch (e) { log.push("catch:" + e); }
        try { await pending; log.push("resumed"); } catch (e) { caught = typeof e; }
    }
    const it = gen();
    armed = () => { it.throw("T"); };
    const first = it.next();
    await ticks();
    assert(log.join(), "catch:T");
    assert(caught, "none");
    assert((await first).value, 1);
}

{
    const log = [];
    const pending = new Promise(() => {});
    async function* gen() {
        try {
            yield 1;
        } finally {
            log.push("finally");
            await pending;
            log.push("resumed");
        }
    }
    const it = gen();
    armed = () => { it.return(7); };
    it.next();
    await ticks();
    assert(log.join(), "finally");
}

{
    const result = Promise.resolve({ value: 1, done: false });
    const inner = { [Symbol.asyncIterator]() { return this; }, next() { return result; } };
    async function* gen() {
        yield* inner;
    }
    const it = gen();
    let second;
    armed = () => { second = it.next(); };
    it.next();
    await ticks();
    assert((await second).value, 1);
}

delete Object.prototype.then;
