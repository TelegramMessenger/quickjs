import { assert } from "./assert.js";

// js_map_clear() cached the next record before deleting the current one.
// Deleting a record frees its value, and when that value is a map iterator
// holding a zombie record, the iterator's finalizer freed that record -- the
// cached next -- so the clear walk then read and deleted freed memory.
// Run under ASan.

function clearFreesCachedNextRecord() {
    const m = new Map();
    m.set('a', 0);
    m.set('b', 1);
    let it = m.keys();
    it.next();
    it.next();
    m.set('a', it); // the map holds its own iterator as a value
    it = null;
    m.delete('b');  // leaves an iterator-held zombie record
    m.clear();      // used to free through the dangling cached pointer
    assert(m.size, 0);
}
clearFreesCachedNextRecord();

// Same lifetime bug through Set.clear(), which shares the record code. The
// iterator element must sit before the zombie it still holds, so that deleting
// the iterator element is what frees the clear loop's cached next record.
function setClearFreesCachedNextRecord() {
    const s = new Set();
    let it = s.values();
    s.add(it); // the set holds its own iterator as an element, before 'b'
    s.add('b');
    it.next(); // onto the iterator element
    it.next(); // onto 'b', which the iterator now keeps alive
    it = null;
    s.delete('b'); // leaves an iterator-held zombie after the iterator element
    s.clear();     // deleting the iterator element frees the cached-next zombie
    assert(s.size, 0);
}
setClearFreesCachedNextRecord();

// A longer chain of iterator-held records stored as map values.
function clearFreesChainedRecords() {
    const m = new Map();
    m.set('a', 0);
    m.set('b', 1);
    m.set('c', 2);
    let it = m.entries();
    it.next();
    it.next();
    it.next();
    m.set('a', it);
    it = null;
    m.delete('b');
    m.delete('c');
    m.clear();
    assert(m.size, 0);
}
clearFreesChainedRecords();

// An iterator left suspended over a cleared map must report completion and
// must not keep reading freed records.
function iteratorAfterClear() {
    const m = new Map([['a', 1], ['b', 2]]);
    const it = m.keys();
    assert(it.next().value, 'a');
    m.clear();
    assert(it.next().done, true);
    assert(m.size, 0);
}
iteratorAfterClear();
