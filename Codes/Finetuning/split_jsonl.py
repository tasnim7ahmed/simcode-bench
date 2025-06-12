import json, random, collections
from pathlib import Path

random.seed(42)
FULL = Path("simcode_full.jsonl")

def load():
    with FULL.open(encoding="utf-8") as f:
        for line in f:
            rec = json.loads(line)
            yield rec["size"], rec
def stratified_split(items):
    buckets = collections.defaultdict(list)
    for size, rec in items:
        buckets[size].append(rec)

    train, val, test = [], [], []
    for subset in buckets.values():
        random.shuffle(subset)
        n = len(subset)
        train.extend(subset[: int(0.60*n)])
        val.extend  (subset[int(0.60*n): int(0.70*n)])
        test.extend (subset[int(0.70*n):            ])
    random.shuffle(train); random.shuffle(val); random.shuffle(test)
    return train, val, test

def dump(name, seq):
    path = Path(f"simcode_{name}.jsonl")
    with path.open("w", encoding="utf-8") as f:
        for r in seq:
            json.dump({"messages": r["messages"]},  # << keep only messages
                      f, ensure_ascii=False)
            f.write("\n")
    print(f"{name:5} → {len(seq)} lines   ({path})")

if __name__ == "__main__":
    train, val, test = stratified_split(list(load()))
    dump("train", train)
    dump("val",   val)
    dump("eval",  test)
