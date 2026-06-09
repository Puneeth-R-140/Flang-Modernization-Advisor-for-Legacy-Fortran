#include "LegacyFortranAdvisor/FlangASTPatternVisitor.h"
#include <iostream>
#include <regex>
#include <unordered_set>
#include <stack>
#include <algorithm>

#ifdef USE_FLANG_PARSER
#include "flang/Parser/parse-tree.h"
#include "flang/Parser/parse-tree-visitor.h"
#include "flang/Parser/provenance.h"
#include "flang/Parser/source.h"

namespace {

// Helper functions copied from PatternDetector.cpp to keep visitor self-contained
bool findHollerith(const std::string &raw, std::string &outConstruct) {
  std::size_t i = 0;
  while (i < raw.size()) {
    if (std::isdigit(static_cast<unsigned char>(raw[i]))) {
      if (i > 0 && (std::isalnum(static_cast<unsigned char>(raw[i - 1])) || raw[i - 1] == '_')) {
        i++;
        continue;
      }
      std::size_t start = i;
      while (i < raw.size() && std::isdigit(static_cast<unsigned char>(raw[i]))) {
        i++;
      }
      if (i < raw.size() && (raw[i] == 'H' || raw[i] == 'h')) {
        int len = std::stoi(raw.substr(start, i - start));
        if (len > 0 && i + 1 + len <= raw.size()) {
          outConstruct = raw.substr(start, i - start + 1 + len);
          return true;
        }
      }
    }
    i++;
  }
  return false;
}

std::string makeModernHollerith(const std::string &legacy) {
  std::size_t hPos = legacy.find_first_of("Hh");
  if (hPos != std::string::npos) {
    std::string strPart = legacy.substr(hPos + 1);
    return "\"" + strPart + "\"";
  }
  return legacy;
}

std::string makeModernDoLoop(const std::string &rawContent) {
  std::string res = rawContent;
  std::size_t doPos = res.find("do");
  if (doPos != std::string::npos) {
    std::size_t i = doPos + 2;
    while (i < res.size() && std::isspace(static_cast<unsigned char>(res[i]))) {
      i++;
    }
    std::size_t digitStart = i;
    while (i < res.size() && std::isdigit(static_cast<unsigned char>(res[i]))) {
      i++;
    }
    if (i > digitStart) {
      res.erase(digitStart, i - digitStart);
    }
  }
  return res + "\n  ! loop body\nend do";
}

// Sub-visitor to collect Names from any AST node
struct NameCollector {
  std::vector<std::string> names;
  bool Pre(const Fortran::parser::Name &x) {
    names.push_back(x.source.ToString());
    return true;
  }
  template <typename T> bool Pre(const T &) { return true; }
  template <typename T> void Post(const T &) {}
};

struct ASTUnitInfo {
  std::string name;
  int startLine = 0;
  bool hasImplicitNone = false;
};

// Main parse tree visitor helper
struct ASTVisitor {
  const Fortran::parser::AllCookedSources &cooked;
  std::vector<LegacyFortranAdvisor::PatternFinding> &findings;
  
  Fortran::parser::CharBlock currentStmtSource;
  std::vector<ASTUnitInfo> unitStack;

  ASTVisitor(const Fortran::parser::AllCookedSources &c,
             std::vector<LegacyFortranAdvisor::PatternFinding> &f)
      : cooked(c), findings(f) {}

  int GetLineNumber(Fortran::parser::CharBlock cb) {
    if (auto range = cooked.GetSourcePositionRange(cb)) {
      return range->first.line;
    }
    return 0;
  }

  std::string GetLocation(Fortran::parser::CharBlock cb) {
    int line = GetLineNumber(cb);
    return "line " + std::to_string(line);
  }

  template <typename T>
  void PushUnit(const std::string &type, const T &node) {
    ASTUnitInfo unit;
    unit.startLine = GetLineNumber(currentStmtSource);
    
    NameCollector collector;
    Fortran::parser::Walk(node, collector);
    if (!collector.names.empty()) {
      unit.name = type + " " + collector.names[0];
    } else {
      unit.name = type;
    }
    unit.hasImplicitNone = false;
    unitStack.push_back(unit);
  }

