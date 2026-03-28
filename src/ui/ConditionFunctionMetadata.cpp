#include "ui/ConditionFunctionMetadata.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace sosr::ui::conditions {
namespace {

int CompareTextInsensitive(std::string_view a_left, std::string_view a_right) {
  const auto leftSize = a_left.size();
  const auto rightSize = a_right.size();
  const auto count = (std::min)(leftSize, rightSize);

  for (std::size_t index = 0; index < count; ++index) {
    const auto left = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_left[index])));
    const auto right = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_right[index])));
    if (left < right) {
      return -1;
    }
    if (left > right) {
      return 1;
    }
  }

  if (leftSize < rightSize) {
    return -1;
  }
  if (leftSize > rightSize) {
    return 1;
  }
  return 0;
}

bool StartsWithInsensitive(std::string_view a_text, std::string_view a_prefix) {
  return a_text.size() >= a_prefix.size() &&
         CompareTextInsensitive(a_text.substr(0, a_prefix.size()), a_prefix) ==
             0;
}

bool ContainsInsensitive(std::string_view a_text,
                         std::string_view a_pattern) {
  if (a_pattern.empty()) {
    return true;
  }

  const auto lower = [](std::string_view a_value) {
    std::string text;
    text.reserve(a_value.size());
    for (const auto ch : a_value) {
      text.push_back(static_cast<char>(
          std::tolower(static_cast<unsigned char>(ch))));
    }
    return text;
  };

  return lower(a_text).find(lower(a_pattern)) != std::string::npos;
}

template <std::size_t N>
bool ContainsName(const std::array<std::string_view, N> &a_names,
                  std::string_view a_name) {
  return std::ranges::any_of(a_names, [&](const auto candidate) {
    return CompareTextInsensitive(candidate, a_name) == 0;
  });
}

constexpr std::array kKnownObsoleteConditionFunctions = {
    std::string_view{"CanHaveFlames"},
    std::string_view{"GetAnimAction"},
    std::string_view{"GetCauseofDeath"},
    std::string_view{"GetClassDefaultMatch"},
    std::string_view{"GetConcussed"},
    std::string_view{"GetHitLocation"},
    std::string_view{"GetInfamy"},
    std::string_view{"GetInfamyNonViolent"},
    std::string_view{"GetInfamyViolent"},
    std::string_view{"GetIsAlignment"},
    std::string_view{"GetIsCreature"},
    std::string_view{"GetIsCreatureType"},
    std::string_view{"GetIsLockBroken"},
    std::string_view{"GetKillingBlowLimb"},
    std::string_view{"GetNoRumors"},
    std::string_view{"GetPersuasionNumber"},
    std::string_view{"GetPlantedExplosive"},
    std::string_view{"GetTotalPersuasionNumber"},
    std::string_view{"HasFlames"},
    std::string_view{"IsHorseStolen"},
    std::string_view{"IsIdlePlaying"},
    std::string_view{"IsPS3"},
    std::string_view{"IsWaiting"},
    std::string_view{"IsWin32"},
    std::string_view{"IsXBox"},
    std::string_view{"MenuMode"}};

// Explicit boolean additions not already covered by the name heuristic below.
// Most were derived from a live CK wiki API walk of Condition_Functions and
// function subpages on 2026-03-28. A few are obvious non-heuristic cases such
// as GetDead/Exists.
constexpr std::array kExplicitBooleanConditionFunctions = {
    std::string_view{"DoesNotExist"},
    std::string_view{"Exists"},
    std::string_view{"EffectWasDualCast"},
    std::string_view{"EPMagicSpellHasSkill"},
    std::string_view{"EPModSkillUsageIsAdvanceSkill"},
    std::string_view{"EPTemperingItemIsEnchanted"},
    std::string_view{"GetAllowWorldInteractions"},
    std::string_view{"GetAnimAction"},
    std::string_view{"GetArrestedState"},
    std::string_view{"GetAttacked"},
    std::string_view{"GetCannibal"},
    std::string_view{"GetCauseofDeath"},
    std::string_view{"GetClassDefaultMatch"},
    std::string_view{"GetCombatTargetHasKeyword"},
    std::string_view{"GetConcussed"},
    std::string_view{"GetCrime"},
    std::string_view{"GetDead"},
    std::string_view{"GetDefaultOpen"},
    std::string_view{"GetDestroyed"},
    std::string_view{"GetDetected"},
    std::string_view{"GetDisabled"},
    std::string_view{"GetEquipped"},
    std::string_view{"GetEquippedShout"},
    std::string_view{"GetHasNote"},
    std::string_view{"GetInCell"},
    std::string_view{"GetInCellParam"},
    std::string_view{"GetInCurrentLoc"},
    std::string_view{"GetInCurrentLocAlias"},
    std::string_view{"GetInCurrentLocFormList"},
    std::string_view{"GetInFaction"},
    std::string_view{"GetInfamy"},
    std::string_view{"GetInfamyNonViolent"},
    std::string_view{"GetInfamyViolent"},
    std::string_view{"GetInSameCell"},
    std::string_view{"GetIntimidateSuccess"},
    std::string_view{"GetInWorldspace"},
    std::string_view{"GetInZone"},
    std::string_view{"GetKillingBlowLimb"},
    std::string_view{"GetKnockedState"},
    std::string_view{"GetLocationAliasCleared"},
    std::string_view{"GetLocked"},
    std::string_view{"GetNoRumors"},
    std::string_view{"GetOffersServicesNow"},
    std::string_view{"GetPCExpelled"},
    std::string_view{"GetPCIsClass"},
    std::string_view{"GetPCIsSex"},
    std::string_view{"GetPlayerControlsDisabled"},
    std::string_view{"GetPlayerTeammate"},
    std::string_view{"GetQuestRunning"},
    std::string_view{"GetRestrained"},
    std::string_view{"GetShouldHelp"},
    std::string_view{"GetSitting"},
    std::string_view{"GetStageDone"},
    std::string_view{"GetTalkedToPC"},
    std::string_view{"GetTotalPersuasionNumber"},
    std::string_view{"GetUnconscious"},
    std::string_view{"GetVampireFeed"},
    std::string_view{"LocAliasIsLocation"},
    std::string_view{"LocationHasKeyword"},
    std::string_view{"LocationHasRefType"},
    std::string_view{"PlayerKnows"},
    std::string_view{"WornHasKeyword"}};

} // namespace

bool IsObsoleteConditionFunction(std::string_view a_name) {
  return ContainsName(kKnownObsoleteConditionFunctions, a_name);
}

bool IsObsoleteConditionFunction(const RE::SCRIPT_FUNCTION &a_command) {
  if (a_command.functionName &&
      IsObsoleteConditionFunction(std::string_view{a_command.functionName})) {
    return true;
  }

  return a_command.helpString != nullptr &&
         ContainsInsensitive(std::string_view{a_command.helpString},
                             "obsolete");
}

bool ReturnsBooleanConditionResult(std::string_view a_name) {
  if (ContainsName(kExplicitBooleanConditionFunctions, a_name)) {
    return true;
  }

  return StartsWithInsensitive(a_name, "GetIs") ||
         StartsWithInsensitive(a_name, "Is") ||
         StartsWithInsensitive(a_name, "Has") ||
         StartsWithInsensitive(a_name, "Can") ||
         StartsWithInsensitive(a_name, "Should") ||
         StartsWithInsensitive(a_name, "Would") ||
         StartsWithInsensitive(a_name, "Same");
}

} // namespace sosr::ui::conditions
