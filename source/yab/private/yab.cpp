#include <yab/private/yaml-deserializer.h>

namespace yab::details {
namespace {
std::unique_ptr<dynamic_deserialization_registry_type>
    dynamic_deserialization_registry = nullptr;
}

dynamic_deserialization_registry_type &get_dynamic_deserialization_registry() {
    if (!dynamic_deserialization_registry) {
        dynamic_deserialization_registry
            = std::make_unique<dynamic_deserialization_registry_type>();
    }
    return *dynamic_deserialization_registry;
}
} // namespace yab::details
