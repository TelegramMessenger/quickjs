import { assert } from "./assert.js";

// FinalizationRegistry.prototype.unregister() freed each removed entry in
// place; when its held value was the last reference to the target of a later
// entry, finalizing it freed that entry under the running walk. Run under ASan.

function heldValueIsNextTarget() {
    const registry = new FinalizationRegistry(() => {});
    const token = {};
    const first = {};
    let second = {};
    registry.register(first, second, token);
    registry.register(second, 42, token);
    second = undefined;
    assert(registry.unregister(token), true);
    assert(registry.unregister(token), false);
}
heldValueIsNextTarget();

function heldValueChain() {
    const registry = new FinalizationRegistry(() => {});
    const token = {};
    const first = {};
    let second = {}, third = {}, fourth = Symbol("fourth");
    registry.register(first, second, token);
    registry.register(second, third, token);
    registry.register(third, fourth, token);
    registry.register(fourth, 42, token);
    second = third = fourth = undefined;
    assert(registry.unregister(token), true);
    assert(registry.unregister(token), false);
}
heldValueChain();

function heldValueIsOtherTokenTarget() {
    const registry = new FinalizationRegistry(() => {});
    const token = {}, other = {};
    const first = {};
    let second = {};
    registry.register(first, second, token);
    registry.register(second, 42, other);
    second = undefined;
    assert(registry.unregister(token), true);
    // finalizing the target removed the other entry too
    assert(registry.unregister(other), false);
    const third = {};
    registry.register(third, 42, other);
    assert(registry.unregister(other), true);
}
heldValueIsOtherTokenTarget();
