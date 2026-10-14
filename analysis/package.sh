#!/usr/bin/env bash
set -e
ROOT=/home/izen/Desktop/pdc_assignment
SUB=$ROOT/23L-0748_Assignment2
"$ROOT/analysis/build_pdf.sh"
cp "$ROOT/analysis/report.pdf" "$SUB/23L-0748_Report.pdf"
(cd "$SUB/part1to4_synchronous" && make clean)
(cd "$SUB/part5_async_deps" && make clean)
rm -f "$ROOT/23L-0748_Assignment2.zip"
(cd "$ROOT" && zip -qr -X 23L-0748_Assignment2.zip 23L-0748_Assignment2)
unzip -l "$ROOT/23L-0748_Assignment2.zip"
