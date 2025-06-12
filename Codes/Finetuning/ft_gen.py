import os, re, json, logging
from pathlib import Path
from dotenv import load_dotenv
from openai import OpenAI
from rich.logging import RichHandler
from tqdm import tqdm

# ───────────── logging ─────────────
logging.basicConfig(level='INFO', handlers=[RichHandler()], format='%(message)s')

# ───────────── paths ───────────────
ROOT        = Path("")                           # adjust if needed
TEST_JSONL  = ROOT / "simcode_eval.jsonl"
BASE_OUT    = ROOT / "Output" / "Finetuned"

GEN_DIR = BASE_OUT / "generated"                 # <<< new
GT_DIR  = BASE_OUT / "groundtruth"               # <<< new
RESULTS_CSV = ROOT / "Results" / "ft_eval_summary.csv"

for p in (GEN_DIR, GT_DIR, RESULTS_CSV.parent):
    p.mkdir(parents=True, exist_ok=True)

# ───────────── model ───────────────
MODEL_NAME = "ft:gpt-4.1-2025-04-14:personal:ns3:BfyICEe4"

# ───────────── helpers ─────────────
CODE_RE = re.compile(r"```(?:cpp|c\+\+)?\s*([\s\S]+?)```", re.IGNORECASE)
def strip_fence(txt: str) -> str:
    m = CODE_RE.search(txt)
    return m.group(1).strip() if m else txt.strip()

def iter_test_records(path: Path):
    """
    Yields: idx, messages_without_assistant, reference_code, stem
    `stem` is taken from file_path if available, else sample_####.
    """
    with path.open(encoding="utf-8") as f:
        for idx, line in enumerate(f, 1):
            rec   = json.loads(line)
            msgs  = rec["messages"]
            ref   = next(m["content"] for m in msgs if m["role"] == "assistant")
            no_asst = [m for m in msgs if m["role"] != "assistant"]

            # stem: use original file basename if provided, else fallback
            if "file_path" in rec:
                stem = Path(rec["file_path"]).stem
            else:
                stem = f"sample_{idx:04d}"

            yield idx, no_asst, strip_fence(ref), stem

# ───────────── main gen loop ───────
def generate_and_save():
    load_dotenv()
    client = OpenAI(api_key=os.environ["OPENAI_API_KEY"])
    logging.info("Using model: %s", MODEL_NAME)

    gen_ok, gen_fail = 0, 0
    for _, prompt_msgs, ref_code, stem in tqdm(iter_test_records(TEST_JSONL),
                                               desc="Evaluating"):
        gen_path = GEN_DIR / f"{stem}.cc"
        gt_path  = GT_DIR  / f"{stem}.cc"

        # write ground-truth once (if not already)
        if not gt_path.exists():
            gt_path.write_text(ref_code, encoding="utf-8")

        try:
            resp = client.chat.completions.create(
                model=MODEL_NAME,
                messages=prompt_msgs
            )
            gen_code = strip_fence(resp.choices[0].message.content)
            gen_path.write_text(gen_code, encoding="utf-8")
            gen_ok += 1
        except Exception as e:
            gen_fail += 1
            logging.error("✗ %s – %s", stem, e)

    # summary
    logging.info("Done: %s generated, %s failed", gen_ok, gen_fail)
    with RESULTS_CSV.open("w", encoding="utf-8") as f:
        f.write("generated,failed\n")
        f.write(f"{gen_ok},{gen_fail}\n")

if __name__ == "__main__":
    generate_and_save()
