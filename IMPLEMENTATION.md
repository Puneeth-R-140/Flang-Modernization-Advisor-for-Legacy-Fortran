# IMPLEMENTATION: Parser, Lexer, AST Visitor and Validator Details

This document explains the internal implementation details of the Flang Modernization Advisor, including lexical normalization, the Flang AST parse-tree visitor, pattern matching algorithms, cross-file structural validation, and prioritization scoring.

---

## 1. Lexical Preprocessor and Statement Normalization

The preprocessing stage in `FlangFrontend.cpp` handles the syntactical normalization of fixed-form and free-form Fortran source styles into logical statements.

### Fixed-Form Normalization
For files identified as fixed-form (e.g., `.f`, `.for`, `.f77` extensions):
1. **Comment Stripping**: Line comments are identified by a character (`C`, `c`, `*`, `!`) in column 1 and ignored. Inline comments starting with `!` are stripped from columns 7-72.
2. **Line Continuation**:
   - Column 6 is checked for continuation indicators (any character other than space or `'0'`).
   - If a continuation is found, the character content from column 7 onwards is appended directly to the preceding statement.
   - If no continuation is present, the current statement is finalized, and a new one is initialized.
3. **Space Stripping**: The preprocessor creates a second representation (`content`) by removing all inline whitespaces and converting the text to lowercase. This provides a unified text segment suitable for simple token comparison (e.g. `implicitnone` or `common/block/`).

### Free-Form Normalization
For files identified as free-form (e.g., `.f90` extension):
1. **Comment Stripping**: Any characters following an unquoted exclamation mark `!` are stripped.
2. **Line Continuation**:
   - If the statement terminates with an ampersand (`&`), the trailing ampersand is removed, and the subsequent non-comment line is appended.
   - If a continuation line begins with a leading ampersand `&` (standard but optional), it is also removed prior to appending, preventing double character insertion.

---

## 2. LLVM Flang AST Parse-Tree Visitor

When `-DUSE_FLANG_PARSER` is defined (WSL/Linux with LLVM Flang 22 dev packages), `FlangFrontend.cpp` invokes `Fortran::parser::Parsing` to execute the compiler's native `Prescan` and `Parse` stages. The resulting `Fortran::parser::Program` parse tree and its `AllCookedSources` provenance map are stored in `SourceAnalysis::parsing` as a `FlangParserHolder`.

### FlangASTPatternVisitor (`src/FlangASTPatternVisitor.cpp`)

`PatternDetector::detectPatterns` retrieves the stored `FlangParserHolder` and instantiates `FlangASTPatternVisitor`. The visitor implements an `ASTVisitor` struct whose `Pre`/`Post` method overloads are dispatched by `Fortran::parser::Walk(program, visitor)`:

```
Fortran::parser::Walk(program, visitor)
    |
    +-- Pre(Statement<T>)       -- captures currentStmtSource CharBlock for every statement
    +-- Pre(ArithmeticIfStmt)   -- Arithmetic IF
    +-- Pre(ComputedGotoStmt)   -- Computed GOTO
    +-- Pre(CommonStmt)         -- COMMON block (extracts variable names from AST)
    +-- Pre(EquivalenceStmt)    -- EQUIVALENCE (extracts names via NameCollector walk)
    +-- Pre(EntryStmt)          -- ENTRY statement
    +-- Pre(StmtFunctionStmt)   -- Statement function (extracts name and argument list)
    +-- Pre(LabelDoStmt)        -- Label-terminated DO loop
    +-- Pre(AssignStmt)         -- ASSIGN statement
    +-- Pre(AssignedGotoStmt)   -- Assigned GOTO
    +-- Pre(PauseStmt)          -- PAUSE statement
    +-- Pre(ImplicitStmt)       -- tracks implicit none per program unit scope
    +-- Pre(MainProgram)        -- pushes scope onto unit stack
    +-- Pre(SubroutineSubprogram) -- pushes scope
    +-- Pre(FunctionSubprogram)   -- pushes scope
    +-- Pre(BlockData)            -- pushes scope
    +-- Post(MainProgram etc.)    -- pops scope, emits implicit typing warning if needed
```

