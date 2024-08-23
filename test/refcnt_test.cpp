/*
 * Copyright 2022 Rive
 */

#include <rive/refcnt.hpp>
#include <catch.hpp>
#include <cstdio>

using namespace rive;

class MyRefCnt : public RefCnt<MyRefCnt>
{
public:
    MyRefCnt() {}
    MyRefCnt(int, float, bool) {}

    void require_count(int value) { REQUIRE(this->debugging_refcnt() == value); }
};

TEST_CASE("refcnt", "[basics]")
{
    MyRefCnt my;
    REQUIRE(my.debugging_refcnt() == 1);
    my.ref();
    REQUIRE(my.debugging_refcnt() == 2);
    my.unref();
    REQUIRE(my.debugging_refcnt() == 1);

    safe_ref(&my);
    REQUIRE(my.debugging_refcnt() == 2);
    safe_unref(&my);
    REQUIRE(my.debugging_refcnt() == 1);

    // just exercise these to be sure they don't crash
    safe_ref((MyRefCnt*)nullptr);
    safe_unref((MyRefCnt*)nullptr);
}

TEST_CASE("rcp", "[basics]")
{
    rcp<MyRefCnt> r0(nullptr);

    REQUIRE(r0.get() == nullptr);
    REQUIRE(!r0);

    auto r1 = make_rcp<MyRefCnt>();
    REQUIRE(r1.get() != nullptr);
    REQUIRE(r1);
    REQUIRE(r1 != r0);
    REQUIRE(r1->debugging_refcnt() == 1);

    auto r2 = r1;
    REQUIRE(r1.get() == r2.get());
    REQUIRE(r1 == r2);
    REQUIRE(r2->debugging_refcnt() == 2);

    auto r3 = make_rcp<MyRefCnt>(1, .5f, false);
    REQUIRE(r3.get() != nullptr);
    REQUIRE(r3);
    REQUIRE(r3 != r1);
    REQUIRE(r3->debugging_refcnt() == 1);

    auto ptr = r2.release();
    REQUIRE(r2.get() == nullptr);
    REQUIRE(r1.get() == ptr);

    // This is important, calling release() does not modify the ref count on the object
    // We have to manage that explicit since we called release()
    REQUIRE(r1->debugging_refcnt() == 2);
    ptr->unref();
    REQUIRE(r1->debugging_refcnt() == 1);

    r1.reset();
    REQUIRE(r1.get() == nullptr);

    struct A : public rive::RefCnt<A>
    {
        int x = 17;
    };
    struct B : public A
    {
        int y = 21;
    };
    auto b = make_rcp<B>();
    CHECK(b->y == 21);
    rcp<A> a = b;
    CHECK(a->x == 17);
    CHECK(static_rcp_cast<B>(a)->y == 21);
    CHECK(rcp<A>(b)->x == 17);
    CHECK(a->debugging_refcnt() == 2);
    CHECK(b->debugging_refcnt() == 2);
    CHECK(static_rcp_cast<B>(std::move(a))->y == 21);
    CHECK(a == nullptr);
    CHECK(b->debugging_refcnt() == 1);
    a = ref_rcp(b.get());
    CHECK(b == a);
    CHECK(a->debugging_refcnt() == 2);
    CHECK(a->x == 17);
    CHECK(static_rcp_cast<B>(a)->y == 21);
}
