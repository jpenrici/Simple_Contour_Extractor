#!/usr/bin/env python3
"""
run.py

Runs the whole pipeline on a real PNG (--input) or, without it, on a generated test image
(circle, rectangle and triangle over a soft gradient, plus optional noise):

    <name>.png --img2csv --> <name>.csv --csv2json --> <name>.json --json2svg --> <name>.svg

<name> is the input file name (or "example" for the generated image); see --name.

Every output is validated (JSON: dimensions, contour ids, ordered/adjacent points; SVG: size,
only M/L/Z commands, coordinates inside the viewBox) and a <name>_overlay.png with the JSON
contours drawn on top of the grayscale image (rebuilt from the CSV) is written, so step 2 can
be checked by eye. Use --no-overlay to skip it.

Standard library only (PNGs are written and inspected with zlib/struct; decoding is left to
img2csv).

Project layout: the script lives in the project root and can be run from any directory.
Executables are taken from <root>/bin (then PATH) and every file is written to <root>/output.

    Img2Svg/
    ├── CMakeLists.txt
    ├── run.py          # execute pipeline
    ├── bin
    │   ├── img2csv     # step 1 - Convert PNG image to grayscale matrix in CSV format
    │   ├── csv2json    # step 2 - Convert img2csv CSV to a specific JSON format
    │   └── json2svg    # step 3 - Converts JSON from csv2json into SVG outlines
    ├── images
    │   ├── logo.png    # C++ logo (https://pt.wikipedia.org/wiki/Ficheiro:ISO_C%2B%2B_Logo.svg)
    │   ├── sample.png  # low-resolution image for inspecting intermediate files (.csv, .json)
    │   └── sample.svg  # Inkscape image used to generate sample.png
    └── src
        ├── img2csv
        │   ├── CMakeLists.txt
        │   └── main.cpp        # C++ algorithm (step 1)
        ├── csv2json
        │   ├── CMakeLists.txt
        │   └── main.f90        # Fortran algorithm (step 2)
        └── json2svg
            ├── CMakeLists.txt
            └── main.cpp        # C++ algorithm (step 3)

Usage:
    python3 run.py --input images/logo.png [--epsilon N] [--name NAME] [--no-overlay]
    python3 run.py [--width N] [--height N] [--noise N] [--seed N] [--epsilon N]
    (both accept --img2csv/--csv2json/--json2svg PATH and --out-dir DIR)

Exit codes: 0 = OK, 1 = pipeline or validation error, 2 = usage error.
"""

import argparse
import json
import random
import re
import shlex
import shutil
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent  # project root: holds bin/ and output/
PALETTE = [
    (255, 64, 64),
    (64, 220, 64),
    (80, 140, 255),
    (255, 200, 40),
    (220, 80, 220),
    (40, 220, 220),
]
SVG_NS = "{http://www.w3.org/2000/svg}"
NUM = r"\d+(?:\.\d+)?"
PATH_RE = re.compile(
    rf"M {NUM} {NUM}(?: L {NUM} {NUM})*(?: Z)?"
)  # the only path syntax json2svg emits
COORD_RE = re.compile(rf"({NUM}) ({NUM})")


def write_png(path, width, height, pixels, channels):
    """Writes an 8-bit PNG: channels=1 -> grayscale, channels=3 -> RGB."""

    def chunk(tag, data):
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data))
        )

    stride = width * channels
    raw = b"".join(
        b"\x00" + bytes(pixels[y * stride : (y + 1) * stride]) for y in range(height)
    )
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 0 if channels == 1 else 2, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )


def png_size(path):
    """Returns (width, height) read from the PNG header; raises ValueError if it is not a PNG."""
    with path.open("rb") as file:
        header = file.read(24)
    if (
        len(header) < 24
        or header[:8] != b"\x89PNG\r\n\x1a\n"
        or header[12:16] != b"IHDR"
    ):
        raise ValueError("not a PNG file")
    return struct.unpack(">II", header[16:24])


def read_gray(path):
    """Reads the grayscale matrix written by img2csv. Returns (width, height, pixels)."""
    rows = [
        bytes(map(int, line.split(",")))
        for line in path.read_text().splitlines()
        if line.strip()
    ]
    if not rows or len({len(row) for row in rows}) != 1:
        raise ValueError(f"{path.name} is not a rectangular 0-255 matrix")
    return len(rows[0]), len(rows), bytearray(b"".join(rows))


