#!/bin/bash

# Run script for SSA based optimizer (created using AI)
# Usage: ./run.sh <input_file>
# Example: ./run.sh input/fib.il

set -e  # Exit on error

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <input_file>"
    echo "Example: $0 input/fib.il"
    exit 1
fi

INPUT_FILE="$1"

# Extract filename without path and extension
BASENAME=$(basename "$INPUT_FILE" .il)

# Define paths
OUTPUT_FILE="output/${BASENAME}.dbre.il"
OUTPUT_DIR="output"
EXPECTED_OUTPUT="input/${BASENAME}.out"
ACTUAL_OUTPUT="output/${BASENAME}.out"

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