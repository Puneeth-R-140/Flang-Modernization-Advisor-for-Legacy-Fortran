#include "LegacyFortranAdvisor/PatternDetector.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>
#include <unordered_set>

namespace LegacyFortranAdvisor {

namespace {

bool startsWith(const std::string &str, const std::string &prefix) {
  return str.rfind(prefix, 0) == 0;
}

bool isUnitStart(const std::string &content) {
  if (startsWith(content, "program")) return true;
  if (startsWith(content, "subroutine")) return true;
  if (startsWith(content, "module") && !startsWith(content, "moduleprocedure")) return true;
  if (startsWith(content, "blockdata")) return true;
  
  if (content.find("function") != std::string::npos) {
    if (startsWith(content, "function")) return true;
    if (startsWith(content, "integerfunction")) return true;
    if (startsWith(content, "realfunction")) return true;
    if (startsWith(content, "doubleprecisionfunction")) return true;
    if (startsWith(content, "logicalfunction")) return true;
    if (startsWith(content, "characterfunction")) return true;
    if (startsWith(content, "complexfunction")) return true;
  }
  return false;
}

bool isUnitEnd(const std::string &content) {
  if (content == "end") return true;
  if (content == "endprogram" || startsWith(content, "endprogram")) return true;
  if (content == "endsubroutine" || startsWith(content, "endsubroutine")) return true;
  if (content == "endfunction" || startsWith(content, "endfunction")) return true;
  if (content == "endmodule" || startsWith(content, "endmodule")) return true;
  if (content == "endblockdata" || startsWith(content, "endblockdata")) return true;
  return false;
}

void parseArithmeticIf(const std::string &content, std::string &expr, std::string &l1, std::string &l2, std::string &l3) {
  std::size_t comma2 = content.rfind(',');
  if (comma2 == std::string::npos) return;
  std::size_t comma1 = content.rfind(',', comma2 - 1);
  if (comma1 == std::string::npos) return;
  
  l3 = content.substr(comma2 + 1);
  l2 = content.substr(comma1 + 1, comma2 - comma1 - 1);
  
  std::size_t openParen = content.find('(');
  std::size_t closeParen = content.rfind(')', comma1 - 1);
  if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen) {
    expr = content.substr(openParen + 1, closeParen - openParen - 1);
    l1 = content.substr(closeParen + 1, comma1 - closeParen - 1);
  }
}

void parseComputedGoto(const std::string &content, std::vector<std::string> &labels, std::string &expr) {
  std::size_t openParen = content.find('(');
  std::size_t closeParen = content.find(')');
  if (openParen == std::string::npos || closeParen == std::string::npos || closeParen <= openParen) return;
  
  std::string labelsStr = content.substr(openParen + 1, closeParen - openParen - 1);
  std::string current;
  for (char c : labelsStr) {
    if (c == ',') {
      if (!current.empty()) {
        labels.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty()) {
    labels.push_back(current);
  }
  
  std::size_t exprStart = closeParen + 1;
  if (exprStart < content.size() && content[exprStart] == ',') {
    exprStart++;
  }
  expr = content.substr(exprStart);
}

std::unordered_set<std::string> extractDeclaredArrays(const std::string &content) {
  std::unordered_set<std::string> arrays;
  std::string decl = content;
  std::vector<std::string> prefixes = {"integer", "real", "doubleprecision", "logical", "character", "complex", "type", "dimension"};
  std::string matchedPrefix;
  for (const auto &pref : prefixes) {
    if (startsWith(decl, pref)) {
      matchedPrefix = pref;
      break;
    }
  }
  if (matchedPrefix.empty()) return arrays;
  
  decl = decl.substr(matchedPrefix.size());
  if (startsWith(decl, "::")) {
    decl = decl.substr(2);
  }
  
  std::string current;
  int parenDepth = 0;
  for (std::size_t i = 0; i < decl.size(); ++i) {
    char c = decl[i];
    if (c == '(') {
      if (parenDepth == 0 && !current.empty()) {
        arrays.insert(current);
      }
      parenDepth++;
    } else if (c == ')') {
      parenDepth--;
      if (parenDepth == 0) {
        current.clear();
      }
    } else if (parenDepth == 0) {
      if (c == ',') {
        current.clear();
      } else if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
        current.push_back(c);
      }
    }
  }
  return arrays;
}

bool isExecutable(const std::string &content) {
  if (startsWith(content, "write") || startsWith(content, "print") ||
      startsWith(content, "read") || startsWith(content, "open") ||
      startsWith(content, "close") || startsWith(content, "goto") ||
      startsWith(content, "call") || startsWith(content, "do") ||
      startsWith(content, "if(") || startsWith(content, "end")) {
    return true;
  }
  
  std::size_t eqPos = content.find('=');
  if (eqPos != std::string::npos && eqPos > 0) {
    std::string lhs = content.substr(0, eqPos);
    if (lhs.find('(') == std::string::npos) {
      if (!startsWith(content, "parameter") && !startsWith(content, "implicit") &&
          !startsWith(content, "data") && !startsWith(content, "dimension")) {
        return true;
      }
    }
  }
  return false;
}

bool isStatementFunctionCandidate(const std::string &content, std::string &outName) {
  std::size_t eqPos = content.find('=');
  if (eqPos == std::string::npos || eqPos == 0) return false;
  std::string lhs = content.substr(0, eqPos);
  if (lhs.empty() || lhs.back() != ')') return false;
  std::size_t openParen = lhs.find('(');
  if (openParen == std::string::npos || openParen == 0) return false;
  
  outName = lhs.substr(0, openParen);
  if (outName.empty() || std::isdigit(static_cast<unsigned char>(outName[0]))) return false;
  for (char c : outName) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
  }
  
  if (outName == "if" || outName == "read" || outName == "write" || outName == "print") return false;
  
  return true;
}

bool isAssumedSize(const std::string &content) {
  bool hasTypePrefix = startsWith(content, "integer") ||
                       startsWith(content, "real") ||
                       startsWith(content, "doubleprecision") ||
                       startsWith(content, "logical") ||
                       startsWith(content, "character") ||
                       startsWith(content, "complex") ||
                       startsWith(content, "type");
  if (!hasTypePrefix) return false;
  
  bool insideParens = false;
  for (char c : content) {
    if (c == '(') insideParens = true;
    else if (c == ')') insideParens = false;
    else if (c == '*' && insideParens) {
      return true;
    }
  }
  return false;
}

std::string stripCommonBlocks(const std::string &commonStmt) {
  std::string result;
  bool insideBlock = false;
  for (char c : commonStmt) {
    if (c == '/') {
      insideBlock = !insideBlock;
    } else if (!insideBlock) {
      result.push_back(c);
    }
  }
  return result;
}

std::vector<std::string> getIdentifiers(const std::string &str) {
  std::vector<std::string> ids;
  std::string current;
  for (char c : str) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
      current.push_back(c);
    } else {
      if (!current.empty()) {
        if (!std::isdigit(static_cast<unsigned char>(current[0]))) {
          ids.push_back(current);
        }
        current.clear();
      }
    }
  }
  if (!current.empty() && !std::isdigit(static_cast<unsigned char>(current[0]))) {
    ids.push_back(current);
  }
  return ids;
}