### Source Location Resolution
Each `Statement<T>` node carries a `CharBlock source` field. When `Pre(Statement<T>)` fires, the visitor stores this as `currentStmtSource`. Pattern handlers call `cooked.GetSourcePositionRange(currentStmtSource)` to resolve exact line numbers from provenance, which are embedded into `Finding::summary` as `(line N)`.

### Hollerith Detection at Statement Level
Hollerith constants are resolved at the Flang scanner stage (before the parse tree is built), so they do not appear as typed AST nodes. The visitor detects them by scanning the raw `Statement<T>::source.ToString()` text in the generic `Pre(Statement<T>)` overload using a digit-then-H pattern matcher.

### Fallback Behavior
If `Prescan` or `Parse` reports errors (typical for legacy snippets with missing modules), `FlangFrontend` catches the failure and falls back gracefully to the custom lexical preprocessor, ensuring pattern scanning is never interrupted.

---

## 3. Text-Based Pattern Matching (Fallback and Windows)

Obsolete constructs are detected using specialized parser rules in `PatternDetector.cpp`:

### Obsolete Control Flow and Formatting
- **Arithmetic IF**: Matches standard pattern shape `if(<expr>)label1,label2,label3` using regular expressions. The values are extracted, and a structured `if ... then / else if / else` replacement is templated.
- **Computed GOTO**: Matches `goto(lbl1,lbl2...),expr` using `std::regex`. The labels are parsed into a vector and mapped to cases in a modern `select case` structure.
- **ASSIGN and Assigned GOTO**: Detects the assignment of code labels to integer variables (e.g. `assign 10 to labelvar`) and indirect jumps (e.g. `goto labelvar`).
- **Label DO Loops**: Detects DO loops utilizing statement termination labels (e.g. `do 50 i = 1, 10`).

### Obsolete Storage and Declarations
- **EQUIVALENCE**: Detects memory aliasing constructs `equivalence(var1, var2)`. Refactoring suggestions propose replacing the construct with standard `pointer` targets or overlay structures.
- **Statement Functions**: Detects non-executable inline definitions occurring in declaration blocks (e.g. `f(x) = x * 2.0`). Checks against active array names to prevent false-positives.
- **Assumed-Size Arrays**: Identifies array parameters declared with a trailing asterisk bounds (`*`), proposing conversion to modern assumed-shape arrays (`arr(:)`).
- **Implicit Typing**: Tracks the declaration list of subroutines, modules, and program units. If an `implicit none` declaration is missing, a warning is raised.

---

## 4. Cross-File Validation Routines

To evaluate code structure beyond single files, `main.cpp` aggregates metadata from all analyzed sources.

### COMMON Block Alignment Validation
1. **Variable Sizing and Resolution**: Each identifier in a `COMMON` block is parsed. The code scans the declaration history in the parent file to resolve its type and array size.
2. **Layout Compilation**: The compiler builds a `CommonBlockLayout` containing the sequence of variables, their type names, and total sizes.
3. **Equivalence Comparison**: When the same `COMMON` block name is declared across multiple files, the engine performs a structural pairwise match:
   - Verifies variable count.
   - Matches types at each index position.
   - Matches array dimensions.
4. **Output Generation**: Mismatches trigger a high-priority `GLOBAL` warning showing exact file line references and generating a synchronized template `MODULE` to declare variables cleanly.

### Module and Call Graph Verification
- **Module Declarations**: Collects all module names defined in the parsed codebase. If a file specifies `use <mod_name>` and it cannot be found, a warning is generated.
- **Subroutine Call Graph**: Maps declared subroutines and functions. Scans all `call <sub_name>` statements. If a call is made to an undefined subroutine (which is not in the list of standard compiler intrinsics like `cpu_time`), it is flagged.
