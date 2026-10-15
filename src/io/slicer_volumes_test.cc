#include "io/slicer_volumes.h"

#include <catch2/catch_all.hpp>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "core/SlicerModifierNode.h"
#include "geometry/Geometry.h"
#include "geometry/PolySet.h"
#include "geometry/linalg.h"
#include "platform/PlatformUtils.h"

namespace {

// A single triangle at height z (3 vertices, 1 face), optionally colored.
std::shared_ptr<PolySet> triangle(double z, bool colored = false)
{
  auto ps = std::make_shared<PolySet>(3);
  ps->vertices = {{0, 0, z}, {1, 0, z}, {0, 1, z}};
  ps->indices = {{0, 1, 2}};
  ps->setTriangular(true);
  if (colored) ps->setColor(Color4f(1.0f, 0.0f, 0.0f, 1.0f));
  return ps;
}

// The vertex indices used by triangles [first, last] of `ps`.
std::set<int> usedVertices(const PolySet& ps, size_t first, size_t last)
{
  std::set<int> used;
  for (size_t i = first; i <= last; ++i) used.insert(ps.indices[i].begin(), ps.indices[i].end());
  return used;
}

}  // namespace

TEST_CASE("buildSlicerObject puts model and volumes into one mesh", "[io][slicer]")
{
  SlicerVolumes volumes;
  volumes.push_back({"ParameterModifier", "dense", {{"fill_density", "80%"}}, triangle(5)});
  volumes.push_back({"SupportBlocker", "blocker", {}, triangle(7)});

  SECTION("single model geometry")
  {
    const auto object = buildSlicerObject(triangle(0, true), volumes, false);
    REQUIRE(object.parts.size() == 3);
    CHECK(object.mesh->indices.size() == 3);
    CHECK(object.mesh->vertices.size() == 9);

    CHECK(object.parts[0].volumeType == "ModelPart");
    CHECK(object.parts[0].name == "OpenSCAD Model");
    CHECK(object.parts[1].volumeType == "ParameterModifier");
    CHECK(object.parts[1].settings == decltype(object.parts[1].settings){{"fill_density", "80%"}});
    CHECK(object.parts[2].volumeType == "SupportBlocker");

    // inclusive, consecutive triangle ranges
    for (size_t i = 0; i < object.parts.size(); ++i) {
      CHECK(object.parts[i].firstTriangle == i);
      CHECK(object.parts[i].lastTriangle == i);
    }
    // every part uses its own contiguous vertex block (PrusaSlicer takes min..max)
    CHECK(usedVertices(*object.mesh, 0, 0) == std::set<int>{0, 1, 2});
    CHECK(usedVertices(*object.mesh, 1, 1) == std::set<int>{3, 4, 5});
    CHECK(usedVertices(*object.mesh, 2, 2) == std::set<int>{6, 7, 8});
    CHECK(object.mesh->vertices[3].z() == 5);

    // model colors survive, volumes stay uncolored
    REQUIRE(object.mesh->color_indices.size() == 3);
    CHECK(object.mesh->color_indices[0] == 0);
    CHECK(object.mesh->color_indices[1] == -1);
    CHECK(object.mesh->color_indices[2] == -1);
  }

  SECTION("each top-level item of a geometry list is its own model part")
  {
    Geometry::Geometries items;
    items.emplace_back(nullptr, triangle(0));
    items.emplace_back(nullptr, triangle(1));
    const auto list = std::make_shared<GeometryList>(items);
    const auto object = buildSlicerObject(list, volumes, true);
    REQUIRE(object.parts.size() == 4);
    CHECK(object.parts[0].name == "Part 1");
    CHECK(object.parts[1].name == "Part 2");
    CHECK(object.parts[1].volumeType == "ModelPart");
    CHECK(object.parts[3].firstTriangle == 3);
    CHECK(object.mesh->color_indices.empty());  // no colors anywhere
  }

  SECTION("empty volumes are skipped")
  {
    SlicerVolumes withEmpty = volumes;
    withEmpty.push_back({"ParameterModifier", "empty", {}, std::make_shared<PolySet>(3)});
    const auto object = buildSlicerObject(triangle(0), withEmpty, false);
    CHECK(object.parts.size() == 3);
  }
}

