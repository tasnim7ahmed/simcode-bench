import sys
import re
import logging
import pandas as pd
from pathlib import Path
import nltk
import sacrebleu
import Levenshtein
from rouge_score import rouge_scorer
from sentence_transformers import SentenceTransformer
from sentence_transformers import models
from sentence_transformers import util
from bert_score import score as bertscore
from metrics_evaluation.metrics import codebleu
from tqdm import tqdm
import itertools

nltk.download('wordnet', quiet=True)
nltk.download('omw-1.4', quiet=True)

SOURCE_PATH = "../Dataset"
OUTPUT_PATH = "../Output"
RESULTS_PATH = "../Results"

Path(SOURCE_PATH).mkdir(parents=True, exist_ok=True)
Path(OUTPUT_PATH).mkdir(parents=True, exist_ok=True)
Path(RESULTS_PATH).mkdir(parents=True, exist_ok=True)

SRC_DIRS = [Path(f'{SOURCE_PATH}/Codes/Large'), Path(f'{SOURCE_PATH}/Codes/Small')]

GEN_DIRS = {
    'instruction_prompt': Path(f'{OUTPUT_PATH}/Basic/Qwen'),
    'cot_prompt': Path(f'{OUTPUT_PATH}/CoT/Qwen'),
    'few_shot_prompt': Path(f'{OUTPUT_PATH}/FewShot/Qwen'),
    'react_prompt': Path(f'{OUTPUT_PATH}/ReAct/Qwen'),
    'expert_prompt': Path(f'{OUTPUT_PATH}/Expert/Qwen'),
    'self_consistency_prompt': Path(f'{OUTPUT_PATH}/SelfConsistency/Qwen')
}

MODEL_NAME = 'qwen-plus-latest'

COMMON_PATTERNS = {
    'for(': None, 'while(': None, 'if(': None, 'std::': None,
    'ns3::': None, 'return': None, '{': None, '}': None, ';': None
}
def filter_common_ngrams(code):
    tokens = code.split()
    return ' '.join(t for t in tokens if t not in COMMON_PATTERNS)

def crystal_bleu(ref, hyp):
    ref_f = filter_common_ngrams(ref)
    hyp_f = filter_common_ngrams(hyp)
    return sacrebleu.corpus_bleu([hyp_f], [[ref_f]]).score

def token_jaccard(ref, hyp):
    tok_ref = set(re.findall(r'[A-Za-z_]\w+', ref))
    tok_hyp = set(re.findall(r'[A-Za-z_]\w+', hyp))
    return len(tok_ref & tok_hyp) / (len(tok_ref | tok_hyp) or 1)

word_embedding_model = models.Transformer('microsoft/codebert-base')
pooling_model = models.Pooling(
    word_embedding_model.get_word_embedding_dimension(),
    pooling_mode='cls'
)
embed_model = SentenceTransformer(modules=[word_embedding_model, pooling_model])

def bertscore_code(refs, hyps):
    P, R, F = bertscore(hyps, refs, lang='en', model_type='microsoft/codebert-base',
                        num_layers=12, verbose=False, idf=False)
    return F.mean().item()



def evaluate_prompt(prompt_name, gen_dir):
    rows = []
    scorer_rouge = rouge_scorer.RougeScorer(['rougeL'], use_stemmer=False)
    gen_files = list(gen_dir.glob('*.cc'))
    if not gen_files:
        logging.warning(f'No generated files found in {gen_dir}')
        return pd.DataFrame()

    # print(gen_files)
    
    for gen_path in tqdm(gen_files, desc=f'Scoring {prompt_name}'):
        hyp_code = gen_path.read_text(encoding='utf-8', errors='ignore')
        ref_code = None
        for src_dir in SRC_DIRS:
            ref_path = src_dir / gen_path.name
            if ref_path.exists():
                ref_code = ref_path.read_text(encoding='utf-8', errors='ignore')
                break
        if not ref_code:
            logging.warning(f'No reference file found for {gen_path.name} in {SRC_DIRS}')
        
        def safe(metric_fn, default=float('nan')):
            try:
                return metric_fn()
            except Exception as e:
                logging.warning(f'Metric failed ({gen_path.name}, {metric_fn.__name__}): {e}')
                return default
        
        row = {
            'file': ref_path.name,
            'BLEU': safe(lambda: sacrebleu.corpus_bleu([hyp_code], [[ref_code]]).score if ref_code else float('nan')),
            'ROUGE_L': safe(lambda: scorer_rouge.score(ref_code, hyp_code)['rougeL'].fmeasure * 100
                           if ref_code else float('nan')),
            'ChrF': safe(lambda: sacrebleu.corpus_chrf([hyp_code], [[ref_code]]).score if ref_code else float('nan')),
            'CodeBLEU': safe(lambda: codebleu(ref_code, hyp_code) * 100 if ref_code else float('nan')),
            'Levenshtein': safe(lambda: Levenshtein.distance(ref_code, hyp_code) if ref_code else float('nan')),
            'CodeBERTScore': safe(lambda: bertscore_code([ref_code], [hyp_code]) * 100 if ref_code else float('nan')),
            'EmbeddingCosine': safe(lambda: util.cos_sim(embed_model.encode(ref_code, convert_to_tensor=True),
                                                        embed_model.encode(hyp_code, convert_to_tensor=True)).item()
                                   if ref_code else float('nan')),
            'CloneJaccard': safe(lambda: token_jaccard(ref_code, hyp_code) * 100 if ref_code else float('nan'))
        }
        rows.append(row)
    
    df = pd.DataFrame(rows)
    df.to_csv(f'{RESULTS_PATH}/{MODEL_NAME}_{prompt_name}_metrics.csv', index=False)
    return df

def main():
    average_rows = []

    for prompt_name, gen_dir in GEN_DIRS.items():
        gen_dir.mkdir(parents=True, exist_ok=True)
        df = evaluate_prompt(prompt_name, gen_dir)
        if not df.empty:
            avg_row = {'Prompt': prompt_name}
            for col in df.columns[1:]:
                avg_row[col] = df[col].mean()
            average_rows.append(avg_row)
    
    avg_df = pd.DataFrame(average_rows)
    avg_df.to_csv(f'{RESULTS_PATH}/{MODEL_NAME}_average_metrics.csv', index=False)
    print('\nAverage Metrics:')
    print(avg_df)

if __name__ == '__main__':
    main()