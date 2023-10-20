#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <yab/private/details.h>
#include <yaml-cpp/yaml.h>

namespace yab {
class lack_of_essentials_error : public std::runtime_error {
public:
    lack_of_essentials_error(const std::type_info &type_info)
        : std::runtime_error{std::string{
                                 "Lack of essentials while deserializing "}
                             + type_info.name()},
          _type_index{type_info} {
    }

    auto type_index() const -> std::type_index {
        return _type_index;
    }

    const std::type_index _type_index;
};

class syntax_error : public std::runtime_error {
public:
    using runtime_error::runtime_error;
};

class type_not_found_error : public std::runtime_error {
public:
    type_not_found_error(const std::string &type_name)
        : std::runtime_error{"Dynamic type name " + type_name + " not found"} {
    }
};

class lack_of_required_property_error : public std::runtime_error {
public:
    lack_of_required_property_error(const std::string &property_name_)
        : std::runtime_error{"Property " + property_name_ + " not found"} {
    }
};

class unrecognized_enumerator_error : public std::runtime_error {
public:
    unrecognized_enumerator_error(const std::string &enumerator_name)
        : std::runtime_error{"Unrecognized enumerator " + enumerator_name} {
    }
};

template <typename Ty>
auto allow_dynamic(const std::string &name_) -> const std::string &;

class yaml_deserializer {
    template <typename Ty>
    friend auto allow_dynamic(const std::string &name_) -> const std::string &;

    struct yaml_node_hasher {
        inline auto operator()(const YAML::Node node) const noexcept
            -> std::size_t {
            return 0;
        }
    };

public:
    template <typename T> using dynamic_deleter = std::function<void(T *)>;

    yaml_deserializer(YAML::Node node)
        : _node(node), _context{std::make_shared<Context>()} {
    }

    template <typename T> auto as() {
        if constexpr (std::is_arithmetic_v<T>
                      || std::is_same_v<T, std::string>) {
            return _node.as<T>();
        } else if constexpr (std::is_enum_v<T>) {
            return _as_enum<T>();
        } else if constexpr (std::is_pointer_v<T>) {
            return _as_pointer<std::remove_pointer_t<T>>();
        } else if constexpr (details::is_vector_v<T>) {
            return _as_vector<typename T::value_type>();
        } else if constexpr (details::is_optional_v<T>) {
            return _as_optional<typename T::value_type>();
        } else if constexpr (deserializable<T>) {
            return _as_object<T>();
        } else {
            static_assert((sizeof(T), false),
                          "Specified type is not deserializable.");
        }
    }

    template <typename T> auto as_dynamic() {
        if constexpr (details::is_vector_v<T>) {
            using ElementType = T::value_type;
            const auto nodes = _node.as<std::vector<YAML::Node>>();
            std::vector<ElementType> v;
            v.reserve(nodes.size());
            for (const auto &node : nodes) {
                v.push_back(_fork(node).as_dynamic<ElementType>());
            }
            return v;
        } else if constexpr (std::is_pointer_v<T>) {
            using ElementType = std::remove_pointer_t<T>;
            auto const [ptr, deleter] = _construct_dynamic<ElementType>();
            // TODO: deleter is discarded.
            return reinterpret_cast<ElementType *>(ptr);
        } else if constexpr (details::is_unique_ptr_v<T>) {
            using ElementType = std::pointer_traits<T>::element_type;
            auto const [ptr, deleter] = _construct_dynamic<ElementType>();
            auto const std_deleter = dynamic_deleter<ElementType>{
                [deleter](ElementType *ptr) { deleter(ptr); }};
            return std::unique_ptr<ElementType, dynamic_deleter<ElementType>>{
                reinterpret_cast<ElementType *>(ptr),
                std_deleter,
            };
        } else if constexpr (details::is_shared_ptr_v<T>) {
            using ElementType = std::pointer_traits<T>::element_type;
            if (auto r = _context->shared_node_map.find(_node);
                r != _context->shared_node_map.end()) {
                return std::static_pointer_cast<ElementType>(r->second);
            } else {
                auto const [ptr, deleter] = _construct_dynamic<ElementType>();
                auto const shared = std::shared_ptr<ElementType>{
                    reinterpret_cast<ElementType *>(ptr),
                    deleter};
                _context->shared_node_map.emplace(_node, shared);
                return shared;
            }
        } else {
            static_assert((sizeof(T), false),
                          "T should be recursive vectors of raw "
                          "pointer/unique_ptr/shared_ptr.");
        }
    }

    template <typename T> auto get(const std::string &property) -> T {
        const auto property_node = _node[property];
        if (!details::is_optional_v<T> && !property_node) {
            throw lack_of_required_property_error{property};
        } else {
            return _fork(property_node).as<T>();
        }
    }

    template <typename T, typename U>
    auto get_or(const std::string &property, U &&default_value) -> T {
        const auto property_node = _node[property];
        if (!property_node) {
            return default_value;
        } else {
            return _fork(property_node).as<T>();
        }
    }

    template <typename T>
    auto get_optional(const std::string &property) -> std::optional<T> {
        return _fork_property(property).as<std::optional<T>>();
    }

    template <typename T> auto get_dynamic(const std::string &property) {
        return _fork_property(property).as_dynamic<T>();
    }

