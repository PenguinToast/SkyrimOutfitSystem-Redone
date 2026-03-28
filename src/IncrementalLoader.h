#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace sosr {
class IncrementalLoader {
public:
  struct Phase {
    std::string status;
    std::size_t unitCount{0};
    std::function<void(std::size_t)> processUnit;
  };

  void Start(std::vector<Phase> a_phases) {
    phases_ = std::move(a_phases);
    phaseIndex_ = 0;
    phaseUnitIndex_ = 0;
    completedUnits_ = 0;
    totalUnits_ = 0;
    for (const auto &phase : phases_) {
      totalUnits_ += phase.unitCount;
    }

    running_ = !phases_.empty();
    status_ = running_ ? phases_.front().status : std::string{};
    AdvancePastEmptyPhases();
  }

  [[nodiscard]] bool Continue(const double a_maxMillisecondsPerTick) {
    if (!running_) {
      return false;
    }

    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    const auto budget =
        std::chrono::duration<double, std::milli>(a_maxMillisecondsPerTick);

    while (running_ && (clock::now() - start) < budget) {
      if (phaseIndex_ >= phases_.size()) {
        running_ = false;
        status_.clear();
        break;
      }

      auto &phase = phases_[phaseIndex_];
      if (phaseUnitIndex_ >= phase.unitCount) {
        ++phaseIndex_;
        phaseUnitIndex_ = 0;
        AdvancePastEmptyPhases();
        continue;
      }

      if (phase.processUnit) {
        phase.processUnit(phaseUnitIndex_);
      }
      ++phaseUnitIndex_;
      ++completedUnits_;
    }

    return running_;
  }

  [[nodiscard]] bool IsRunning() const { return running_; }

  [[nodiscard]] float GetProgress() const {
    if (!running_) {
      return 1.0f;
    }

    const auto totalUnits = (std::max)(totalUnits_, std::size_t{1});
    return static_cast<float>(completedUnits_) / static_cast<float>(totalUnits);
  }

  [[nodiscard]] std::string_view GetStatus() const { return status_; }

private:
  void AdvancePastEmptyPhases() {
    while (phaseIndex_ < phases_.size() &&
           phases_[phaseIndex_].unitCount == 0) {
      ++phaseIndex_;
      phaseUnitIndex_ = 0;
    }

    if (phaseIndex_ >= phases_.size()) {
      running_ = false;
      status_.clear();
      return;
    }

    status_ = phases_[phaseIndex_].status;
  }

  std::vector<Phase> phases_;
  std::size_t phaseIndex_{0};
  std::size_t phaseUnitIndex_{0};
  std::size_t completedUnits_{0};
  std::size_t totalUnits_{0};
  bool running_{false};
  std::string status_;
};
} // namespace sosr
