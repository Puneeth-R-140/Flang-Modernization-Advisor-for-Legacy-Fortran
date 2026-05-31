#pragma once

#include "LegacyFortranAdvisor/Types.h"

namespace LegacyFortranAdvisor {

class PatternDetector {
public:
  std::vector<PatternFinding> detectPatterns(const SourceAnalysis &analysis);
  std::vector<CommonBlockLayout> extractCommonLayouts(const SourceAnalysis &analysis);
  FileDependencies extractDependencies(const SourceAnalysis &analysis);
};

} // namespace LegacyFortranAdvisor
