#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ems04_tables.py — bit-level self-check of the GENERATED board tables.

Independently re-parses Appli/App/ems04_tables_gen.c (the artifact that is
actually compiled into the firmware — not the generator's in-memory state)
and compares element-by-element, at the IEEE-754 bit level, against the S1
frozen exports:

  1. ems04_bsfc_lut   == exported/bsfc_lut.f64   (376/376)
  2. ems04_soc_grid   == exported/soc_grid.f64   (171/171)
  3. ems04_dlam_grid  == exported/dlam_grid.f64  (41/41)
  4. ems04_pg_palette[ems04_pg_index[i]] == exported/policy_table_Pg.f64[i]
     for every one of the 597,960 elements (bit pattern equality).

Exit code 0 only if every check passes. Writes a machine-readable report to
Host/results/ems04_tables/tables_check_report.json (summary intended for the
project README).

Run:  .venv_tf/bin/python Host/tools/check_ems04_tables.py
"""
import json
import os
import re
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.normpath(os.path.join(HERE, "..", ".."))
REPO = os.path.normpath(os.path.join(PROJ,
    "..", "..", "..", "..", "..", ".."))
EXPORTED_DEFAULT = os.path.join(
    REPO, "experiments", "softhil_ems03_2026-09-25", "ems04_s1", "exported")
EXPORTED = os.environ.get("EMS04_S1_EXPORTED_DIR", EXPORTED_DEFAULT)

C_SRC = os.path.join(PROJ, "Appli", "App", "ems04_tables_gen.c")

N_LAM, N_PBAR, N_REG = 3020, 66, 3
N_BSFC, N_SOC, N_DLAM = 376, 171, 41
PG_N = N_LAM * N_PBAR * N_REG


def load_f64(name, expect_elems):
    path = os.path.join(EXPORTED, name)
    a = np.fromfile(path, dtype="<f8")
    if a.size != expect_elems:
        raise SystemExit("FAIL: %s has %d elems, expected %d"
                         % (name, a.size, expect_elems))
    return a


def parse_c_array(src, cname):
    """Extract one const array initializer; return list of tokens."""
    m = re.search(r"\b" + cname + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\};",
                  src, re.S)
    if not m:
        raise SystemExit("FAIL: array %s not found in generated C" % cname)
    return [t for t in (tok.strip() for tok in m.group(1).split(",")) if t]


def tokens_to_doubles(tokens, expect_n, cname):
    if len(tokens) != expect_n:
        raise SystemExit("FAIL: %s has %d literals, expected %d"
                         % (cname, len(tokens), expect_n))
    out = np.empty(expect_n, dtype="<f8")
    for i, t in enumerate(tokens):
        out[i] = float.fromhex(t)  # exact IEEE-754 round-trip
    return out


def tokens_to_u8(tokens, expect_n, cname):
    if len(tokens) != expect_n:
        raise SystemExit("FAIL: %s has %d literals, expected %d"
                         % (cname, len(tokens), expect_n))
    vals = [int(t) for t in tokens]
    if min(vals) < 0 or max(vals) > 255:
        raise SystemExit("FAIL: %s values out of u8 range" % cname)
    return np.array(vals, dtype=np.uint8)


def bits_equal(a, b):
    return a.size == b.size and a.tobytes() == b.tobytes()


def n_bitdiff(a, b):
    return int(np.count_nonzero(a.view(np.uint64) != b.view(np.uint64)))


def main():
    with open(C_SRC, "r") as f:
        src = f.read()

    report = {"schema": "ems04_tables_check_v1", "c_source":
              os.path.relpath(C_SRC, REPO),
              "exported_dir": os.path.relpath(EXPORTED, REPO)}

    # ---- 1..3: small f64 tables ----
    for cname, fname, n in (("ems04_bsfc_lut", "bsfc_lut.f64", N_BSFC),
                            ("ems04_soc_grid", "soc_grid.f64", N_SOC),
                            ("ems04_dlam_grid", "dlam_grid.f64", N_DLAM)):
        got = tokens_to_doubles(parse_c_array(src, cname), n, cname)
        ref = load_f64(fname, n)
        ok = bits_equal(got, ref)
        report[cname] = {"elems": n, "bit_exact": ok,
                         "n_bitdiff": n_bitdiff(got, ref)}
        if not ok:
            raise SystemExit("FAIL: %s != %s (%d bit diffs)"
                             % (cname, fname, n_bitdiff(got, ref)))
        print("PASS %s == %s  (%d/%d elements, bit-exact)"
              % (cname, fname, n, n))

    # ---- 4: Pg palette path, 597,960 elements ----
    pal_tokens = parse_c_array(src, "ems04_pg_index")  # touch: ensure present
    palette = tokens_to_doubles(parse_c_array(src, "ems04_pg_palette"),
                                len(parse_c_array(src, "ems04_pg_palette")),
                                "ems04_pg_palette")
    idx = tokens_to_u8(pal_tokens, PG_N, "ems04_pg_index")
    if palette.size > 256:
        raise SystemExit("FAIL: palette size %d > 256" % palette.size)
    if int(idx.max()) >= palette.size:
        raise SystemExit("FAIL: pg_index out of palette range")
    # palette uniqueness (bit-level)
    seen = set()
    for i in range(palette.size):
        key = struct.pack("<d", palette[i])
        if key in seen:
            raise SystemExit("FAIL: duplicate palette entry at %d" % i)
        seen.add(key)
    ref_pg = load_f64("policy_table_Pg.f64", PG_N)
    rebuilt = palette[idx]
    ok = bits_equal(rebuilt, ref_pg)
    n_bad = n_bitdiff(rebuilt, ref_pg)
    report["ems04_pg_palette_path"] = {
        "pg_elems": PG_N,
        "palette_n": int(palette.size),
        "palette_unique_bit_level": True,
        "index_elems": PG_N,
        "bit_exact": ok,
        "n_bitdiff": n_bad,
        "verdict": ("palette[idx[i]] == policy_table_Pg.f64[i] for %d/%d "
                    "elements" % (PG_N, PG_N)) if ok else "FAIL",
    }
    if not ok:
        raise SystemExit("FAIL: Pg palette path not bit-exact (%d/%d diffs)"
                         % (n_bad, PG_N))
    print("PASS ems04_pg_palette[ems04_pg_index[i]] == policy_table_Pg.f64[i]"
          "  (%d/%d elements, bit-exact; palette=%d, index u8)"
          % (PG_N, PG_N, palette.size))

    report["overall"] = True
    res_dir = os.path.join(PROJ, "Host", "results", "ems04_tables")
    os.makedirs(res_dir, exist_ok=True)
    out = os.path.join(res_dir, "tables_check_report.json")
    with open(out, "w") as f:
        json.dump(report, f, indent=2)
    print("report: %s" % os.path.relpath(out, REPO))
    print("OK: all table checks bit-exact")


if __name__ == "__main__":
    sys.exit(main())
