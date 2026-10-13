#!/usr/bin/env bash
# Inline generated tables / test logs into report.html, then print it to PDF
# with headless Chrome.
set -e
cd "$(dirname "$0")"
python3 - <<'EOF'
import html, re
from pathlib import Path
src = Path("report.html").read_text()
src = re.sub(r"<!--INCLUDE:(\S+)-->", lambda m: Path(m.group(1)).read_text(), src)
src = re.sub(r"<!--INCLUDE-PRE:(\S+)-->",
             lambda m: "<pre class='log'>" + html.escape(Path(m.group(1)).read_text().rstrip()) + "</pre>", src)
Path("report_built.html").write_text(src)
EOF
google-chrome --headless=new --disable-gpu --no-pdf-header-footer \
    --print-to-pdf="$PWD/report.pdf" "file://$PWD/report_built.html" 2>/dev/null
echo "wrote $PWD/report.pdf"
