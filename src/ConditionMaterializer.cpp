#include "ConditionMaterializer.h"

#include "ArmorUtils.h"
#include "RE/A/ActorValueList.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstdint>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
using Clause = sosr::conditions::Clause;
using Comparator = sosr::conditions::Comparator;
using Connective = sosr::conditions::Connective;
using Definition = sosr::conditions::Definition;
using ParamType = RE::SCRIPT_PARAM_TYPE;

struct NativeLiteral {
  const RE::SCRIPT_FUNCTION *command{nullptr};
  std::string functionName;
  std::array<std::string, 2> arguments{};
  std::array<ParamType, 2> parameterTypes{ParamType::kForm, ParamType::kForm};
  std::uint16_t parameterCount{0};
  Comparator comparator{Comparator::Equal};
  std::string comparand{"1"};
};

using OrClause = std::vector<NativeLiteral>;
using ConditionCnf = std::vector<OrClause>;
using ConditionVisitSet = std::unordered_set<std::string>;

union ConditionParam {
  char c;
  std::int32_t i;
  float f;
  RE::TESForm *form;
};

std::string TrimText(std::string_view a_text) {
  std::size_t start = 0;
  while (start < a_text.size() &&
         std::isspace(static_cast<unsigned char>(a_text[start])) != 0) {
    ++start;
  }

  std::size_t end = a_text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(a_text[end - 1])) != 0) {
    --end;
  }

  return std::string(a_text.substr(start, end - start));
}

bool EqualsInsensitive(std::string_view a_left, std::string_view a_right) {
  return a_left.size() == a_right.size() &&
         std::ranges::equal(
             a_left, a_right, [](const char a_lhs, const char a_rhs) {
               return std::tolower(static_cast<unsigned char>(a_lhs)) ==
                      std::tolower(static_cast<unsigned char>(a_rhs));
             });
}

ParamType ResolveEditorParamType(const std::string_view a_functionName,
                                 const std::uint16_t a_paramIndex,
                                 const ParamType a_type) {
  if (a_paramIndex == 0 && EqualsInsensitive(a_functionName, "GetIsID")) {
    return ParamType::kActorBase;
  }

  return a_type;
}

std::string Uppercase(std::string_view a_text) {
  std::string result(a_text);
  std::ranges::transform(result, result.begin(),
                         [](const unsigned char a_char) {
                           return static_cast<char>(std::toupper(a_char));
                         });
  return result;
}

Comparator InvertComparator(const Comparator a_comparator) {
  switch (a_comparator) {
  case Comparator::Equal:
    return Comparator::NotEqual;
  case Comparator::NotEqual:
    return Comparator::Equal;
  case Comparator::Greater:
    return Comparator::LessOrEqual;
  case Comparator::GreaterOrEqual:
    return Comparator::Less;
  case Comparator::Less:
    return Comparator::GreaterOrEqual;
  case Comparator::LessOrEqual:
    return Comparator::Greater;
  }

  return Comparator::NotEqual;
}

std::string ComparatorToken(const Comparator a_comparator) {
  switch (a_comparator) {
  case Comparator::Equal:
    return "==";
  case Comparator::NotEqual:
    return "!=";
  case Comparator::Greater:
    return ">";
  case Comparator::GreaterOrEqual:
    return ">=";
  case Comparator::Less:
    return "<";
  case Comparator::LessOrEqual:
    return "<=";
  }

  return "==";
}

RE::CONDITION_ITEM_DATA::OpCode ToOpCode(const Comparator a_comparator) {
  switch (a_comparator) {
  case Comparator::Equal:
    return RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
  case Comparator::NotEqual:
    return RE::CONDITION_ITEM_DATA::OpCode::kNotEqualTo;
  case Comparator::Greater:
    return RE::CONDITION_ITEM_DATA::OpCode::kGreaterThan;
  case Comparator::GreaterOrEqual:
    return RE::CONDITION_ITEM_DATA::OpCode::kGreaterThanOrEqualTo;
  case Comparator::Less:
    return RE::CONDITION_ITEM_DATA::OpCode::kLessThan;
  case Comparator::LessOrEqual:
    return RE::CONDITION_ITEM_DATA::OpCode::kLessThanOrEqualTo;
  }

  return RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
}

