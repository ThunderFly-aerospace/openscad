/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "core/CameraNode.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include <vector>

#include "core/Builtins.h"
#include "core/Children.h"
#include "core/ModuleInstantiation.h"
#include "core/Parameters.h"
#include "core/TransformNode.h"
#include "core/module.h"
#include "geometry/linalg.h"
#include "utils/printutils.h"

static std::shared_ptr<AbstractNode> builtin_camera(const ModuleInstantiation *inst,
                                                    Arguments arguments, const Children& children)
{
  auto node = std::make_shared<CameraNode>(inst);

  Parameters parameters = Parameters::parse(std::move(arguments), inst->location(), {},
                                            {"focal", "sensor", "fov", "width", "height", "name",
                                             "projection", "marker", "show_modifiers"});

  node->focal = parameters.get("focal", node->focal);
  node->sensor = parameters.get("sensor", node->sensor);
  if (node->focal <= 0) {
    LOG(message_group::Warning, inst->location(), parameters.documentRoot(),
        "camera() focal must be > 0, ignoring value %1$.3f", node->focal);
    node->focal = 35.0;
  }
  if (node->sensor <= 0) {
    LOG(message_group::Warning, inst->location(), parameters.documentRoot(),
        "camera() sensor must be > 0, ignoring value %1$.3f", node->sensor);
    node->sensor = 24.0;
  }

  // An explicit fov (degrees) overrides the focal/sensor derivation.
  if (parameters["fov"].type() == Value::Type::NUMBER) {
    const double fov = parameters["fov"].toDouble();
    if (fov > 0 && fov < 180) {
      node->fov = fov;
    } else {
      LOG(message_group::Warning, inst->location(), parameters.documentRoot(),
          "camera() fov must be in (0, 180), ignoring value %1$.3f", fov);
    }
  }

  const double w = parameters.get("width", static_cast<double>(node->pixel_width));
  const double h = parameters.get("height", static_cast<double>(node->pixel_height));
  if (w >= 1) node->pixel_width = static_cast<unsigned int>(std::lround(w));
  if (h >= 1) node->pixel_height = static_cast<unsigned int>(std::lround(h));

  node->camera_name = parameters.get("name", node->camera_name);

  // marker: explicit frustum length in mm; <= 0 or absent keeps auto (-1).
  const double marker = parameters.get("marker", -1.0);
  if (marker > 0) node->marker = marker;

  if (parameters["show_modifiers"].type() == Value::Type::BOOL) {
    node->show_modifiers = parameters["show_modifiers"].toBool();
  }

  if (parameters["projection"].type() == Value::Type::STRING) {
    const auto p = boost::algorithm::to_lower_copy(parameters["projection"].toString());
    if (p == "ortho" || p == "orthogonal") {
      node->projection = CameraNode::Projection::ORTHOGONAL;
    } else if (p == "persp" || p == "perspective") {
      node->projection = CameraNode::Projection::PERSPECTIVE;
    } else {
      LOG(message_group::Warning, inst->location(), parameters.documentRoot(),
          "camera() unknown projection \"%1$s\", using \"perspective\"", p);
    }
  }

  return children.instantiate(node);
}

std::string CameraNode::toString() const
{
  std::ostringstream stream;
  stream << "camera(";
  if (this->fov >= 0) {
    stream << "fov = " << this->fov;
  } else {
    stream << "focal = " << this->focal << ", sensor = " << this->sensor;
  }
  stream << ", width = " << this->pixel_width << ", height = " << this->pixel_height
         << ", projection = \""
         << (this->projection == Projection::ORTHOGONAL ? "orthogonal" : "perspective") << "\"";
  if (!this->camera_name.empty()) {
    stream << ", name = \"" << this->camera_name << "\"";
  }
  if (this->marker > 0) {
    stream << ", marker = " << this->marker;
  }
  if (!this->show_modifiers) {
    stream << ", show_modifiers = false";
  }
  stream << ")";
  return stream.str();
}

std::string CameraNode::name() const
{
  return "camera";
}

static void collectCamerasRecursive(const AbstractNode& node, const Transform3d& parent,
                                    std::vector<CameraInfo>& out)
{
  Transform3d current = parent;
  if (const auto *transform = dynamic_cast<const TransformNode *>(&node)) {
    current = parent * transform->matrix;
  }
  if (const auto *camera = dynamic_cast<const CameraNode *>(&node)) {
    out.push_back({current, camera});
  }
  for (const auto& child : node.getChildren()) {
    if (child) collectCamerasRecursive(*child, current, out);
  }
}

std::vector<CameraInfo> collectCameras(const std::shared_ptr<const AbstractNode>& root)
{
  std::vector<CameraInfo> cameras;
  if (root) {
    collectCamerasRecursive(*root, Transform3d::Identity(), cameras);
  }
  return cameras;
}

void register_builtin_camera()
{
  Builtins::init("camera", new BuiltinModule(builtin_camera),
                 {
                   "camera(focal = 35, sensor = 24, width = 1920, height = 1080)",
                   "camera(fov = 45, width = 1920, height = 1080)",
                   "camera(..., name = \"front\", projection = \"perspective\")",
                   "camera(..., marker = 40)  // frustum length in mm (default auto)",
                   "camera(..., show_modifiers = false)  // hide slicer_modifier() regions",
                 });
}
