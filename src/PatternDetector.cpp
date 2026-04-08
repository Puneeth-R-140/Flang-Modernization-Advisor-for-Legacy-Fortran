#include "LegacyFortranAdvisor/PatternDetector.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>

namespace LegacyFortranAdvisor {

namespace {

std::string toLower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

bool startsWith(const std::string &text, const std::string &prefix) {
  return text.rfind(prefix, 0) == 0;
}

} // namespace

std::vector<PatternFinding> PatternDetector::detectPatterns(const SourceAnalysis &analysis) {
  std::vector<PatternFinding> findings;

  const std::regex arithmeticIfPattern(R"(^\s*if\s*\(.*\)\s*\d+\s*,\s*\d+\s*,\s*\d+)");
  const std::regex computedGotoPattern(R"(^\s*goto\s*\([^\)]*\)\s*,)");
  const std::regex assumedSizeDeclPattern(
      R"(^\s*(integer|real|double\s+precision|logical|character|complex|type)\b.*\([^\)]*\*\s*\))");

  for (std::size_t i = 0; i < analysis.lines.size(); ++i) {
    const std::string lowered = toLower(analysis.lines[i]);
    const std::string location = "line " + std::to_string(i + 1);

    if (std::regex_search(lowered, arithmeticIfPattern)) {
      findings.push_back({"arithmetic_if", location,
                          "Arithmetic IF should be rewritten to IF / ELSE IF / ELSE."});
    }

    if (std::regex_search(lowered, computedGotoPattern)) {
      findings.push_back({"computed_goto", location,
                          "Computed GOTO should be replaced with structured SELECT CASE."});
    }

    if (startsWith(lowered, "      equivalence") || startsWith(lowered, "equivalence")) {
      findings.push_back({"equivalence", location,
                          "EQUIVALENCE introduces aliasing; migrate to explicit data structures."});
    }

    if (startsWith(lowered, "      common") || startsWith(lowered, "common")) {
      findings.push_back({"common_block", location,
                          "COMMON blocks should move to MODULE variables with explicit interfaces."});
    }

    if (startsWith(lowered, "      implicit") || startsWith(lowered, "implicit")) {
      if (lowered.find("implicit none") == std::string::npos) {
        findings.push_back({"implicit_typing", location,
                            "Implicit typing should be removed by adding IMPLICIT NONE and declarations."});
      }
    }

    if (startsWith(lowered, "      entry") || startsWith(lowered, "entry")) {
      findings.push_back({"entry_statement", location,
                          "ENTRY statements should be split into separate procedures."});
    }

    if (std::regex_search(lowered, assumedSizeDeclPattern)) {
      findings.push_back({"assumed_size_array", location,
                          "Assumed-size arrays should migrate to explicit-shape or assumed-rank interfaces."});
    }
  }

  if (analysis.fixedFormHint) {
    findings.push_back({"fixed_form_source", "file",
                        "Fixed-form source should be converted to free-form Fortran."});
  }

  std::cerr << "[PatternDetector] Detected " << findings.size() << " pattern finding(s)\n";
  return findings;
}

} // namespace LegacyFortranAdvisor
