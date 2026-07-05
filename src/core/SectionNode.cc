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

#include "core/SectionNode.h"

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

static std::shared_ptr<AbstractNode> builtin_section(const ModuleInstantiation *inst,
                                                     Arguments arguments, const Children& children)
{
  auto node = std::make_shared<SectionNode>(inst);

  Parameters parameters =
    Parameters::parse(std::move(arguments), inst->location(), {"name"}, {"flip", "hatch"});

  node->section_name = parameters.get("name", node->section_name);
  if (parameters["flip"].type() == Value::Type::BOOL) node->flip = parameters["flip"].toBool();
  node->hatch = parameters.get("hatch", node->hatch);

  return children.instantiate(node);
}

std::string SectionNode::toString() const
{
  std::ostringstream stream;
  stream << "section(name = \"" << this->section_name << "\"";
  if (this->flip) stream << ", flip = true";
  if (this->hatch >= 0) stream << ", hatch = " << this->hatch;
  stream << ")";
  return stream.str();
}

std::string SectionNode::name() const
{
  return "section";
}

static void collectSectionsRecursive(const AbstractNode& node, const Transform3d& parent,
                                     std::vector<SectionInfo>& out)
{
  Transform3d current = parent;
  if (const auto *transform = dynamic_cast<const TransformNode *>(&node)) {
    current = parent * transform->matrix;
  }
  if (const auto *section = dynamic_cast<const SectionNode *>(&node)) {
    out.push_back({current, section});
  }
  for (const auto& child : node.getChildren()) {
    if (child) collectSectionsRecursive(*child, current, out);
  }
}

std::vector<SectionInfo> collectSections(const std::shared_ptr<const AbstractNode>& root)
{
  std::vector<SectionInfo> sections;
  if (root) {
    collectSectionsRecursive(*root, Transform3d::Identity(), sections);
  }
  return sections;
}

void register_builtin_section()
{
  Builtins::init("section", new BuiltinModule(builtin_section),
                 {
                   "section(name = \"A\")  // cutting plane = local XY, view along +Z",
                   "section(name = \"A\", flip = true)  // keep/view the other half",
                   "section(..., hatch = 3)  // hatch spacing in drawing-mm",
                 });
}