  void PopUnit() {
    if (!unitStack.empty()) {
      auto unit = unitStack.back();
      unitStack.pop_back();
      if (!unit.hasImplicitNone) {
        findings.push_back({
          "implicit_typing",
          "line " + std::to_string(unit.startLine),
          "Implicit typing is active in program unit '" + unit.name + "'; add IMPLICIT NONE and declare variables explicitly.",
          {},
          "! implicit typing default",
          "implicit none\n! Explicitly declare all variables, e.g.:\ninteger :: var_name"
        });
      }
    }
  }

  // Intercept every statement's source location
  template <typename T>
  bool Pre(const Fortran::parser::Statement<T> &x) {
    currentStmtSource = x.source;
    
    // Check statement-level text patterns on raw string (like Hollerith, which are resolved at scanner stage)
    std::string rawText = x.source.ToString();
    std::string hollerithConst;
    if (findHollerith(rawText, hollerithConst)) {
      std::string modern = "! Use character literal instead of Hollerith constant\n" + makeModernHollerith(hollerithConst);
      findings.push_back({
        "hollerith_constant",
        GetLocation(x.source),
        "Hollerith constant '" + hollerithConst + "' is an obsolete data representation.",
        {},
        hollerithConst,
        modern
      });
    }
    
    return true;
  }
  template <typename T>
  void Post(const Fortran::parser::Statement<T> &) {}

  // Scopes and Program Units
  bool Pre(const Fortran::parser::MainProgram &x) {
    PushUnit("program", x);
    return true;
  }
  void Post(const Fortran::parser::MainProgram &) {
    PopUnit();
  }

  bool Pre(const Fortran::parser::SubroutineSubprogram &x) {
    PushUnit("subroutine", x);
    return true;
  }
  void Post(const Fortran::parser::SubroutineSubprogram &) {
    PopUnit();
  }

  bool Pre(const Fortran::parser::FunctionSubprogram &x) {
    PushUnit("function", x);
    return true;
  }
  void Post(const Fortran::parser::FunctionSubprogram &) {
    PopUnit();
  }

  bool Pre(const Fortran::parser::BlockData &x) {
    PushUnit("block data", x);
    return true;
  }
  void Post(const Fortran::parser::BlockData &) {
    PopUnit();
  }

  // Obsolete constructs overloads
  bool Pre(const Fortran::parser::ImplicitStmt &x) {
    if (!unitStack.empty()) {
      std::string rawText = currentStmtSource.ToString();
      // Remove spaces and make lowercase for robust matching
      rawText.erase(std::remove_if(rawText.begin(), rawText.end(), ::isspace), rawText.end());
      std::transform(rawText.begin(), rawText.end(), rawText.begin(), ::tolower);
      if (rawText.find("implicitnone") != std::string::npos) {
        unitStack.back().hasImplicitNone = true;
      }
    }
    return true;
  }
  void Post(const Fortran::parser::ImplicitStmt &) {}

  bool Pre(const Fortran::parser::ArithmeticIfStmt &x) {
    std::string rawText = currentStmtSource.ToString();
    
    // Extract expression from matching outer parentheses
    std::string expr = "";
    std::size_t openP = rawText.find('(');
    std::size_t closeP = rawText.rfind(')');
    if (openP != std::string::npos && closeP != std::string::npos && closeP > openP) {
      expr = rawText.substr(openP + 1, closeP - openP - 1);
    }
    
    std::string l1 = std::to_string(std::get<1>(x.t));
    std::string l2 = std::to_string(std::get<2>(x.t));
    std::string l3 = std::to_string(std::get<3>(x.t));
    
    std::string modern = "if (" + expr + " .lt. 0) then\n  goto " + l1 + "\nelse if (" + expr + " .eq. 0) then\n  goto " + l2 + "\nelse\n  goto " + l3 + "\nend if";
    findings.push_back({
      "arithmetic_if",
      GetLocation(currentStmtSource),
      "Arithmetic IF should be rewritten to IF / ELSE IF / ELSE.",
      {},
      rawText,
      modern
    });
    return true;
  }
  void Post(const Fortran::parser::ArithmeticIfStmt &) {}

