#!/bin/bash

# Run script for SSA based optimizer
# Usage: ./run.sh <input_file>
# Example: ./run.sh input/fib.lvn.il

set -e  # Exit on error

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <input_file>"
    echo "Example: $0 input/fib.lvn.il"
    exit 1
fi

INPUT_FILE="$1"

# Extract filename without path and .lvn.il extension
FILENAME=$(basename "$INPUT_FILE")
BASENAME="${FILENAME%.lvn.il}"

# Validate extension
if [ "$FILENAME" = "$BASENAME" ]; then
    echo "Error: input file must have .lvn.il extension"
    exit 1
fi

# Define paths
OUTPUT_DIR="output"
OUTPUT_FILE="${OUTPUT_DIR}/${BASENAME}.dbre.il"
EXPECTED_OUTPUT="input/${BASENAME}.out"
ACTUAL_OUTPUT="${OUTPUT_DIR}/${BASENAME}.out"

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

./ssa_optimizer "$INPUT_FILE"

./iloc -s "$OUTPUT_FILE" > "$ACTUAL_OUTPUT"

# Remove all blank lines
sed -i '/^[[:space:]]*$/d' "$ACTUAL_OUTPUT"

diff -u --strip-trailing-cr "$EXPECTED_OUTPUT" "$ACTUAL_OUTPUT" || {
    echo "Output differs from expected output. See diff above."
    exit 1
}