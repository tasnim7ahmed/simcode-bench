import sys, subprocess, json, math, os, re, logging, itertools
from pathlib import Path
import pandas as pd
from tqdm import tqdm

import nltk
nltk.download("wordnet", quiet=True)
nltk.download("omw-1.4", quiet=True)

import sacrebleu, Levenshtein
from rouge_score import rouge_scorer
from nltk.translate import meteor_score
from sentence_transformers import SentenceTransformer, util
from bert_score import score as bertscore

# ---------- 2. (optional) CodeBLEU helper --------------------------------------------------------
# lightweight re-implementation of CodeBLEU n-gram + syntax components
# (For full official CodeBLEU, clone https://github.com/microsoft/CodeXGLUE/tree/main/Code-Code/code-to-code-trans and import the calc_codebleu script.)

def simple_codebleu(ref: str, hyp: str) -> float:
    """
    Very small-footprint approximation:
    BLEU (4-gram) * 0.5  +  AST token overlap * 0.5
    AST token overlap ≈ Jaccard of brace / keyword tokens.
    """
    bleu = sacrebleu.corpus_bleu([hyp], [[ref]]).score / 100
    # crude 'syntax token' set: language keywords + braces/semicolons
    keywords = re.findall(r"\b(?:if|else|for|while|return|int|double|float"
                          r"|std|using|namespace|class|struct|void|auto)\b|\{|\}|;", ref)
    kw_ref = set(keywords)
    kw_hyp = set(re.findall(r"\b(?:if|else|for|while|return|int|double|float"
                            r"|std|using|namespace|class|struct|void|auto)\b|\{|\}|;", hyp))
    jacc = len(kw_ref & kw_hyp) / (len(kw_ref | kw_hyp) or 1)
    return 0.5 * bleu + 0.5 * jacc

# ---------- 3. CrystalBLEU helper ---------------------------------------------------------------
COMMON_PATTERNS = {
    "for(": None, "while(": None, "if(": None, "std::": None,
    "ns3::": None, "return": None, "{": None, "}": None, ";": None,
}
def filter_common_ngrams(code: str) -> str:
    tokens = code.split()
    return " ".join(t for t in tokens if t not in COMMON_PATTERNS)

def crystal_bleu(ref: str, hyp: str) -> float:
    ref_f = filter_common_ngrams(ref)
    hyp_f = filter_common_ngrams(hyp)
    return sacrebleu.corpus_bleu([hyp_f], [[ref_f]]).score

# ---------- 4. clone-similarity (token Jaccard) -------------------------------------------------
def token_jaccard(ref: str, hyp: str) -> float:
    tok_ref = set(re.findall(r"[A-Za-z_]\w+", ref))
    tok_hyp = set(re.findall(r"[A-Za-z_]\w+", hyp))
    return len(tok_ref & tok_hyp) / (len(tok_ref | tok_hyp) or 1)

# ---------- 5. CodeBERT models (embedding + BERTScore) ------------------------------------------
embed_model = SentenceTransformer("microsoft/codebert-base")
# bert-score with same model
def bertscore_code(refs, hyps):
    P, R, F = bertscore(hyps, refs, lang="en", model_type="microsoft/codebert-base",
                        num_layers=12, verbose=False, idf=False)
    return F.mean().item()

# ---------- 6. data paths -----------------------------------------------------------------------
SRC_DIRS = [Path("../Dataset/Codes/Large"), Path("../Dataset/Codes/Small")]
GEN_DIR  = Path("BasicEvaluation/Gemini")          # from earlier notebook cells
GEN_DIR.mkdir(parents=True, exist_ok=True)

# ---------- 7. iterate & compute metrics --------------------------------------------------------
rows = []
scorer_rouge = rouge_scorer.RougeScorer(["rougeL"], use_stemmer=False)

for ref_path in tqdm(list(itertools.chain.from_iterable(d.glob("*.cc") for d in SRC_DIRS)),
                     desc="Scoring"):
    ref_code = ref_path.read_text(encoding="utf-8", errors="ignore")
    gen_path = GEN_DIR / ref_path.name
    if gen_path.exists():
        hyp_code = gen_path.read_text(encoding="utf-8", errors="ignore")
    else:
        hyp_code = None  # will yield NaNs

    def safe(metric_fn, *, default=float("nan")):
        try:
            return metric_fn()
        except Exception as e:
            logging.warning("Metric failed (%s, %s): %s", ref_path.name, metric_fn.__name__, e)
            return default

    row = {
        "file": ref_path.name,
        "BLEU": safe(lambda: sacrebleu.corpus_bleu([hyp_code], [[ref_code]]).score if hyp_code else float("nan")),
        "ROUGE_L": safe(lambda: scorer_rouge.score(ref_code, hyp_code)["rougeL"].fmeasure * 100
                        if hyp_code else float("nan")),
        "METEOR": safe(lambda: meteor_score.single_meteor_score(ref_code, hyp_code) * 100
                       if hyp_code else float("nan")),
        "ChrF": safe(lambda: sacrebleu.corpus_chrf([hyp_code], [[ref_code]]).score if hyp_code else float("nan")),
        "CrystalBLEU": safe(lambda: crystal_bleu(ref_code, hyp_code) if hyp_code else float("nan")),
        "CodeBLEU": safe(lambda: simple_codebleu(ref_code, hyp_code) * 100 if hyp_code else float("nan")),
        "Levenshtein": safe(lambda: Levenshtein.distance(ref_code, hyp_code) if hyp_code else float("nan")),
        "CodeBERTScore": safe(lambda: bertscore_code([ref_code], [hyp_code]) * 100 if hyp_code else float("nan")),
        "CodeBLEURT": float("nan"),   # placeholder – requires heavy checkpoint; fill if available
        "EmbeddingCosine": safe(lambda: util.cos_sim(embed_model.encode(ref_code, convert_to_tensor=True),
                                                     embed_model.encode(hyp_code, convert_to_tensor=True)).item()
                                if hyp_code else float("nan")),
        "CloneJaccard": safe(lambda: token_jaccard(ref_code, hyp_code) * 100 if hyp_code else float("nan")),
    }
    rows.append(row)

# ---------- 8. create DataFrame & averages ------------------------------------------------------
df = pd.DataFrame(rows)
avg_row = {"file": "AVERAGE"}
for col in df.columns[1:]:
    avg_row[col] = df[col].mean()
df = pd.concat([df, pd.DataFrame([avg_row])], ignore_index=True)

csv_path = Path("code_metrics.csv")
df.to_csv(csv_path, index=False)
print(f"✅ Saved metrics to {csv_path.resolve()}")
