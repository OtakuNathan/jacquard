#!/usr/bin/env python3
"""Generate/check a Jacquard spec or run TLC with an explicit expected outcome."""

import argparse
import difflib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time


def emitted(emitter, *args):
    return subprocess.run(
        [str(emitter.resolve()), *args], check=True, capture_output=True,
        text=True, timeout=30,
    ).stdout


def generate(emitter, output, *, check=False, terminals=None):
    name = emitted(emitter, "name").strip()
    if not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", name):
        raise RuntimeError(f"Invalid module/file name emitted: {name!r}")
    tla = emitted(emitter)
    cfg = emitted(emitter, "cfg")
    if terminals is not None:
        if f"AllowedTerminals == {terminals}" not in tla.splitlines():
            raise RuntimeError(f"{name}: wrong terminal declaration; expected {terminals}")
        for line in ("INVARIANT TypeOK", "INVARIANT NoStuck", "CHECK_DEADLOCK FALSE"):
            if line not in cfg.splitlines():
                raise RuntimeError(f"{name}: missing config line: {line}")

    module = output / f"{name}.tla"
    config = output / f"{name}.cfg"
    if not check:
        output.mkdir(parents=True, exist_ok=True)
    for path, contents in ((module, tla), (config, cfg)):
        if check:
            previous = path.read_text(encoding="utf-8")
            if previous != contents:
                sys.stderr.writelines(difflib.unified_diff(
                    previous.splitlines(keepends=True), contents.splitlines(keepends=True),
                    fromfile=str(path), tofile=f"generated/{path.name}",
                ))
                raise RuntimeError(f"Spec drift: {path}")
        else:
            path.write_bytes(contents.encode("utf-8"))
    return module, config


def copy_input(source, directory):
    target = directory / source.name
    if target.exists():
        if source.read_bytes() != target.read_bytes():
            raise RuntimeError(f"Conflicting model input: {source.name}")
    else:
        shutil.copyfile(source, target)
    return target


def check_model(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    log = output / "tlc.log"
    report_path = output / "report.json"
    trace_path = output / "counterexample.txt"
    if trace_path.exists():
        trace_path.unlink()  # Never leave a stale counterexample on a safe rerun.
    kind = args.kind or ("mutant" if args.expect_invariant else "safety")
    report = dict(kind=kind, expected_invariant=args.expect_invariant,
                  exit_code=None, observed_invariant=None, passed=False)
    start = time.monotonic()
    log.write_text("", encoding="utf-8")

    try:
        # Run in a fresh directory: earlier runs must not supply a missing
        # imported module or stale configuration to TLC. Keep inputs as evidence.
        with tempfile.TemporaryDirectory(prefix="run-", dir=output) as tmp:
            work = Path(tmp)
            module = config = None
            if args.emitter:
                module, config = generate(args.emitter, work)
            if args.module:
                module = copy_input(args.module, work)
                if not args.config:
                    config = copy_input(args.module.with_suffix(".cfg"), work)
            if args.config:
                config = copy_input(args.config, work)
            for support in args.support_module:
                copy_input(support, work)
            for path in work.iterdir():
                if path.is_file():
                    shutil.copyfile(path, output / path.name)

            if module is None or config is None:
                raise RuntimeError("A TLA+ module and configuration are required")
            report.update(module=module.name, config=config.name)
            command = [args.java, "-Xmx" + args.heap, "-XX:+UseParallelGC", "-cp",
                       str(args.jar.resolve()), "tlc2.TLC", "-workers", str(args.workers),
                       "-config", config.name, module.name]
            report["command"] = command
            with log.open("w", encoding="utf-8") as stream:
                try:
                    report["exit_code"] = subprocess.run(
                        command, cwd=work, stdout=stream, stderr=subprocess.STDOUT,
                        timeout=args.timeout,
                    ).returncode
                except subprocess.TimeoutExpired:
                    report.update(exit_code=124, error="TLC timed out")
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        report["error"] = str(error)

    contents = log.read_text(encoding="utf-8", errors="replace")
    violation = re.search(
        r"^Error: Invariant (\w+) is violated(?:\.| by the initial state:)[ \t]*$",
        contents, re.MULTILINE,
    )
    if violation:
        report["observed_invariant"] = violation.group(1)
        # Retain TLC's actual trace, including its action names and state values.
        trace = contents[violation.start():]
        end = re.search(r"^(?:\d+ states generated|Finished in |The number of states|Progress\()", trace, re.MULTILINE)
        trace_path.write_text(trace[:end.start()] if end else trace, encoding="utf-8")
        report["counterexample"] = trace_path.name

    counts = re.findall(r"(\d+) states generated, (\d+) distinct states found", contents)
    if counts:
        report["states_generated"], report["distinct_states"] = map(int, counts[-1])
    if args.expect_invariant:
        # TLC exit 12 is a safety invariant violation. Parse/IO errors, tool
        # crashes, other invariants, timeouts and safe mutants must all fail.
        passed = report["exit_code"] == 12 and report["observed_invariant"] == args.expect_invariant
    else:
        passed = report["exit_code"] == 0 and "Model checking completed. No error has been found." in contents
    report["passed"] = passed and "error" not in report
    report["seconds"] = round(time.monotonic() - start, 3)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if not report["passed"]:
        print(contents, file=sys.stderr)
        print(json.dumps(report, indent=2), file=sys.stderr)
        return 1
    if violation:
        print(f"Expected {kind} counterexample: {violation.group(1)} ({trace_path})")
    else:
        print(f"Model safe: {report['module']} ({log})")
    print(f"Report: {report_path}")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    gen = commands.add_parser("generate", help="emit or compare the TLA+/TLC files")
    gen.add_argument("--emitter", type=Path, required=True)
    gen.add_argument("--output", type=Path, required=True)
    gen.add_argument("--check", action="store_true")
    gen.add_argument("--terminals", help="expected TLA+ terminal set; check the generator contract")
    check = commands.add_parser("check", help="run a safe model, mutant or reachability witness")
    check.add_argument("--emitter", type=Path, help="generate the base module with this executable")
    check.add_argument("--module", type=Path, help="handwritten module (may EXTEND generated base)")
    check.add_argument("--config", type=Path, help="override TLC configuration")
    check.add_argument("--support-module", type=Path, action="append", default=[])
    check.add_argument("--jar", type=Path, required=True)
    check.add_argument("--java", default="java")
    check.add_argument("--output", type=Path, required=True)
    check.add_argument("--expect-invariant")
    check.add_argument("--kind", choices=("safety", "mutant", "witness"))
    check.add_argument("--timeout", type=int, default=60)
    check.add_argument("--workers", type=int, default=1)
    check.add_argument("--heap", default="256m", help="maximum JVM heap (e.g. 512m)")
    args = parser.parse_args()
    if args.command == "generate":
        try:
            module, config = generate(args.emitter, args.output, check=args.check, terminals=args.terminals)
        except (OSError, RuntimeError, subprocess.SubprocessError) as error:
            print(error, file=sys.stderr)
            return 1
        print(f"{'Checked' if args.check else 'Generated'} {module} and {config}")
        return 0
    if not args.emitter and not args.module:
        check.error("supply --emitter and/or --module")
    if not args.jar.is_file():
        check.error(f"TLC jar not found: {args.jar}")
    if args.timeout < 1 or args.workers < 1:
        check.error("timeout and workers must be positive")
    if args.kind and (args.kind == "safety") == bool(args.expect_invariant):
        check.error("mutants and witnesses require --expect-invariant; safety tests must not specify one")
    return check_model(args)


if __name__ == "__main__":
    sys.exit(main())
