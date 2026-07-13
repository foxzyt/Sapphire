import sys

with open('sapphire_grad.sp', 'r', encoding='utf-8') as f:
    grad = f.read()

with open('test_nlp.sp', 'r', encoding='utf-8') as f:
    nlp = f.read()

# Remove import
nlp = nlp.replace('import "sapphire_grad.sp";', '')

with open('run_nlp_clean_py.sp', 'w', encoding='utf-8', newline='') as f:
    f.write(grad)
    f.write("\n")
    f.write(nlp)
