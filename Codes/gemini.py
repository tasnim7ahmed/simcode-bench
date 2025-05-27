import os
import re
import yaml
import logging
import pandas as pd
from pathlib import Path
from dotenv import load_dotenv
import google.generativeai as genai
from rich.logging import RichHandler
from tqdm import tqdm
import time

logging.basicConfig(level='INFO', handlers=[RichHandler()], format='%(message)s')

PROMPT_YAML = Path('../prompt.yaml')

SOURCE_PATH = "../Dataset"
OUTPUT_PATH = "../Output"
RESULTS_PATH = "../Results"

Path(SOURCE_PATH).mkdir(parents=True, exist_ok=True)
Path(OUTPUT_PATH).mkdir(parents=True, exist_ok=True)
Path(RESULTS_PATH).mkdir(parents=True, exist_ok=True)

SRC_DIRS = [Path(f'{SOURCE_PATH}/Prompts/Large'), Path(f'{SOURCE_PATH}/Prompts/Small')]

OUT_DIRS = {
    'instruction_prompt': Path(f'{OUTPUT_PATH}/Basic/Gemini'),
    'cot_prompt': Path(f'{OUTPUT_PATH}/CoT/Gemini'),
    'few_shot_prompt': Path(f'{OUTPUT_PATH}/FewShot/Gemini'),
    'react_prompt': Path(f'{OUTPUT_PATH}/ReAct/Gemini'),
    'expert_prompt': Path(f'{OUTPUT_PATH}/Expert/Gemini'),
    'self_consistency_prompt': Path(f'{OUTPUT_PATH}/SelfConsistency/Gemini')
}

MODEL_NAME = 'gemini-2.0-flash'

CODE_BLOCK_RE = re.compile(r"```(?:cpp|c\+\+)?\s*([\s\S]+?)```", re.IGNORECASE)
NUM_PROBLEMS = 2

def extract_code(text):
    m = CODE_BLOCK_RE.search(text)
    return m.group(1).strip() if m else text.strip()

def load_prompts():
    return yaml.safe_load(PROMPT_YAML.open())

def collect_description_files():
    txt_files = []
    for d in SRC_DIRS:
        txt_files.extend(sorted(d.glob('*.txt')))
    return txt_files[:NUM_PROBLEMS] if NUM_PROBLEMS != -1 else txt_files

def generate_and_save(prompts, txt_files):
    load_dotenv()
    genai.configure(api_key=os.environ['GEMINI_API_KEY'])
    print(os.environ['GEMINI_API_KEY'])

    model = genai.GenerativeModel(MODEL_NAME)
    
    results = {key: [] for key in prompts.keys()}
    
    for src in tqdm(txt_files, desc='Generating'):
        description = src.read_text(encoding='utf-8').strip()
        
        for prompt_name, prompt_template in prompts.items():
            prompt = prompt_template.replace('{program_description}', description).replace('{problem_description}', description)
            out_dir = OUT_DIRS[prompt_name]
            out_dir.mkdir(parents=True, exist_ok=True)
            outfile = out_dir / f"{src.stem}.cc"
            
            try:
                resp = model.generate_content(prompt)
                code = extract_code(resp.text)
                outfile.write_text(code, encoding='utf-8')
                results[prompt_name].append({
                    'file': src.name,
                    'generated': True,
                    'output_path': str(outfile)
                })
                logging.info(f'✓ {prompt_name}: {src.name} → {outfile}')
            except Exception as e:
                results[prompt_name].append({
                    'file': src.name,
                    'generated': False,
                    'output_path': 'N/A'
                })
                logging.error(f'✗ {prompt_name}: {src.name} – {e}')
    return results

def save_results(results):    
    summary = {
        'Prompt': [],
        'Generation_Success_Rate': []
    }
    for prompt_name, result_list in results.items():
        success_rate = sum(r['generated'] for r in result_list) / len(result_list) if result_list else 0
        summary['Prompt'].append(prompt_name)
        summary['Generation_Success_Rate'].append(success_rate)
    
    summary_df = pd.DataFrame(summary)
    print("\nAverage Generation Success Rates:")
    print(summary_df)
    summary_df.to_csv(f'{RESULTS_PATH}/{MODEL_NAME}_generation_summary.csv', index=False)

def main():
    prompts = load_prompts()
    txt_files = collect_description_files()
    results = generate_and_save(prompts, txt_files)
    save_results(results)

if __name__ == '__main__':
    main()