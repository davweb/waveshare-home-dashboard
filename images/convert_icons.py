#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "cairosvg",
#   "pyyaml",
# ]
# ///

import sys
import yaml
from pathlib import Path
import cairosvg

def main():
    script_dir = Path(__file__).parent
    yaml_path = script_dir / "icons.yaml"

    with open(yaml_path) as f:
        config = yaml.safe_load(f)

    sizes = {s["name"]: s["size_px"] for s in config["sizes"]}

    ok = 0
    errors = 0
    for icon in config["icons"]:
        input_path = script_dir / icon["input"]
        output_path = script_dir / icon["output"]
        size_px = sizes[icon["size"]]

        if not input_path.exists():
            print(f"ERROR: {input_path} not found", file=sys.stderr)
            errors += 1
            continue

        cairosvg.svg2png(
            url=str(input_path),
            write_to=str(output_path),
            output_width=size_px,
            output_height=size_px,
        )
        print(f"{icon['input']} -> {icon['output']} ({size_px}px)")
        ok += 1

    print(f"\n{ok} converted, {errors} errors")
    if errors:
        sys.exit(1)

if __name__ == "__main__":
    main()