std::vector<std::string> extractVariablesForEquivalence(const std::string &content) {
  auto ids = getIdentifiers(content);
  std::vector<std::string> vars;
  for (auto id : ids) {
    std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) {
      return std::tolower(c);
    });
    if (id != "equivalence") {
      vars.push_back(id);
    }
  }
  return vars;
}

std::vector<std::string> extractVariablesForCommon(const std::string &content) {
  std::string stripped = stripCommonBlocks(content);
  auto ids = getIdentifiers(stripped);
  std::vector<std::string> vars;
  for (auto id : ids) {
    std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) {
      return std::tolower(c);
    });
    if (id != "common") {
      vars.push_back(id);
    }
  }
  return vars;
}

struct UnitInfo {
  std::string name;
  int startLine = 1;
  bool hasImplicitNone = false;
  bool isActive = false;
  int statementCount = 0;
};

} // namespace

std::vector<PatternFinding> PatternDetector::detectPatterns(const SourceAnalysis &analysis) {
  std::vector<PatternFinding> findings;

  const std::regex arithmeticIfPattern(R"(^if\(.+\)\d+,\d+,\d+$)");
  const std::regex computedGotoPattern(R"(^goto\(\d+(,\d+)*\),?.+$)");

  std::unordered_set<std::string> declaredArrays;
  bool seenExecutableInUnit = false;

  std::vector<UnitInfo> units;
  UnitInfo currentUnit;
  currentUnit.startLine = 1;
  currentUnit.name = "Implicit Main Program";
  currentUnit.isActive = true;

  for (const auto &stmt : analysis.statements) {
    std::string location = "line " + std::to_string(stmt.startLine);
    if (stmt.startLine != stmt.endLine) {
      location += "-" + std::to_string(stmt.endLine);
    }

    if (isUnitStart(stmt.content)) {
      if (currentUnit.isActive && currentUnit.statementCount > 0) {
        units.push_back(currentUnit);
      }
      currentUnit = UnitInfo();
      currentUnit.startLine = stmt.startLine;
      currentUnit.name = stmt.rawContent;
      currentUnit.isActive = true;
      
      declaredArrays.clear();
      seenExecutableInUnit = false;
    } else {
      currentUnit.statementCount++;
    }

    if (stmt.content == "implicitnone") {
      currentUnit.hasImplicitNone = true;
    }

    if (isUnitEnd(stmt.content)) {
      if (currentUnit.isActive && currentUnit.statementCount > 0) {
        units.push_back(currentUnit);
        currentUnit = UnitInfo();
      }
    }

    auto arrays = extractDeclaredArrays(stmt.content);
    declaredArrays.insert(arrays.begin(), arrays.end());

    if (!seenExecutableInUnit && isExecutable(stmt.content)) {
      seenExecutableInUnit = true;
    }

    if (std::regex_match(stmt.content, arithmeticIfPattern)) {
      std::string expr, l1, l2, l3;
      parseArithmeticIf(stmt.content, expr, l1, l2, l3);
      std::string modern = "if (" + expr + " .lt. 0) then\n  goto " + l1 + "\nelse if (" + expr + " .eq. 0) then\n  goto " + l2 + "\nelse\n  goto " + l3 + "\nend if";
      findings.push_back({"arithmetic_if", location,
                          "Arithmetic IF should be rewritten to IF / ELSE IF / ELSE.",
                          {}, stmt.rawContent, modern});
    }

    if (std::regex_match(stmt.content, computedGotoPattern)) {
      std::vector<std::string> labels;
      std::string expr;
      parseComputedGoto(stmt.content, labels, expr);
      std::string modern = "select case (" + expr + ")\n";
      for (size_t idx = 0; idx < labels.size(); ++idx) {
        modern += "case (" + std::to_string(idx + 1) + ")\n  goto " + labels[idx] + "\n";
      }
      modern += "case default\n  ! optional fallback\nend select";
      findings.push_back({"computed_goto", location,
                          "Computed GOTO should be replaced with structured SELECT CASE.",
                          {}, stmt.rawContent, modern});
    }

    if (startsWith(stmt.content, "equivalence(")) {
      auto vars = extractVariablesForEquivalence(stmt.rawContent);
      std::string modern = "! Replace equivalence with target/pointer or structure overlay\n";
      if (vars.size() >= 2) {
        modern += "type_name, target :: " + vars[0] + "\n";
        for (size_t idx = 1; idx < vars.size(); ++idx) {
          modern += "type_name, pointer :: " + vars[idx] + "\n" + vars[idx] + " => " + vars[0] + "\n";
        }
      }
      findings.push_back({"equivalence", location,
                          "EQUIVALENCE introduces aliasing; migrate to explicit data structures.",
                          vars, stmt.rawContent, modern});
    }

    if (startsWith(stmt.content, "common/") || 
        (startsWith(stmt.content, "common") && stmt.content.find('=') == std::string::npos)) {
      auto vars = extractVariablesForCommon(stmt.rawContent);
      std::string modern = "module global_variables_mod\n  implicit none\n  ! Declare common variables here\n";
      for (const auto &var : vars) {
        modern += "  type_name :: " + var + "\n";
      }
      modern += "end module global_variables_mod\n\n! In procedures:\nuse global_variables_mod, only : ";
      for (size_t idx = 0; idx < vars.size(); ++idx) {
        modern += vars[idx] + (idx + 1 < vars.size() ? ", " : "");
      }
      findings.push_back({"common_block", location,
                          "COMMON blocks should move to MODULE variables with explicit interfaces.",
                          vars, stmt.rawContent, modern});
    }

    if (startsWith(stmt.content, "entry") && stmt.content.find('=') == std::string::npos) {
      std::string modern = "! Split procedure with ENTRY into separate procedures\n"
                           "! and call shared internal logic or define generic interfaces.";
      findings.push_back({"entry_statement", location,
                          "ENTRY statements should be split into separate procedures.",
                          {}, stmt.rawContent, modern});
    }

    if (isAssumedSize(stmt.content)) {
      std::string modern = "! Use assumed-shape interface instead of assumed-size (*)\n"
                           "type_name, intent(in) :: arr(:)";
      findings.push_back({"assumed_size_array", location,
                          "Assumed-size arrays should migrate to explicit-shape or assumed-rank interfaces.",
                          {}, stmt.rawContent, modern});
    }

    std::string stmtFuncName;
    if (!seenExecutableInUnit && isStatementFunctionCandidate(stmt.content, stmtFuncName)) {
      if (declaredArrays.find(stmtFuncName) == declaredArrays.end()) {
        std::size_t eqPos = stmt.rawContent.find('=');
        std::string lhs = stmt.rawContent.substr(0, eqPos);
        std::string rhs = stmt.rawContent.substr(eqPos + 1);
        std::size_t openParen = lhs.find('(');
        std::size_t closeParen = lhs.rfind(')');
        std::string args;
        if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen) {
          args = lhs.substr(openParen + 1, closeParen - openParen - 1);
        }
        std::string modern = "contains\n  pure function " + stmtFuncName + "(" + args + ") result(res)\n    ! Declare argument types\n    implicit none\n    res = " + rhs + "\n  end function " + stmtFuncName;
        findings.push_back({"statement_function", location,
                            "Statement function '" + stmtFuncName + "' is an obsolete construct; replace with internal function or expression.",
                            {stmtFuncName}, stmt.rawContent, modern});
      }
    }
  }

  if (currentUnit.isActive && currentUnit.statementCount > 0) {
    units.push_back(currentUnit);
  }

  for (const auto &unit : units) {
    if (!unit.hasImplicitNone) {
      std::string loc = "line " + std::to_string(unit.startLine);
      std::string modern = "implicit none\n! Explicitly declare all variables, e.g.:\ninteger :: var_name";
      findings.push_back({"implicit_typing", loc,
                          "Implicit typing is active in program unit '" + unit.name + "'; add IMPLICIT NONE and declare variables explicitly.",
                          {}, "! implicit typing default", modern});
    }
  }

  if (analysis.fixedFormHint) {
    std::string modern = "! Convert source layout to free-form Fortran:\n"
                         "! - Change comments from column 1 'C'/'*' to '!'\n"
                         "! - Remove statement label column constraints (columns 1-5)\n"
                         "! - Change line continuations from column 6 characters to '&' at end of line";
    findings.push_back({"fixed_form_source", "file",
                        "Fixed-form source should be converted to free-form Fortran.",
                        {}, "! fixed-form file format", modern});
  }

  std::cerr << "[PatternDetector] Detected " << findings.size() << " pattern finding(s)\n";
  return findings;
}

} // namespace LegacyFortranAdvisor