def render_scene(width, height, noise, seed):
    """Returns width*height grayscale bytes; shapes scale with the image size."""
    rng = random.Random(seed)
    cx, cy, r2 = 0.28 * width, 0.50 * height, (0.22 * min(width, height)) ** 2
    rx0, ry0, rx1, ry1 = 0.55 * width, 0.12 * height, 0.90 * width, 0.42 * height
    tri = [
        (0.55 * width, 0.88 * height),
        (0.90 * width, 0.88 * height),
        (0.725 * width, 0.55 * height),
    ]
    sides = list(zip(tri, tri[1:] + tri[:1]))

    def in_triangle(x, y):
        # Inside when the point is on the same side of all three edges.
        return (
            len(
                {
                    (bx - ax) * (y - ay) - (by - ay) * (x - ax) >= 0
                    for (ax, ay), (bx, by) in sides
                }
            )
            == 1
        )

    pixels = bytearray(width * height)
    for y in range(height):
        for x in range(width):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r2:
                v = 220
            elif rx0 <= x <= rx1 and ry0 <= y <= ry1:
                v = 170
            elif in_triangle(x, y):
                v = 250
            else:
                v = 40 + 40 * x // width
            pixels[y * width + x] = min(255, max(0, v + rng.randint(-noise, noise)))
    return pixels


def render_overlay(gray, width, height, edges):
    """RGB copy of the image with every contour painted in its own color."""
    rgb = bytearray(3 * width * height)
    for channel in range(3):
        rgb[channel::3] = gray
    for edge in edges:
        color = bytes(PALETTE[edge["id"] % len(PALETTE)])
        for p in edge["points"]:
            i = 3 * (p["y"] * width + p["x"])
            rgb[i : i + 3] = color
    return rgb


def validate_json(doc, width, height):
    """Checks the JSON schema and that every contour really is an ordered path."""
    if (doc.get("width"), doc.get("height")) != (width, height):
        raise ValueError(
            f"size mismatch: JSON says {doc.get('width')}x{doc.get('height')}, image is {width}x{height}"
        )
    for expected_id, edge in enumerate(doc["edges"], start=1):
        if edge["id"] != expected_id:
            raise ValueError(
                f"contour ids must be sequential: got {edge['id']}, expected {expected_id}"
            )
        points = edge["points"]
        if not points:
            raise ValueError(f"contour {expected_id} has no points")
        for p in points:
            if not (0 <= p["x"] < width and 0 <= p["y"] < height):
                raise ValueError(
                    f"contour {expected_id}: point {p} is outside the image"
                )
        for a, b in zip(points, points[1:]):
            if max(abs(a["x"] - b["x"]), abs(a["y"] - b["y"])) != 1:
                raise ValueError(
                    f"contour {expected_id}: points {a} -> {b} are not 8-neighbours"
                )


def validate_svg(path, width, height, contour_ids):
    """Checks the SVG size, the path syntax (M/L/Z only) and the bounds. Returns (paths, points)."""
    root = ET.parse(path).getroot()
    if (root.get("width"), root.get("height"), root.get("viewBox")) != (
        str(width),
        str(height),
        f"0 0 {width} {height}",
    ):
        raise ValueError(
            f"size mismatch: {root.get('width')}x{root.get('height')}, viewBox '{root.get('viewBox')}'"
        )
    paths = root.findall(f"{SVG_NS}path")
    total = 0
    for element in paths:
        d = element.get("d", "")
        if not PATH_RE.fullmatch(d):
            raise ValueError(
                f"unexpected path data (only M, L and Z are allowed): '{d[:60]}'"
            )
        if element.get("data-id") not in {str(i) for i in contour_ids}:
            raise ValueError(
                f"path refers to an unknown contour id: {element.get('data-id')}"
            )
        coords = [(float(x), float(y)) for x, y in COORD_RE.findall(d)]
        if not all(0 <= x <= width and 0 <= y <= height for x, y in coords):
            raise ValueError(
                f"contour {element.get('data-id')}: a point is outside the viewBox"
            )
        total += len(coords)
    return len(paths), total


def shown(path):
    """Path as displayed and passed to the tools: relative to the project root when inside it."""
    try:
        return str(Path(path).relative_to(ROOT))
    except ValueError:
        return str(path)


def find_exe(name, override):
    """--<name> if given (path or PATH lookup, then current directory); else <root>/bin, then PATH."""
    candidates = (
        [override, f"./{override}"] if override else [str(ROOT / "bin" / name), name]
    )
    for candidate in candidates:
        path = shutil.which(candidate)
        if path is not None:
            return str(
                Path(path).absolute()
            )  # steps run with cwd=ROOT, so never pass a cwd-relative path
    where = f"'{override}'" if override else f"{ROOT / 'bin'} or in PATH"
    sys.exit(
        f"Error: executable '{name}' not found: looked for {where} (build the project or use --{name})"
    )


def run_step(title, cmd):
    print(f"{title}\n    $ {shlex.join([shown(cmd[0]), *cmd[1:]])}")
    proc = subprocess.run(
        cmd, capture_output=True, text=True, cwd=ROOT
    )  # same relative paths as printed
    for line in proc.stdout.splitlines():
        print(f"    {line}")
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr)
        print(
            f"Error: '{Path(cmd[0]).name}' failed with exit code {proc.returncode}",
            file=sys.stderr,
        )
        sys.exit(proc.returncode)


