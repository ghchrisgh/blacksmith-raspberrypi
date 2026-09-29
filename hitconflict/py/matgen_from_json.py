#!/usr/bin/env python3
"""
matgen_from_json.py - parses a dram_map.json (from the efn tool) and outputs the
Blacksmith mat-gen.py fields: num_banks, dram_fns, row_fn, col_fn.

Method (as described by the contact):
  - COLUMN bits  = vary WITHIN a row   (change the column, not the row)
  - ROW bits     = vary BETWEEN rows   (change the row)
  - BANK functions = bit (combinations) that are constant per bank and
                     distinguish banks; found via XOR brute-force.

Usage:  python3 matgen_from_json.py dram_map.json [address_width_bits]
        address_width_bits default = 30  (1 GB hugepage offset space)
"""
import json, sys
from itertools import combinations

def load_banks(path):
    d = json.load(open(path))
    big = {bk: rows for bk, rows in d.items()
           if sum(len(a) for a in rows.values()) > 500}
    addrs_per_bank = {bk: [int(a) for r in rows.values() for a in r]
                      for bk, rows in big.items()}
    return big, addrs_per_bank

def varying_bits(addrs, maxb):
    o = 0; a_ = (1 << 64) - 1
    for a in addrs:
        o |= a; a_ &= a
    return set(b for b in range(maxb) if ((o & ~a_) >> b) & 1)

def col_bits(big, maxb):
    """Bits that vary within a full row (== 64 addresses)."""
    c = set()
    for rows in big.values():
        for v in rows.values():
            if len(v) == 64:
                c |= varying_bits([int(x) for x in v], maxb)
    return c

def row_bits(big, maxb, exclude):
    """Bits that vary between row representatives (excluding col/bank).
    Takes ALL rows (including the 1-address rows from the coarse probes),
    so that the high address bits from the coarse run are captured too."""
    r = set()
    for rows in big.values():
        reps = [int(v[0]) for v in rows.values()]
        r |= varying_bits(reps, maxb)
    return set(b for b in r if b not in exclude)

def xor_val(a, bits):
    v = 0
    for b in bits:
        v ^= (a >> b) & 1
    return v

def bank_functions(big, apb, maxb, purity=0.95):
    """Finds XOR functions (size 1..3) that are (for the majority) constant
    per bank and distinguish banks. Selects a minimal, linearly
    independent set (over GF(2))."""
    cand = sorted(varying_bits([a for v in apb.values() for a in v], maxb))
    found = []
    for size in (1, 2, 3):
        for combo in combinations(cand, size):
            doms = []
            ok = True
            for bk in big:
                c0 = sum(1 for a in apb[bk] if xor_val(a, combo) == 0)
                n = len(apb[bk])
                if max(c0, n - c0) / n < purity:
                    ok = False; break
                doms.append(0 if c0 >= n - c0 else 1)
            if ok and len(set(doms)) > 1:
                # Signature = vector of dominant values across the banks
                found.append((combo, tuple(doms)))
    # minimal independent set (greedy over GF(2) on the signatures)
    basis_sigs, chosen = [], []
    def reduce_vec(v, basis):
        for b in basis:
            hb = max(i for i in range(len(b)) if b[i])
            if v[hb]:
                v = tuple(x ^ y for x, y in zip(v, b))
        return v
    # sort by function size, so that single bits are preferred
    for combo, sig in sorted(found, key=lambda x: len(x[0])):
        v = list(sig)
        red = reduce_vec(tuple(v), basis_sigs)
        if any(red):
            basis_sigs.append(red)
            chosen.append(combo)
    return chosen

def mask(bits):
    m = 0
    for b in bits:
        m |= (1 << b)
    return m

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "dram_map.json"
    width = int(sys.argv[2]) if len(sys.argv) > 2 else 30
    maxb = width

    big, apb = load_banks(path)
    num_banks = len(big)

    bank_fns = bank_functions(big, apb, maxb)
    bank_bits_used = set(b for fn in bank_fns for b in fn)

    cols = col_bits(big, maxb)
    rows_meas = row_bits(big, maxb, exclude=cols | bank_bits_used)

    # ---- Compute values ----
    hi_bank = max(bank_bits_used)
    full = (1 << width) - 1
    bank_mask    = mask(bank_bits_used)
    col_meas     = mask(cols)                    # only measured Column-Bits
    row_meas     = mask(rows_meas)               # only measured Row-Bits
    row_mask     = mask(range(hi_bank + 1, width))   # Row until address...
    col_mask     = full & ~bank_mask & ~row_mask     # Byte-Offset-Bits 0-5

    bankfn_masks = ", ".join(f"0x{mask(fn):X}" for fn in bank_fns)
    bankfn_desc  = ", ".join("^".join("b" + str(b) for b in fn) for fn in bank_fns)

    # ===================================================================
    #  BLOCK 1 : what was actually measured in the dram_map.json
    # ===================================================================
    print("=" * 60)
    print("  MEASURED  -  this is what's actually in the dram_map.json")
    print("=" * 60)
    print(f"File:       {path}")
    print(f"Banks:       {num_banks}")
    print(f"Column-Bits: {sorted(cols)}      "
          f"(vary WITHIN a row)  -> 0x{col_meas:X}")
    print(f"Bank-Bits:   {sorted(bank_bits_used)}   "
          f"({bankfn_desc}, constant per bank)  -> {bankfn_masks}")
    print(f"Row-Bits:    {sorted(rows_meas)}  "
          f"(vary BETWEEN rows)  -> 0x{row_meas:X}")
    print("  (only bits that varied at all in the measured range)")

    # ===================================================================
    #  BLOCK 2 : prepared as mat-gen.py expects it
    # ===================================================================
    print()
    print("=" * 60)
    print(f"  mat-gen.py  -  prepared for {width}-bit address space")
    print("=" * 60)
    print(f"num_banks = {num_banks}")
    print(f"dram_fns = [{bankfn_masks}]")
    print(f"row_fn   = 0x{row_mask:X}")
    print(f"col_fn   = 0x{col_mask:X}")
    print()
    print("# Preparation relative to the measurement:")
    print(f"#   col_fn : Byte-Offset-Bits 0-5 added    (0x{col_meas:X} -> 0x{col_mask:X})")
    print(f"#   row_fn : extended up to Bit {width-1}          (0x{row_meas:X} -> 0x{row_mask:X})")
    print(f"#   dram_fns: unchanged (bank bits directly)")
    cover = col_mask | bank_mask | row_mask
    print(f"#   Gap-free check: col|bank|row = 0x{cover:X} "
          f"(should be 0x{full:X}) -> {'OK' if cover == full else 'ERROR'}")

if __name__ == "__main__":
    main()
