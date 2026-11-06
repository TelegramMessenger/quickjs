import { assert } from "./assert.js";

// String.prototype.replace(re, "") takes JS_RegExpDelete's fast path. It must
// read and convert lastIndex before it takes the compiled bytecode: the
// conversion can recompile the RegExp. The fork keeps recently compiled
// bytecode in a per-context cache (16 slots), which keeps the old bytecode
// alive, so check the order by the result rather than by freed memory: the
// replace must use the pattern compiled during the conversion.

function recompileDuringLastIndex(evict) {
    const regexp = new RegExp("a", "g");
    Object.defineProperty(regexp, "constructor", {
        get() {
            regexp.lastIndex = {
                valueOf() {
                    regexp.compile("b", "g");
                    if (evict) {
                        // push the old bytecode out of the pattern cache
                        for (let i = 0; i < 64; i++)
                            new RegExp("x" + i, "g");
                    }
                    return 0;
                },
            };
            return RegExp;
        },
    });
    return regexp;
}

for (const evict of [false, true]) {
    const regexp = recompileDuringLastIndex(evict);
    assert("aaa".replace(regexp, ""), "aaa");
    assert(regexp.source, "b");
    assert(regexp.lastIndex, 0);
    assert("abab".replace(regexp, ""), "aa");
}
