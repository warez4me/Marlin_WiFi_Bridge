import sys, re, os

try: import jsbeautifier
except ImportError: jsbeautifier = None

def raw_fmt(code):
    """Разлепка 'топором' (честная структура)"""
    code = code.replace('{', ' {\n    ').replace('}', '\n}\n').replace(';', ';\n    ')
    code = code.replace('=>', ' => ').replace('&&', ' && ').replace('||', ' || ')
    return re.sub(r'\n\s*\n', '\n', code)

def lib_fmt(code):
    """Интеллектуальный разбор (логика вложений)"""
    if not jsbeautifier: return "Error: jsbeautifier not installed"
    o = jsbeautifier.default_options()
    o.indent_size = 4
    o.keep_array_indentation = True
    return jsbeautifier.beautify(code, o)

def save_html(path, code, mode):
    title = f"Marlin Bridge - {mode} Mode"
    html = f"""<html><head><title>{title}</title><style>
        body {{ background:#1e1e1e; color:#ce9178; font-family:monospace; padding:20px; }}
        pre {{ white-space:pre-wrap; line-height:1.5; border-left:3px solid #555; padding-left:10px; }}
        .m {{ color:#f1c40f; font-weight:bold; margin-bottom:10px; border-bottom:1px solid #333; }}
    </style></head><body><div class='m'>MODE: {mode}</div><pre><code>{code.replace('<','&lt;').replace('>','&gt;')}</code></pre></body></html>"""
    with open(path, 'w', encoding='utf-8') as f: f.write(html)

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 pretty.py <file.in>")
        return

    fin = sys.argv[1]
    try:
        with open(fin, 'r', encoding='utf-8') as f: data = f.read()

        # Распаковка из .h (поддержка zipper.py)
        if fin.endswith('.h'):
            m = re.search(r'="(.+)"', data, re.DOTALL)
            if m: data = m.group(1).replace('\\"', '"').replace('\\n', '\n').replace('\\t', '\t')

        # Генерируем оба варианта
        # Ищем только то, что внутри <script>
        script_match = re.search(r'<script>(.*?)</script>', data, re.DOTALL)
        if script_match:
            js_only = script_match.group(1)
            # Отдаем библиотеке только чистый JS
            save_html("l_pretty.html", lib_fmt(js_only), "LIB (JS-Only)")

        # А для RAW оставляем всё как есть (весь HTML+CSS+JS)
        save_html("r_pretty.html", raw_fmt(data), "RAW (Full Hybrid)")
        ##save_html("r_pretty.html", raw_fmt(data), "RAW (Regex)")
        ##save_html("l_pretty.html", lib_fmt(data), "LIB (Beautifier)")

        print(f"Done! Created r_pretty.html and l_pretty.html from {fin}")
    except Exception as e: print(f"Error: {e}")

if __name__ == "__main__": main()
