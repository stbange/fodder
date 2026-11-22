"""Check LuaCov's report against the shipped pack scripts."""
from pathlib import Path
import re

report = Path("build/coverage/lua.txt").read_text()
files = list(Path("assets/packs").glob("*/scripts/**/*.lua"))
assert files, "No Lua gameplay scripts found."
for script in files:
    match = re.search(r"^" + re.escape(script.as_posix()) + r"\s+(\d+)\s+(\d+)\s+([\d.]+)%\s*$",
                      report, re.MULTILINE)
    if not match or float(match.group(3)) < 95:
        raise SystemExit(f"Lua unit coverage for {script} is below 95% or missing.")
    print(f"{script} has {match.group(3)}% unit line coverage.")
