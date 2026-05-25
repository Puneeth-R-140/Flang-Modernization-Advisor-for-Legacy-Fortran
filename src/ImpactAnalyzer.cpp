#include "LegacyFortranAdvisor/ImpactAnalyzer.h"
#include <iostream>
#include <cctype>

namespace LegacyFortranAdvisor {

namespace {

bool isIdentifierChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

int countIdentifierReferences(const std::string &rawContent, const std::string &varName) {
  int count = 0;
  std::size_t pos = rawContent.find(varName);
  while (pos != std::string::npos) {
    bool leadingOk = (pos == 0 || !isIdentifierChar(rawContent[pos - 1]));
    bool trailingOk = (pos + varName.size() == rawContent.size() || !isIdentifierChar(rawContent[pos + varName.size()]));
    if (leadingOk && trailingOk) {
      count++;
    }
    pos = rawContent.find(varName, pos + 1);
  }
  return count;
}

} // namespace

std::vector<ImpactSummary> ImpactAnalyzer::analyze(const std::vector<PatternFinding> &findings,
                                                   const SourceAnalysis &analysis) {
  std::cerr << "[ImpactAnalyzer] Computing impact for " << findings.size() << " finding(s)\n";
  std::vector<ImpactSummary> result;

  for (const auto &finding : findings) {
    ImpactSummary summary;
    summary.finding = finding;
    summary.affectedFiles = 1;

    int refCount = 0;
    if (!finding.variables.empty()) {
      for (const auto &var : finding.variables) {
        for (const auto &stmt : analysis.statements) {
          refCount += countIdentifierReferences(stmt.rawContent, var);
        }
      }
      // If the variables are only referenced in the declaration itself, refCount would be equal to the number of variables.
      // We subtract the declaration occurrences (usually 1 per variable) to get reference usage count, but clamp to at least 1.
      int declOccurrences = finding.variables.size();
      int actualRefs = refCount - declOccurrences;
      summary.dependentConstructs = std::max(1, actualRefs);
    } else {
      summary.dependentConstructs = 1;
    }

    if (finding.kind == "equivalence") {
      summary.aliasingRisk = true;
      summary.behaviorChangeRisk = true;
    } else if (finding.kind == "common_block") {
      summary.aliasingRisk = true;
      summary.behaviorChangeRisk = true;
    } else if (finding.kind == "computed_goto" || finding.kind == "arithmetic_if") {
      summary.behaviorChangeRisk = true;
      // For control flow, count labels or control variables
      summary.dependentConstructs = 3;
    } else if (finding.kind == "entry_statement") {
      summary.behaviorChangeRisk = true;
      summary.dependentConstructs = 3;
    } else if (finding.kind == "assumed_size_array") {
      summary.behaviorChangeRisk = true;
      summary.dependentConstructs = 2;
    } else if (finding.kind == "statement_function") {
      summary.behaviorChangeRisk = true;
      // Keep summary.dependentConstructs as computed above
    } else if (finding.kind == "implicit_typing") {
      summary.behaviorChangeRisk = false;
      summary.dependentConstructs = 2;
    } else if (finding.kind == "fixed_form_source") {
      summary.behaviorChangeRisk = false;
      summary.dependentConstructs = 1;
    } else {
      summary.behaviorChangeRisk = false;
      summary.dependentConstructs = 1;
    }

    result.push_back(summary);
  }

  return result;
}

} // namespace LegacyFortranAdvisor
