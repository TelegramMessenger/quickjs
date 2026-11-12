import { assert } from "./assert.js";

// The lookahead sees the keyword `in` where the tokenizer reads an identifier written with an escape.

function parseError(src, how) {
    try {
        if (how === "eval")
            (0, eval)(src);
        else
            new Function(src);
    } catch (e) {
        return e;
    }
    return undefined;
}

for (const how of ["function", "eval"]) {
    for (const src of [
        "#x in\\u0061",
        "#x in\\u{61}",
        "#x in\\u",
        "class C { #x; static m(o) { return #x in\\u0062 } }",
        "class C { #x; static m(o) { return #x i\\u006e o } }",
    ]) {
        var e = parseError(src, how);
        assert(typeof e, "object", src);
        assert(e instanceof SyntaxError, true, src);
        let n = 0;
        for (const k in e)
            n++;
        assert(n, 0, src);
    }
}

// The operator itself still works.
class C {
    #x = 1;
    static has(o) { return #x in o; }
}
assert(C.has(new C()), true);
assert(C.has({}), false);