const RE::SCRIPT_FUNCTION *
FindConditionFunction(const std::string_view a_name) {
  const auto trimmed = TrimText(a_name);
  if (trimmed.empty()) {
    return nullptr;
  }

  const auto *command =
      RE::SCRIPT_FUNCTION::LocateScriptCommand(trimmed.c_str());
  if (!command || !command->conditionFunction) {
    return nullptr;
  }

  return command;
}

template <class T> T *LookupTypedFormByToken(const std::string &a_token) {
  if (auto *form = RE::TESForm::LookupByEditorID(a_token)) {
    if (auto *typed = form->As<T>()) {
      return typed;
    }
  }

  auto *dataHandler = RE::TESDataHandler::GetSingleton();
  if (!dataHandler) {
    return nullptr;
  }

  for (auto *form : dataHandler->GetFormArray<T>()) {
    if (sosr::armor::GetEditorID(form) == a_token) {
      return form;
    }
  }

  return nullptr;
}

RE::TESObjectREFR *LookupReferenceByToken(const std::string &a_token) {
  if (EqualsInsensitive(a_token, "Player")) {
    return RE::PlayerCharacter::GetSingleton();
  }

  if (auto *form = RE::TESForm::LookupByEditorID(a_token)) {
    if (auto *ref = form->As<RE::TESObjectREFR>()) {
      return ref;
    }
  }

  return nullptr;
}

RE::TESForm *LookupGenericFormByToken(const std::string &a_token) {
  if (auto *form = RE::TESForm::LookupByEditorID(a_token)) {
    return form;
  }

  auto *dataHandler = RE::TESDataHandler::GetSingleton();
  if (!dataHandler) {
    return nullptr;
  }

  for (const auto &forms : dataHandler->formArrays) {
    for (auto *form : forms) {
      if (form && sosr::armor::GetEditorID(form) == a_token) {
        return form;
      }
    }
  }

  return nullptr;
}

