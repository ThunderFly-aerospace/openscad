#include "gui/ViewScope.h"

namespace ViewScope {

std::vector<Block> resolveBlocks(const std::vector<std::string>& lines,
                                 const std::vector<ViewRef>& views)
{
  std::vector<Block> blocks;
  blocks.reserve(views.size());
  const int n = static_cast<int>(lines.size());

  for (const auto& v : views) {
    Block b;
    b.path = v.path;
    b.first0 = v.firstLine1 - 1;
    b.close0 = b.first0;
    b.braced = false;

    // Scan forward from the header, matching braces. A '{' before any ';' means a
    // block body; a ';' first means a single-statement view (no place to insert).
    int depth = 0;
    bool started = false;
    bool done = false;
    for (int li = b.first0; li >= 0 && li < n && !done; ++li) {
      const std::string& s = lines[li];
      for (char ch : s) {
        if (ch == '{') {
          ++depth;
          started = true;
        } else if (ch == '}') {
          if (started) {
            --depth;
            if (depth == 0) {
              b.close0 = li;
              b.braced = true;
              done = true;
              break;
            }
          }
        } else if (ch == ';' && !started) {
          b.close0 = li;
          done = true;
          break;
        }
      }
    }
    blocks.push_back(std::move(b));
  }
  return blocks;
}

std::vector<std::string> ownerPath(const std::vector<Block>& blocks, int line0)
{
  const std::vector<std::string> *best = nullptr;
  int bestFirst = -1;
  for (const auto& b : blocks) {
    if (line0 >= b.first0 && line0 <= b.close0) {
      // Innermost = the containing block with the greatest header line.
      if (b.first0 > bestFirst) {
        bestFirst = b.first0;
        best = &b.path;
      }
    }
  }
  return best ? *best : std::vector<std::string>{};
}

bool isPrefix(const std::vector<std::string>& prefix, const std::vector<std::string>& path)
{
  if (prefix.size() > path.size()) return false;
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (prefix[i] != path[i]) return false;
  }
  return true;
}

int insertLine(const std::vector<Block>& blocks, const std::vector<std::string>& active)
{
  if (active.empty()) return -1;
  for (const auto& b : blocks) {
    if (b.braced && b.path == active) return b.close0;
  }
  return -1;
}

}  // namespace ViewScope
