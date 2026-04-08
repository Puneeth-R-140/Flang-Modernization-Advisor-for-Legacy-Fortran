#pragma once

#include "LegacyFortranAdvisor/Types.h"
#include <vector>

namespace LegacyFortranAdvisor {

class Prioritization {
public:
  std::vector<PlanEntry> generatePlan(const std::vector<ImpactSummary> &impacts);
};

} // namespace LegacyFortranAdvisor
