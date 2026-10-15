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

#include "core/SlicerModifierNode.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/Arguments.h"
#include "core/Builtins.h"
#include "core/Children.h"
#include "core/ColorUtil.h"
#include "core/ModuleInstantiation.h"
#include "core/TransformNode.h"
#include "core/Value.h"
#include "core/module.h"
#include "geometry/linalg.h"
#include "utils/printutils.h"

namespace {

// Setting keys of PrusaSlicer 2.9.2 (src/libslic3r/PrintConfig.hpp).
// An unknown key makes PrusaSlicer reject the whole 3MF, so only these are
// ever written.
enum class SettingKind { Bool, Int, Float, Percent, FloatOrPercent, Enum };

struct SettingKey {
  std::string_view name;
  SettingKind kind;
};

// PrintRegionConfig + "extruder": settings a modifier volume can override,
// with their PrusaSlicer option type.
constexpr SettingKey kRegionKeys[] = {
  {"automatic_infill_combination", SettingKind::Bool},
  {"automatic_infill_combination_max_layer_height", SettingKind::FloatOrPercent},
  {"bottom_fill_pattern", SettingKind::Enum},
  {"bottom_solid_layers", SettingKind::Int},
  {"bottom_solid_min_thickness", SettingKind::Float},
  {"bridge_angle", SettingKind::Float},
  {"bridge_flow_ratio", SettingKind::Float},
  {"bridge_speed", SettingKind::Float},
  {"enable_dynamic_overhang_speeds", SettingKind::Bool},
  {"ensure_vertical_shell_thickness", SettingKind::Enum},
  {"external_perimeter_extrusion_width", SettingKind::FloatOrPercent},
  {"external_perimeter_speed", SettingKind::FloatOrPercent},
  {"external_perimeters_first", SettingKind::Bool},
  {"extra_perimeters", SettingKind::Bool},
  {"extra_perimeters_on_overhangs", SettingKind::Bool},
  {"extruder", SettingKind::Int},
  {"fill_angle", SettingKind::Float},
  {"fill_density", SettingKind::Percent},
  {"fill_pattern", SettingKind::Enum},
  {"fuzzy_skin", SettingKind::Enum},
  {"fuzzy_skin_point_dist", SettingKind::Float},
  {"fuzzy_skin_thickness", SettingKind::Float},
  {"gap_fill_enabled", SettingKind::Bool},
  {"gap_fill_speed", SettingKind::Float},
  {"infill_anchor", SettingKind::FloatOrPercent},
  {"infill_anchor_max", SettingKind::FloatOrPercent},
  {"infill_every_layers", SettingKind::Int},
  {"infill_extruder", SettingKind::Int},
  {"infill_extrusion_width", SettingKind::FloatOrPercent},
  {"infill_overlap", SettingKind::FloatOrPercent},
  {"infill_speed", SettingKind::Float},
  {"ironing", SettingKind::Bool},
  {"ironing_flowrate", SettingKind::Percent},
  {"ironing_spacing", SettingKind::Float},
  {"ironing_speed", SettingKind::Float},
  {"ironing_type", SettingKind::Enum},
  {"only_one_perimeter_first_layer", SettingKind::Bool},
  {"over_bridge_speed", SettingKind::FloatOrPercent},
  {"overhang_speed_0", SettingKind::FloatOrPercent},
  {"overhang_speed_1", SettingKind::FloatOrPercent},
  {"overhang_speed_2", SettingKind::FloatOrPercent},
  {"overhang_speed_3", SettingKind::FloatOrPercent},
  {"overhangs", SettingKind::Bool},
  {"perimeter_extruder", SettingKind::Int},
  {"perimeter_extrusion_width", SettingKind::FloatOrPercent},
  {"perimeter_speed", SettingKind::Float},
  {"perimeters", SettingKind::Int},
  {"scarf_seam_entire_loop", SettingKind::Bool},
  {"scarf_seam_length", SettingKind::Float},
  {"scarf_seam_max_segment_length", SettingKind::Float},
  {"scarf_seam_on_inner_perimeters", SettingKind::Bool},
  {"scarf_seam_only_on_smooth", SettingKind::Bool},
  {"scarf_seam_placement", SettingKind::Enum},
  {"scarf_seam_start_height", SettingKind::Percent},
  {"small_perimeter_speed", SettingKind::FloatOrPercent},
  {"solid_infill_below_area", SettingKind::Float},
  {"solid_infill_every_layers", SettingKind::Int},
  {"solid_infill_extruder", SettingKind::Int},
  {"solid_infill_extrusion_width", SettingKind::FloatOrPercent},
  {"solid_infill_speed", SettingKind::FloatOrPercent},
  {"thin_walls", SettingKind::Bool},
  {"top_fill_pattern", SettingKind::Enum},
  {"top_infill_extrusion_width", SettingKind::FloatOrPercent},
  {"top_one_perimeter_type", SettingKind::Enum},
  {"top_solid_infill_speed", SettingKind::FloatOrPercent},
  {"top_solid_layers", SettingKind::Int},
  {"top_solid_min_thickness", SettingKind::Float},
  {"wipe_into_infill", SettingKind::Bool},
};

// PrintObjectConfig: per-object settings, without effect on a modifier volume.
constexpr std::string_view kObjectKeys[] = {
  "brim_separation",
  "brim_type",
  "brim_width",
  "dont_support_bridges",
  "elefant_foot_compensation",
  "extrusion_width",
  "first_layer_acceleration_over_raft",
  "first_layer_speed_over_raft",
  "interface_shells",
  "interlocking_beam",
  "interlocking_beam_layer_count",
  "interlocking_beam_width",
  "interlocking_boundary_avoidance",
  "interlocking_depth",
  "interlocking_orientation",
  "layer_height",
  "min_bead_width",
  "min_feature_size",
  "mmu_segmented_region_interlocking_depth",
  "mmu_segmented_region_max_width",
  "perimeter_generator",
  "raft_contact_distance",
  "raft_expansion",
  "raft_first_layer_density",
  "raft_first_layer_expansion",
  "raft_layers",
  "seam_position",
  "slice_closing_radius",
  "slicing_mode",
  "staggered_inner_seams",
  "support_material",
  "support_material_angle",
  "support_material_auto",
  "support_material_bottom_contact_distance",
  "support_material_bottom_interface_layers",
  "support_material_buildplate_only",
  "support_material_closing_radius",
  "support_material_contact_distance",
  "support_material_enforce_layers",
  "support_material_extruder",
  "support_material_extrusion_width",
  "support_material_interface_contact_loops",
  "support_material_interface_extruder",
  "support_material_interface_layers",
  "support_material_interface_pattern",
  "support_material_interface_spacing",
  "support_material_interface_speed",
  "support_material_pattern",
  "support_material_spacing",
  "support_material_speed",
  "support_material_style",
  "support_material_synchronize_layers",
  "support_material_threshold",
  "support_material_with_sheath",
  "support_material_xy_spacing",
  "support_tree_angle",
  "support_tree_angle_slow",
  "support_tree_branch_diameter",
  "support_tree_branch_diameter_angle",
  "support_tree_branch_diameter_double_wall",
  "support_tree_branch_distance",
  "support_tree_tip_diameter",
  "support_tree_top_rate",
  "thick_bridges",
  "wall_distribution_count",
  "wall_transition_angle",
  "wall_transition_filter_deviation",
  "wall_transition_length",
  "wipe_into_objects",
  "xy_size_compensation",
};

const SettingKey *findRegionKey(const std::string& key)
{
  for (const auto& k : kRegionKeys) {
    if (k.name == key) return &k;
  }
  return nullptr;
}

// Numbers are always written in the C locale ("2.5", never "2,5").
std::string formatNumber(double d)
{
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out.precision(15);
  out << d;
  return out.str();
}

bool isNumberText(std::string text, bool allowPercent)
{
  if (allowPercent && !text.empty() && text.back() == '%') text.pop_back();
  std::istringstream in(text);
  in.imbue(std::locale::classic());
  double d;
  in >> d;
  return !in.fail() && (in >> std::ws).eof();
}

const char *kindDescription(SettingKind kind)
{
  switch (kind) {
  case SettingKind::Bool:           return "a boolean";
  case SettingKind::Int:            return "an integer";
  case SettingKind::Float:          return "a number";
  case SettingKind::Percent:        return "a percentage (number or \"N%\")";
  case SettingKind::FloatOrPercent: return "a number or \"N%\"";
  default:                          return "a string";
  }
}

// Serialize a setting value the way PrusaSlicer's set_deserialize() reads it.
// A value PrusaSlicer cannot parse (bool, int, number) makes it reject the
// whole 3MF, so anything not matching `kind` is refused here. Enum strings are
// passed through: PrusaSlicer substitutes an unknown one with a warning.
bool serializeSetting(const Value& value, SettingKind kind, std::string& out)
{
  const auto type = value.type();
  if (type == Value::Type::NUMBER && !std::isfinite(value.toDouble())) return false;
  switch (kind) {
  case SettingKind::Bool:
    if (type == Value::Type::BOOL) {
      out = value.toBool() ? "1" : "0";
      return true;
    }
    if (type == Value::Type::NUMBER && (value.toDouble() == 0 || value.toDouble() == 1)) {
      out = value.toDouble() == 1 ? "1" : "0";
      return true;
    }
    return false;
  case SettingKind::Int:
    if (type == Value::Type::NUMBER && value.toDouble() == std::floor(value.toDouble())) {
      out = formatNumber(value.toDouble());
      return true;
    }
    if (type == Value::Type::STRING) {
      const std::string text = value.toString();
      std::istringstream in(text);
      in.imbue(std::locale::classic());
      long long i;
      in >> i;
      if (in.fail() || !(in >> std::ws).eof()) return false;
      out = text;
      return true;
    }
    return false;
  case SettingKind::Float:
  case SettingKind::Percent:
  case SettingKind::FloatOrPercent: {
    const bool percent = kind == SettingKind::Percent;
    if (type == Value::Type::NUMBER) {
      // PrusaSlicer reads a bare number as a ratio for percent options ("50" -> 5000%).
      out = formatNumber(value.toDouble()) + (percent ? "%" : "");
      return true;
    }
    if (type == Value::Type::STRING && isNumberText(value.toString(), kind != SettingKind::Float)) {
      out = value.toString();
      if (percent && out.back() != '%') out += "%";
      return true;
    }
    return false;
  }
  case SettingKind::Enum:
    if (type == Value::Type::STRING) {
      out = value.toString();
      return true;
    }
    return false;
  }
  return false;
}

bool parseType(const std::string& s, SlicerModifierNode::Type& type)
{
  using T = SlicerModifierNode::Type;
  for (const T t : {T::MODIFIER, T::NEGATIVE, T::SUPPORT_BLOCKER, T::SUPPORT_ENFORCER}) {
    if (s == SlicerModifierNode::typeKeyword(t)) {
      type = t;
      return true;
    }
  }
  return false;
}

}  // namespace

