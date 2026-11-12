import { assert } from "./assert.js";

function symbolDescriptionOfInternedSlice() {
    const parent = "0".repeat(1999) + "1" + "7".repeat(1000);
    const slice = parent.substring(0, 2000);
    const holder = {};
    holder[slice] = 1;
    const desc = Symbol(slice).description;
    assert(desc.length, 2000);
    assert(desc === slice);
    assert(parseFloat(desc), 1);
    assert(parseInt(desc, 10), 1);
    assert(Number(desc), 1);
    assert(BigInt(desc), 1n);
    assert(Object.keys(holder)[0] === slice);
}
symbolDescriptionOfInternedSlice();

function otherAtomKinds() {
    const parent = "0".repeat(1999) + "1" + "7".repeat(1000);
    const slice = parent.substring(0, 2000);
    const holder = {};
    holder[slice] = 1;
    const desc = Symbol.for(slice).description;
    assert(parseFloat(desc), 1);
    assert(Symbol.keyFor(Symbol.for(slice)) === slice);

    const wide = ("б".repeat(1999) + "1" + "7".repeat(1000)).substring(0, 2000);
    const holder2 = {};
    holder2[wide] = 1;
    assert(Symbol(wide).description === wide);
    assert(Object.keys(holder2)[0].length, 2000);
}
otherAtomKinds();

function sliceAsSymbolAndKey() {
    const parent = "k".repeat(1500) + "0".repeat(1100) + "5" + "9".repeat(900);
    const slice = parent.substring(1500, 2601);
    const sym = Symbol(slice);
    assert(sym.description === slice);
    assert(parseFloat(sym.description), 5);
    const o = { [slice]: 1 };
    assert(o[parent.substring(1500, 2601)], 1);
    assert(parseFloat(Object.keys(o)[0]), 5);
}
sliceAsSymbolAndKey();
