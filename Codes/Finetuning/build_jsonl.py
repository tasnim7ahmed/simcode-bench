import json, sys, re
from pathlib import Path
from tqdm import tqdm

# --------- configuration ---------
ROOT = Path("../../Dataset")
OUTFILE = Path("simcode_full.jsonl")

PROMPT_DIRS = [ROOT / "Prompts" / "Large",
               ROOT / "Prompts" / "Small"]

SOL_DIRS = {
    "Large": ROOT / "Codes" / "Large",
    "Small": ROOT / "Codes" / "Small"
}

SYSTEM_MSG = """
You are a computer networking expert that works with network simulation in ns-3. Your task is to generate **only** the ns-3 C++ source code for the given program description.
Output a single compilable .cc file – no commentary, no markdown fences, no explanations.
Use ns-3 public APIs, assume ns-3 ≥ 3.41, and write error-free C++17.
"""

CODE_BLOCK_RE = re.compile(r"```(?:cpp|c\+\+)?\s*([\s\S]+?)```",
                           re.IGNORECASE)
# ----------------------------------

def clean_code(text: str) -> str:
    """Strip markdown fences if the solution files contain them."""
    m = CODE_BLOCK_RE.search(text)
    return m.group(1).strip() if m else text.strip()

def main() -> None:
    records = []
    for prompt_dir in PROMPT_DIRS:
        size = prompt_dir.name
        sol_dir = SOL_DIRS[size]
        for txt_path in tqdm(sorted(prompt_dir.glob("*.txt")),
                             desc=f"pairing {size}"):
            stem = txt_path.stem
            sol_path = sol_dir / f"{stem}.cc"
            if not sol_path.exists():
                print(f"WARNING: no solution for {txt_path}", file=sys.stderr)
                continue

            prompt_text = txt_path.read_text(encoding="utf-8").strip()
            code_text   = clean_code(sol_path.read_text(encoding="utf-8"))

            record = {
                "size": size,
                "messages": [
                    {"role": "system",  "content": SYSTEM_MSG},
                    {"role": "user",    "content": f"Simulation Description:\n{prompt_text}"},
                    {"role": "assistant","content": code_text}
                ]
            }
            records.append(record)

    with OUTFILE.open("w", encoding="utf-8") as f:
        for r in records:
            json.dump(r, f, ensure_ascii=False)
            f.write("\n")
    print(f"wrote {len(records)} examples to {OUTFILE}")

if __name__ == "__main__":
    main()