ConditionParam ParseParam(const std::string &a_text, const ParamType a_type) {
  ConditionParam param{};
  const auto trimmed = TrimText(a_text);

  switch (a_type) {
  case ParamType::kChar:
  case ParamType::kInt:
  case ParamType::kStage:
  case ParamType::kRelationshipRank:
    param.i = std::stoi(trimmed);
    break;
  case ParamType::kFloat:
    param.f = std::stof(trimmed);
    break;
  case ParamType::kActorValue: {
    auto actorValue =
        RE::ActorValueList::LookupActorValueByName(trimmed.c_str());
    if (actorValue == RE::ActorValue::kNone) {
      actorValue = RE::ActorValueList::LookupActorValueByName(
          Uppercase(trimmed).c_str());
    }
    param.i = static_cast<std::int32_t>(std::to_underlying(actorValue));
    break;
  }
  case ParamType::kAxis:
    param.i = EqualsInsensitive(trimmed, "X")
                  ? 0
                  : (EqualsInsensitive(trimmed, "Y") ? 1 : 2);
    break;
  case ParamType::kSex:
    param.i = EqualsInsensitive(trimmed, "MALE")
                  ? static_cast<std::int32_t>(RE::SEX::kMale)
                  : static_cast<std::int32_t>(RE::SEX::kFemale);
    break;
  case ParamType::kCastingSource:
    if (EqualsInsensitive(trimmed, "LEFT")) {
      param.i =
          static_cast<std::int32_t>(RE::MagicSystem::CastingSource::kLeftHand);
    } else if (EqualsInsensitive(trimmed, "RIGHT")) {
      param.i =
          static_cast<std::int32_t>(RE::MagicSystem::CastingSource::kRightHand);
    } else if (EqualsInsensitive(trimmed, "VOICE")) {
      param.i =
          static_cast<std::int32_t>(RE::MagicSystem::CastingSource::kOther);
    } else {
      param.i =
          static_cast<std::int32_t>(RE::MagicSystem::CastingSource::kInstant);
    }
    break;
  case ParamType::kObjectRef:
    param.form = LookupReferenceByToken(trimmed);
    break;
  case ParamType::kActor:
  case ParamType::kActorBase:
  case ParamType::kNPC:
    param.form = LookupTypedFormByToken<RE::TESNPC>(trimmed);
    break;
  case ParamType::kRace:
    param.form = LookupTypedFormByToken<RE::TESRace>(trimmed);
    break;
  case ParamType::kClass:
    param.form = LookupTypedFormByToken<RE::TESClass>(trimmed);
    break;
  case ParamType::kFaction:
    param.form = LookupTypedFormByToken<RE::TESFaction>(trimmed);
    break;
  case ParamType::kGlobal:
    param.form = LookupTypedFormByToken<RE::TESGlobal>(trimmed);
    break;
  case ParamType::kQuest:
    param.form = LookupTypedFormByToken<RE::TESQuest>(trimmed);
    break;
  case ParamType::kKeyword:
    param.form = LookupTypedFormByToken<RE::BGSKeyword>(trimmed);
    break;
  case ParamType::kPerk:
    param.form = LookupTypedFormByToken<RE::BGSPerk>(trimmed);
    break;
  case ParamType::kVoiceType:
    param.form = LookupTypedFormByToken<RE::BGSVoiceType>(trimmed);
    break;
  case ParamType::kCell:
    param.form = LookupTypedFormByToken<RE::TESObjectCELL>(trimmed);
    break;
  case ParamType::kLocation:
    param.form = LookupTypedFormByToken<RE::BGSLocation>(trimmed);
    break;
  case ParamType::kWeather:
    param.form = LookupTypedFormByToken<RE::TESWeather>(trimmed);
    break;
  case ParamType::kShout:
    param.form = LookupTypedFormByToken<RE::TESShout>(trimmed);
    break;
  case ParamType::kWordOfPower:
    param.form = LookupTypedFormByToken<RE::TESWordOfPower>(trimmed);
    break;
  case ParamType::kFormList:
    param.form = LookupTypedFormByToken<RE::BGSListForm>(trimmed);
    break;
  case ParamType::kSpellItem:
    param.form = LookupTypedFormByToken<RE::SpellItem>(trimmed);
    break;
  case ParamType::kObject:
  case ParamType::kInventoryObject:
  case ParamType::kKnowableForm:
  case ParamType::kForm:
  default:
    param.form = LookupGenericFormByToken(trimmed);
    break;
  }

  return param;
}

std::string BuildLiteralSignature(const NativeLiteral &a_literal) {
  std::string signature = a_literal.functionName;
  for (std::uint16_t paramIndex = 0; paramIndex < a_literal.parameterCount &&
                                     paramIndex < a_literal.arguments.size();
       ++paramIndex) {
    signature.push_back('(');
    signature.append(a_literal.arguments[paramIndex]);
    signature.push_back(')');
  }
  signature.push_back(' ');
  signature.append(ComparatorToken(a_literal.comparator));
  signature.push_back(' ');
  signature.append(a_literal.comparand);
  return signature;
}

