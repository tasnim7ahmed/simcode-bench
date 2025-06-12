#!/usr/bin/env bash
# ------------------------------------------------------------------
#  run_ns3_batch.sh — Evaluate each generated .cc file and
#                     compute accuracy per <prompt>/<model>.
# ------------------------------------------------------------------
#  Args
#     $1  GEN_ROOT   (Output folder that contains Basic/, CoT/, …)
#     $2  OUT_DIR    (where run logs will be written)
# ------------------------------------------------------------------

set -uo pipefail          # NO “-e”: we handle all errors manually
shopt -s nullglob         # empty globs expand to nothing

# MAX_PER_BUCKET=5
NS3ROOT="$HOME/ns-allinone-3.41/ns-3.41"   # change if different
GEN_ROOT="$1"
OUT_DIR="$2"

summary_file="$OUT_DIR/summary_qwen.txt"          # summary table lives here
success_list="$OUT_DIR/success_paths_qwen.txt"    # NEW: list of source files that ran OK

: > "$summary_file"      # start both files empty for every batch run
: > "$success_list"

prompt_types=(Basic CoT Expert FewShot ReAct SelfConsistency)

declare -A TOTAL
declare -A SUCCESS

mkdir -p "$OUT_DIR"
cd "$NS3ROOT" || { echo "NS3ROOT not found: $NS3ROOT"; exit 1; }

echo "🚀  Running benchmarks …"

for prompt in "${prompt_types[@]}"; do
  pr_dir="$GEN_ROOT/$prompt"
  [[ -d $pr_dir ]] || continue

  for model_dir in "$pr_dir"/*/; do           # e.g. Output/Basic/Openai/
    [[ -d $model_dir ]] || continue
    model=$(basename "$model_dir")
    key="$prompt/$model"

    TOTAL[$key]=0
    SUCCESS[$key]=0

    for src in "$model_dir"/*.cc; do
      [[ -e $src ]] || continue
      (( TOTAL[$key]++ ))

      base=$(basename "$src" .cc)
      tmp="scratch/auto_${prompt}_${model}_${base}.cc"
      log_dir="$OUT_DIR/$prompt/$model"
      mkdir -p "$log_dir"
      log="$log_dir/${base}.txt"

      # clean any temp copies from previous loop
      rm -f scratch/auto_* scratch/[0-9]*.cc scratch/[0-9]*.h 2>/dev/null

      cp "$src" "$tmp"

      # -------- configure ----------
      if ! timeout 30s ./ns3 configure &>>"$log"; then
        echo "✗  configure failed or timed out ($key/$base)" | tee -a "$log"
        rm -f "$tmp"
        continue
      fi

      # -------- build --------------
      if ! timeout 30s ./ns3 build "scratch/$(basename "$tmp" .cc)" &>>"$log"; then
        echo "✗  build failed or timed out ($key/$base)" | tee -a "$log"
        rm -f "$tmp"
        continue
      fi

      # -------- run ----------------
      timeout 30s ./ns3 run "scratch/$(basename "$tmp" .cc)" |& tee -a "$log"
      exit_code=${PIPESTATUS[0]}
      if [[ $exit_code -eq 124 ]]; then
        echo "⚠️  runtime timed out ($key/$base)" | tee -a "$log"
      elif [[ $exit_code -eq 0 ]]; then
        (( SUCCESS[$key]++ ))
        echo "$src" >> "$success_list"
      else
        echo "✗  runtime failed ($key/$base) [exit:$exit_code]" | tee -a "$log"
      fi

      rm -f "$tmp"
    done
  done
  done

{
  echo
  printf '┌%-24s┬%12s┬%10s┐\n' ' Prompt / Model' 'Passed/Total' 'Accuracy'
  for key in "${!TOTAL[@]}"; do
    t=${TOTAL[$key]}
    s=${SUCCESS[$key]}
    acc=$(awk "BEGIN{printf \"%.2f\", (t==0)?0:100*s/t}")
    printf '│ %-23s│  %3d / %3d │  %6s%% │\n' "$key" "$s" "$t" "$acc"
  done
  printf '└──────────────────────────────┴────────────┴──────────┘\n'
} | tee -a "$summary_file"