bool isPrusaSlicerRegionKey(const std::string& key) { return findRegionKey(key) != nullptr; }

bool isPrusaSlicerObjectKey(const std::string& key)
{
  return std::find(std::begin(kObjectKeys), std::end(kObjectKeys), key) != std::end(kObjectKeys);
}

static std::shared_ptr<AbstractNode> builtin_slicer_modifier(const ModuleInstantiation *inst,
                                                             Arguments arguments,
                                                             const Children& children)
{
  auto node = std::make_shared<SlicerModifierNode>(inst);
  const std::string& docRoot = arguments.documentRoot();

  for (const auto& arg : arguments) {
    if (!arg.name) {
      LOG(message_group::Warning, inst->location(), docRoot,
          "slicer_modifier() ignoring positional argument, settings must be named");
      continue;
    }
    const std::string& key = *arg.name;
    if (key == "type") {
      if (arg->type() != Value::Type::STRING || !parseType(arg->toString(), node->type)) {
        LOG(message_group::Warning, inst->location(), docRoot,
            "slicer_modifier() unknown type %1$s, expected \"modifier\", \"negative\", "
            "\"support_blocker\" or \"support_enforcer\"; using \"modifier\"",
            arg->toEchoStringNoThrow());
      }
      continue;
    }
    if (key == "color") {
      std::optional<Color4f> color;
      if (arg->type() == Value::Type::STRING) {
        color = OpenSCAD::parse_color(arg->toString());
      } else if (arg->type() == Value::Type::VECTOR) {
        const auto& vec = arg->toVector();
        if (vec.size() == 3 || vec.size() == 4) {
          Vector4f c;
          bool ok = true;
          for (size_t i = 0; i < 4; ++i) {
            if (i >= vec.size()) {
              c[i] = 1.0f;
            } else if (vec[i].type() == Value::Type::NUMBER && vec[i].toDouble() >= 0 &&
                       vec[i].toDouble() <= 1) {
              c[i] = static_cast<float>(vec[i].toDouble());
            } else {
              ok = false;
            }
          }
          if (ok) color = Color4f(c);
        }
      }
      if (color) {
        node->region_color = *color;
        node->has_color = true;
      } else {
        LOG(message_group::Warning, inst->location(), docRoot,
            "slicer_modifier() unable to parse color %1$s, using the default",
            arg->toEchoStringNoThrow());
      }
      continue;
    }
    if (key == "name") {
      if (arg->type() == Value::Type::STRING) {
        node->volume_name = arg->toString();
      } else {
        LOG(message_group::Warning, inst->location(), docRoot,
            "slicer_modifier() name must be a string, ignoring %1$s", arg->toEchoStringNoThrow());
      }
      continue;
    }
    const SettingKey *setting = findRegionKey(key);
    if (!setting) {
      if (isPrusaSlicerObjectKey(key)) {
        LOG(message_group::Warning, inst->location(), docRoot,
            "slicer_modifier() setting '%1$s' is a per-object setting and has no effect on a "
            "modifier, ignoring it",
            key);
      } else {
        LOG(message_group::Warning, inst->location(), docRoot,
            "slicer_modifier() unknown PrusaSlicer setting '%1$s', ignoring it", key);
      }
      continue;
    }
    std::string value;
    if (!serializeSetting(arg.value, setting->kind, value)) {
      LOG(message_group::Warning, inst->location(), docRoot,
          "slicer_modifier() setting '%1$s' expects %2$s, ignoring %3$s", key,
          kindDescription(setting->kind), arg->toEchoStringNoThrow());
      continue;
    }
    auto it = std::find_if(node->settings.begin(), node->settings.end(),
                           [&](const auto& s) { return s.first == key; });
    if (it != node->settings.end()) {
      it->second = value;
    } else {
      node->settings.emplace_back(key, value);
    }
  }

  if (node->type != SlicerModifierNode::Type::MODIFIER && !node->settings.empty()) {
    LOG(message_group::Warning, inst->location(), docRoot,
        "slicer_modifier() settings have no effect on type \"%1$s\", ignoring them",
        SlicerModifierNode::typeKeyword(node->type));
    node->settings.clear();
  }

  // Wrap all children in one group, so that evaluating it yields their union.
  auto group = std::make_shared<GroupNode>(inst);
  children.instantiate(group);
  node->children.push_back(group);
  return node;
}