std::string BuildCnfSignature(const ConditionCnf &a_cnf) {
  std::string signature;
  bool firstGroup = true;
  for (const auto &group : a_cnf) {
    if (!firstGroup) {
      signature.append(" AND ");
    }
    firstGroup = false;

    signature.push_back('(');
    bool firstLiteral = true;
    for (const auto &literal : group) {
      if (!firstLiteral) {
        signature.append(" OR ");
      }
      firstLiteral = false;
      signature.append(BuildLiteralSignature(literal));
    }
    signature.push_back(')');
  }
  return signature;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
ConditionCnf AndCnf(const ConditionCnf &a_left, const ConditionCnf &a_right) {
  ConditionCnf result = a_left;
  result.insert(result.end(), a_right.begin(), a_right.end());
  return result;
}

ConditionCnf OrCnf(const ConditionCnf &a_left, const ConditionCnf &a_right) {
  if (a_left.empty()) {
    return a_right;
  }
  if (a_right.empty()) {
    return a_left;
  }

  ConditionCnf result;
  result.reserve(a_left.size() * a_right.size());
  for (const auto &leftGroup : a_left) {
    for (const auto &rightGroup : a_right) {
      OrClause merged = leftGroup;
      merged.insert(merged.end(), rightGroup.begin(), rightGroup.end());
      result.push_back(std::move(merged));
    }
  }
  return result;
}

ConditionCnf NegateCnf(const ConditionCnf &a_cnf) {
  ConditionCnf result;
  bool firstGroup = true;

  for (const auto &group : a_cnf) {
    ConditionCnf negatedGroup;
    negatedGroup.reserve(group.size());
    for (const auto &literal : group) {
      auto negatedLiteral = literal;
      negatedLiteral.comparator = InvertComparator(negatedLiteral.comparator);
      negatedGroup.push_back(OrClause{std::move(negatedLiteral)});
    }

    if (firstGroup) {
      result = std::move(negatedGroup);
      firstGroup = false;
    } else {
      result = OrCnf(result, negatedGroup);
    }
  }

  return result;
}

std::optional<bool> EvaluateNestedConditionPolarity(const Clause &a_clause) {
  if (a_clause.comparator != Comparator::Equal &&
      a_clause.comparator != Comparator::NotEqual) {
    return std::nullopt;
  }

  const auto comparand = TrimText(a_clause.comparand);
  if (comparand != "0" && comparand != "1") {
    return std::nullopt;
  }

  const bool truthy = comparand == "1";
  return (a_clause.comparator == Comparator::Equal && truthy) ||
         (a_clause.comparator == Comparator::NotEqual && !truthy);
}

std::optional<NativeLiteral> BuildNativeLiteral(const Clause &a_clause) {
  const auto *command = FindConditionFunction(a_clause.functionName);
  if (!command) {
    return std::nullopt;
  }

  NativeLiteral literal;
  literal.command = command;
  literal.functionName = TrimText(a_clause.functionName);
  literal.parameterCount = command->numParams;
  literal.comparator = a_clause.comparator;
  literal.comparand = TrimText(a_clause.comparand);

  for (std::uint16_t paramIndex = 0;
       paramIndex < command->numParams && paramIndex < 2; ++paramIndex) {
    literal.arguments[paramIndex] = TrimText(a_clause.arguments[paramIndex]);
    literal.parameterTypes[paramIndex] = ResolveEditorParamType(
        literal.functionName, paramIndex,
        command->params ? command->params[paramIndex].paramType.get()
                        : ParamType::kForm);
  }

  return literal;
}

std::optional<ConditionCnf>
BuildConditionCnf(const Definition &a_definition,
                  const std::vector<Definition> &a_conditions,
                  ConditionVisitSet &a_visiting);

std::optional<ConditionCnf>
BuildClauseCnf(const Clause &a_clause,
               const std::vector<Definition> &a_conditions,
               ConditionVisitSet &a_visiting) {
  if (!a_clause.customConditionId.empty()) {
    const auto *definition = sosr::conditions::FindDefinitionById(
        a_conditions, a_clause.customConditionId);
    if (!definition) {
      return std::nullopt;
    }

    const auto polarity = EvaluateNestedConditionPolarity(a_clause);
    if (!polarity.has_value()) {
      return std::nullopt;
    }

    auto nestedCnf = BuildConditionCnf(*definition, a_conditions, a_visiting);
    if (!nestedCnf) {
      return std::nullopt;
    }

    if (*polarity) {
      return nestedCnf;
    }
    return NegateCnf(*nestedCnf);
  }

  auto literal = BuildNativeLiteral(a_clause);
  if (!literal) {
    return std::nullopt;
  }

  return ConditionCnf{OrClause{std::move(*literal)}};
}

std::optional<ConditionCnf>
BuildConditionCnf(const Definition &a_definition,
                  const std::vector<Definition> &a_conditions,
                  ConditionVisitSet &a_visiting) {
  if (a_definition.clauses.empty()) {
    return std::nullopt;
  }
  if (!a_definition.id.empty() && !a_visiting.insert(a_definition.id).second) {
    return std::nullopt;
  }

  auto currentBlock =
      BuildClauseCnf(a_definition.clauses.front(), a_conditions, a_visiting);
  if (!currentBlock) {
    if (!a_definition.id.empty()) {
      a_visiting.erase(a_definition.id);
    }
    return std::nullopt;
  }

  ConditionCnf result;
  bool hasResult = false;

  for (std::size_t index = 1; index < a_definition.clauses.size(); ++index) {
    auto clauseCnf =
        BuildClauseCnf(a_definition.clauses[index], a_conditions, a_visiting);
    if (!clauseCnf) {
      if (!a_definition.id.empty()) {
        a_visiting.erase(a_definition.id);
      }
      return std::nullopt;
    }

    const auto connective = a_definition.clauses[index - 1].connectiveToNext;
    if (connective == Connective::Or) {
      currentBlock = OrCnf(*currentBlock, *clauseCnf);
    } else {
      if (!hasResult) {
        result = *currentBlock;
        hasResult = true;
      } else {
        result = AndCnf(result, *currentBlock);
      }
      currentBlock = std::move(clauseCnf);
    }
  }

  if (!hasResult) {
    result = *currentBlock;
  } else {
    result = AndCnf(result, *currentBlock);
  }

  if (!a_definition.id.empty()) {
    a_visiting.erase(a_definition.id);
  }
  return result;
}

std::optional<RE::CONDITION_ITEM_DATA>
BuildConditionItemData(const NativeLiteral &a_literal,
                       const bool a_isORToNext) {
  RE::CONDITION_ITEM_DATA data{};

  const auto functionIndex =
      std::to_underlying(a_literal.command->output) - 0x1000;
  data.functionData.function =
      static_cast<RE::FUNCTION_DATA::FunctionID>(functionIndex);

  for (std::uint16_t paramIndex = 0;
       paramIndex < a_literal.parameterCount && paramIndex < 2; ++paramIndex) {
    const auto &argument = a_literal.arguments[paramIndex];
    if (argument.empty()) {
      continue;
    }

    const auto param =
        ParseParam(argument, a_literal.parameterTypes[paramIndex]);
    if ((a_literal.parameterTypes[paramIndex] != ParamType::kChar &&
         a_literal.parameterTypes[paramIndex] != ParamType::kInt &&
         a_literal.parameterTypes[paramIndex] != ParamType::kStage &&
         a_literal.parameterTypes[paramIndex] != ParamType::kRelationshipRank &&
         a_literal.parameterTypes[paramIndex] != ParamType::kFloat &&
         a_literal.parameterTypes[paramIndex] != ParamType::kActorValue &&
         a_literal.parameterTypes[paramIndex] != ParamType::kAxis &&
         a_literal.parameterTypes[paramIndex] != ParamType::kSex &&
         a_literal.parameterTypes[paramIndex] != ParamType::kCastingSource) &&
        !param.form) {
      return std::nullopt;
    }

    data.functionData.params[paramIndex] = std::bit_cast<void *>(param);
  }

  data.flags.opCode = ToOpCode(a_literal.comparator);
  data.flags.isOR = a_isORToNext;
  data.comparisonValue.f = std::stof(a_literal.comparand);
  data.flags.global = false;
  return data;
}
} // namespace