  bool Pre(const Fortran::parser::ComputedGotoStmt &x) {
    std::string rawText = currentStmtSource.ToString();
    
    // Extract expression from after the closing parenthesis of labels list
    std::string expr = "";
    std::size_t closeP = rawText.rfind(')');
    if (closeP != std::string::npos && closeP + 1 < rawText.size()) {
      expr = rawText.substr(closeP + 1);
      // Strip leading/trailing spaces and commas
      expr.erase(0, expr.find_first_not_of(" \t,"));
      expr.erase(expr.find_last_not_of(" \t,") + 1);
    }
    
    std::vector<std::string> labels;
    for (auto lbl : std::get<std::list<Fortran::parser::Label>>(x.t)) {
      labels.push_back(std::to_string(lbl));
    }
    
    std::string modern = "select case (" + expr + ")\n";
    for (size_t idx = 0; idx < labels.size(); ++idx) {
      modern += "case (" + std::to_string(idx + 1) + ")\n  goto " + labels[idx] + "\n";
    }
    modern += "case default\n  ! optional fallback\nend select";
    
    findings.push_back({
      "computed_goto",
      GetLocation(currentStmtSource),
      "Computed GOTO should be replaced with structured SELECT CASE.",
      {},
      rawText,
      modern
    });
    return true;
  }
  void Post(const Fortran::parser::ComputedGotoStmt &) {}

  bool Pre(const Fortran::parser::CommonStmt &x) {
    std::string rawText = currentStmtSource.ToString();
    
    // Extract all variable names from blocks
    std::vector<std::string> vars;
    for (const auto &block : x.v) {
      for (const auto &cbo : std::get<std::list<Fortran::parser::CommonBlockObject>>(block.t)) {
        vars.push_back(std::get<Fortran::parser::Name>(cbo.t).source.ToString());
      }
    }
    
    std::string modern = "module global_variables_mod\n  implicit none\n  ! Declare common variables here\n";
    for (const auto &var : vars) {
      modern += "  type_name :: " + var + "\n";
    }
    modern += "end module global_variables_mod\n\n! In procedures:\nuse global_variables_mod, only : ";
    for (size_t idx = 0; idx < vars.size(); ++idx) {
      modern += vars[idx] + (idx + 1 < vars.size() ? ", " : "");
    }
    
    findings.push_back({
      "common_block",
      GetLocation(currentStmtSource),
      "COMMON blocks should move to MODULE variables with explicit interfaces.",
      vars,
      rawText,
      modern
    });
    return true;
  }
  void Post(const Fortran::parser::CommonStmt &) {}

  bool Pre(const Fortran::parser::EquivalenceStmt &x) {
    std::string rawText = currentStmtSource.ToString();
    
    NameCollector collector;
    Fortran::parser::Walk(x, collector);
    auto vars = collector.names;
    
    std::string modern = "! Replace equivalence with target/pointer or structure overlay\n";
    if (vars.size() >= 2) {
      modern += "type_name, target :: " + vars[0] + "\n";
      for (size_t idx = 1; idx < vars.size(); ++idx) {
        modern += "type_name, pointer :: " + vars[idx] + "\n" + vars[idx] + " => " + vars[0] + "\n";
      }
    }
    
    findings.push_back({
      "equivalence",
      GetLocation(currentStmtSource),
      "EQUIVALENCE introduces aliasing; migrate to explicit data structures.",
      vars,
      rawText,
      modern
    });
    return true;
  }
  void Post(const Fortran::parser::EquivalenceStmt &) {}

  bool Pre(const Fortran::parser::EntryStmt &x) {
    std::string rawText = currentStmtSource.ToString();
    std::string modern = "! Split procedure with ENTRY into separate procedures\n"
                         "! and call shared internal logic or define generic interfaces.";
    
    findings.push_back({
      "entry_statement",
      GetLocation(currentStmtSource),
      "ENTRY statements should be split into separate procedures.",
      {},
      rawText,
      modern
    });
    return true;
  }
  void Post(const Fortran::parser::EntryStmt &) {}

