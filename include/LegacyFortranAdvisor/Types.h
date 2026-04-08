#pragma once

#include <string>
#include <vector>

namespace LegacyFortranAdvisor {

struct PatternFinding {
  std::string kind;
  std::string location;
  std::string message;
};

struct SourceAnalysis {
  std::string filePath;
  std::vector<std::string> lines;
  bool fixedFormHint = false;
};

struct ImpactSummary {
  PatternFinding finding;
  int affectedFiles = 0;
  int dependentConstructs = 0;
  bool aliasingRisk = false;
  bool behaviorChangeRisk = false;
};

struct PlanEntry {
  std::string summary;
  std::string effort;
  std::string safety;
  int priority = 0;
};

} // namespace LegacyFortranAdvisor