namespace sosr::conditions {
void RebuildConditionDependencyMetadata(std::vector<Definition> &a_conditions) {
  for (auto &condition : a_conditions) {
    condition.referencedConditionIds.clear();
    condition.reverseDependencyIds.clear();

    std::unordered_set<std::string> seenReferencedConditionIds;
    for (const auto &clause : condition.clauses) {
      if (clause.customConditionId.empty()) {
        continue;
      }
      if (seenReferencedConditionIds.insert(clause.customConditionId).second) {
        condition.referencedConditionIds.push_back(clause.customConditionId);
      }
    }
  }

  for (auto &condition : a_conditions) {
    for (const auto &referencedConditionId : condition.referencedConditionIds) {
      if (auto *referenced =
              FindDefinitionById(a_conditions, referencedConditionId)) {
        referenced->reverseDependencyIds.push_back(condition.id);
      }
    }
  }
}

void InvalidateConditionMaterializationCaches(
    std::vector<Definition> &a_conditions) {
  for (auto &condition : a_conditions) {
    condition.transientCache.Clear();
  }
}

void InvalidateConditionMaterializationCachesFrom(
    std::vector<Definition> &a_conditions, const std::string_view a_conditionId) {
  std::vector<std::string> stack;
  std::unordered_set<std::string> visited;
  stack.emplace_back(a_conditionId);

  while (!stack.empty()) {
    auto conditionId = std::move(stack.back());
    stack.pop_back();

    if (!visited.insert(conditionId).second) {
      continue;
    }

    auto *condition = FindDefinitionById(a_conditions, conditionId);
    if (!condition) {
      continue;
    }

    condition->transientCache.Clear();
    for (const auto &reverseDependencyId : condition->reverseDependencyIds) {
      stack.push_back(reverseDependencyId);
    }
  }
}

std::optional<MaterializedCondition>
MaterializeConditionById(std::string_view a_conditionId,
                         std::vector<Definition> &a_conditions) {
  auto *definition = FindDefinitionById(a_conditions, a_conditionId);
  if (!definition) {
    return std::nullopt;
  }
  if (definition->transientCache.valid && definition->transientCache.condition) {
    return MaterializedCondition{
        .condition = definition->transientCache.condition,
        .signature = definition->transientCache.signature,
        .refreshTargets =
            RefreshTargets{definition->transientCache.refreshActorFormIDs,
                           definition->transientCache.refreshUseNearbyFallback}};
  }

  ConditionVisitSet visiting;
  auto cnf = BuildConditionCnf(*definition, a_conditions, visiting);
  if (!cnf || cnf->empty()) {
    definition->transientCache.Clear();
    return std::nullopt;
  }

  auto condition = std::make_shared<RE::TESCondition>();
  RE::TESConditionItem *previous = nullptr;

  for (const auto &group : *cnf) {
    for (std::size_t literalIndex = 0; literalIndex < group.size();
         ++literalIndex) {
      const auto data = BuildConditionItemData(group[literalIndex],
                                               literalIndex + 1 < group.size());
      if (!data) {
        definition->transientCache.Clear();
        return std::nullopt;
      }

      auto *item = new RE::TESConditionItem();
      item->data = *data;
      item->next = nullptr;
      if (previous) {
        previous->next = item;
      } else {
        condition->head = item;
      }
      previous = item;
    }
  }

  const auto refreshTargets = BuildRefreshTargets(condition);
  definition->transientCache.valid = true;
  definition->transientCache.condition = condition;
  definition->transientCache.signature = BuildCnfSignature(*cnf);
  definition->transientCache.refreshActorFormIDs.assign(
      refreshTargets.actorFormIDs.begin(), refreshTargets.actorFormIDs.end());
  definition->transientCache.refreshUseNearbyFallback =
      refreshTargets.useNearbyFallback;

  return MaterializedCondition{
      .condition = definition->transientCache.condition,
      .signature = definition->transientCache.signature,
      .refreshTargets = refreshTargets};
}
} // namespace sosr::conditions
