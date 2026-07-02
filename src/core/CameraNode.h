#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/BaseVisitable.h"
#include "core/ModuleInstantiation.h"
#include "core/node.h"
#include "geometry/linalg.h"

/*!
   A camera() marker node placed in the model tree.

   It carries no geometry of its own: in every visitor it is treated like an
   empty group (see NodeVisitor.h), so it is invisible in preview, render and
   export and does not disturb measurement. Its purpose is to record a camera
   pose (via the accumulated transform of its parents) together with lens and
   sensor parameters, so that the GUI can render an offscreen snapshot from it.

   The lens is described either by focal length + sensor height (from which the
   vertical field of view is derived) or by an explicit fov. Pixel dimensions
   set the aspect ratio and output resolution.
 */
class CameraNode : public AbstractNode
{
public:
  VISITABLE();
  CameraNode(const ModuleInstantiation *mi) : AbstractNode(mi) {}
  std::string toString() const override;
  std::string name() const override;

  enum class Projection { PERSPECTIVE, ORTHOGONAL };

  // Lens: either focal + sensor (mm) -> derived fov, or explicit fov (deg).
  double focal{35.0};   // focal length in mm
  double sensor{24.0};  // vertical sensor size in mm (full-frame default)
  double fov{-1.0};     // if >= 0, use directly instead of focal/sensor

  unsigned int pixel_width{1920};
  unsigned int pixel_height{1080};

  std::string camera_name;

  Projection projection{Projection::PERSPECTIVE};
};

// A camera marker together with its accumulated world transform, produced by
// walking the node tree (see collectCameras).
struct CameraInfo {
  Transform3d world;
  const CameraNode *node;
};

// Walk the node tree from `root`, accumulating TransformNode matrices, and
// return every camera() marker with its world transform (in tree order).
std::vector<CameraInfo> collectCameras(const std::shared_ptr<const AbstractNode>& root);
