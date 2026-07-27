#include "gui/ViewScope.h"

#include <string>
#include <vector>

#include <catch2/catch_all.hpp>

using ViewScope::Block;
using ViewScope::ViewRef;
using Path = std::vector<std::string>;

static std::vector<std::string> splitLines(const std::string& s)
{
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == '\n') {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}

TEST_CASE("ViewScope resolves nested block ranges and owners", "[viewscope]")
{
  // 0: view("assembly") {
  // 1:   cube(10);
  // 2:   // @dim a b            <- owned by assembly
  // 3:   view("exploded") {
  // 4:     // @dim c d          <- owned by assembly/exploded
  // 5:   }
  // 6: }
  // 7: // @dim e f              <- common (root)
  const auto lines = splitLines(
    "view(\"assembly\") {\n"
    "  cube(10);\n"
    "  // @dim a b\n"
    "  view(\"exploded\") {\n"
    "    // @dim c d\n"
    "  }\n"
    "}\n"
    "// @dim e f");

  const std::vector<ViewRef> views = {
    {Path{"assembly"}, 1},               // 1-based header line
    {Path{"assembly", "exploded"}, 4},
  };

  const auto blocks = ViewScope::resolveBlocks(lines, views);
  REQUIRE(blocks.size() == 2);
  CHECK(blocks[0].braced);
  CHECK(blocks[0].first0 == 0);
  CHECK(blocks[0].close0 == 6);  // outer } is line 6
  CHECK(blocks[1].braced);
  CHECK(blocks[1].first0 == 3);
  CHECK(blocks[1].close0 == 5);  // inner } is line 5

  CHECK(ViewScope::ownerPath(blocks, 2) == Path{"assembly"});
  CHECK(ViewScope::ownerPath(blocks, 4) == (Path{"assembly", "exploded"}));  // innermost wins
  CHECK(ViewScope::ownerPath(blocks, 7).empty());                            // common
}

TEST_CASE("ViewScope prefix visibility follows accumulation down", "[viewscope]")
{
  const Path root{};
  const Path assembly{"assembly"};
  const Path exploded{"assembly", "exploded"};

  // common (root) is visible in every view
  CHECK(ViewScope::isPrefix(root, root));
  CHECK(ViewScope::isPrefix(root, assembly));
  CHECK(ViewScope::isPrefix(root, exploded));
  // an assembly-owned annotation shows in assembly and below, not at root
  CHECK_FALSE(ViewScope::isPrefix(assembly, root));
  CHECK(ViewScope::isPrefix(assembly, assembly));
  CHECK(ViewScope::isPrefix(assembly, exploded));
  // an exploded-owned annotation shows only in exploded
  CHECK_FALSE(ViewScope::isPrefix(exploded, assembly));
  CHECK(ViewScope::isPrefix(exploded, exploded));
  // a sibling branch is never a prefix
  CHECK_FALSE(ViewScope::isPrefix(Path{"other"}, exploded));
}

TEST_CASE("ViewScope insertLine targets the active block's closing brace", "[viewscope]")
{
  const auto lines = splitLines(
    "view(\"assembly\") {\n"
    "  cube(10);\n"
    "  view(\"exploded\") {\n"
    "  }\n"
    "}\n"
    "view(\"single\") sphere(5);");
  const std::vector<ViewRef> views = {
    {Path{"assembly"}, 1},
    {Path{"assembly", "exploded"}, 3},
    {Path{"single"}, 6},
  };
  const auto blocks = ViewScope::resolveBlocks(lines, views);

  CHECK(ViewScope::insertLine(blocks, Path{"assembly"}) == 4);              // before outer }
  CHECK(ViewScope::insertLine(blocks, Path{"assembly", "exploded"}) == 3);  // before inner }
  CHECK(ViewScope::insertLine(blocks, Path{}) == -1);                       // root -> EOF
  CHECK(ViewScope::insertLine(blocks, Path{"missing"}) == -1);

  // A single-statement view has no brace body: not insertable.
  REQUIRE(blocks[2].path == Path{"single"});
  CHECK_FALSE(blocks[2].braced);
  CHECK(ViewScope::insertLine(blocks, Path{"single"}) == -1);
}
