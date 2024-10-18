#include "utils/lite_rtti.hpp"
#include <catch.hpp>

using namespace rive;

TEST_CASE("lite rtti behaves correctly", "[lite_rtti]")
{
    // make sure strings that only differ by 1 value
    // at the end or begnining are result in
    // different hashed values
    CHECK(CONST_ID(abcd) != CONST_ID(abcc));
    CHECK(CONST_ID(abcd) != CONST_ID(bbcd));

    class A : public ENABLE_LITE_RTTI(A)
    {};
    A a;
    CHECK(CONST_ID(A) == a.liteTypeID());

    class B : public LITE_RTTI_OVERRIDE(A, B)
    {};
    B b;
    CHECK(CONST_ID(B) != CONST_ID(A));
    CHECK(b.liteTypeID() != a.liteTypeID());
    CHECK(b.liteTypeID() == CONST_ID(B));

    class C : public LITE_RTTI_OVERRIDE(B, C)
    {};
    const C c;
    CHECK(CONST_ID(C) != CONST_ID(A));
    CHECK(CONST_ID(C) != CONST_ID(B));
    CHECK(c.liteTypeID() != a.liteTypeID());
    CHECK(c.liteTypeID() != b.liteTypeID());
    CHECK(c.liteTypeID() == CONST_ID(C));

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
    struct D : public ENABLE_LITE_RTTI(D)
    {
        D() = delete;
        D(float x_, int y_) : x(x_), y(y_) {}
        float x;
        int y;
    };

    struct E : public LITE_RTTI_OVERRIDE(D, E)
    {
        E(float x_, int y_) : lite_rtti_override(x_, y_) {}
    };

    D* pD_e = new E(4.5f, 6);
    E* pE = lite_rtti_cast<E*>(pD_e);
    CHECK(pD_e->liteTypeID() == CONST_ID(E));
    REQUIRE(pE != nullptr);
    CHECK(pE->x == 4.5f);
    CHECK(pE->y == 6);
    delete pD_e;

    // Check rcp
    class F : public RefCnt<F>, public ENABLE_LITE_RTTI(F)
    {};
    class G : public LITE_RTTI_OVERRIDE(F, G)
    {};
    class H : public LITE_RTTI_OVERRIDE(F, H)
    {};
    rcp<F> pF_g = make_rcp<G>();
    rcp<G> pG = lite_rtti_rcp_cast<G>(pF_g);
    rcp<H> pH = lite_rtti_rcp_cast<H>(pF_g);
    CHECK(pG != nullptr);
    CHECK(pH == nullptr);
}
