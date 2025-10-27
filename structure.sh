#!/bin/bash

# Get list of directories in external/ and convert to pipe-separated pattern
EXTERNAL_DIRS=$(ls external 2>/dev/null | tr '\n' '|' | sed 's/|$//')

# Get list of directories in build/ and convert to pipe-separated pattern
BUILD_DIRS=$(ls build 2>/dev/null | tr '\n' '|' | sed 's/|$//')

# Combine patterns with | (OR) operator for tree's -I flag
IGNORE_PATTERN="${EXTERNAL_DIRS}|${BUILD_DIRS}"

# Display project structure, excluding external and build directories
tree -I "${IGNORE_PATTERN}"