const char *SlicerModifierNode::typeKeyword(Type type)
{
  switch (type) {
  case Type::NEGATIVE:         return "negative";
  case Type::SUPPORT_BLOCKER:  return "support_blocker";
  case Type::SUPPORT_ENFORCER: return "support_enforcer";
  default:                     return "modifier";
  }
}

const char *SlicerModifierNode::prusaVolumeType() const
{
  switch (this->type) {
  case Type::NEGATIVE:         return "NegativeVolume";
  case Type::SUPPORT_BLOCKER:  return "SupportBlocker";
  case Type::SUPPORT_ENFORCER: return "SupportEnforcer";
  default:                     return "ParameterModifier";
  }
}

Color4f SlicerModifierNode::defaultRegionColor()
{
  return {0.70f, 0.30f, 0.90f, 1.0f};  // purple: rare in models, distinct from the default yellow
}

std::string SlicerModifierNode::toString() const
{
  std::ostringstream stream;
  stream << "slicer_modifier(type = \"" << typeKeyword(this->type) << "\"";
  if (!this->volume_name.empty()) {
    stream << ", name = " << QuotedString(this->volume_name);
  }
  if (this->has_color) {
    stream << ", color = [" << this->region_color.r() << ", " << this->region_color.g() << ", "
           << this->region_color.b() << ", " << this->region_color.a() << "]";
  }
  for (const auto& [key, value] : this->settings) {
    stream << ", " << key << " = " << QuotedString(value);
  }
  stream << ")";
  return stream.str();
}

