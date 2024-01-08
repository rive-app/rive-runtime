#include <catch.hpp>
#include "rive/math/mat2d.hpp"
#include "rive/math/math_types.hpp"

namespace rive
{
TEST_CASE("mapPoints", "[Mat2D]")
{
    std::vector<Vec2D> testPts{{0, 0}, {1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    for (int i = 0; i < 100; ++i)
    {
        testPts.push_back(
            {static_cast<float>(rand() % 201 - 100), static_cast<float>(rand() % 201 - 100)});
    }
    size_t n = testPts.size();
    std::vector<Vec2D> dstPts(n);
    std::vector<Vec2D> expectedPts(n);

    auto checkMatrix = [&](Mat2D m) {
        // Map a single point.
        m.mapPoints(dstPts.data(), testPts.data() + 2, 1);
        expectedPts[0] = m * testPts[2];
        CHECK(dstPts[0] == expectedPts[0]);

        // Map n - 1 points (ensures we test one even-length and one odd-length array).
        m.mapPoints(dstPts.data(), testPts.data() + 1, n - 1);
        for (size_t i = 0; i < n - 1; ++i)
            expectedPts[i] = m * testPts[i + 1];
        CHECK(std::equal(dstPts.begin(), dstPts.end() - 1, expectedPts.begin()));

        // Map n points.
        m.mapPoints(dstPts.data(), testPts.data(), n);
        for (size_t i = 0; i < n; ++i)
            expectedPts[i] = m * testPts[i];
        CHECK(dstPts == expectedPts);
    };
    checkMatrix(Mat2D());
    checkMatrix(Mat2D(1, 0, 0, 1, 2, -3));
    checkMatrix(Mat2D(4, 0, 0, -5, 0, 0));
    checkMatrix(Mat2D(4, 0, 0, 5, -6, 7));
    checkMatrix(Mat2D(0, 8, 9, 0, 10, 11));
    checkMatrix(Mat2D(-12, -13, -14, -15, -16, -17));
    checkMatrix(Mat2D(18, 19, 20, 21, 22, 23));
    checkMatrix(Mat2D(-25, 26, 27, -28, 29, -30));
}

// Check Mat2D::findMaxScale.
TEST_CASE("findMaxScale", "[Mat2D]")
{
    Mat2D identity;
    // CHECK(1 == identity.getMinScale());
    CHECK(1 == identity.findMaxScale());
    // success = identity.getMinMaxScales(scales);
    // CHECK(success && 1 == scales[0] && 1 == scales[1]);

    Mat2D scale = Mat2D::fromScale(2, 4);
    // CHECK(2 == scale.getMinScale());
    CHECK(4 == scale.findMaxScale());
    // success = scale.getMinMaxScales(scales);
    // CHECK(success && 2 == scales[0] && 4 == scales[1]);

    Mat2D transpose = Mat2D(0,
                            3,
                            6,
                            0,
                            std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity());
    CHECK(transpose.findMaxScale() == 6);

    Mat2D rot90Scale = Mat2D::fromRotation(math::PI / 2);
    rot90Scale = Mat2D::fromScale(1.f / 4, 1.f / 2) * rot90Scale;
    // CHECK(1.f / 4 == rot90Scale.getMinScale());
    CHECK(1.f / 2 == rot90Scale.findMaxScale());
    // success = rot90Scale.getMinMaxScales(scales);
    // CHECK(success && 1.f / 4 == scales[0] && 1.f / 2 == scales[1]);

    Mat2D rotate = Mat2D::fromRotation(128 * math::PI / 180);
    // CHECK(math::nearly_equal(1, rotate.getMinScale(), math::EPSILON));
    CHECK(math::nearly_equal(1, rotate.findMaxScale(), math::EPSILON));
    // success = rotate.getMinMaxScales(scales);
    // CHECK(success);
    // CHECK(math::nearly_equal(1, scales[0], math::EPSILON));
    // CHECK(math::nearly_equal(1, scales[1], math::EPSILON));

    Mat2D translate = Mat2D::fromTranslate(10, -5);
    // CHECK(1 == translate.getMinScale());
    CHECK(1 == translate.findMaxScale());
    // success = translate.getMinMaxScales(scales);
    // CHECK(success && 1 == scales[0] && 1 == scales[1]);

    // skbug.com/4718
    Mat2D big(2.39394089e+36f,
              3.9159619e+36f,
              8.85347779e+36f,
              1.44823453e+37f,
              9.26526204e+36f,
              1.51559342e+37f);
    CHECK(big.findMaxScale() == 0);

    // // skbug.com/4718
    // Mat2D givingNegativeNearlyZeros(0.00436534f,
    //                                 0.00358857f,
    //                                 0.114138f,
    //                                 0.0936228f,
    //                                 0.37141f,
    //                                 -0.0174198f);
    // success = givingNegativeNearlyZeros.getMinMaxScales(scales);
    // CHECK(success && 0 == scales[0]);

    constexpr int kNumBaseMats = 4;
    Mat2D baseMats[kNumBaseMats] = {scale, rot90Scale, rotate, translate};
    constexpr int kNumMats = 2 * kNumBaseMats;
    Mat2D mats[kNumMats];
    for (size_t i = 0; i < kNumBaseMats; ++i)
    {
        mats[i] = baseMats[i];
        bool invertible = mats[i].invert(&mats[i + kNumBaseMats]);
        REQUIRE(invertible);
    }
    srand(0);
    for (int m = 0; m < 1000; ++m)
    {
        Mat2D mat;
        for (int i = 0; i < 4; ++i)
        {
            int x = rand() % kNumMats;
            mat = mats[x] * mat;
        }

        // float minScale = mat.findMinScale();
        float maxScale = mat.findMaxScale();
        // REQUIRE(minScale >= 0);
        REQUIRE(maxScale >= 0);

        // test a bunch of vectors. All should be scaled by between minScale and maxScale
        // (modulo some error) and we should find a vector that is scaled by almost each.
        static const float gVectorScaleTol = (105 * 1.f) / 100;
        static const float gCloseScaleTol = (97 * 1.f) / 100;
        float max = 0, min = std::numeric_limits<float>::max();
        constexpr int kNumVectors = 1000;
        Vec2D vectors[kNumVectors];
        for (size_t i = 0; i < kNumVectors; ++i)
        {
            vectors[i].x = rand() * 2.f / static_cast<float>(RAND_MAX) - 1;
            vectors[i].y = rand() * 2.f / static_cast<float>(RAND_MAX) - 1;
            vectors[i] = vectors[i].normalized();
            vectors[i] = {mat[0] * vectors[i].x + mat[2] * vectors[i].y,
                          mat[1] * vectors[i].x + mat[3] * vectors[i].y};
        }
        for (size_t i = 0; i < kNumVectors; ++i)
        {
            float d = vectors[i].length();
            REQUIRE(d / maxScale < gVectorScaleTol);
            // REQUIRE(minScale / d < gVectorScaleTol);
            if (max < d)
            {
                max = d;
            }
            if (min > d)
            {
                min = d;
            }
        }
        REQUIRE(max / maxScale >= gCloseScaleTol);
        // REQUIRE(minScale / min >= gCloseScaleTol);
    }
}

TEST_CASE("mapBoundingBox", "[Mat2D]")
{
    const std::vector<Vec2D> testPts{{0, 0}, {1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, -1}};
    std::vector<Vec2D> mappedPts(testPts.size());
    auto checkMatrix = [&](Mat2D m) {
        // Check zero points.
        AABB mappedBbox = m.mapBoundingBox(nullptr, 0);
        CHECK(mappedBbox.left() == 0);
        CHECK(mappedBbox.top() == 0);
        CHECK(mappedBbox.right() == 0);
        CHECK(mappedBbox.bottom() == 0);

        // Find a single point boinding box.
        for (const Vec2D pt : testPts)
        {
            mappedBbox = m.mapBoundingBox(&pt, 1);
            Vec2D mappedPt;
            m.mapPoints(&mappedPt, &pt, 1);
            CHECK(mappedBbox.left() == Approx(mappedPt.x));
            CHECK(mappedBbox.top() == Approx(mappedPt.y));
            CHECK(mappedBbox.right() == Approx(mappedPt.x));
            CHECK(mappedBbox.bottom() == Approx(mappedPt.y));
        }

        // Check n - 1 points (ensures we test one even-length and one odd-length array).
        m.mapPoints(mappedPts.data(), testPts.data() + 1, testPts.size() - 1);
        mappedBbox = m.mapBoundingBox(testPts.data() + 1, testPts.size() - 1);
        AABB testBbox = {1e9f, 1e9f, -1e9f, -1e9f};
        for (size_t i = 0; i < testPts.size() - 1; ++i)
        {
            AABB::expandTo(testBbox, mappedPts[i]);
        }
        CHECK(mappedBbox.left() == Approx(testBbox.left()));
        CHECK(mappedBbox.top() == Approx(testBbox.top()));
        CHECK(mappedBbox.right() == Approx(testBbox.right()));
        CHECK(mappedBbox.bottom() == Approx(testBbox.bottom()));

        // Check n points.
        m.mapPoints(mappedPts.data(), testPts.data(), testPts.size());
        mappedBbox = m.mapBoundingBox(testPts.data(), testPts.size());
        testBbox = {1e9f, 1e9f, -1e9f, -1e9f};
        for (size_t i = 0; i < testPts.size(); ++i)
        {
            AABB::expandTo(testBbox, mappedPts[i]);
        }
        CHECK(mappedBbox.left() == Approx(testBbox.left()));
        CHECK(mappedBbox.top() == Approx(testBbox.top()));
        CHECK(mappedBbox.right() == Approx(testBbox.right()));
        CHECK(mappedBbox.bottom() == Approx(testBbox.bottom()));

        // Check mapping of a bounding box.
        Vec2D bboxPts[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
        AABB mappedBboxFromPts = m.mapBoundingBox(bboxPts, 4);
        AABB mappedBboxFromAABB = m.mapBoundingBox(AABB{0, 0, 1, 1});
        CHECK(mappedBboxFromPts.left() == Approx(mappedBboxFromAABB.left()));
        CHECK(mappedBboxFromPts.top() == Approx(mappedBboxFromAABB.top()));
        CHECK(mappedBboxFromPts.right() == Approx(mappedBboxFromAABB.right()));
        CHECK(mappedBboxFromPts.bottom() == Approx(mappedBboxFromAABB.bottom()));
    };
    checkMatrix(Mat2D());
    checkMatrix(Mat2D(1, 0, 0, 1, 2, -3));
    checkMatrix(Mat2D(4, 0, 0, -5, 0, 0));
    checkMatrix(Mat2D(4, 0, 0, 5, -6, 7));
    checkMatrix(Mat2D(0, 8, 9, 0, 10, 11));
    checkMatrix(Mat2D(-12, -13, -14, -15, -16, -17));
    checkMatrix(Mat2D(18, 19, 20, 21, 22, 23));
    checkMatrix(Mat2D(-25, 26, 27, -28, 29, -30));
}
} // namespace rive
