#pragma once

#include "LegacyFortranAdvisor/Types.h"

namespace LegacyFortranAdvisor {

class PatternDetector {
public:
  std::vector<PatternFinding> detectPatterns(const SourceAnalysis &analysis);
};

} // namespace LegacyFortranAdvisor
