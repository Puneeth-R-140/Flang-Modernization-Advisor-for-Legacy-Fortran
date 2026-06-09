#pragma once

#include "LegacyFortranAdvisor/Types.h"
#include <vector>

#ifdef USE_FLANG_PARSER
#include "flang/Parser/parsing.h"

namespace LegacyFortranAdvisor {
struct FlangParserHolder {
  Fortran::parser::AllSources allSources;
  Fortran::parser::AllCookedSources allCookedSources{allSources};
  Fortran::parser::Parsing parsing{allCookedSources};
};
}
#endif

namespace LegacyFortranAdvisor {

class FlangASTPatternVisitor {
public:
#ifdef USE_FLANG_PARSER
  explicit FlangASTPatternVisitor(const Fortran::parser::AllCookedSources &cooked);
  std::vector<PatternFinding> analyze(const Fortran::parser::Program &program);
private:
  const Fortran::parser::AllCookedSources &cooked_;
#endif
};

} // namespace LegacyFortranAdvisor
