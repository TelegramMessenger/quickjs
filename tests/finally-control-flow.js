import { assert } from "./assert.js";

var log = [];
function run(f) { log = []; var r; try { r = f(); } catch (e) { r = "threw " + e; } return r + ":" + log.join(); }
function drive(gen, how) {
    log = [];
    var g = gen(), out = [], r;
    try {
        out.push(g.next().value);
        r = how == "return" ? g.return("R") : how == "throw" ? g.throw("T") : g.next();
        out.push(r.value + (r.done ? "!" : ""));
        while (!r.done) { r = g.next(); out.push(r.value + (r.done ? "!" : "")); }
    } catch (e) { out.push("threw " + e); }
    return out.join() + ":" + log.join();
}
var cases = [
  () => run(function () { try { try { log.push(1); } finally { log.push(2); return "a"; } } finally { log.push(3); } }),
  () => run(function () { try { log.push(1); } finally { for (var i = 0; i < 3; i++) { try { log.push("t" + i); } finally { if (i == 1) break; log.push("f" + i); } } log.push("after"); } return "b"; }),
  () => run(function () { for (var i = 0; i < 3; i++) { try { if (i == 0) throw "x"; log.push(i); } finally { if (i < 2) continue; log.push("last"); } } return "c"; }),
  () => run(function () { try { throw "e"; } finally { try { return "d"; } finally { log.push("inner"); } } }),
  () => run(function () { out: try { try { log.push(1); } finally { try { break out; } finally { log.push(2); } } } finally { log.push(3); } return "e"; }),
  () => run(function () { try { return "lost"; } finally { switch (1) { case 1: log.push("s"); break; default: log.push("no"); } } }),
  () => run(function () { try { try { throw 1; } finally { log.push("f1"); } } catch (e) { try { return "g" + e; } finally { log.push("f2"); } } finally { log.push("f3"); } }),
  () => drive(function* () { try { yield 1; } finally { log.push("a"); yield 2; log.push("b"); } }, "return"),
  () => drive(function* () { try { yield 1; } finally { try { yield 2; } finally { log.push("in"); } log.push("out"); } }, "throw"),
  () => drive(function* () { for (var i = 0; i < 2; i++) { try { yield i; } finally { if (i == 0) continue; log.push("end"); } } return "h"; }, "return"),
];
var expected = [
    "a:1,2,3",
    "b:1,t0,f0,t1,after",
    "c:1,2,last",
    "d:inner",
    "e:1,2,3",
    "lost:s",
    "g1:f1,f2,f3",
    "1,2,R!:a,b",
    "1,2,threw T:in,out",
    "0,1,h!:end",
];
for (var i = 0; i < cases.length; i++)
    assert(cases[i](), expected[i], "case " + i);
