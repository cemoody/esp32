#!/bin/bash
# Generates laura_prompt.h from laura.md
# Run before compiling: ./generate_prompt.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INPUT="$SCRIPT_DIR/laura.md"
OUTPUT="$SCRIPT_DIR/laura_prompt.h"

if [ ! -f "$INPUT" ]; then
  echo "Error: $INPUT not found"
  exit 1
fi

cat > "$OUTPUT" << 'HEADER'
#pragma once
// Auto-generated from laura.md — do not edit manually
// Regenerate with: ./generate_prompt.sh

static const char LAURA_PROMPT[] = R"PROMPT(
HEADER

cat "$INPUT" >> "$OUTPUT"

cat >> "$OUTPUT" << 'FOOTER'
)PROMPT";
FOOTER

echo "Generated $OUTPUT from $INPUT"
