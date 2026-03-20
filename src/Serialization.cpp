#include "Serialization.h"

#include "Menu.h"

namespace sosng::serialization {
void SaveCallback(SKSE::SerializationInterface *a_skse) {
  Menu::GetSingleton()->GetWorkbench().Serialize(a_skse);
}

void LoadCallback(SKSE::SerializationInterface *a_skse) {
  Menu::GetSingleton()->GetWorkbench().Deserialize(a_skse);
}

void RevertCallback([[maybe_unused]] SKSE::SerializationInterface *a_skse) {
  Menu::GetSingleton()->GetWorkbench().Revert();
}
} // namespace sosng::serialization
