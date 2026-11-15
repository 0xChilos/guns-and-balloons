"""Update only fuel timing assets after compiling the per-fuel pause property."""
from pathlib import Path
namespace = {"__name__": "configure_stove"}
exec(compile(Path(__file__).with_name("configure_stove.py").read_text(encoding="utf-8"), "configure_stove.py", "exec"), namespace)
namespace["configure_fuel_assets"]()
