total_lines=$(find . -type f -name "*.cc" -print0 | xargs -0 cat | wc -l)
file_count=$(find . -type f -name "*.cc" | wc -l)
if [ "$file_count" -gt 0 ]; then
  echo "scale=2; $total_lines / $file_count" | bc
  echo "$file_count" files
  echo "$total_lines" lines
else
  echo "No .cc files found."
fi

