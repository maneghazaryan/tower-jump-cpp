#include <doctest/doctest.h>

#include "core/ecs.hpp"

using namespace tj;

namespace {
struct Pos { float x, y; };
struct Tag { int v; };
}  // namespace

TEST_CASE("entities are recycled with a new generation") {
    Registry r;
    Entity a = r.create();
    r.destroy(a);
    Entity b = r.create();
    CHECK(ecs_detail::indexOf(a) == ecs_detail::indexOf(b));  // same slot reused
    CHECK(a != b);                                            // but a different handle
    CHECK_FALSE(r.alive(a));
    CHECK(r.alive(b));
}

TEST_CASE("removing a component keeps the dense array packed") {
    Registry r;
    Entity e[4];
    for (int i = 0; i < 4; ++i) {
        e[i] = r.create();
        r.add<Pos>(e[i], float(i), 0.0f);
    }
    r.destroy(e[1]);
    auto& set = r.storage<Pos>();
    CHECK(set.size() == 3);
    CHECK(r.get<Pos>(e[3]).x == 3.0f);  // the last element moved into the hole, still reachable
    CHECK(r.get<Pos>(e[0]).x == 0.0f);
    CHECK_FALSE(set.contains(e[1]));
}

TEST_CASE("each() visits only entities that have both components") {
    Registry r;
    Entity a = r.create(), b = r.create(), c = r.create();
    r.add<Pos>(a, 1.0f, 1.0f);
    r.add<Pos>(b, 2.0f, 2.0f);
    r.add<Tag>(b, 7);
    r.add<Tag>(c, 9);
    int visited = 0;
    r.each<Tag, Pos>([&](Entity e, Tag& t, Pos& p) {
        ++visited;
        CHECK(e == b);
        CHECK(t.v == 7);
        CHECK(p.x == 2.0f);
    });
    CHECK(visited == 1);
}

TEST_CASE("a stale handle never reads a recycled slot's component") {
    Registry r;
    Entity a = r.create();
    r.add<Tag>(a, 1);
    r.destroy(a);
    Entity b = r.create();
    r.add<Tag>(b, 2);
    CHECK(r.tryGet<Tag>(a) == nullptr);
    CHECK(r.get<Tag>(b).v == 2);
}

TEST_CASE("steady-state create/destroy does not grow storage") {
    Registry r;
    r.storage<Pos>().reserve(16);
    for (int round = 0; round < 1000; ++round) {
        Entity e = r.create();
        r.add<Pos>(e, 0.0f, 0.0f);
        r.destroy(e);
    }
    CHECK(r.slotCount() == 1);
    CHECK(r.storage<Pos>().capacity() == 16);
}
