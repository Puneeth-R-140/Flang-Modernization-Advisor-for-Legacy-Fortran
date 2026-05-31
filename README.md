# Flang Modernization Advisor for Legacy Fortran

A comprehensive static analysis framework and interactive web dashboard designed to detect obsolete Fortran constructs, validate cross-file structural layouts (like `COMMON` blocks), verify subroutine/module call graphs, and generate prioritized modernization refactoring plans.

## Features
- **Comprehensive Pattern Detection**: Scans and identifies 13 distinct legacy Fortran anti-patterns:
  - Arithmetic IF
  - Computed GOTO
  - EQUIVALENCE statements
  - COMMON blocks
  - Statement Functions
  - ENTRY statements
  - Assumed-size arrays
  - Implicit typing (missing `implicit none`)
  - Hollerith constants
  - ASSIGN and Assigned GOTO statements
  - PAUSE statements
  - Label-terminated DO loops
- **Cross-File Layout Verification**: Validates type, sizing, and alignment uniformity of shared `COMMON` blocks across multiple translation units, flagging structural mismatches.
- **Dependency & Call Graph Validation**: Analyzes module imports (`use`) and external subroutine calls (`call`) across files to flag undefined resources (skipping standard intrinsics like `cpu_time`).
- **Interactive Web Interface**: A glassmorphic web dashboard that supports:
  - Drag-and-drop uploads of multiple Fortran source files or directories.
  - Live syntax-highlighted code viewer.
  - Interactive clickable alerts that highlight line ranges and auto-switch files.
  - Side-by-side legacy vs. modernized code refactoring previews.

---

## Project Structure
- `build.sh` - Automated Release build compiler script
- `run.sh` - Executable wrapper script to run the compiler tool
- `CMakeLists.txt` - CMake build configuration
- `app.py` - Python HTTP server serving the Web API and static frontend
- `src/` - Core static analysis C++ engine
  - `main.cpp` - Entrypoint scanner, directory crawler, and cross-file validator
  - `FlangFrontend.cpp` - Preprocessor normalizer (handles spacing, comments, continuation lines)
  - `PatternDetector.cpp` - Parser/detector rules for all obsolete constructs
  - `ImpactAnalyzer.cpp` - Semantic impact, modern equivalency, and risk parser
  - `Prioritization.cpp` - Multi-criteria scoring engine
- `include/LegacyFortranAdvisor/` - C++ Header files defining API data structures
- `web/` - Web dashboard assets
  - `index.html` - Premium glassmorphic interactive frontend UI
- `tests/` - Obsolete Fortran code fixtures
  - `legacy_patterns.f90` - Comprehensive obsolete patterns test case
  - `mismatch_patterns.f90` - Mismatched `COMMON` layout test case
- `DESIGN.md` - Technical design choices, preprocessor pipeline, and alternative analyses
- `IMPLEMENTATION.md` - Lexical preprocessor, parser details, and validation routine implementation
- `EVALUATION.md` - Performance metrics, baseline accuracy, and test suite evaluation

---

## Getting Started

### Prerequisites
- CMake 3.22 or higher
- C++20 Compiler (MSVC on Windows, GCC/Clang on Unix)
- Python 3.x (only required for running the web dashboard)
- LLVM / Clang (the engine links against LLVM components to prepare for deep parse-tree analysis)

### Build
To compile the C++ analyzer in `Release` configuration, execute the build script:
```bash
./build.sh
```
This compiles the executable and outputs it to `build/Release/flang-modernization-advisor` (or `.exe` on Windows).

### Run (CLI)
You can analyze a single file or an entire directory of Fortran source files by executing `run.sh`:
```bash
# Analyze a folder of Fortran files
./run.sh tests/

# Save analysis to a JSON report for downstream integration
./run.sh tests/ --output build/plan.json
```

### Run (Web Dashboard)
To launch the interactive dashboard, run the local Python server:
```bash
python app.py
```
Then, open your web browser and navigate to:
```
http://localhost:8000
```
On the dashboard:
1. Click **Load Multi-File Mismatch Example** to immediately populate the analyzer with the multi-file test fixture demonstrating cross-file mismatches.
2. Drag and drop your own Fortran files or folders to view modernization plans.
