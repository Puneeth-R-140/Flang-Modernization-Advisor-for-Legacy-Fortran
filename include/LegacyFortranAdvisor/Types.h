#pragma once

#include <string>
#include <vector>

namespace LegacyFortranAdvisor {

struct PatternFinding {
  std::string kind;
  std::string location;
  std::string message;
  std::vector<std::string> variables;
  std::string legacyConstruct;
  std::string modernEquivalent;
};

struct NormalizedStatement {
  std::string content;          // Normalized content (lowercase, spaces stripped or unified)
  std::string rawContent;       // Unmodified content across original lines
  int startLine = 0;            // 1-based original start line
  int endLine = 0;              // 1-based original end line
};

struct SourceAnalysis {
  std::string filePath;
  std::vector<std::string> lines;
  std::vector<NormalizedStatement> statements;
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
  std::string legacyConstruct;
  std::string modernEquivalent;
};

} // namespace LegacyFortranAdvisor
