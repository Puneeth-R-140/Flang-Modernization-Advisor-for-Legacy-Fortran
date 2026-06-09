#include "LegacyFortranAdvisor/PatternDetector.h"
#ifdef USE_FLANG_PARSER
#include "LegacyFortranAdvisor/FlangASTPatternVisitor.h"
#endif
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

bool isIdentifierChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

CommonVariable resolveVariable(const std::string &varName, const SourceAnalysis &analysis) {
  CommonVariable info;
  info.name = varName;
  
  for (const auto &stmt : analysis.statements) {
    bool isDecl = startsWith(stmt.content, "integer") ||
                  startsWith(stmt.content, "real") ||
                  startsWith(stmt.content, "doubleprecision") ||
                  startsWith(stmt.content, "logical") ||
                  startsWith(stmt.content, "character") ||
                  startsWith(stmt.content, "complex") ||
                  startsWith(stmt.content, "type") ||
                  startsWith(stmt.content, "dimension");
    if (!isDecl) continue;
    
    std::size_t pos = stmt.rawContent.find(varName);
    while (pos != std::string::npos) {
      bool leadingOk = (pos == 0 || !isIdentifierChar(stmt.rawContent[pos - 1]));
      bool trailingOk = (pos + varName.size() == stmt.rawContent.size() || !isIdentifierChar(stmt.rawContent[pos + varName.size()]));
      if (leadingOk && trailingOk) {
        if (!startsWith(stmt.content, "dimension")) {
          std::vector<std::string> prefixes = {"integer", "real", "doubleprecision", "logical", "character", "complex", "type"};
          for (const auto &pref : prefixes) {
            if (startsWith(stmt.content, pref)) {
              info.type = pref;
              break;
            }
          }
        }
        
        std::size_t nextCharPos = pos + varName.size();
        while (nextCharPos < stmt.rawContent.size() && std::isspace(static_cast<unsigned char>(stmt.rawContent[nextCharPos]))) {
          nextCharPos++;
        }
        if (nextCharPos < stmt.rawContent.size() && stmt.rawContent[nextCharPos] == '(') {
          std::size_t closeParen = stmt.rawContent.find(')', nextCharPos);
          if (closeParen != std::string::npos && closeParen > nextCharPos) {
            std::string dims = stmt.rawContent.substr(nextCharPos + 1, closeParen - nextCharPos - 1);
            int size = 1;
            std::string currentDim;
            for (char c : dims) {
              if (c == ',') {
                int d = std::atoi(currentDim.c_str());
                if (d > 0) size *= d;
                currentDim.clear();
              } else if (std::isdigit(static_cast<unsigned char>(c))) {
                currentDim.push_back(c);
              }
            }
            if (!currentDim.empty()) {
              int d = std::atoi(currentDim.c_str());
              if (d > 0) size *= d;
            }
            info.arraySize = size;
          }
        }
      }
      pos = stmt.rawContent.find(varName, pos + 1);
    }
  }
  return info;
}

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

} // namespace

