#!/usr/bin/env bash
# ------------------------------------------------------------------
#  .vscode/run_ns3_finetune.sh — Run all generated NS-3 .cc files,
#                              log outputs, and compute overall accuracy.
# ------------------------------------------------------------------
#  Hardcoded paths (from project root):
#    Generated CCs: Codes/Finetuning/Output/Finetuned/generated
#    Logs & summary: Codes/Finetuning/NS3_logs
# ------------------------------------------------------------------

set -uo pipefail          # NO “-e”: handle errors manually
shopt -s nullglob         # empty globs expand to nothing

# Locate project root (one level up from this script)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Paths
NS3ROOT="$HOME/ns-allinone-3.41/ns-3.41"
SRC_DIR="$PROJECT_ROOT/Codes/Finetuning/Output/Finetuned/generated"
OUT_DIR="$PROJECT_ROOT/Codes/Finetuning/NS3_logs"

# Summary and success list files
summary_file="$OUT_DIR/summary.txt"
success_list="$OUT_DIR/success_list.txt"

# Initialize output directory and logs
mkdir -p "$OUT_DIR"
: > "$summary_file"
: > "$success_list"

TOTAL=0
SUCCESS=0

# Enter ns-3 root
cd "$NS3ROOT" || { echo "NS3ROOT not found: $NS3ROOT"; exit 1; }

echo "🚀  Running ns3 benchmarks on generated folder…"

# Iterate over every .cc in generated directory
for src in "$SRC_DIR"/*.cc; do
  [[ -e $src ]] || continue
  (( TOTAL++ ))
  base=$(basename "$src" .cc)
  tmp="scratch/${base}.cc"
  log="$OUT_DIR/${base}.txt"

  # Clean up previous scratch files
  rm -f "$tmp" scratch/[0-9]*.cc scratch/[0-9]*.h 2>/dev/null

  # Copy source into scratch
  cp "$src" "$tmp"

  # Configure ns-3
  if ! ./ns3 configure &>>"$log"; then
    echo "✗ configure failed (${base})" | tee -a "$log"
    rm -f "$tmp"
    continue
  fi

  # Build example
  if ! ./ns3 build "scratch/${base}" &>>"$log"; then
    echo "✗ build failed (${base})" | tee -a "$log"
    rm -f "$tmp"
    continue
  fi

  # Run example\   
  ./ns3 run "scratch/${base}" |& tee -a "$log"
  if [[ ${PIPESTATUS[0]} -eq 0 ]]; then
    (( SUCCESS++ ))
    echo "$src" >> "$success_list"
  else
    echo "✗ runtime failed (${base})" | tee -a "$log"
  fi

  # Remove temporary source
  rm -f "$tmp"
done

# Compute pass rate
acc=$(awk "BEGIN{printf \"%.2f\", (TOTAL==0)?0:100*SUCCESS/TOTAL}")

# Print summary table
{
  echo
  printf '┌%-24s┬%12s┬%10s┐\n' ' File Set' 'Passed/Total' 'Accuracy'
  printf '│ %-23s│ %3d / %3d │ %6s%% │\n' "Generated" "$SUCCESS" "$TOTAL" "$acc"
  printf '└──────────────────────────────┴────────────┴──────────┘\n'
} | tee -a "$summary_file"
