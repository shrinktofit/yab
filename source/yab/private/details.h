#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace yab {
class yaml_deserializer;

template <typename Ty> struct serialization_traits {};

namespace details {
template <typename T> struct is_vector : std::false_type {};

template <typename T> struct is_vector<std::vector<T>> : std::true_type {};

template <typename T>
inline constexpr static bool is_vector_v = is_vector<T>::value;

template <typename T> struct is_optional : std::false_type {};

template <typename T> struct is_optional<std::optional<T>> : std::true_type {};

template <typename T>
inline constexpr static bool is_optional_v = is_optional<T>::value;

template <typename T> struct is_unique_ptr : std::false_type {};

template <typename T, typename Deleter>
struct is_unique_ptr<std::unique_ptr<T, Deleter>> : std::true_type {};

template <typename T>
inline constexpr static bool is_unique_ptr_v = is_unique_ptr<T>::value;

template <typename T> struct is_shared_ptr : std::false_type {};

template <typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

template <typename T>
inline constexpr static bool is_shared_ptr_v = is_shared_ptr<T>::value;

struct type_erased_ptr {
    void *ptr{};
    std::function<void(void *)> deleter;
};

using dynamic_deserialization_registry_type
    = std::map<std::string,
               std::function<type_erased_ptr(yaml_deserializer &&)>>;

auto get_dynamic_deserialization_registry()
    -> dynamic_deserialization_registry_type &;
} // namespace details

template <typename T>
concept deserializable_class
    = std::is_class_v<T> && requires(yaml_deserializer &deserializer) {
          serialization_traits<T>::deserialize(deserializer);
      };

template <typename T>
concept deserializable_keyed_enum = std::is_enum_v<T> && requires {
    {
        serialization_traits<T>::enumerators
    } -> std::convertible_to<std::map<std::string, T>>;
};

template <typename T>
concept deserializable_enum = deserializable_keyed_enum<T> || std::is_enum_v<T>;

template <typename T>
concept deserializable = deserializable_class<T> || deserializable_enum<T>;

} // namespace yab
