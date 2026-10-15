#include "io/slicer_volumes.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/SlicerModifierNode.h"
#include "core/node.h"
#include "geometry/Geometry.h"
#include "geometry/GeometryEvaluator.h"
#include "geometry/PolySet.h"
#include "geometry/PolySetUtils.h"
#include "io/export.h"
#include "utils/printutils.h"

#ifdef ENABLE_MANIFOLD
#include "geometry/manifold/ManifoldGeometry.h"
#include "geometry/manifold/manifoldutils.h"
#endif

const char *const PRUSASLICER_MODEL_CONFIG_PATH = "/Metadata/Slic3r_PE_model.config";
// PrusaSlicer finds the file by its path and ignores relationships; lib3mf
// requires one for every attachment.
const char *const PRUSASLICER_MODEL_CONFIG_RELATIONSHIP =
  "http://schemas.slic3r.org/3mf/2017/06/model_config";

namespace {

std::shared_ptr<const PolySet> asTriangles(const std::shared_ptr<const Geometry>& geom)
{
  auto ps = PolySetUtils::getGeometryAsPolySet(geom);
  if (!ps || ps->isEmpty() || ps->getDimension() != 3) return nullptr;
  if (ps->isTriangular()) return ps;
  return PolySetUtils::tessellate_faces(*ps);
}

void appendPart(PolySet& out, const PolySet& part, bool keepColors)
{
  const auto vertexOffset = static_cast<int>(out.vertices.size());
  out.vertices.insert(out.vertices.end(), part.vertices.begin(), part.vertices.end());

  const auto colorOffset = static_cast<int32_t>(out.colors.size());
  if (keepColors) out.colors.insert(out.colors.end(), part.colors.begin(), part.colors.end());

  for (size_t i = 0; i < part.indices.size(); ++i) {
    auto face = part.indices[i];
    for (auto& idx : face) idx += vertexOffset;
    out.indices.push_back(std::move(face));
    int32_t color = -1;
    if (keepColors && i < part.color_indices.size() && part.color_indices[i] >= 0) {
      color = part.color_indices[i] + colorOffset;
    }
    out.color_indices.push_back(color);
  }
}

std::string escapeXml(const std::string& value)
{
  std::string quoted;
  quoted.reserve(value.size());
  for (const char c : value) {
    switch (c) {
    case '&':  quoted += "&amp;"; break;
    case '<':  quoted += "&lt;"; break;
    case '>':  quoted += "&gt;"; break;
    case '"':  quoted += "&quot;"; break;
    case '\'': quoted += "&apos;"; break;
    default:   quoted += c; break;
    }
  }
  return quoted;
}

}  // namespace

std::shared_ptr<const SlicerVolumes> collectSlicerVolumes(const Tree& tree,
                                                         const std::shared_ptr<const AbstractNode>& root)
{
  auto volumes = std::make_shared<SlicerVolumes>();
  const auto modifiers = collectSlicerModifiers(root);
  if (modifiers.empty()) return volumes;

  GeometryEvaluator geomevaluator(tree);
  int counter = 0;
  for (const auto& info : modifiers) {
    ++counter;
    const SlicerModifierNode& node = *info.node;
    std::shared_ptr<const PolySet> region;
    if (!node.getChildren().empty()) {
      region = asTriangles(geomevaluator.evaluateGeometry(*node.getChildren().front(), false));
    }
    if (!region) {
      LOG(message_group::Warning, node.modinst->location(), "",
          "slicer_modifier() has no 3D geometry, not exported");
      continue;
    }
    auto world = std::make_shared<PolySet>(*region);
    world->transform(info.world);

    SlicerVolume volume;
    volume.volumeType = node.prusaVolumeType();
    volume.name = node.volume_name.empty() ? "slicer_modifier " + std::to_string(counter)
                                           : node.volume_name;
    volume.settings = node.settings;
    volume.geometry = world;
    volume.displayColor = node.regionColor();
    volumes->push_back(std::move(volume));
  }
  return volumes;
}

