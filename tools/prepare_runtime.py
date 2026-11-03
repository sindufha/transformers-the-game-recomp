"""Prepare checked project runtime copies for CMake or another build system."""
import argparse
from pathlib import Path
from build_probe import prepare_runtime

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lift-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    prepare_runtime(args.lift_dir, args.out)
