#include "utils/lite_rtti.hpp"
#include <catch.hpp>

using namespace rive;

namespace
{
struct DestructorTest
{
  ~DestructorTest() { s_destructCount++; }

  static inline uint32_t s_destructCount = 0;
};
} // namespace

TEST_CASE("lite rtti behaves correctly", "[lite_rtti]")
{
  // make sure strings that only differ by 1 value
  // at the end or begnining are result in
  // different hashed values
  static_assert(CONST_ID(abcd) != CONST_ID(abcc));
  static_assert(CONST_ID(abcd) != CONST_ID(bbcd));

  class NotRTTI
  {};

  class A : public ENABLE_LITE_RTTI(A)
  {};
  A a;
  CHECK(A::TypeIDForRTTI == CONST_ID(A));

  class B : public LITE_RTTI_OVERRIDE(A, B)
  {};
  B b;
  CHECK(B::TypeIDForRTTI == CONST_ID(B));
  CHECK(B::TypeIDForRTTI != A::TypeIDForRTTI);

  class C : public LITE_RTTI_OVERRIDE(B, C)
  {};
  C c;
  CHECK(C::TypeIDForRTTI == CONST_ID(C));
  CHECK(C::TypeIDForRTTI != B::TypeIDForRTTI);
  CHECK(C::TypeIDForRTTI != A::TypeIDForRTTI);

  class J : public LITE_RTTI_OVERRIDE(A, J)
  {};

  J j;

  A* pA = &a;
  A* pA_b = &b;
  A* pA_c = &c;
  A* pA_j = &j;

  CHECK(lite_rtti_cast<B*>(pA) == nullptr);
  CHECK(lite_rtti_cast<C*>(pA) == nullptr);
  CHECK(lite_rtti_cast<J*>(pA) == nullptr);
  CHECK(lite_rtti_cast<const B*>(pA) == nullptr);
  CHECK(lite_rtti_cast<const C*>(pA) == nullptr);
  CHECK(lite_rtti_cast<const J*>(pA) == nullptr);
  CHECK(lite_rtti_cast<const B*>(const_cast<const A*>(pA)) == nullptr);
  CHECK(lite_rtti_cast<const C*>(const_cast<const A*>(pA)) == nullptr);
  CHECK(lite_rtti_cast<const J*>(const_cast<const A*>(pA)) == nullptr);

  CHECK(lite_rtti_cast<B*>(pA_b) == &b);
  CHECK(lite_rtti_cast<C*>(pA_b) == nullptr);
  CHECK(lite_rtti_cast<J*>(pA_b) == nullptr);
  CHECK(lite_rtti_cast<const B*>(pA_b) == &b);
  CHECK(lite_rtti_cast<const C*>(pA_b) == nullptr);
  CHECK(lite_rtti_cast<const J*>(pA_b) == nullptr);
  CHECK(lite_rtti_cast<const B*>(const_cast<const A*>(pA_b)) == &b);
  CHECK(lite_rtti_cast<const C*>(const_cast<const A*>(pA_b)) == nullptr);
  CHECK(lite_rtti_cast<const J*>(const_cast<const A*>(pA_b)) == nullptr);

  CHECK(lite_rtti_cast<B*>(pA_c) == &c);
  CHECK(lite_rtti_cast<C*>(pA_c) == &c);
  CHECK(lite_rtti_cast<J*>(pA_c) == nullptr);
  CHECK(lite_rtti_cast<const B*>(pA_c) == &c);
  CHECK(lite_rtti_cast<const C*>(pA_c) == &c);
  CHECK(lite_rtti_cast<const J*>(pA_c) == nullptr);
  CHECK(lite_rtti_cast<const B*>(const_cast<const A*>(pA_c)) == &c);
  CHECK(lite_rtti_cast<const C*>(const_cast<const A*>(pA_c)) == &c);
  CHECK(lite_rtti_cast<const J*>(const_cast<const A*>(pA_c)) == nullptr);

  CHECK(lite_rtti_cast<B*>(pA_j) == nullptr);
  CHECK(lite_rtti_cast<C*>(pA_j) == nullptr);
  CHECK(lite_rtti_cast<J*>(pA_j) == &j);
  CHECK(lite_rtti_cast<const B*>(pA_j) == nullptr);
  CHECK(lite_rtti_cast<const C*>(pA_j) == nullptr);
  CHECK(lite_rtti_cast<const J*>(pA_j) == &j);
  CHECK(lite_rtti_cast<const B*>(const_cast<const A*>(pA_j)) == nullptr);
  CHECK(lite_rtti_cast<const C*>(const_cast<const A*>(pA_j)) == nullptr);
  CHECK(lite_rtti_cast<const J*>(const_cast<const A*>(pA_j)) == &j);

  B* pB = &b;
  B* pB_c = &c;

  CHECK(lite_rtti_cast<C*>(pB) == nullptr);
  CHECK(lite_rtti_cast<const C*>(pB) == nullptr);
  CHECK(lite_rtti_cast<const C*>(const_cast<const B*>(pB)) == nullptr);

  CHECK(lite_rtti_cast<C*>(pB_c) == &c);
  CHECK(lite_rtti_cast<const C*>(pB_c) == &c);
  CHECK(lite_rtti_cast<const C*>(const_cast<const B*>(pB_c)) == &c);

  A* nil = nullptr;
  CHECK(lite_rtti_cast<B*>(nil) == nullptr);
  CHECK(lite_rtti_cast<const C*>(nil) == nullptr);

  // Check constructor arguments and destructors.
  struct D : public ENABLE_LITE_RTTI(D)
  {
    D() = delete;
    D(float x_, int y_) : x(x_), y(y_) {}
    virtual ~D() = default;
    float x;
    int y;
    DestructorTest dt;
  };

  struct E : public LITE_RTTI_OVERRIDE(D, E)
  {
    E(float x_, int y_) : lite_rtti_override(x_, y_) {}
  };

  CHECK(DestructorTest::s_destructCount == 0);
  D* pD_e = new E(4.5f, 6);
  E* pE = lite_rtti_cast<E*>(pD_e);
  REQUIRE(pE != nullptr);
  CHECK(pE->x == 4.5f);
  CHECK(pE->y == 6);
  delete pD_e;
  CHECK(DestructorTest::s_destructCount == 1);

  // Check rcp
  class F : public RefCnt<F>, public ENABLE_LITE_RTTI(F)
  {
  public:
    virtual ~F() = default;
  };
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
