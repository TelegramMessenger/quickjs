import { assert } from "./assert.js";

var names = [];
for (var i = 0; i < 65535; i++)
    names.push("_" + i.toString(36));

function compile(count, last) {
    try {
        return (0, eval)("(function (" + names.slice(0, count).join(",") +
                         ", " + last + ") { return 1; })");
    } catch (e) {
        return e;
    }
}

for (var last of ["x", "[x]", "{x}", "{}"]) {
    var full = compile(65535, last);
    assert(full instanceof SyntaxError, true,
           "a parameter " + last + " past the limit must be rejected, got " +
           typeof full);
    assert(typeof compile(65534, last), "function",
           "a parameter " + last + " within the limit must compile");
}
assert(typeof compile(65535, "...[x]"), "function",
       "a rest parameter takes no slot");
assert((0, eval)("(function (a, [b], {c}) { return a + b + c; })")(1, [2], {c: 3}),
       6, "destructuring parameters must still work");
