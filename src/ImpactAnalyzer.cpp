#include "LegacyFortranAdvisor/ImpactAnalyzer.h"
#include <iostream>

namespace LegacyFortranAdvisor {

std::vector<ImpactSummary> ImpactAnalyzer::analyze(const std::vector<PatternFinding> &findings,
                                                   const SourceAnalysis &analysis) {
  (void)analysis;
  std::cerr << "[ImpactAnalyzer] Computing impact for " << findings.size() << " finding(s)\n";
  std::vector<ImpactSummary> result;

  for (const auto &finding : findings) {
    ImpactSummary summary;
    summary.finding = finding;
    summary.affectedFiles = 1;

    if (finding.kind == "equivalence") {
      summary.dependentConstructs = 4;
      summary.aliasingRisk = true;
      summary.behaviorChangeRisk = true;
    } else if (finding.kind == "common_block") {
      summary.dependentConstructs = 3;
      summary.aliasingRisk = true;
      summary.behaviorChangeRisk = true;
    } else if (finding.kind == "computed_goto" || finding.kind == "arithmetic_if") {
      summary.dependentConstructs = 3;
      summary.behaviorChangeRisk = true;
    } else if (finding.kind == "entry_statement") {
      summary.dependentConstructs = 3;
      summary.behaviorChangeRisk = true;
    } else if (finding.kind == "assumed_size_array") {
      summary.dependentConstructs = 2;
      summary.behaviorChangeRisk = true;
    } else if (finding.kind == "implicit_typing") {
      summary.dependentConstructs = 2;
      summary.behaviorChangeRisk = false;
    } else if (finding.kind == "fixed_form_source") {
      summary.dependentConstructs = 1;
      summary.behaviorChangeRisk = false;
    } else {
      summary.dependentConstructs = 1;
      summary.behaviorChangeRisk = false;
    }

    result.push_back(summary);
  }

  return result;
}

} // namespace LegacyFortranAdvisor
