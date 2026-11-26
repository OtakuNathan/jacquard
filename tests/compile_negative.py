#!/usr/bin/env python3
"""Require a C++14 compilation failure with the expected diagnostic."""

import argparse
from pathlib import Path
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--style", choices=("gnu", "msvc"), required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--include", action="append", default=[])
    parser.add_argument("--define", action="append", default=[])
    parser.add_argument("--expect", required=True, help="regex for the intended compiler diagnostic")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    command = [args.compiler]
    if args.style == "msvc":
        command += ["/nologo", "/std:c++14", "/Zs", "/W4", "/WX"]
        prefix = "/"
    else:
        command += ["-std=c++14", "-fsyntax-only", "-Wall", "-Wextra", "-Werror", "-pedantic-errors"]
        prefix = "-"
    command += [prefix + "I" + path for path in args.include]
    command += [prefix + "D" + define for define in args.define]
    command.append(str(args.source))

    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=45)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(f"COMMAND: {command!r}\nEXIT: {result.returncode}\n{result.stdout}", encoding="utf-8")
    if result.returncode <= 0 or not re.search(args.expect, result.stdout):
        print(result.stdout, file=sys.stderr)
        print(f"Expected a compilation failure matching {args.expect!r}; see {args.output}", file=sys.stderr)
        return 1
    print(f"Expected compilation failure: {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
