#!/usr/bin/env python3
"""
OpenSmell Electronic Nose — Interoperability Experiment Script
Usage:
    python experiment.py <csv_path>
    python experiment.py <csv_path> --reference <reference_csv>
"""

import argparse
import sys
from pathlib import Path

import numpy as np
from sklearn.metrics.pairwise import cosine_similarity


def process(filepath):
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "opensmell"))
    import opensmell
    return opensmell.process(filepath)


def load_reference_chemoprint(filepath):
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "opensmell"))
    import opensmell
    result = opensmell.process(filepath)
    return result.chemoprint


def print_table(results):
    sep = "+" + "-" * 30 + "+" + "-" * 14 + "+" + "-" * 14 + "+"
    print(sep)
    print(f"| {'Substance':<28} | {'Confidence':<12} | {'Chemoprint':<12} |")
    print(sep)
    for r in results:
        chemo_str = f"({len(r['chemoprint'])},)"
        print(f"| {r['substance']:<28} | {r['confidence']:<12.4f} | {chemo_str:<12} |")
    print(sep)


def print_cosine_similarity(substance, sim, threshold=0.75):
    status = "PASS" if sim >= threshold else "FAIL"
    print(f"\n  Cosine similarity vs reference: {sim:.4f}  [{status}]")
    if status == "PASS":
        print(f"  ✓ {substance}: similarity > {threshold} — interoperability verified")
    else:
        print(f"  ✗ {substance}: similarity < {threshold} — check sensor / protocol")


def main():
    parser = argparse.ArgumentParser(description="OpenSmell E-Nose Experiment Runner")
    parser.add_argument("csv", help="Path to sensor CSV file")
    parser.add_argument("--reference", help="Path to reference CSV for cosine similarity")
    args = parser.parse_args()

    print("=" * 60)
    print("  OpenSmell Electronic Nose — Experiment Results")
    print("=" * 60)

    csv_path = Path(args.csv)
    if not csv_path.exists():
        print(f"Error: {args.csv} not found", file=sys.stderr)
        sys.exit(1)

    print(f"\n  Input CSV: {csv_path.absolute()}")
    print(f"  DHT22: awaiting hardware")

    result = process(str(csv_path))

    results = [
        {
            "substance": result.substance,
            "confidence": result.confidence,
            "chemoprint": result.chemoprint,
        }
    ]
    print_table(results)

    if result.warning:
        print(f"\n  ⚠ {result.warning}")

    if args.reference:
        ref_path = Path(args.reference)
        if not ref_path.exists():
            print(f"Error: {args.reference} not found", file=sys.stderr)
            sys.exit(1)
        ref_chemo = load_reference_chemoprint(str(ref_path))
        sim = cosine_similarity(result.chemoprint.reshape(1, -1), ref_chemo.reshape(1, -1))[0, 0]
        print_cosine_similarity(result.substance, sim)
    else:
        print(f"\n  No reference CSV provided. Skipping cosine similarity check.")
        print(f"  Use --reference <csv> to compute interoperability score.")

    print("=" * 60)
    print(f"  Verdict: {result.substance} detected with {result.confidence:.1%} confidence")
    print("=" * 60)


if __name__ == "__main__":
    main()
