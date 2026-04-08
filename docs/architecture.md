# Architecture Overview

## Components

- `FlangFrontend`
  - Responsible for parsing Fortran source files and building the semantic model.
  - Will integrate with Flang internal APIs or the Flang frontend library.

- `PatternDetector`
  - Traverses parse trees and semantic data to find legacy Fortran constructs.
  - Initial targets: arithmetic IF, computed GOTO, EQUIVALENCE, COMMON, implicit typing, statement functions, fixed-form formatting, assumed-size arrays, ENTRY.

- `ImpactAnalyzer`
  - Computes the effect of each modernization candidate on files, symbols, aliases, and behavior.
  - Classifies semantic risk categories.

- `Prioritization`
  - Generates ordered modernization recommendations.
  - Assigns `trivial`, `moderate`, or `complex` effort levels and `safe`, `review-needed`, or `risky` safety ratings.

## Data Flow

1. Parse source files with `FlangFrontend`.
2. Detect legacy patterns via `PatternDetector`.
3. Analyze semantic impact with `ImpactAnalyzer`.
4. Generate a modernization plan with `Prioritization`.