TEST_CASE("prusaSlicerModelConfig writes PrusaSlicer's part description", "[io][slicer]")
{
  const std::vector<SlicerPart> parts = {
    {0, 11, "ModelPart", "body", {}},
    {12, 23, "ParameterModifier", "a\"b<c>", {{"fill_density", "80%"}, {"perimeters", "5"}}},
    {24, 35, "NegativeVolume", "cut", {}},
  };
  const std::string xml = prusaSlicerModelConfig(2, "OpenSCAD Model", parts);
  const std::string expected = R"(<?xml version="1.0" encoding="UTF-8"?>
<config>
 <object id="2" instances_count="1">
  <metadata type="object" key="name" value="OpenSCAD Model"/>
  <volume firstid="0" lastid="11">
   <metadata type="volume" key="name" value="body"/>
   <metadata type="volume" key="volume_type" value="ModelPart"/>
  </volume>
  <volume firstid="12" lastid="23">
   <metadata type="volume" key="name" value="a&quot;b&lt;c&gt;"/>
   <metadata type="volume" key="modifier" value="1"/>
   <metadata type="volume" key="volume_type" value="ParameterModifier"/>
   <metadata type="volume" key="fill_density" value="80%"/>
   <metadata type="volume" key="perimeters" value="5"/>
  </volume>
  <volume firstid="24" lastid="35">
   <metadata type="volume" key="name" value="cut"/>
   <metadata type="volume" key="volume_type" value="NegativeVolume"/>
  </volume>
 </object>
</config>
)";
  CHECK(xml == expected);
}

TEST_CASE("PrusaSlicer setting key tables", "[io][slicer]")
{
  CHECK(isPrusaSlicerRegionKey("fill_density"));
  CHECK(isPrusaSlicerRegionKey("perimeters"));
  CHECK(isPrusaSlicerRegionKey("extruder"));
  CHECK_FALSE(isPrusaSlicerRegionKey("layer_height"));
  CHECK(isPrusaSlicerObjectKey("layer_height"));
  CHECK(isPrusaSlicerObjectKey("support_material"));
  CHECK_FALSE(isPrusaSlicerRegionKey("bogus_key"));
  CHECK_FALSE(isPrusaSlicerObjectKey("bogus_key"));
}

#ifdef ENABLE_MANIFOLD
TEST_CASE("colorSlicerRegions recolors the model inside a region", "[io][slicer]")
{
  // ManifoldGeometry::toPolySet() reads the color scheme (resources path).
  PlatformUtils::registerApplicationPath(std::filesystem::current_path().generic_string());
  // 10 mm cube, region = cube [5, 15]^3: the model part x, y, z >= 5 is recolored.
  auto cube = [](double lo, double hi) {
    auto ps = std::make_shared<PolySet>(3);
    for (int i = 0; i < 8; ++i) {
      ps->vertices.emplace_back(i & 1 ? hi : lo, i & 2 ? hi : lo, i & 4 ? hi : lo);
    }
    ps->indices = {{0, 2, 3}, {0, 3, 1}, {4, 5, 7}, {4, 7, 6}, {0, 1, 5}, {0, 5, 4},
                   {2, 6, 7}, {2, 7, 3}, {0, 4, 6}, {0, 6, 2}, {1, 3, 7}, {1, 7, 5}};
    ps->setTriangular(true);
    return ps;
  };
  const Color4f red(1.0f, 0.0f, 0.0f, 1.0f);
  SlicerVolumes volumes;
  volumes.push_back({"ParameterModifier", "r", {}, cube(5, 15), red});

  const auto out = colorSlicerRegions(cube(0, 10), volumes);
  REQUIRE(out);
  REQUIRE(out->color_indices.size() == out->indices.size());
  size_t redFaces = 0;
  for (size_t i = 0; i < out->indices.size(); ++i) {
    const int ci = out->color_indices[i];
    if (ci < 0 || !(out->colors[ci] == red)) continue;
    ++redFaces;
    for (const int v : out->indices[i]) {
      // every red face lies inside the region (and the model)
      CHECK(out->vertices[v].minCoeff() >= 5 - 1e-9);
      CHECK(out->vertices[v].maxCoeff() <= 10 + 1e-9);
    }
  }
  CHECK(redFaces > 0);
  // the outer shape is unchanged
  const auto bbox = out->getBoundingBox();
  CHECK(bbox.min().isApprox(Vector3d(0, 0, 0)));
  CHECK(bbox.max().isApprox(Vector3d(10, 10, 10)));

  SECTION("no volumes or a region missing the model leave the model as it is")
  {
    const auto model = cube(0, 10);
    CHECK(colorSlicerRegions(model, {}) == model);
    SlicerVolumes far;
    far.push_back({"ParameterModifier", "far", {}, cube(20, 30), red});
    const auto untouched = colorSlicerRegions(model, far);
    for (size_t i = 0; i < untouched->color_indices.size(); ++i) {
      const int ci = untouched->color_indices[i];
      CHECK((ci < 0 || !(untouched->colors[ci] == red)));
    }
  }
}
#endif
