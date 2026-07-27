#pragma once

#include <string>
#include <vector>

// Fáze 15: mapping between source lines and the view() block that encloses them.
//
// A "view path" is the chain of view() names from the root down to a block
// (e.g. {"assembly","exploded"}). The empty path is the root (common geometry,
// outside any view()). An annotation (// @dim, // @sheet, ...) belongs to the
// deepest view() block whose braces enclose its line; it is shown when that
// owner path is a prefix of the active view path (accumulation down the tree).
//
// These helpers are pure (no Qt) so they can be unit-tested; MainWindow drives
// them with the editor text and the views found by collectViews().
namespace ViewScope {

// One view() block resolved to a line range in one file.
struct Block {
  std::vector<std::string> path;  // full view path (root..this)
  int first0{-1};                 // 0-based line of the view( header
  int close0{-1};                 // 0-based line of the matching } (braced) or the
                                  // statement's ; (single-statement form)
  bool braced{false};             // true if the body is a { ... } block
};

// One view() to resolve: its full path and the 1-based header line (as reported
// by ViewInfo.firstLine).
struct ViewRef {
  std::vector<std::string> path;
  int firstLine1{-1};
};

// Resolve every view header to its block range by matching braces in `lines`
// (0-based). Views whose body is a single statement (no { }) get braced=false and
// close0 at their terminating ';'. Input order is preserved.
std::vector<Block> resolveBlocks(const std::vector<std::string>& lines,
                                 const std::vector<ViewRef>& views);

// Deepest (innermost) block whose range encloses `line0`, i.e. its view path;
// empty if the line is outside every view() block (common / root).
std::vector<std::string> ownerPath(const std::vector<Block>& blocks, int line0);

// True if `prefix` is a (non-strict) prefix of `path` — i.e. an annotation owned
// by `prefix` is visible when `path` is the active view.
bool isPrefix(const std::vector<std::string>& prefix, const std::vector<std::string>& path);

// 0-based line to insert a new annotation for the active view `active`: the line
// of the closing '}' of the block whose path equals `active` (insert before it).
// Returns -1 when `active` is empty (root -> caller appends at end of file) or the
// matching block is single-statement / not found.
int insertLine(const std::vector<Block>& blocks, const std::vector<std::string>& active);

}  // namespace ViewScope
