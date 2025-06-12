#!/usr/bin/env python3
import argparse
from collections import defaultdict
import os

def calculate_accuracy(success_file, total=400):
    # Map the folder names under Output/ to the prompt keys we used in the runner
    prompt_map = {
        'Basic': 'instruction_prompt',
        'CoT': 'cot_prompt',
        'FewShot': 'few_shot_prompt',
        'ReAct': 'react_prompt',
        'Expert': 'expert_prompt',
        'SelfConsistency': 'self_consistency_prompt'
    }
    # Map the model‐folder names to your LLM identifiers
    model_map = {
        'Gemini': 'gemini-2.0-flash',
        'Openai': 'gpt-4.1'
    }

    # Count successes for each (model, prompt) pair
    counts = defaultdict(int)
    with open(success_file, 'r') as f:
        for line in f:
            path = line.strip()
            if not path:
                continue
            parts = path.split(os.sep)
            # Expect paths like …/Output/<Prompt>/<Model>/<N>.cc
            try:
                idx = parts.index('Output')
                prompt_dir = parts[idx + 1]
                model_dir = parts[idx + 2]
            except (ValueError, IndexError):
                continue

            prompt = prompt_map.get(prompt_dir)
            model = model_map.get(model_dir)
            if prompt and model:
                counts[(model, prompt)] += 1

    # Print a table of results
    print(f"{'Model':<20} {'Approach':<25} {'Success':>8} {'Total':>8} {'Accuracy':>10}")
    for model in model_map.values():
        for prompt in prompt_map.values():
            success = counts.get((model, prompt), 0)
            acc = (success / total) * 100
            print(f"{model:<20} {prompt:<25} {success:>8} {total:>8} {acc:>9.2f}%")

if __name__ == '__main__':
    calculate_accuracy("E:\\LLM\\simcode-bench\\Results\\NS3\\success_paths.txt", 400)