std::string SlicerModifierNode::name() const
{
  return "slicer_modifier";
}

static void collectSlicerModifiersRecursive(const AbstractNode& node, const Transform3d& parent,
                                            std::vector<SlicerModifierInfo>& out)
{
  Transform3d current = parent;
  if (const auto *transform = dynamic_cast<const TransformNode *>(&node)) {
    current = parent * transform->matrix;
  }
  if (const auto *modifier = dynamic_cast<const SlicerModifierNode *>(&node)) {
    out.push_back({current, modifier});
    return;
  }
  for (const auto& child : node.getChildren()) {
    if (child) collectSlicerModifiersRecursive(*child, current, out);
  }
}

std::vector<SlicerModifierInfo> collectSlicerModifiers(const std::shared_ptr<const AbstractNode>& root)
{
  std::vector<SlicerModifierInfo> modifiers;
  if (root) {
    collectSlicerModifiersRecursive(*root, Transform3d::Identity(), modifiers);
  }
  return modifiers;
}

void register_builtin_slicer_modifier()
{
  Builtins::init(
    "slicer_modifier", new BuiltinModule(builtin_slicer_modifier),
    {
      "slicer_modifier(fill_density = \"80%\", perimeters = 5, ...)  // PrusaSlicer setting keys",
      "slicer_modifier(type = \"modifier\" | \"negative\" | \"support_blocker\" | \"support_enforcer\")",
      "slicer_modifier(..., name = \"hole\", color = \"red\")  // display color of the region",
    });
}
