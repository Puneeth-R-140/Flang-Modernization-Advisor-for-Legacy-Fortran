#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include "LegacyFortranAdvisor/FlangFrontend.h"
#include "LegacyFortranAdvisor/PatternDetector.h"
#include "LegacyFortranAdvisor/ImpactAnalyzer.h"
#include "LegacyFortranAdvisor/Prioritization.h"

namespace fs = std::filesystem;

namespace {

std::string escapeJson(const std::string &text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (char ch : text) {
    switch (ch) {
    case '\\': escaped += "\\\\"; break;
    case '"': escaped += "\\\""; break;
    case '\n': escaped += "\\n"; break;
    case '\r': escaped += "\\r"; break;
    case '\t': escaped += "\\t"; break;
    default: escaped += ch; break;
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
    out << "      \"priority\": " << entry.priority << ",\n";
    out << "      \"legacy\": \"" << escapeJson(entry.legacyConstruct) << "\",\n";
    out << "      \"modern\": \"" << escapeJson(entry.modernEquivalent) << "\"\n";
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

std::vector<std::string> collectFiles(const std::string &path) {
  std::vector<std::string> files;
  try {
    if (fs::is_directory(path)) {
      for (const auto &entry : fs::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) {
          std::string ext = entry.path().extension().string();
          std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return std::tolower(c);
          });
          if (ext == ".f90" || ext == ".f" || ext == ".f77" || ext == ".for") {
            files.push_back(entry.path().string());
          }
        }
      }
    } else if (fs::is_regular_file(path)) {
      files.push_back(path);
    }
  } catch (const std::exception &e) {
    std::cerr << "Error scanning path " << path << ": " << e.what() << "\n";
  }
  return files;
}

std::string formatBlockVariables(const std::vector<LegacyFortranAdvisor::CommonVariable> &vars) {
  std::string res;
  for (size_t idx = 0; idx < vars.size(); ++idx) {
    res += vars[idx].name + " (" + vars[idx].type + " size " + std::to_string(vars[idx].arraySize) + ")";
    if (idx + 1 < vars.size()) res += ", ";
  }
  return res;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: flang-modernization-advisor <source-file-or-dir> [--output plan.json]" << std::endl;
    return 1;
  }

  std::string inputPath;
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

