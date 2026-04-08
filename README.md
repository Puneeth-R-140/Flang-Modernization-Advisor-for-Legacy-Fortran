# Flang Modernization Advisor for Legacy Fortran

A static analysis tool for detecting legacy Fortran anti-patterns and generating prioritized modernization recommendations.

## Goals
- Detect legacy patterns such as arithmetic IF, computed GOTO, EQUIVALENCE, COMMON, implicit typing, statement functions, fixed-form formatting, assumed-size arrays, and ENTRY statements.
- Analyze semantic impact and risk for modernization.
- Produce a prioritized plan with effort estimates and safety ratings.

## Project Structure
- `CMakeLists.txt` - build configuration
- `src/` - core application sources
- `include/LegacyFortranAdvisor/` - public headers
- `tests/` - legacy Fortran fixtures and planned test cases
- `docs/` - architecture and design notes

## Build
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Run
```bash
./build/Debug/flang-modernization-advisor ./tests/legacy_patterns.f90
```

Optional JSON output:
```bash
./build/Debug/flang-modernization-advisor ./tests/legacy_patterns.f90 --output ./build/plan.json
```

## Current Detection Coverage
- arithmetic IF
- computed GOTO
- EQUIVALENCE
- COMMON blocks
- optional fixed-form file hint (extension-based)

## Current Output
- Sorted modernization plan entries
- Effort labels: low, moderate, high
- Safety labels: safe, review-needed
- Optional JSON report for downstream tooling

## Next Steps
1. Replace heuristic scanning with real Flang parse tree and semantic integration.
2. Expand detection to statement functions, ENTRY, and implicit typing scenarios.
3. Add fixture-based regression tests for each supported pattern.
4. Add cross-file impact analysis for module and call graph dependencies.
