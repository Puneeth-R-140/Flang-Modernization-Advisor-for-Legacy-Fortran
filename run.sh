#!/bin/bash

# Find the directory of the script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Possible binary paths relative to workspace root
BIN_PATHS=(
  "build/Release/flang-modernization-advisor.exe"
  "build/Debug/flang-modernization-advisor.exe"
  "build/flang-modernization-advisor.exe"
  "build_wsl/flang-modernization-advisor"
  "build/Release/flang-modernization-advisor"
  "build/Debug/flang-modernization-advisor"
  "build/flang-modernization-advisor"
)

ADVISOR_EXE=""

for path in "${BIN_PATHS[@]}"; do
  full_path="$SCRIPT_DIR/$path"
  if [ -f "$full_path" ]; then
    ADVISOR_EXE="$full_path"
    break
  fi
done

if [ -z "$ADVISOR_EXE" ]; then
  echo "Error: flang-modernization-advisor executable not found."
  echo "Please build the project first by running ./build.sh"
  exit 1
fi

# Run the executable, passing all incoming arguments
"$ADVISOR_EXE" "$@"
