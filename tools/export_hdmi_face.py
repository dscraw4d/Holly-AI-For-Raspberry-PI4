"""Compatibility entry point for lossless portrait generation."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name("export_portraits.py")),run_name="__main__")
