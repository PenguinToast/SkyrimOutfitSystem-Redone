#include "ui/ConditionFunctionMetadata.h"

#include "StringUtils.h"

#include <algorithm>
#include <array>
#include <string>

namespace sosr::ui::conditions {
namespace {

template <std::size_t N>
bool ContainsName(const std::array<std::string_view, N> &a_names,
                  std::string_view a_name) {
  return std::ranges::any_of(a_names, [&](const auto candidate) {
    return sosr::strings::CompareTextInsensitive(candidate, a_name) == 0;
  });
}

constexpr std::array kKnownObsoleteConditionFunctions = {
    std::string_view{"CanHaveFlames"},
    std::string_view{"GetActorsInHigh"},
    std::string_view{"GetAnimAction"},
    std::string_view{"GetCauseofDeath"},
    std::string_view{"GetClassDefaultMatch"},
    std::string_view{"GetCombatGroupMemberCount"},
    std::string_view{"GetConcussed"},
    std::string_view{"GetCrime"},
    std::string_view{"GetGroupMemberCount"},
    std::string_view{"GetGroupTargetCount"},
    std::string_view{"GetHitLocation"},
    std::string_view{"GetInfamy"},
    std::string_view{"GetInfamyNonViolent"},
    std::string_view{"GetInfamyViolent"},
    std::string_view{"GetIsAlignment"},
    std::string_view{"GetIsCreature"},
    std::string_view{"GetIsCreatureType"},
    std::string_view{"GetIsLockBroken"},
    std::string_view{"GetIsUsedItemEquipType"},
    std::string_view{"GetKillingBlowLimb"},
    std::string_view{"GetNoRumors"},
    std::string_view{"GetPlayerTeammate"},
    std::string_view{"GetPlayerTeammateCount"},
    std::string_view{"GetPersuasionNumber"},
    std::string_view{"GetPlantedExplosive"},
    std::string_view{"GetQuestVariable"},
    std::string_view{"GetShouldHelp"},
    std::string_view{"GetTotalPersuasionNumber"},
    std::string_view{"HasFlames"},
    std::string_view{"IsActorUsingATorch"},
    std::string_view{"IsHorseStolen"},
    std::string_view{"IsIdlePlaying"},
    std::string_view{"IsPS3"},
    std::string_view{"IsWeaponInList"},
    std::string_view{"IsWaiting"},
    std::string_view{"IsWin32"},
    std::string_view{"IsXBox"},
    std::string_view{"MenuMode"},
};

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
    std::string_view{"GetActorCrimePlayerEnemy"},
    std::string_view{"GetAlarmed"},
    std::string_view{"GetAllowWorldInteractions"},
    std::string_view{"GetArrestedState"},
    std::string_view{"GetAttacked"},
    std::string_view{"GetCannibal"},
    std::string_view{"GetCombatTargetHasKeyword"},
    std::string_view{"GetConcussed"},
    std::string_view{"GetCrime"},
    std::string_view{"GetDead"},
    std::string_view{"GetDefaultOpen"},
    std::string_view{"GetDestroyed"},
    std::string_view{"GetDetected"},
    std::string_view{"GetDisabled"},
    std::string_view{"GetDisease"},
    std::string_view{"GetEquipped"},
    std::string_view{"GetEquippedShout"},
    std::string_view{"GetHasNote"},
    std::string_view{"GetIgnoreCrime"},
    std::string_view{"GetIgnoreFriendlyHits"},
    std::string_view{"GetInCell"},
    std::string_view{"GetInCellParam"},
    std::string_view{"GetInContainer"},
    std::string_view{"GetInCurrentLoc"},
    std::string_view{"GetInCurrentLocAlias"},
    std::string_view{"GetInCurrentLocFormList"},
    std::string_view{"GetInFaction"},
    std::string_view{"GetInSameCell"},
    std::string_view{"GetIntimidateSuccess"},
    std::string_view{"GetInWorldspace"},
    std::string_view{"GetInZone"},
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
    std::string_view{"GetStageDone"},
    std::string_view{"GetTalkedToPC"},
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
         sosr::strings::ContainsInsensitive(
             std::string_view{a_command.helpString}, "obsolete");
}

bool ReturnsBooleanConditionResult(std::string_view a_name) {
  if (ContainsName(kExplicitBooleanConditionFunctions, a_name)) {
    return true;
  }

  return sosr::strings::StartsWithInsensitive(a_name, "GetIs") ||
         sosr::strings::StartsWithInsensitive(a_name, "Is") ||
         sosr::strings::StartsWithInsensitive(a_name, "Has") ||
         sosr::strings::StartsWithInsensitive(a_name, "Can") ||
         sosr::strings::StartsWithInsensitive(a_name, "Should") ||
         sosr::strings::StartsWithInsensitive(a_name, "Would") ||
         sosr::strings::StartsWithInsensitive(a_name, "Same");
}

} // namespace sosr::ui::conditions
