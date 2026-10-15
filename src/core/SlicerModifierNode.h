#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/BaseVisitable.h"
#include "core/ModuleInstantiation.h"
#include "core/node.h"
#include "geometry/linalg.h"

/*!
   A slicer_modifier() node: a region that a slicer treats specially (different
   settings, negative volume, support blocker/enforcer). Targets PrusaSlicer.

   Its children define the region's shape but are NOT part of the model: the
   GeometryEvaluator skips them (F6, STL, ... are unaffected); the preview,
   camera snapshots and drawings color the part of the model inside the region
   (see CSGTreeEvaluator::buildCSGTree). The 3MF exporter picks
   the nodes up via collectSlicerModifiers() and writes them as extra volumes
   of the model object, see io/slicer_volumes.h.

   All children are wrapped in a single group child, so evaluating that child
   yields the union of the region.
 */
class SlicerModifierNode : public AbstractNode
{
public:
  VISITABLE();
  SlicerModifierNode(const ModuleInstantiation *mi) : AbstractNode(mi) {}
  std::string toString() const override;
  std::string name() const override;

  enum class Type { MODIFIER, NEGATIVE, SUPPORT_BLOCKER, SUPPORT_ENFORCER };

  Type type{Type::MODIFIER};
  std::string volume_name;
  // Display color of the model part inside the region (preview, cameras,
  // drawings); only meaningful when has_color, otherwise defaultRegionColor().
  Color4f region_color;
  bool has_color{false};
  Color4f regionColor() const { return has_color ? region_color : defaultRegionColor(); }
  static Color4f defaultRegionColor();
  // PrusaSlicer config key -> value, already serialized, in source order.
  std::vector<std::pair<std::string, std::string>> settings;

  // PrusaSlicer "volume_type" string for this node's type.
  const char *prusaVolumeType() const;
  // The keyword used in slicer_modifier(type = ...).
  static const char *typeKeyword(Type type);
};

// A slicer_modifier node together with its accumulated world transform.
struct SlicerModifierInfo {
  Transform3d world;
  const SlicerModifierNode *node;
};

// Walk the node tree from `root`, accumulating TransformNode matrices, and
// return every slicer_modifier() with its world transform (in tree order).
// Nested slicer_modifier()s inside another one are not collected.
std::vector<SlicerModifierInfo> collectSlicerModifiers(const std::shared_ptr<const AbstractNode>& root);

// True if `key` is a PrusaSlicer per-region setting (valid on a modifier volume).
bool isPrusaSlicerRegionKey(const std::string& key);
// True if `key` is a PrusaSlicer per-object setting (no effect on a modifier).
bool isPrusaSlicerObjectKey(const std::string& key);