    if (inputPath.empty()) {
      inputPath = arg;
    } else {
      std::cerr << "Error: multiple input paths are not supported" << std::endl;
      return 1;
    }
  }

  if (inputPath.empty()) {
    std::cerr << "Error: input file or directory is required" << std::endl;
    return 1;
  }

  std::vector<std::string> files = collectFiles(inputPath);
  if (files.empty()) {
    std::cerr << "Error: no Fortran files found under path: " << inputPath << std::endl;
    return 2;
  }

  std::vector<LegacyFortranAdvisor::PlanEntry> globalPlan;
  std::unordered_map<std::string, std::vector<LegacyFortranAdvisor::CommonBlockLayout>> globalCommonRegistry;
  std::vector<LegacyFortranAdvisor::FileDependencies> globalDependencies;

  LegacyFortranAdvisor::FlangFrontend frontend;
  LegacyFortranAdvisor::PatternDetector detector;
  LegacyFortranAdvisor::ImpactAnalyzer analyzer;
  LegacyFortranAdvisor::Prioritization planner;

  for (const auto &file : files) {
    auto AST = frontend.parseFile(file);
    if (!AST) {
      std::cerr << "Warning: failed to parse " << file << ", skipping.\n";
      continue;
    }

    auto localFindings = detector.detectPatterns(*AST);
    
    auto localCommonLayouts = detector.extractCommonLayouts(*AST);
    for (auto &layout : localCommonLayouts) {
      globalCommonRegistry[layout.blockName].push_back(layout);
    }

    auto localDeps = detector.extractDependencies(*AST);
    globalDependencies.push_back(localDeps);

    auto localImpacts = analyzer.analyze(localFindings, *AST);
    auto localPlan = planner.generatePlan(localImpacts);
    
    for (auto &entry : localPlan) {
      entry.summary = "[" + fs::path(file).filename().string() + "] " + entry.summary;
      globalPlan.push_back(entry);
    }
  }

  // Cross-File COMMON Block structural mismatch checks
  for (const auto &[blockName, layouts] : globalCommonRegistry) {
    if (layouts.size() < 2) continue;
    
    const auto &baseline = layouts[0];
    for (size_t i = 1; i < layouts.size(); ++i) {
      const auto &compare = layouts[i];
      bool mismatch = false;
      std::string reason;

      if (baseline.variables.size() != compare.variables.size()) {
        mismatch = true;
        reason = "Variable count mismatch (" + std::to_string(baseline.variables.size()) + " vs " + std::to_string(compare.variables.size()) + ")";
      } else {
        for (size_t j = 0; j < baseline.variables.size(); ++j) {
          if (baseline.variables[j].type != compare.variables[j].type) {
            mismatch = true;
            reason = "Variable type mismatch at index " + std::to_string(j) + " ('" + baseline.variables[j].name + "' type '" + baseline.variables[j].type + "' vs '" + compare.variables[j].name + "' type '" + compare.variables[j].type + "')";
            break;
          }
          if (baseline.variables[j].arraySize != compare.variables[j].arraySize) {
            mismatch = true;
            reason = "Variable array size mismatch at index " + std::to_string(j) + " ('" + baseline.variables[j].name + "' size " + std::to_string(baseline.variables[j].arraySize) + " vs '" + compare.variables[j].name + "' size " + std::to_string(compare.variables[j].arraySize) + ")";
            break;
          }
        }
      }

      if (mismatch) {
        LegacyFortranAdvisor::PlanEntry mismatchEntry;
        mismatchEntry.summary = "[GLOBAL] Structural mismatch in COMMON block /" + blockName + "/ between " +
                                fs::path(baseline.sourceFile).filename().string() + " (line " + std::to_string(baseline.line) + ") and " +
                                fs::path(compare.sourceFile).filename().string() + " (line " + std::to_string(compare.line) + "). Reason: " + reason;
        mismatchEntry.effort = "moderate";
        mismatchEntry.safety = "review-needed";
        mismatchEntry.priority = 90;
        
        mismatchEntry.legacyConstruct = "COMMON /" + blockName + "/ in " + fs::path(baseline.sourceFile).filename().string() + ":\n  " + formatBlockVariables(baseline.variables) + "\n\n" +
                                        "COMMON /" + blockName + "/ in " + fs::path(compare.sourceFile).filename().string() + ":\n  " + formatBlockVariables(compare.variables);
                                        
        mismatchEntry.modernEquivalent = "module " + blockName + "_common_mod\n  implicit none\n  ! Declare synchronized layout here:\n";
        for (const auto &var : baseline.variables) {
          mismatchEntry.modernEquivalent += "  " + var.type + " :: " + var.name + (var.arraySize > 1 ? "(" + std::to_string(var.arraySize) + ")" : "") + "\n";
        }
        mismatchEntry.modernEquivalent += "end module " + blockName + "_common_mod";
        
        globalPlan.push_back(mismatchEntry);
      }
    }
  }

  // Cross-File Module and Subroutine Dependency Validation
  std::unordered_set<std::string> globalDeclaredModules;
  std::unordered_set<std::string> globalDeclaredSubroutines;
  
  std::unordered_set<std::string> intrinsics = {
    "cpu_time", "date_and_time", "random_number", "random_seed", "system_clock",
    "execute_command_line", "get_command_argument", "get_environment_variable"
  };

  for (const auto &dep : globalDependencies) {
    for (const auto &mod : dep.declaredModules) {
      globalDeclaredModules.insert(mod);
    }
    for (const auto &sub : dep.declaredSubroutines) {
      globalDeclaredSubroutines.insert(sub);
    }
  }

  for (const auto &dep : globalDependencies) {
    std::string filename = fs::path(dep.filePath).filename().string();
    
    // Check used modules
    for (const auto &used : dep.usedModules) {
      if (globalDeclaredModules.find(used) == globalDeclaredModules.end()) {
        LegacyFortranAdvisor::PlanEntry mismatchEntry;
        mismatchEntry.summary = "[GLOBAL] Module '" + used + "' used in " + filename + " is not defined in any of the analyzed files.";
        mismatchEntry.effort = "moderate";
        mismatchEntry.safety = "review-needed";
        mismatchEntry.priority = 70;
        mismatchEntry.legacyConstruct = "use " + used;
        mismatchEntry.modernEquivalent = "! Ensure module '" + used + "' is defined in the source code or module file is linked:\nmodule " + used + "\n  implicit none\n  ! module variables and interfaces\nend module " + used;
        globalPlan.push_back(mismatchEntry);
      }
    }
    
    // Check called subroutines
    for (const auto &called : dep.calledSubroutines) {
      if (intrinsics.find(called) == intrinsics.end() && 
          globalDeclaredSubroutines.find(called) == globalDeclaredSubroutines.end()) {
        LegacyFortranAdvisor::PlanEntry mismatchEntry;
        mismatchEntry.summary = "[GLOBAL] Subroutine/function '" + called + "' called in " + filename + " is not defined in any of the analyzed files.";
        mismatchEntry.effort = "moderate";
        mismatchEntry.safety = "review-needed";
        mismatchEntry.priority = 70;
        mismatchEntry.legacyConstruct = "call " + called + "(...)";
        mismatchEntry.modernEquivalent = "! Define the missing subroutine or link the object file:\nsubroutine " + called + "()\n  implicit none\n  ! subroutine implementation\nend subroutine " + called;
        globalPlan.push_back(mismatchEntry);
      }
    }
  }

  // Sort global plan
  std::sort(globalPlan.begin(), globalPlan.end(), [](auto &a, auto &b) {
    if (a.priority == b.priority) {
      return a.summary < b.summary;
    }
    return a.priority > b.priority;
  });

  std::cout << "Modernization plan for " << inputPath << ":\n";
  for (const auto &entry : globalPlan) {
    std::cout << "- " << entry.summary << " [effort=" << entry.effort << ", safety=" << entry.safety << "]\n";
  }

  if (!outputPath.empty()) {
    if (!writeJsonPlan(outputPath, inputPath, globalPlan)) {
      std::cerr << "Error: failed to write output file " << outputPath << std::endl;
      return 3;
    }
    std::cout << "Wrote JSON plan to " << outputPath << "\n";
  }

  return 0;
}
