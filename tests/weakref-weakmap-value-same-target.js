import { assert } from "./assert.js";

// reset_weak_ref() cleared a WeakRef's target in its first pass but only
// detached the WeakRef object in the second, after freeing WeakMap values and
// FinalizationRegistry held values. When such a value was the last reference
// to a WeakRef for the same target, freeing it ran the WeakRef finalizer with
// an undefined target while its record was still in the list being walked:
// abort() in get_first_weak_ref(). Symbol targets are freed synchronously, so
// they reach the finalizer inside the walk; object targets defer it to the
// zero-refcount list. Run under ASan.

function symbolTargetWeakMapValue() {
    let key = Symbol("key");
    let ref = new WeakRef(key);
    const map = new WeakMap();
    map.set(key, ref);
    ref = undefined;
    key = undefined;
}
symbolTargetWeakMapValue();

function symbolTargetMapRecordFirst() {
    let key = Symbol("key");
    const map = new WeakMap();
    map.set(key, {});
    let ref = new WeakRef(key);
    map.set(key, ref);
    ref = undefined;
    key = undefined;
}
symbolTargetMapRecordFirst();

function symbolTargetHeldValue() {
    let key = Symbol("key");
    let ref = new WeakRef(key);
    const registry = new FinalizationRegistry(() => {});
    registry.register(key, ref);
    ref = undefined;
    key = undefined;
}
symbolTargetHeldValue();

function symbolTargetSurvivingWeakRef() {
    let key = Symbol("key");
    const ref = new WeakRef(key);
    const map = new WeakMap();
    map.set(key, ref);
    assert(ref.deref(), key);
    key = undefined;
    assert(ref.deref(), undefined);
}
symbolTargetSurvivingWeakRef();

function objectTargetWeakMapValue() {
    let key = {};
    let ref = new WeakRef(key);
    const map = new WeakMap();
    map.set(key, ref);
    ref = undefined;
    key = undefined;
}
objectTargetWeakMapValue();

function objectTargetSurvivingWeakRef() {
    let key = {};
    const ref = new WeakRef(key);
    const map = new WeakMap();
    map.set(key, ref);
    assert(ref.deref(), key);
    key = undefined;
    assert(ref.deref(), undefined);
}
objectTargetSurvivingWeakRef();
