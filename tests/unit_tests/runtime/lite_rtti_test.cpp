#include "utils/lite_rtti.hpp"
#include <catch.hpp>

using namespace rive;

TEST_CASE("lite rtti behaves correctly", "[lite_rtti]")
{
    class A : public enable_lite_rtti<A>
    {};
    A a;
    CHECK(lite_type_id<A>() == lite_type_id<A>());
    CHECK(lite_type_id<const A>() == lite_type_id<A>());
    CHECK(lite_type_id<const A>() == a.liteTypeID());

    class B : public lite_rtti_override<A, B>
    {};
    B b;
    CHECK(lite_type_id<B>() != lite_type_id<A>());
    CHECK(b.liteTypeID() != a.liteTypeID());
    CHECK(b.liteTypeID() == lite_type_id<const B>());

    class C : public lite_rtti_override<B, C>
    {};
    const C c;
    CHECK(lite_type_id<C>() != lite_type_id<A>());
    CHECK(lite_type_id<C>() != lite_type_id<B>());
    CHECK(c.liteTypeID() != a.liteTypeID());
    CHECK(c.liteTypeID() != b.liteTypeID());
    CHECK(c.liteTypeID() == lite_type_id<C>());

    A* pA = &a;
    A* pA_b = &b;
    const A* pA_c = &c;

    CHECK(lite_rtti_cast<B*>(pA) == nullptr);
    CHECK(lite_rtti_cast<const C*>(pA) == nullptr);

    CHECK(lite_rtti_cast<const B*>(pA_b) == &b);
    CHECK(lite_rtti_cast<C*>(pA_b) == nullptr);

    CHECK(lite_rtti_cast<const B*>(pA_c) == nullptr);
    CHECK(lite_rtti_cast<C*>(const_cast<A*>(pA_c)) == &c);

    const B* pB_c = &c;
    CHECK(lite_rtti_cast<B*>(const_cast<B*>(pB_c)) == nullptr);
    CHECK(lite_rtti_cast<const C*>(pB_c) == &c);

    A* nil = nullptr;
    CHECK(lite_rtti_cast<B*>(nil) == nullptr);
    CHECK(lite_rtti_cast<const C*>(nil) == nullptr);

    // Check constructor arguments.
    struct D : public enable_lite_rtti<D>
    {
        D() = delete;
        D(float x_, int y_) : x(x_), y(y_) {}
        float x;
        int y;
    };

    struct E : public lite_rtti_override<D, E>
    {
        E(float x_, int y_) : lite_rtti_override(x_, y_) {}
    };

    D* pD_e = new E(4.5f, 6);
    E* pE = lite_rtti_cast<E*>(pD_e);
    CHECK(pD_e->liteTypeID() == lite_type_id<const E>());
    REQUIRE(pE != nullptr);
    CHECK(pE->x == 4.5f);
    CHECK(pE->y == 6);
    delete pD_e;

    // Check rcp
    class F : public RefCnt<F>, public enable_lite_rtti<F>
    {};
    class G : public lite_rtti_override<F, G>
    {};
    class H : public lite_rtti_override<F, H>
    {};
    rcp<F> pF_g = make_rcp<G>();
    rcp<G> pG = lite_rtti_rcp_cast<G>(pF_g);
    rcp<H> pH = lite_rtti_rcp_cast<H>(pF_g);
    CHECK(pG != nullptr);
    CHECK(pH == nullptr);
}
