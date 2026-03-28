#include "Serialization.h"

#include "ui/ConditionParamOptionCache.h"
#include "ui/Menu.h"

namespace sosr::serialization {
void SaveCallback(SKSE::SerializationInterface *a_skse) {
  Menu::GetSingleton()->GetWorkbench().Serialize(a_skse);
  Menu::GetSingleton()->SerializeConditions(a_skse);
}

void LoadCallback(SKSE::SerializationInterface *a_skse) {
  ui::conditions::ConditionParamOptionCache::Get().Reset();
  Menu::GetSingleton()->GetWorkbench().Deserialize(a_skse);
  Menu::GetSingleton()->DeserializeConditions(a_skse);
  Menu::GetSingleton()->GetWorkbench().SyncDynamicArmorVariantsExtended(
      Menu::GetSingleton()->GetConditions());
}

void RevertCallback([[maybe_unused]] SKSE::SerializationInterface *a_skse) {
  ui::conditions::ConditionParamOptionCache::Get().Reset();
  Menu::GetSingleton()->GetWorkbench().Revert();
  Menu::GetSingleton()->RevertConditions();
}
} // namespace sosr::serialization
