import { assert } from "./assert.js";

function wideSlice(s) {
    return ("ሴ" + s).slice(1);
}

function checkSearch(a, b, c, n, wrapHay, wrapNeedle) {
    const run = a.repeat(n);
    const plainNeedle = run + b;
    const needle = wrapNeedle(plainNeedle);
    const miss = run + c;
    const at = 3 * (n + 1);
    const second = at + n + 2;
    const hay = wrapHay(miss.repeat(3) + plainNeedle + c + plainNeedle);

    assert(hay.indexOf(needle), at);
    assert(hay.indexOf(needle, at + 1), second);
    assert(hay.indexOf(needle, second + 1), -1);
    assert(hay.lastIndexOf(needle), second);
    assert(hay.lastIndexOf(needle, second - 1), at);
    assert(hay.lastIndexOf(needle, at - 1), -1);
    assert(hay.includes(needle), true);
    assert(hay.includes(needle, second + 1), false);
    assert(hay.startsWith(needle, at), true);
    assert(hay.startsWith(needle, at + 1), false);
    assert(hay.endsWith(needle, second + n + 1), true);
    assert(hay.indexOf(wrapNeedle(a + plainNeedle)), -1);
    assert(hay.lastIndexOf(wrapNeedle(plainNeedle + b)), -1);

    const parts = hay.split(needle);
    assert(parts.length, 3);
    assert(parts[0], miss.repeat(3));
    assert(parts[1], c);
    assert(hay.split(needle, 1).length, 1);
    assert(hay.replace(needle, "-").length, hay.length - n);
    assert(hay.replace(needle, "-").indexOf("-"), at);
    assert(hay.replaceAll(needle, "-").length, hay.length - 2 * n);
    assert(hay.replaceAll(needle, "").indexOf(c, at), at);

    for (const k of [0, 1, 255, 256, 1022, 1023, 1024, 1025, 2047, 2048, n - 1]) {
        if (k >= n)
            continue;
        const near = a.repeat(k) + c + a.repeat(n - k - 1) + b;
        const hay2 = wrapHay(near + plainNeedle + near);
        assert(hay2.indexOf(needle), n + 1);
        assert(hay2.lastIndexOf(needle), n + 1);
        assert(hay2.includes(wrapNeedle(near + near)), false);
    }
}

const same = (s) => s;
const widen = (s) => s + "ሴ";

for (const n of [1, 2, 255, 256, 257, 1023, 1024, 1025, 1026, 2048, 2049, 3073, 5000]) {
    checkSearch("a", "b", "c", n, same, same);
    checkSearch("а", "б", "в", n, same, same);
    checkSearch("a", "б", "c", n, same, same);
    checkSearch("a", "b", "c", n, widen, same);
    checkSearch("a", "b", "c", n, same, wideSlice);
    checkSearch("a", "b", "c", n, widen, wideSlice);
}

assert("abc".indexOf(""), 0);
assert("abc".lastIndexOf(""), 3);
assert("abc".split("").join("|"), "a|b|c");
assert("abc".replaceAll("", "-"), "-a-b-c-");
assert("".split("").length, 0);
assert("a".repeat(3000).split("a").length, 3001);
assert("a".repeat(3000).replaceAll("a", "").length, 0);