std::shared_ptr<const PolySet> colorSlicerRegions(const std::shared_ptr<const PolySet>& model,
                                                  const SlicerVolumes& volumes)
{
#ifdef ENABLE_MANIFOLD
  if (!model || model->isEmpty() || volumes.empty()) return model;
  const auto start = ManifoldUtils::createManifoldFromPolySet(*model);
  if (!start || !start->isValid() || start->isEmpty()) return model;
  ManifoldGeometry result = *start;
  for (const auto& volume : volumes) {
    const auto region = ManifoldUtils::createManifoldFromPolySet(*volume.geometry);
    if (!region || !region->isValid() || region->isEmpty()) continue;
    ManifoldGeometry inside = result * *region;
    if (inside.isEmpty()) continue;
    inside.setColor(volume.displayColor);
    result = (result - *region) + inside;
  }
  return result.toPolySet();
#else
  return model;
#endif
}

SlicerObject buildSlicerObject(const std::shared_ptr<const Geometry>& model,
                               const SlicerVolumes& volumes, bool sortParts)
{
  SlicerObject object;
  object.mesh = std::make_shared<PolySet>(3);
  object.mesh->setTriangular(true);

  auto add = [&](const std::shared_ptr<const PolySet>& ps, SlicerPart part, bool keepColors) {
    if (!ps || ps->indices.empty()) return;
    std::shared_ptr<const PolySet> source = ps;
    if (sortParts) source = createSortedPolySet(*ps);
    part.firstTriangle = object.mesh->indices.size();
    appendPart(*object.mesh, *source, keepColors);
    part.lastTriangle = object.mesh->indices.size() - 1;
    object.parts.push_back(std::move(part));
  };

  Geometry::Geometries items;
  if (const auto list = std::dynamic_pointer_cast<const GeometryList>(model)) {
    items = list->flatten();
  } else if (model) {
    items.emplace_back(nullptr, model);
  }
  const bool single = items.size() == 1;
  int partIndex = 0;
  for (const auto& item : items) {
    ++partIndex;
    const std::string name = single ? "OpenSCAD Model" : "Part " + std::to_string(partIndex);
    add(asTriangles(item.second), {0, 0, "ModelPart", name, {}}, true);
  }
  for (const auto& volume : volumes) {
    add(volume.geometry, {0, 0, volume.volumeType, volume.name, volume.settings}, false);
  }

  // No colors anywhere: drop the all -1 color indices again.
  if (object.mesh->colors.empty()) object.mesh->color_indices.clear();
  return object;
}

std::string prusaSlicerModelConfig(int objectId, const std::string& objectName,
                                   const std::vector<SlicerPart>& parts)
{
  std::ostringstream xml;
  auto metadata = [&](const char *type, const std::string& key, const std::string& value) {
    xml << "   <metadata type=\"" << type << "\" key=\"" << escapeXml(key) << "\" value=\""
        << escapeXml(value) << "\"/>\n";
  };

  xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  xml << "<config>\n";
  xml << " <object id=\"" << objectId << "\" instances_count=\"1\">\n";
  xml << "  <metadata type=\"object\" key=\"name\" value=\"" << escapeXml(objectName) << "\"/>\n";
  for (const auto& part : parts) {
    xml << "  <volume firstid=\"" << part.firstTriangle << "\" lastid=\"" << part.lastTriangle
        << "\">\n";
    metadata("volume", "name", part.name);
    // PrusaSlicer itself writes both: "modifier" for readers older than 1.42.
    if (part.volumeType == "ParameterModifier") metadata("volume", "modifier", "1");
    metadata("volume", "volume_type", part.volumeType);
    for (const auto& [key, value] : part.settings) metadata("volume", key, value);
    xml << "  </volume>\n";
  }
  xml << " </object>\n";
  xml << "</config>\n";
  return xml.str();
}
