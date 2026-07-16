import os
import re

site_dir = 'site'

def update_sapphire_code(code):
    # Remove explicit types from variables and parameters
    # function name(string a, int b) -> function name(a, b)
    code = re.sub(r'\b(int|bool|string|double|float)\s+([a-zA-Z_]\w*)', r'\2', code)
    # Remove return types: ) void { -> ) {
    code = re.sub(r'\)\s+(void|int|bool|string|double|float)\s*\{', r') {', code)
    # var int x = 5 -> var x = 5
    code = re.sub(r'var\s+(int|bool|string|double|float)\s+([a-zA-Z_]\w*)', r'var \2', code)
    return code

def process_html_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Find all <pre><code class="language-sapphire">...</code></pre> blocks
    def replacer(match):
        prefix = match.group(1)
        code = match.group(2)
        suffix = match.group(3)
        updated_code = update_sapphire_code(code)
        return prefix + updated_code + suffix

    new_content = re.sub(r'(<code class="language-sapphire">)(.*?)(</code>)', replacer, content, flags=re.DOTALL)
    
    if new_content != content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"Updated {filepath}")

for filename in os.listdir(site_dir):
    if filename.endswith('.html'):
        process_html_file(os.path.join(site_dir, filename))

print("Done.")
