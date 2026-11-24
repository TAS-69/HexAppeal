#include "EquippedDefinition.hpp"

namespace equipped_example {
static hexappeal::WardrobeClient client;
static uint64_t registration = 0;

bool Update(bool resourcesReady, const std::string& unavailableReason, std::string& error) {
    if (!client.Connect()) {
        error = client.LastError();
        return false;
    }
    if (!client.HasItemControls()) {
        error = "This Hex Appeal build has no equipped item controls.";
        return false;
    }
    if (!registration && Register_example_emberglass_cloak(client, registration) != HA_OK) {
        error = client.LastError();
        return false;
    }
    if (client.Available(registration, resourcesReady, unavailableReason.c_str()) != HA_OK) {
        error = client.LastError();
        return false;
    }
    error.clear();
    return true;
}

bool Shutdown(std::string& error) {
    const auto result = client.Unregister(registration);
    if (result != HA_OK && result != HA_NOT_FOUND) {
        error = client.LastError();
        return false;
    }
    client.Reset();
    error.clear();
    return true;
}
}
