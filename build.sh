#!/bin/bash

# Build script for SSA based optimizer (created using AI)
# This script compiles the optimizer using the Makefile

set -e  # Exit on error

# Clean any previous builds
make clean 2>/dev/null || true

# Compile the optimizer
make