  bool Pre(const Fortran::parser::StmtFunctionStmt &x) {
    std::string rawText = currentStmtSource.ToString();
    std::string stmtFuncName = std::get<Fortran::parser::Name>(x.t).source.ToString();
    
    // Get arguments list
    std::string args = "";
    bool first = true;
    for (const auto &argName : std::get<std::list<Fortran::parser::Name>>(x.t)) {
      if (!first) args += ", ";
      args += argName.source.ToString();
      first = false;
    }
    
    std::size_t eqPos = rawText.find('=');
    std::string rhs = (eqPos != std::string::npos) ? rawText.substr(eqPos + 1) : "";
    // Strip spaces
    rhs.erase(0, rhs.find_first_not_of(" \t"));
    rhs.erase(rhs.find_last_not_of(" \t") + 1);
    
    std::string modern = "contains\n  pure function " + stmtFuncName + "(" + args + ") result(res)\n    ! Declare argument types\n    implicit none\n    res = " + rhs + "\n  end function " + stmtFuncName;
    
    findings.push_back({
      "statement_function",
      GetLocation(currentStmtSource),
      "Statement function '" + stmtFuncName + "' is an obsolete construct; replace with internal function or expression.",
      {stmtFuncName},
      rawText,
      modern
    });
    return true;
  }
  void Post(const Fortran::parser::StmtFunctionStmt &) {}

  bool Pre(const Fortran::parser::LabelDoStmt &x) {
    std::string rawText = currentStmtSource.ToString();
    std::string modern = makeModernDoLoop(rawText);
    
    findings.push_back({
      "label_do_loop",
      GetLocation(currentStmtSource),
      "Label-terminated DO loop is obsolete; convert to block DO / END DO.",
      {},
      rawText,
      modern
    });
    return true;
  }
  void Post(const Fortran::parser::LabelDoStmt &) {}

  bool Pre(const Fortran::parser::AssignStmt &x) {
    std::string rawText = currentStmtSource.ToString();
    std::string modern = "! Replace ASSIGN with standard integer assignment\n! E.g. labelvar = 10";
    findings.push_back({
      "assign_statement",
      GetLocation(currentStmtSource),
      "ASSIGN statement is obsolete; use integer variable assignment.",
      {},
      rawText,
      modern
    });
    return true;
  }
  void Post(const Fortran::parser::AssignStmt &) {}

  bool Pre(const Fortran::parser::AssignedGotoStmt &x) {
    std::string rawText = currentStmtSource.ToString();
    std::string suffix = std::get<Fortran::parser::Name>(x.t).source.ToString();
    std::string modern = "! Replace assigned GOTO with structured SELECT CASE or IF:\nif (" + suffix + " == 10) then\n  goto 10\nend if";
    
    findings.push_back({
      "assigned_goto",
      GetLocation(currentStmtSource),
      "Assigned GOTO is obsolete; replace with SELECT CASE or IF blocks.",
      {},
      rawText,
      modern
    });
    return true;
  }
  void Post(const Fortran::parser::AssignedGotoStmt &) {}

  bool Pre(const Fortran::parser::PauseStmt &x) {
    std::string rawText = currentStmtSource.ToString();
    std::string modern = "! PAUSE is obsolete; replace with print and read statements\nwrite(*,*) \"PAUSE: Press Enter to continue...\"\nread(*,*)";
    
    findings.push_back({
      "pause_statement",
      GetLocation(currentStmtSource),
      "PAUSE statement is obsolete; replace with READ statement.",
      {},
      rawText,
      modern
    });
    return true;
  }
  void Post(const Fortran::parser::PauseStmt &) {}

  // Fallbacks for generic nodes to continue traversing
  template <typename T> bool Pre(const T &) { return true; }
  template <typename T> void Post(const T &) {}
};

} // namespace

namespace LegacyFortranAdvisor {

FlangASTPatternVisitor::FlangASTPatternVisitor(const Fortran::parser::AllCookedSources &cooked)
    : cooked_(cooked) {}

std::vector<PatternFinding> FlangASTPatternVisitor::analyze(const Fortran::parser::Program &program) {
  std::vector<PatternFinding> findings;
  ASTVisitor visitor(cooked_, findings);
  Fortran::parser::Walk(program, visitor);
  return findings;
}

} // namespace LegacyFortranAdvisor

#endif
