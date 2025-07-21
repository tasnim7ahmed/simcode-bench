# SIMCODE Benchmark: Natural Language to ns-3 Network Simulation Code Generation

---

## Overview

SIMCODE is a comprehensive benchmark for evaluating large language models (LLMs) on the task of generating ns-3 C++ network simulation code from natural language descriptions. The benchmark features a diverse dataset of real-world networking problems, multiple prompt types, and rigorous evaluation using both automated metrics and executable test cases.

---

**Paper:** [SIMCODE: A Benchmark for Natural Language to ns-3 Network Simulation Code Generation](https://arxiv.org/abs/2507.11014)

**This paper has been accepted for presentation at the $50^{th}$ IEEE Conference on Local Computer Networks (LCN), Special Track on Large Language Models and Networking.**

---

## Dataset Structure

The dataset is organized into three difficulty categories, each containing multiple subcategories (e.g., wifi, tcp, udp, etc.). Each problem consists of a natural language prompt and a reference ns-3 C++ solution. The dataset also includes a suite of test cases for each problem to enable execution-based evaluation.

### Dataset Summary Table

| **Category (Difficulty)** | **# Problems** | **Avg. Sol. Length** | **Avg. # Tests** |
|--------------------------|:--------------:|:--------------------:|:----------------:|
| Introductory             | 111            | ~60                  | 3–5              |
| Intermediate             | 101            | ~110                 | 5–8              |
| Advanced                 | 189            | 150+                 | 8–10             |

---

## Prompt Types

The benchmark evaluates LLMs using several prompt engineering strategies, each designed to test different aspects of model reasoning and code generation:

- **Instruction:** Direct instruction to generate code from a description.
- **Chain-of-Thought (CoT):** Prompts that encourage step-by-step reasoning before code generation.
- **Few-shot:** Prompts that provide example input-output pairs.
- **ReAct:** Prompts that require the model to reason and act in sequence.
- **Expert:** Prompts that emphasize best practices and code quality.
- **Self-consistency:** Prompts that require the model to self-review and correct its output.

Prompt templates are defined in `prompt.yaml`.

---

## Evaluation Metrics

The benchmark uses a combination of automated and execution-based metrics:

- **CodeBLEU:** Measures code similarity, incorporating syntax, data flow, and structure.
- **CodeBERTScore:** Embedding-based semantic similarity for code.
- **Execution Accuracy:** Percentage of generated solutions passing execution stage.
- **Pass@1:** Probability that at least one generated solution passes all tests.

Additional metrics such as SacreBLEU, ROUGE, and Levenshtein distance are also available.

Metric implementations are in `Codes/metrics_evaluation/metrics/`, including custom adaptations for code.

---

## Benchmark Results

| **Model**      | **Prompt**        | **CodeBLEU** | **CodeBERT** | **Exec. Acc.** | **Pass@1** |
|---------------|-------------------|:------------:|:------------:|:--------------:|:----------:|
| **Gemini-2.0** | Instruction       | 0.732        | 0.927        | 0.133          | 0.055      |
|               | CoT               | 0.731        | 0.927        | 0.155          | 0.055      |
|               | Few-shot          | 0.722        | 0.932        | 0.190          | 0.065      |
|               | ReAct             | 0.726        | 0.928        | 0.143          | 0.045      |
|               | Expert            | 0.720        | 0.926        | 0.205          | 0.115      |
|               | Self-consistency  | 0.743        | 0.929        | 0.065          | 0.010      |
| **GPT-4.1**   | Instruction       | 0.760        | 0.930        | 0.265          | 0.088      |
|               | CoT               | 0.763        | 0.931        | 0.293          | 0.153      |
|               | Few-shot          | 0.765        | 0.934        | 0.280          | 0.140      |
|               | ReAct             | 0.765        | 0.931        | 0.283          | 0.140      |
|               | Expert            | 0.756        | 0.931        | 0.258          | 0.123      |
|               | Self-consistency  | 0.773        | 0.929        | 0.285          | 0.133      |
| **Qwen-3**    | Instruction       | 0.733        | 0.934        | 0.260          | 0.163      |
|               | CoT               | 0.732        | 0.933        | 0.263          | 0.193      |
|               | Few-shot          | 0.732        | 0.935        | 0.220          | 0.100      |
|               | ReAct             | 0.724        | 0.933        | 0.245          | 0.125      |
|               | Expert            | 0.729        | 0.933        | 0.248          | 0.122      |
|               | Self-consistency  | 0.748        | 0.933        | 0.223          | 0.095      |
| **GPT-4.1 (FT)** | Instruction    | **0.785**    | **0.940**    | **0.306**      | --         |

---

## Usage

### Requirements

Install dependencies with:

```bash
pip install -r requirements.txt
```

### Generating Code

Scripts for code generation with different LLMs are in `Codes/` (e.g., `openai_gen.py`, `gemini.py`, `qwen_gen.py`). Each script reads prompts, queries the model, and saves generated code.

### Evaluation

- Use `eval.py` and `eval_ft.py` for metric-based evaluation.
- Use `.vscode/run_ns3_batch.sh` and `.vscode/run_ns3_finetune.sh` for execution-based evaluation of generated code.

---

## Metrics Implementation

- **CodeBLEU & RUBY:** Custom implementations using graph edit distance and Python AST, adapted from [typilus](https://github.com/JetBrains-Research/typilus).
- **SacreBLEU:** Modified to support code tokenization.
- See `Codes/metrics_evaluation/metrics/README.md` for details.

---

## Citation

If you use SIMCODE, please cite:

**Paper:** [https://arxiv.org/abs/2507.11014](https://arxiv.org/abs/2507.11014)

**This paper has been accepted for presentation at the $50^{th}$ IEEE Conference on Local Computer Networks (LCN), Special Track on Large Language Models and Networking.**

```bibtex
@misc{ahmed2025simcodebenchmarknaturallanguage,
      title={SIMCODE: A Benchmark for Natural Language to ns-3 Network Simulation Code Generation}, 
      author={Tasnim Ahmed and Mirza Mohammad Azwad and Salimur Choudhury},
      year={2025},
      eprint={2507.11014},
      archivePrefix={arXiv},
      primaryClass={cs.NI},
      url={https://arxiv.org/abs/2507.11014}, 
}
```

---

For more details, see the code and documentation in each subdirectory. 