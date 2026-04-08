#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "LegacyFortranAdvisor/FlangFrontend.h"
#include "LegacyFortranAdvisor/PatternDetector.h"
#include "LegacyFortranAdvisor/ImpactAnalyzer.h"
#include "LegacyFortranAdvisor/Prioritization.h"

namespace {

std::string escapeJson(const std::string &text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (char ch : text) {
    switch (ch) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped += ch;
      break;
    }
  }
  return escaped;
}

bool writeJsonPlan(const std::string &outputPath,
                   const std::string &sourceFile,
                   const std::vector<LegacyFortranAdvisor::PlanEntry> &plan) {
  std::ofstream out(outputPath);
  if (!out) {
    return false;
  }

  out << "{\n";
  out << "  \"source\": \"" << escapeJson(sourceFile) << "\",\n";
  out << "  \"plan\": [\n";
  for (std::size_t i = 0; i < plan.size(); ++i) {
    const auto &entry = plan[i];
    out << "    {\n";
    out << "      \"summary\": \"" << escapeJson(entry.summary) << "\",\n";
    out << "      \"effort\": \"" << escapeJson(entry.effort) << "\",\n";
    out << "      \"safety\": \"" << escapeJson(entry.safety) << "\",\n";
    out << "      \"priority\": " << entry.priority << "\n";
    out << "    }";
    if (i + 1 < plan.size()) {
      out << ",";
    }
    out << "\n";
  }
  out << "  ]\n";
  out << "}\n";

  return true;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: flang-modernization-advisor <source-file> [--output plan.json]" << std::endl;
    return 1;
  }

  std::string sourceFile;
  std::string outputPath;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--output") {
      if (i + 1 >= argc) {
        std::cerr << "Error: --output requires a file path" << std::endl;
        return 1;
      }
      outputPath = argv[++i];
      continue;
    }

    if (!arg.empty() && arg[0] == '-') {
      std::cerr << "Error: unknown option " << arg << std::endl;
      return 1;
    }

    if (sourceFile.empty()) {
      sourceFile = arg;
    } else {
      std::cerr << "Error: multiple source files are not supported" << std::endl;
      return 1;
    }
  }

  if (sourceFile.empty()) {
    std::cerr << "Error: source file is required" << std::endl;
    return 1;
  }

  LegacyFortranAdvisor::FlangFrontend frontend;
  auto AST = frontend.parseFile(sourceFile);
  if (!AST) {
    std::cerr << "Error: failed to parse " << sourceFile << std::endl;
    return 2;
  }

  LegacyFortranAdvisor::PatternDetector detector;
  auto findings = detector.detectPatterns(*AST);

  LegacyFortranAdvisor::ImpactAnalyzer analyzer;
  auto impacts = analyzer.analyze(findings, *AST);

  LegacyFortranAdvisor::Prioritization planner;
  auto plan = planner.generatePlan(impacts);

  std::cout << "Modernization plan for " << sourceFile << ":\n";
  for (const auto &entry : plan) {
    std::cout << "- " << entry.summary << " [effort=" << entry.effort << ", safety=" << entry.safety << "]\n";
  }

  if (!outputPath.empty()) {
    if (!writeJsonPlan(outputPath, sourceFile, plan)) {
      std::cerr << "Error: failed to write output file " << outputPath << std::endl;
      return 3;
    }
    std::cout << "Wrote JSON plan to " << outputPath << "\n";
  }

  return 0;
}
