# yab

A YAML deserialization library, based on [yaml-cpp](https://github.com/jbeder/yaml-cpp).

# Usage

## Basic Usage

```cpp
#include <yab/yab.h>
#include <format>
#include <iostream>

// Your class.
struct Vec3 { double x = 0, y = 0, z = 0; }

// Deserialization routine.
template <>
struct yab::serialization_traits {
    template <typename Deserializer>
    Vec3 deserialize(Deserializer &deserializer) {
        return yab::chain(deserializer, Vec3{})
            .get("x", &Vec3::x)
            .get("y", &Vec3::y)
            .get("z", &Vec3::z)
            .object;
    }
};

int main(int argc, char *argv[]) {
    // Load YAML.
    const auto node = YAML::Load(R"(
x: 1
y: 2
z: 3
    )");

    // Deserialize.
    const auto output = yab::yaml_deserializer{node}.as<Vec3>();

    // Output:
    // x: 1, y: 2, z: 3
    std::cout << std::format("x: {}, y: {}, z: {}\n", output.x, output.y, output.z);
}
```
