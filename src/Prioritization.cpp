#include "LegacyFortranAdvisor/Prioritization.h"
#include <algorithm>
#include <iostream>

namespace LegacyFortranAdvisor {

namespace {

std::string effortLabel(int dependentConstructs, bool behaviorChangeRisk) {
  if (dependentConstructs >= 4 || (dependentConstructs >= 3 && behaviorChangeRisk)) {
    return "high";
  }
  if (dependentConstructs >= 2) {
    return "moderate";
  }
  return "low";
}

std::string safetyLabel(bool aliasingRisk, bool behaviorChangeRisk) {
  if (aliasingRisk || behaviorChangeRisk) {
    return "review-needed";
  }
  return "safe";
}

} // namespace

std::vector<PlanEntry> Prioritization::generatePlan(const std::vector<ImpactSummary> &impacts) {
  std::cerr << "[Prioritization] Generating modernization plan from " << impacts.size()
            << " impact item(s)\n";
  std::vector<PlanEntry> plan;

  for (const auto &entry : impacts) {
    PlanEntry item;
    item.summary = entry.finding.message + " (" + entry.finding.location + ")";
    item.effort = effortLabel(entry.dependentConstructs, entry.behaviorChangeRisk);
    item.safety = safetyLabel(entry.aliasingRisk, entry.behaviorChangeRisk);
    item.legacyConstruct = entry.finding.legacyConstruct;
    item.modernEquivalent = entry.finding.modernEquivalent;

    int priority = 40;
    priority += entry.dependentConstructs * 10;
    if (entry.aliasingRisk) {
      priority += 20;
    }
    if (entry.behaviorChangeRisk) {
      priority += 15;
    }
    priority = std::clamp(priority, 1, 100);
    item.priority = priority;

    plan.push_back(item);
  }

  std::sort(plan.begin(), plan.end(), [](auto &a, auto &b) {
    if (a.priority == b.priority) {
      return a.summary < b.summary;
    }
    return a.priority > b.priority;
  });
  return plan;
}

} // namespace LegacyFortranAdvisor
