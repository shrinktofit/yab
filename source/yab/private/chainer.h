#pragma once

#include <yab/private/yaml-deserializer.h>

namespace yab {
template <typename TObject> struct deserializer_chainer {
    yaml_deserializer &deserializer;

    TObject &&object;

    template <typename TProperty>
    auto set_if(const std::string &property_key,
                TProperty(std::decay_t<TObject>::*member_ptr))
        -> deserializer_chainer & {
        auto property
            = deserializer.template get_optional<std::remove_cv_t<TProperty>>(
                property_key);
        if (property) {
            object.*member_ptr = std::move(*property);
        }
        return *this;
    }

    template <typename TProperty>
    auto set(const std::string &property_key,
             TProperty(std::decay_t<TObject>::*member_ptr))
        -> deserializer_chainer & {
        object.*member_ptr = deserializer.template get<TProperty>(property_key);
        return *this;
    }
};

template <typename T> auto bind(yaml_deserializer &deserializer, T &&object) {
    return deserializer_chainer<T>{.deserializer{deserializer},
                                   .object{std::forward<T>(object)}};
}
} // namespace yab
