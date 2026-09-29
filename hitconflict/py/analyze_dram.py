#!/usr/bin/env python3
# analyze_dram.py: extracts the Column, Row and Bank bits from dram_map.json.
#
# Logic:
#   - Bits that vary WITHIN a row              -> Column bits
#   - Bits that vary BETWEEN rows (in 1 bank)  -> Row bits
#   - Bit masks whose XOR parity is constant
#     within each bank, but differs between
#     banks                                    -> Bank functions
#
# Usage:  python3 analyze_dram.py dram_map.json

import json
import sys
from itertools import combinations

MAX_BIT = 40   # up to which bit (physical address)


def popcount(x):
    return bin(x).count("1")


def varying_bits(addrs):
    """Bits that are not identical everywhere in the address list."""
    or_all = 0
    and_all = (1 << 64) - 1
    for a in addrs:
        or_all |= a
        and_all &= a
    var = or_all & ~and_all
    return [b for b in range(MAX_BIT) if (var >> b) & 1]


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "dram_map.json"
    with open(path) as f:
        data = json.load(f)

    # data["bank N"]["row M"] = [phys-Adressen]
    banks = {bk: {rk: [int(a) for a in addrs] for rk, addrs in rows.items()}
             for bk, rows in data.items()}

    all_addrs = [a for rows in banks.values()
                 for addrs in rows.values() for a in addrs]

    # ---- Übersicht ----
    print("=== Overview ===")
    print(f"Banks: {len(banks)}, total addresses: {len(all_addrs)}")
    for bk, rows in banks.items():
        sizes = [len(a) for a in rows.values()]
        print(f"  {bk}: {len(rows)} rows, addr/row min={min(sizes)} "
              f"max={max(sizes)} avg={sum(sizes)//len(sizes)}")
    print(f"\nVarying bits in the dataset: {varying_bits(all_addrs)}")

    # ---- Column bits: vary WITHIN a row ----
    col = set()
    for rows in banks.values():
        for addrs in rows.values():
            col.update(varying_bits(addrs))
    col = sorted(col)
    print(f"\nColumn bits (vary within a row): {col}")

    # ---- Row bits: vary BETWEEN rows (within a bank), but are not Column bits ----
    row = set()
    for rows in banks.values():
        reps = [addrs[0] for addrs in rows.values()]   # one representative per row
        for b in varying_bits(reps):
            if b not in col:
                row.add(b)
    row = sorted(row)
    print(f"Row bits (vary between rows):          {row}")

    # ---- Bank functions: XOR brute-force ----
    # Candidates: varying bits that are not pure Column bits
    cand = [b for b in varying_bits(all_addrs) if b not in col]
    print(f"\nCandidate bits for bank functions: {cand}")

    bank_fns = []
    for k in range(1, 4):                     # XOR of 1, 2 or 3 bits
        for combo in combinations(cand, k):
            mask = 0
            for b in combo:
                mask |= (1 << b)
            # Parity per bank: must be constant within each bank
            parity_per_bank = {}
            ok = True
            for bk, rows in banks.items():
                ps = {popcount(a & mask) & 1
                      for addrs in rows.values() for a in addrs}
                if len(ps) != 1:              # not constant within bank
                    ok = False
                    break
                parity_per_bank[bk] = ps.pop()
            # must also distinguish banks (not identical everywhere)
            if ok and len(set(parity_per_bank.values())) > 1:
                bank_fns.append(combo)

    print("\nPossible bank functions (XOR of the bits):")
    if not bank_fns:
        print("  (none found — range may be too small or measurement noisy)")
    for fn in bank_fns:
        bits = " ^ ".join(f"bit{b}" for b in fn)
        print(f"  {bits}")


if __name__ == "__main__":
    main()
