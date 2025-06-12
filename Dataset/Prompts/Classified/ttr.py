import os
import re

def compute_ttr(text):
    words = re.findall(r'\b\w+\b', text.lower())
    if not words:
        return 0.0
    return len(set(words)) / len(words)

def find_txt_files(directory):
    txt_files = []
    for root, _, files in os.walk(directory):
        for file in files:
            if file.lower().endswith('.txt'):
                txt_files.append(os.path.join(root, file))
    return txt_files

def classify_ttr(ttr_value):
    if ttr_value >= 0.50:
        return "High lexical diversity"
    elif ttr_value >= 0.30:
        return "Moderate lexical diversity"
    else:
        return "Low lexical diversity"

def main(directory):
    txt_files = find_txt_files(directory)
    if not txt_files:
        print("No .txt files found in the specified directory.")
        return

    print(f"{'File':<60} {'TTR':>10}")
    print("-" * 70)
    total_ttr = 0
    file_count = 0
    for file_path in txt_files:
        try:
            with open(file_path, 'r', encoding='utf-8') as file:
                text = file.read()
            ttr = compute_ttr(text)
            total_ttr += ttr
            file_count += 1
            print(f"{file_path:<60} {ttr:>10.4f}")
        except Exception as e:
            print(f"Error processing {file_path}: {e}")

    if file_count == 0:
        print("No valid text files to process.")
        return

    average_ttr = total_ttr / file_count
    classification = classify_ttr(average_ttr)
    print(f"\nAverage TTR: {average_ttr:.4f} — {classification}")

if __name__ == "__main__":
    directory_to_search = input("Enter the directory path to search: ")
    main(directory_to_search)
