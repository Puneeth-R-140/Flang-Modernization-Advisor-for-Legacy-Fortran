#include "LegacyFortranAdvisor/FlangFrontend.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace LegacyFortranAdvisor {

std::unique_ptr<SourceAnalysis> FlangFrontend::parseFile(const std::string &path) {
  std::ifstream input(path);
  if (!input) {
    std::cerr << "[FlangFrontend] Failed to open source file: " << path << "\n";
    return nullptr;
  }

  auto analysis = std::make_unique<SourceAnalysis>();
  analysis->filePath = path;

  std::string line;
  while (std::getline(input, line)) {
    analysis->lines.push_back(line);
  }

  std::filesystem::path sourcePath(path);
  std::string extension = sourcePath.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  analysis->fixedFormHint = (extension == ".f" || extension == ".for" || extension == ".f77");

  std::cerr << "[FlangFrontend] Parsed " << analysis->lines.size() << " line(s) from " << path << "\n";
  return analysis;
}

} // namespace LegacyFortranAdvisor
