#pragma once

#include "LegacyFortranAdvisor/Types.h"

namespace LegacyFortranAdvisor {

class ImpactAnalyzer {
public:
  std::vector<ImpactSummary> analyze(const std::vector<PatternFinding> &findings,
                                     const SourceAnalysis &analysis);
};

} // namespace LegacyFortranAdvisor
