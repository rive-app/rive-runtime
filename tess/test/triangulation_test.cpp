#include "rive/shapes/path.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/tess/tess_render_path.hpp"
#include "rive_file_reader.hpp"

class TestRenderPath : public rive::TessRenderPath
{
public:
    std::vector<rive::Vec2D> vertices;
    std::vector<uint16_t> indices;

protected:
    virtual void addTriangles(rive::Span<const rive::Vec2D> vts,
                              rive::Span<const uint16_t> idx) override
    {
        vertices.insert(vertices.end(), vts.begin(), vts.end());
        indices.insert(indices.end(), idx.begin(), idx.end());
    }

    void setTriangulatedBounds(const rive::AABB& value) override {}
};

TEST_CASE("simple triangle path triangulates as expected", "[file]")
{
    auto file = ReadRiveFile("../test/assets/triangle.riv");
    auto artboard = file->artboard();
    artboard->advance(0.0f);

    auto path = artboard->find<rive::Path>("triangle_path");
    REQUIRE(path != nullptr);
    TestRenderPath renderPath;
    path->buildPath(renderPath);

    rive::Mat2D identity;
    TestRenderPath shapeRenderPath;
    shapeRenderPath.addRenderPath(&renderPath, identity);
    shapeRenderPath.triangulate();
    REQUIRE(shapeRenderPath.vertices.size() == 4);
    REQUIRE(shapeRenderPath.indices.size() == 3);
    REQUIRE(shapeRenderPath.indices[0] == 2);
    REQUIRE(shapeRenderPath.indices[1] == 0);
    REQUIRE(shapeRenderPath.indices[2] == 1);
}