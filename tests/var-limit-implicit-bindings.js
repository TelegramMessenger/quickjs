import { assert } from "./assert.js";

var names = [];
for (var i = 0; i < 65535; i++)
    names.push("_" + i.toString(36));

function compile(count, head, tail) {
    try {
        return (0, eval)(head + " var " + names.slice(0, count).join(",") +
                         "; " + tail);
    } catch (e) {
        return e;
    }
}

var cases = [
    ["(function f() {", "return arguments; })"],
    ["(function f() {", "return this; })"],
    ["(function f() {", "return new.target; })"],
    ["(function f() {", "return f; })"],
    ["(function f() {", "return () => arguments; })"],
    ["(function f() {", "return () => this; })"],
    ["(function f() {", "return () => new.target; })"],
    ["(function f() {", "return () => f; })"],
    ["(function f() {", "return () => () => arguments; })"],
    ["({ m() {", "return super.x; } }).m"],
    ["({ m() {", "return () => super.x; } }).m"],
    ["(class extends Object { constructor() {", "super(); } })"],
    ["(class extends Object { constructor() {", "(() => super())(); } })"],
];

for (var c of cases) {
    var full = compile(65535, c[0], c[1]);
    assert(full instanceof SyntaxError, true,
           c[0] + " " + c[1] + " needs a local slot and has none left: it" +
           " must be rejected, got " + typeof full);
    assert(typeof compile(65000, c[0], c[1]), "function",
           c[0] + " " + c[1] + " within the limit must compile");
}

assert(typeof compile(65535, "(function f() {", "return 1; })"), "function",
       "a function that needs no implicit binding may use every slot");
assert(typeof compile(65534, "(function f() {", "return arguments; })"),
       "function", "one free slot is enough for arguments");
assert(compile(3, "(function f() {",
               "return [arguments.length, this, typeof f, (() => this)()]; })")
       .call(7, 1, 2).join(), "2,7,function,7",
       "implicit bindings must still resolve");