std::vector<PatternFinding> PatternDetector::detectPatterns(const SourceAnalysis &analysis) {
#ifdef USE_FLANG_PARSER
  if (analysis.parsing) {
    auto holder = std::static_pointer_cast<FlangParserHolder>(analysis.parsing);
    if (holder->parsing.parseTree()) {
      FlangASTPatternVisitor visitor(holder->parsing.allCooked());
      return visitor.analyze(*holder->parsing.parseTree());
    }
  }
#endif
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

    // 1. Hollerith Constants
    std::string hollerithConst;
    if (findHollerith(stmt.rawContent, hollerithConst)) {
      std::string modern = "! Use character literal instead of Hollerith constant\n" + makeModernHollerith(hollerithConst);
      findings.push_back({"hollerith_constant", location,
                          "Hollerith constant '" + hollerithConst + "' is an obsolete data representation.",
                          {}, hollerithConst, modern});
    }

    // 2. ASSIGN & Assigned GOTO
    if (startsWith(stmt.content, "assign") && stmt.content.find("to") != std::string::npos) {
      std::string modern = "! Replace ASSIGN with standard integer assignment\n! E.g. labelvar = 10";
      findings.push_back({"assign_statement", location,
                          "ASSIGN statement is obsolete; use integer variable assignment.",
                          {}, stmt.rawContent, modern});
    }
    if (startsWith(stmt.content, "goto") && 
        stmt.content.find('(') == std::string::npos && 
        stmt.content.find('=') == std::string::npos) {
      std::string suffix = stmt.content.substr(4);
      bool isNumeric = !suffix.empty();
      for (char c : suffix) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
          isNumeric = false;
          break;
        }
      }
      if (!isNumeric && !suffix.empty()) {
        std::string modern = "! Replace assigned GOTO with structured SELECT CASE or IF:\nif (" + suffix + " == 10) then\n  goto 10\nend if";
        findings.push_back({"assigned_goto", location,
                            "Assigned GOTO is obsolete; replace with SELECT CASE or IF blocks.",
                            {}, stmt.rawContent, modern});
      }
    }

    // 3. PAUSE Statement
    if (startsWith(stmt.content, "pause") && stmt.content.find('=') == std::string::npos) {
      std::string modern = "! PAUSE is obsolete; replace with print and read statements\nwrite(*,*) \"PAUSE: Press Enter to continue...\"\nread(*,*)";
      findings.push_back({"pause_statement", location,
                          "PAUSE statement is obsolete; replace with READ statement.",
                          {}, stmt.rawContent, modern});
    }

    // 4. Label DO Loops
    if (startsWith(stmt.content, "do") && 
        stmt.content.size() > 2 && 
        std::isdigit(static_cast<unsigned char>(stmt.content[2])) &&
        stmt.content.find('=') != std::string::npos &&
        stmt.content.find(',') != std::string::npos) {
      std::string modern = makeModernDoLoop(stmt.rawContent);
      findings.push_back({"label_do_loop", location,
                          "Label-terminated DO loop is obsolete; convert to block DO / END DO.",
                          {}, stmt.rawContent, modern});
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

std::vector<CommonBlockLayout> PatternDetector::extractCommonLayouts(const SourceAnalysis &analysis) {
  std::vector<CommonBlockLayout> layouts;
  
  for (const auto &stmt : analysis.statements) {
    if (startsWith(stmt.content, "common/") || 
        (startsWith(stmt.content, "common") && stmt.content.find('=') == std::string::npos)) {
      
      std::string blockName = "blank_common";
      std::size_t slash1 = stmt.content.find('/');
      if (slash1 != std::string::npos) {
        std::size_t slash2 = stmt.content.find('/', slash1 + 1);
        if (slash2 != std::string::npos && slash2 > slash1) {
          blockName = stmt.content.substr(slash1 + 1, slash2 - slash1 - 1);
        }
      }
      
      CommonBlockLayout layout;
      layout.blockName = blockName;
      layout.sourceFile = analysis.filePath;
      layout.line = stmt.startLine;
      
      auto vars = extractVariablesForCommon(stmt.rawContent);
      for (const auto &var : vars) {
        layout.variables.push_back(resolveVariable(var, analysis));
      }
      
      layouts.push_back(layout);
    }
  }
  return layouts;
}

FileDependencies PatternDetector::extractDependencies(const SourceAnalysis &analysis) {
  FileDependencies deps;
  deps.filePath = analysis.filePath;
  
  auto toLowerString = [](std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
      return std::tolower(c);
    });
    return str;
  };
  
  for (const auto &stmt : analysis.statements) {
    auto ids = getIdentifiers(stmt.rawContent);
    if (ids.size() < 2) continue;
    
    std::string firstId = toLowerString(ids[0]);
    
    if (startsWith(stmt.content, "module") && !startsWith(stmt.content, "moduleprocedure")) {
      deps.declaredModules.push_back(toLowerString(ids[1]));
    }
    
    if (startsWith(stmt.content, "subroutine")) {
      deps.declaredSubroutines.push_back(toLowerString(ids[1]));
    } else {
      for (size_t idx = 0; idx + 1 < ids.size(); ++idx) {
        if (toLowerString(ids[idx]) == "function") {
          deps.declaredSubroutines.push_back(toLowerString(ids[idx + 1]));
          break;
        }
      }
    }
    
    if (startsWith(stmt.content, "use") && stmt.content.find('=') == std::string::npos) {
      deps.usedModules.push_back(toLowerString(ids[1]));
    }
    
    if (startsWith(stmt.content, "call") && stmt.content.find('=') == std::string::npos) {
      deps.calledSubroutines.push_back(toLowerString(ids[1]));
    }
  }
  
  return deps;
}

} // namespace LegacyFortranAdvisor
