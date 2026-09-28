"""The part of an Apple crash report (.ips) worth reading in a CI log: the
exception, the termination reason and the crashed thread's frames.

    python3 tools/apple/ips_summary.py <report.ips> [frames]

A .ips file is two JSON documents, a one-line header and the report; macOS
writes one to ~/Library/Logs/DiagnosticReports for every crash, a simulator
app's included. .github/workflows/apple.yml prints one when the game ends
without its capture.
"""

import json
import sys


def main(argv):
    if len(argv) < 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    limit = int(argv[2]) if len(argv) > 2 else 30
    text = open(argv[1], encoding="utf-8", errors="replace").read()
    header, _, body = text.partition("\n")
    try:
        report = json.loads(body)
    except json.JSONDecodeError:
        print(text[:4000])
        return 0

    print(f"== {argv[1]}")
    print("header:", header[:300])
    for key in ("exception", "termination", "asi", "ktriageinfo"):
        if key in report:
            print(f"{key}: {json.dumps(report[key])[:1200]}")

    images = report.get("usedImages", [])
    threads = report.get("threads", [])
    faulting = report.get("faultingThread", 0)
    if not threads:
        return 0
    thread = threads[faulting] if faulting < len(threads) else threads[0]
    print(f"thread {faulting} ({thread.get('name', thread.get('queue', ''))}):")
    for i, frame in enumerate(thread.get("frames", [])[:limit]):
        index = frame.get("imageIndex", -1)
        image = images[index].get("name", "?") if 0 <= index < len(images) else "?"
        symbol = frame.get("symbol", hex(frame.get("imageOffset", 0)))
        print(f"  {i:2d} {image:28s} {symbol} +{frame.get('symbolLocation', 0)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
