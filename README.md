# Flang Modernization Advisor for Legacy Fortran

A comprehensive static analysis framework and interactive web dashboard designed to detect obsolete Fortran constructs, validate cross-file structural layouts (like `COMMON` blocks), verify subroutine/module call graphs, and generate prioritized modernization refactoring plans.

## Features
- **Dual-Path Detection Engine**: Detects 13 distinct legacy Fortran anti-patterns through two complementary detection strategies:
  - **AST Visitor Layer** (`FlangASTPatternVisitor`): Traverses Flang parse-tree nodes directly via `Fortran::parser::Walk`, used in WSL/Linux builds where LLVM Flang libraries are available (`-DUSE_FLANG_PARSER`).
  - **Text Preprocessor Layer** (`PatternDetector`): Normalized statement-level matching that runs as a fallback on Windows and as a supplemental pass on all platforms.
- **Patterns Detected** (all 13):
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
- **Dependency and Call Graph Validation**: Analyzes module imports (`use`) and external subroutine calls (`call`) across files to flag undefined resources (skipping standard intrinsics like `cpu_time`).
- **Interactive Web Interface**: A glassmorphic web dashboard that supports:
  - Drag-and-drop uploads of multiple Fortran source files or directories.
  - Live syntax-highlighted code viewer.
  - Interactive clickable alerts that highlight line ranges and auto-switch files.
  - Side-by-side legacy vs. modernized code refactoring previews.

---

## Project Structure
- `build.sh` - Automated Release build script (auto-detects WSL/Linux vs. Windows)
- `run.sh` - Executable wrapper script to run the compiled analyzer
- `CMakeLists.txt` - CMake build configuration
- `app.py` - Python HTTP server serving the Web API and static frontend
- `src/` - Core static analysis C++ engine
  - `main.cpp` - Entrypoint scanner, directory crawler, and cross-file validator
  - `FlangFrontend.cpp` - Preprocessor normalizer and Flang compiler parser driver
  - `PatternDetector.cpp` - Text-based detector rules for all obsolete constructs (fallback)
  - `FlangASTPatternVisitor.cpp` - AST parse-tree visitor using `Fortran::parser::Walk` (WSL/Linux, `USE_FLANG_PARSER`)
  - `ImpactAnalyzer.cpp` - Semantic impact, modern equivalency, and risk classifier
  - `Prioritization.cpp` - Multi-criteria scoring engine
- `include/LegacyFortranAdvisor/` - C++ header files defining API data structures
  - `FlangASTPatternVisitor.h` - Visitor class interface
- `web/` - Web dashboard assets
  - `index.html` - Premium glassmorphic interactive frontend UI
- `tests/` - Obsolete Fortran code fixtures
  - `legacy_patterns.f90` - Comprehensive obsolete patterns test case
  - `mismatch_patterns.f90` - Mismatched `COMMON` layout test case
  - `enterprise_simulation.f90` - Enterprise simulation legacy test case
  - `modernized_simulation.f90` - Modernized variant implementing 4 safe transformations
- `DESIGN.md` - Technical design choices, dual-path architecture, and alternative analyses
- `IMPLEMENTATION.md` - Lexical preprocessor, AST visitor, and validation routine implementation
- `EVALUATION.md` - Performance metrics, baseline accuracy, and test suite evaluation
- `CASE_STUDY.md` - Case study applying advisor recommendations to the simulation codebase

---

## Getting Started

### Prerequisites
- CMake 3.22 or higher
- C++20 Compiler (MSVC on Windows, GCC/Clang on Unix)
- Python 3.x (only required for running the web dashboard)
- **For Windows**: LLVM/Clang binaries installed (e.g. clang+llvm-22.1.1-x86_64-pc-windows-msvc)
- **For WSL / Linux**: Install LLVM, Flang, and MLIR development libraries:
  ```bash
  sudo apt-get install llvm-22-dev libflang-22-dev libmlir-22-dev mlir-22-tools
  ```

### Build
To compile the C++ analyzer in `Release` configuration, execute the build script:

- **On Windows Native Terminal (PowerShell/CMD)**:
  ```powershell
  ./build.sh
  ```
  Compiles into `build/Release/flang-modernization-advisor.exe` using standard lexical analysis rules (text-based detector only).

- **From Windows Host Terminal using WSL**:
  ```powershell
  wsl ./build.sh
  ```

- **On Linux / Inside WSL Terminal**:
  ```bash
  ./build.sh
  ```
  Automatically detects LLVM/Flang libraries and compiles into `build_wsl/flang-modernization-advisor` with `-DUSE_FLANG_PARSER` enabled, activating the native AST parse-tree visitor layer.

### Run (CLI)
You can analyze a single file or an entire directory of Fortran source files using the `run.sh` script:

- **From Windows Host Terminal (PowerShell/CMD)**:
  ```powershell
  # Run the native Windows binary
  ./run.sh tests/

  # Or run the WSL binary from the Windows host
  wsl ./run.sh tests/
  ```

- **From Linux / Inside WSL Terminal**:
  ```bash
  # Run the compiled Linux binary directly
  ./run.sh tests/

  # Save analysis to a JSON report
  ./run.sh tests/ --output build_wsl/plan.json
  ```

### Run (Web Dashboard)
To launch the interactive dashboard, run the local Python server:

- **From Windows Host Terminal**:
  ```powershell
  python app.py
  ```

- **From Linux / Inside WSL Terminal**:
  ```bash
  python3 app.py
  ```

Then, open your web browser and navigate to:
```
http://localhost:8000
```
On the dashboard:
1. Click **Load Multi-File Mismatch Example** to immediately populate the analyzer with the multi-file test fixture demonstrating cross-file mismatches.
2. Drag and drop your own Fortran files or folders to view modernization plans.
