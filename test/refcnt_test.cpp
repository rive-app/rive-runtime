/*
 * Copyright 2022 Rive
 */

#include <rive/refcnt.hpp>
#include <catch.hpp>
#include <cstdio>

using namespace rive;

class MyRefCnt : public RefCnt<MyRefCnt> {
public:
    MyRefCnt() {}

    void require_count(int value) { REQUIRE(this->debugging_refcnt() == value); }
};

TEST_CASE("refcnt", "[basics]") {
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

TEST_CASE("rcp", "[basics]") {
    rcp<MyRefCnt> r0(nullptr);

    REQUIRE(r0.get() == nullptr);
    REQUIRE(!r0);

    rcp<MyRefCnt> r1(new MyRefCnt);
    REQUIRE(r1.get() != nullptr);
    REQUIRE(r1);
    REQUIRE(r1 != r0);
    REQUIRE(r1->debugging_refcnt() == 1);

    auto r2 = r1;
    REQUIRE(r1.get() == r2.get());
    REQUIRE(r1 == r2);
    REQUIRE(r2->debugging_refcnt() == 2);

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
}
