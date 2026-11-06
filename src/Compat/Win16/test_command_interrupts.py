#!/usr/bin/env python3
"""Exercise native CLI transactions with real host SIGINT and output failures.

Use an ASan/UBSan console from build_extract_host_tests.sh, a sufficiently
large BCJ2 fixture from make_bcj2_fixtures.py, and its uncompressed input.
DOS keyboard and BIOS-vector tests remain separate guest integration tests.
"""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--console", required=True, type=Path)
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path,
                        help="New, disposable test directory (must not exist)")
    args = parser.parse_args()
    console, archive = args.console.resolve(), args.archive.resolve()
    original = archive.read_bytes()
    data = args.input.read_bytes()
    if not data:
        parser.error("The input fixture must not be empty")
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    cache = work / "cache"
    cache.mkdir()
    env = dict(os.environ, TEMP=str(cache))
    (work / "BIG.BIN").write_bytes(data * max(1, (4 * 1024 * 1024 // len(data)) + 1))
    cases = [
        ("test", ["t", str(archive)]),
        ("extract", ["x", str(archive), "OUT"]),
        ("create", ["a", "NEW.7Z", "-m0=lzma", "BIG.BIN"]),
        ("update", ["a", "UP.7Z", "-m0=lzma", "BIG.BIN"]),
        ("hash", ["h", "-scrc*", "BIG.BIN"]),
    ]
    results = []
    for name, command in cases:
        if name == "update":
            (work / "UP.7Z").write_bytes(original)
        process = subprocess.Popen([str(console)] + command, cwd=work, env=env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            banner = process.stdout.readline()
            assert b"native 16-bit" in banner, banner
            deadline = time.monotonic() + 15
            while True:
                assert process.poll() is None, (name, "finished before interruption")
                if name in ("test", "extract"):
                    ready = bool(list(cache.iterdir()))
                elif name == "create":
                    path = work / "NEW.7Z"
                    ready = path.exists() and path.stat().st_size > 32
                elif name == "update":
                    ready = any(p.is_file() and p.name not in ("BIG.BIN", "UP.7Z")
                                and p.suffix.lower() != ".log" for p in work.iterdir())
                else:
                    time.sleep(.02)
                    ready = True
                if ready:
                    break
                assert time.monotonic() < deadline, (name, "did not start")
                time.sleep(.001)
            process.send_signal(signal.SIGINT)
            out, err = process.communicate(timeout=20)
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()
        (work / (name + ".log")).write_bytes(banner + out + err)
        assert process.returncode == 255, (name, process.returncode, out, err)
        assert b"Everything is Ok" not in out and b"Operation cancelled" in out
        assert not list(cache.iterdir()), (name, "temporary dictionary left behind")
        assert not (work / "OUT").exists() and not (work / "NEW.7Z").exists()
        if name == "update":
            assert (work / "UP.7Z").read_bytes() == original
        results.append({"operation": name, "exit_code": 255,
                        "scratch_empty": True, "rollback_verified": True})
        print("PASS SIGINT", name, flush=True)
    if Path("/dev/full").exists():
        with open("/dev/full", "wb") as full:
            result = subprocess.run([str(console), "a", "FULL.7Z", "BIG.BIN"],
                                    cwd=work, env=env, stdout=full,
                                    stderr=subprocess.PIPE, timeout=20)
        assert result.returncode == 2 and b"Cannot write command output" in result.stderr
        assert not (work / "FULL.7Z").exists()
        results.append({"output_write_failure_exit_code": 2, "archive_not_created": True})
        print("PASS stdout failure", flush=True)
    (work / "results.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