    template <typename T, typename U>
    auto get_dynamic_or(const std::string &property, U &&default_value) -> T {
        const auto property_node = _node[property];
        if (!property_node) {
            return default_value;
        } else {
            return _fork(property_node).as_dynamic<T>();
        }
    }

    template <typename T> auto get_essentials() -> std::shared_ptr<T> {
        const auto r
            = _context->essentials_map.find(std::type_index{typeid(T)});
        if (r != _context->essentials_map.end()) {
            return std::static_pointer_cast<T>(r->second);
        } else {
            throw lack_of_essentials_error{typeid(T)};
        }
    }

    template <typename T, typename... Args>
    auto emplace_essentials(Args &&...args) -> std::shared_ptr<T> {
        const auto essentials
            = std::make_shared<T>(std::forward<Args>(args)...);
        _context->essentials_map.emplace(typeid(T), essentials);
        return essentials;
    }

    auto has_property(const std::string &property) -> bool {
        return !!_node[property];
    }

private:
    struct Context {
        std::unordered_map<std::type_index, std::shared_ptr<void>>
            essentials_map;

        std::unordered_map<YAML::Node, std::shared_ptr<void>, yaml_node_hasher>
            shared_node_map;
    };

    YAML::Node _node;

    std::shared_ptr<Context> _context;

    yaml_deserializer(YAML::Node node, std::shared_ptr<Context> context)
        : _node(node), _context(context) {
    }

    auto _fork(YAML::Node node) -> yaml_deserializer {
        return {node, _context};
    }

    auto _fork_property(const std::string &property) -> yaml_deserializer {
        return _fork(_node[property]);
    }

    template <typename T>
    auto _construct_type_erased_static_ptr() -> details::type_erased_ptr {
        const auto deserialize_result
            = serialization_traits<T>::deserialize(*this);
        using DeserializeResult = std::decay_t<decltype(deserialize_result)>;
        if constexpr (std::is_pointer_v<DeserializeResult>) {
            return {
                .ptr{deserialize_result},
            };
        } else if constexpr (std::is_copy_constructible_v<T>) {
            auto const ptr = new T{deserialize_result};
            auto const deleter
                = [](void *ptr) { delete reinterpret_cast<T *>(ptr); };
            return {
                .ptr{ptr},
                .deleter{deleter},
            };
        } else {
            static_assert((sizeof(T), false),
                          "Could not form a pointer to current type since it's "
                          "not copy constructible.");
        }
    }

    template <deserializable TElement> auto _as_pointer() {
        return _construct_static_ptr_to<TElement>();
    }

    template <deserializable T> auto _construct_static_ptr_to() {
        const auto deserialize_result
            = serialization_traits<T>::deserialize(*this);
        using DeserializeResult = std::decay_t<decltype(deserialize_result)>;
        if constexpr (std::is_pointer_v<DeserializeResult>) {
            return deserialize_result;
        } else if constexpr (std::is_copy_constructible_v<T>) {
            return new T{deserialize_result};
        } else {
            static_assert((sizeof(T), false),
                          "Could not form a pointer to current type since it's "
                          "not copy constructible.");
        }
    }

    template <typename TElement>
    auto _construct_dynamic() -> details::type_erased_ptr {
        const auto dyn_type_field = _node["type"];
        const auto dyn_value_field = _node["value"];
        if (!dyn_type_field
            || dyn_type_field.Type() != YAML::NodeType::Scalar) {
            throw syntax_error{
                "Expected 'type' field specifying the dynamic type name."};
        }
        if (!dyn_value_field) {
            throw syntax_error{
                "Expected 'value' field specifying the dynamic value."};
        }
        const auto dyn_type_name = dyn_type_field.as<std::string>();
        const auto &registry = details::get_dynamic_deserialization_registry();
        const auto dyn_register = registry.find(dyn_type_name);
        if (dyn_register == registry.end()) {
            throw type_not_found_error{dyn_type_name};
        }
        const auto dyn_result = dyn_register->second(_fork(dyn_value_field));
        return dyn_result;
    }

    template <typename T> auto _as_enum() {
        static_assert(std::is_enum_v<T>);
        if constexpr (deserializable_keyed_enum<T>) {
            const auto &enumerators = serialization_traits<T>::enumerators;
            const auto enumerator_name = _node.as<std::string>();
            const auto r = enumerators.find(enumerator_name);
            if (r != enumerators.end()) {
                return static_cast<T>(r->second);
            } else {
                throw unrecognized_enumerator_error{enumerator_name};
            }
        } else {
            return static_cast<T>(_node.as<std::underlying_type_t<T>>());
        }
    }

    template <typename T> auto _as_vector() {
        const auto nodes = _node.as<std::vector<YAML::Node>>();
        std::vector<T> v;
        v.reserve(nodes.size());
        for (const auto node : nodes) {
            v.push_back(_fork(node).as<T>());
        }
        return v;
    }

    template <typename T> auto _as_optional() -> std::optional<T> {
        if (!_node) {
            return std::nullopt;
        } else {
            return as<T>();
        }
    }

    template <deserializable T> auto _as_object() {
        return serialization_traits<T>::deserialize(*this);
    }
};

template <typename T>
auto allow_dynamic(const std::string &name_) -> const std::string & {
    details::get_dynamic_deserialization_registry().emplace(
        name_,
        [](yaml_deserializer &&deserializer) {
            return deserializer._construct_type_erased_static_ptr<T>();
        });
    return name_;
}
} // namespace yab