def main():
    parser = argparse.ArgumentParser(
        description="Run img2csv -> csv2json -> json2svg on a real PNG (--input) or on a generated test image."
    )
    parser.add_argument(
        "--input", help="PNG to process (default: generate a test image)"
    )
    parser.add_argument(
        "--name",
        help="base name of the output files (default: the input file name, or 'example')",
    )
    parser.add_argument(
        "--out-dir",
        default=str(ROOT / "output"),
        help=f"where the files are written (default: {ROOT / 'output'})",
    )
    parser.add_argument(
        "--epsilon",
        type=float,
        default=1.0,
        help="Ramer-Douglas-Peucker tolerance (default: 1.0)",
    )
    parser.add_argument(
        "--no-overlay",
        action="store_true",
        help="skip the contour overlay PNG (about +2 s per 6 megapixels)",
    )
    parser.add_argument(
        "--img2csv", help="img2csv executable (default: <root>/bin/img2csv, then PATH)"
    )
    parser.add_argument(
        "--csv2json",
        help="csv2json executable (default: <root>/bin/csv2json, then PATH)",
    )
    parser.add_argument(
        "--json2svg",
        help="json2svg executable (default: <root>/bin/json2svg, then PATH)",
    )
    generated = parser.add_argument_group(
        "generated test image (not allowed together with --input)"
    )
    generated.add_argument("--width", type=int, help="default: 256")
    generated.add_argument("--height", type=int, help="default: 192")
    generated.add_argument(
        "--noise", type=int, help="+/- random gray noise per pixel (default: 3)"
    )
    generated.add_argument("--seed", type=int, help="default: 0")
    args = parser.parse_args()

    defaults = {"width": 256, "height": 192, "noise": 3, "seed": 0}
    given = [option for option in defaults if getattr(args, option) is not None]
    if args.input and given:
        parser.error(
            f"--{given[0]} only applies to the generated test image (not allowed with --input)"
        )
    for option, value in defaults.items():
        if getattr(args, option) is None:
            setattr(args, option, value)
    if args.width < 1 or args.height < 1 or args.noise < 0 or args.epsilon < 0:
        parser.error("width and height must be >= 1; noise and epsilon must be >= 0")
    name = args.name or (Path(args.input).stem if args.input else "example")
    if not name or Path(name).name != name:
        parser.error("--name must be a plain file name (no directories)")

    img2csv = find_exe("img2csv", args.img2csv)
    csv2json = find_exe("csv2json", args.csv2json)
    json2svg = find_exe("json2svg", args.json2svg)
    out = Path(args.out_dir).resolve()
    out.mkdir(parents=True, exist_ok=True)  # csv2json does not create directories
    csv, js, svg, overlay = (
        out / f"{name}{suffix}" for suffix in (".csv", ".json", ".svg", "_overlay.png")
    )

    if args.input:
        png = Path(args.input).resolve()
        try:
            width, height = png_size(png)
        except (OSError, ValueError) as error:
            sys.exit(f"Error: cannot use '{args.input}': {error}")
        print(f"[1/5] Using {shown(png)} ({width}x{height})")
    else:
        png, (width, height) = out / f"{name}.png", (args.width, args.height)
        print(f"[1/5] Generating {width}x{height} test image -> {shown(png)}")
        write_png(
            png, width, height, render_scene(width, height, args.noise, args.seed), 1
        )

    run_step(
        "[2/5] img2csv (PNG -> CSV)", [img2csv, "-i", shown(png), "-o", shown(csv)]
    )
    run_step(
        "[3/5] csv2json (CSV -> contours)",
        [csv2json, "-i", shown(csv), "-o", shown(js)],
    )
    run_step(
        "[4/5] json2svg (contours -> SVG)",
        [json2svg, "-i", shown(js), "-o", shown(svg), "-e", str(args.epsilon)],
    )

    print("[5/5] Validating output")
    doc = json.loads(js.read_text())
    try:
        validate_json(doc, width, height)
    except ValueError as error:
        sys.exit(f"Error: invalid {shown(js)}: {error}")
    try:
        paths, svg_points = validate_svg(
            svg, width, height, [e["id"] for e in doc["edges"]]
        )
    except (ValueError, ET.ParseError) as error:
        sys.exit(f"Error: invalid {shown(svg)}: {error}")
    if not args.no_overlay:
        try:
            gray_width, gray_height, gray = read_gray(csv)
            if (gray_width, gray_height) != (width, height):
                raise ValueError(
                    f"CSV is {gray_width}x{gray_height}, image is {width}x{height}"
                )
        except ValueError as error:
            sys.exit(f"Error: invalid {shown(csv)}: {error}")
        write_png(
            overlay, width, height, render_overlay(gray, width, height, doc["edges"]), 3
        )

    json_points = sum(len(e["points"]) for e in doc["edges"])
    print(
        f"    contours: {len(doc['edges'])}, points: {json_points} (JSON) -> {svg_points} (SVG, {paths} paths)"
    )
    produced = (
        ([] if args.input else [png])
        + [csv, js, svg]
        + ([] if args.no_overlay else [overlay])
    )
    print(f"Done. Files in '{shown(out)}': {', '.join(p.name for p in produced)}")


if __name__ == "__main__":
    main()
