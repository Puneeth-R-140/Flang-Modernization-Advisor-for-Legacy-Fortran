#pragma once

#include <memory>
#include <string>
#include "LegacyFortranAdvisor/Types.h"

namespace LegacyFortranAdvisor {

class FlangFrontend {
public:
  std::unique_ptr<SourceAnalysis> parseFile(const std::string &path);
};

} // namespace LegacyFortranAdvisor
