import { assert } from "./assert.js";

function compile_error(src) {
    try {
        new Function(src);
    } catch (e) {
        return e.name;
    }
    return "none";
}

assert(compile_error('var {"a" + x} = {};'), "SyntaxError");
assert(compile_error('var {if + x} = {};'), "SyntaxError");
assert(compile_error('var {0 ...x} = [];'), "SyntaxError");
assert(compile_error('var x; ({[0] ...x} = []);'), "SyntaxError");
assert(compile_error('for (var {0 ...x} of []) ;'), "SyntaxError");
assert(compile_error('try {} catch ({0 ...x}) {}'), "SyntaxError");
assert(compile_error('function f({if { b, ...rest}) {}'), "SyntaxError");

// The look-ahead takes this regexp for a division and misses the rest element: it has to be an error that script can catch.
var r = compile_error('function f({ a = function(){ if (x) /{/.test(y); }, ...r }) {}');
assert(r === "none" || r === "SyntaxError");

var { "a": a, 0: b, [1 + 1]: c, if: d, e, f = 6, ...rest } =
    { a: 1, 0: 2, 2: 3, if: 4, e: 5, z: 7 };
assert(a, 1);
assert(b, 2);
assert(c, 3);
assert(d, 4);
assert(e, 5);
assert(f, 6);
assert(rest.z, 7);
function g({ "k": [k0, ...ks], while: w = 9 }) { return k0 + ks.length + w; }
assert(g({ k: [1, 2, 3] }), 12);
