#include "Serialization.h"

#include "ui/ConditionParamOptionCache.h"
#include "ui/Menu.h"

namespace sosr::serialization {
void SaveCallback(SKSE::SerializationInterface *a_skse) {
  Menu::GetSingleton()->GetWorkbench().Serialize(a_skse);
}

void LoadCallback(SKSE::SerializationInterface *a_skse) {
  ui::conditions::ConditionParamOptionCache::Get().Reset();
  Menu::GetSingleton()->GetWorkbench().Deserialize(a_skse);
}

void RevertCallback([[maybe_unused]] SKSE::SerializationInterface *a_skse) {
  ui::conditions::ConditionParamOptionCache::Get().Reset();
  Menu::GetSingleton()->GetWorkbench().Revert();
}
} // namespace sosr::serialization
