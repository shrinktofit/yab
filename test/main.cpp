#include <catch2/catch_test_macros.hpp>
#include <yab/yab.h>

struct Vec3 {
    double x = 0, y = 0, z = 0;
};

struct ChainTestVec3 : public Vec3 {};

template <> struct yab::serialization_traits<ChainTestVec3> {
    using class_type = ChainTestVec3;

    template <yab::deserializer<class_type> Deserializer>
    static class_type deserialize(Deserializer &deserializer) {
        auto chainer = yab::bind(deserializer, class_type{});
        REQUIRE(&(chainer.set("x", &class_type::x)) == &chainer);
        REQUIRE(chainer.object.x == 0.1);

        chainer.set("y", &class_type::y);
        REQUIRE(chainer.object.y == 2.0);

        chainer.set("z", &class_type::z);
        REQUIRE(chainer.object.z == -0.05);

        return chainer.object;
    }
};

TEST_CASE("Chain set") {
    const auto node = YAML::Load(R"(
x: 0.1
y: 2
z: -0.05
)");

    yab::yaml_deserializer deserializer{node};
    const auto output = deserializer.as<ChainTestVec3>();
    REQUIRE(output.x == 0.1);
    REQUIRE(output.y == 2.0);
    REQUIRE(output.z == -0.05);
}
