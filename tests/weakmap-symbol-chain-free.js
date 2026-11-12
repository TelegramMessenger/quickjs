import { assert } from "./assert.js";

// Each symbol is the last reference to the next one: freeing the first frees them all. Run under ASan.
const N = 100000;

function weakMapValueChain() {
    const map = new WeakMap();
    let first = Symbol("first");
    let last = first;
    let middle;
    for (let i = 0; i < N; i++) {
        let next = Symbol();
        map.set(last, next);
        if (i == N / 2)
            middle = new WeakRef(next);
        last = next;
        next = undefined;
    }
    assert(map.has(first), true);
    assert(typeof middle.deref(), "symbol");
    last = undefined;
    first = undefined;
    assert(middle.deref(), undefined, "the chain must be released");
}
weakMapValueChain();

function registryTokenChain() {
    const registry = new FinalizationRegistry(() => {});
    let first = Symbol("first");
    let last = first;
    let middle;
    for (let i = 0; i < N; i++) {
        let next = Symbol();
        registry.register(last, undefined, next);
        if (i == N / 2)
            middle = new WeakRef(next);
        last = next;
        next = undefined;
    }
    assert(typeof middle.deref(), "symbol");
    last = undefined;
    first = undefined;
    assert(middle.deref(), undefined, "the chain must be released");
}
registryTokenChain();

function mixedRecords() {
    const map = new WeakMap();
    const set = new WeakSet();
    const registry = new FinalizationRegistry(() => {});
    let key = Symbol("key");
    let value = Symbol("value");
    const ref = new WeakRef(key);
    const valueRef = new WeakRef(value);
    map.set(key, value);
    map.set(value, {});
    set.add(key);
    registry.register(key, value, value);
    value = undefined;
    assert(typeof valueRef.deref(), "symbol");
    key = undefined;
    assert(ref.deref(), undefined);
    assert(typeof valueRef.deref(), "symbol", "held for the cleanup callback");
}
mixedRecords();

function heldValueChainFreedWithTheRuntime() {
    const registry = new FinalizationRegistry(() => {});
    const first = Symbol("first");
    let last = first;
    for (let i = 0; i < N; i++) {
        let next = Symbol();
        registry.register(last, next);
        last = next;
        next = undefined;
    }
    globalThis.__heldValueChainFirst = first;
    globalThis.__heldValueChainRegistry = registry;
}
heldValueChainFreedWithTheRuntime();
