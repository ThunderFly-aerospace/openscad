#include "utils/filename_utils.h"

#include <glib.h>

#include <set>
#include <string>
#include <vector>

std::string sanitizeFilenamePart(const std::string& name)
{
  std::string ascii;
  if (g_utf8_validate(name.c_str(), static_cast<gssize>(name.size()), nullptr)) {
    // "C" locale: locale-independent transliteration table (ř -> r, ß -> ss, …);
    // characters it cannot map become '?', which the filter below turns into '_'.
    gchar *t = g_str_to_ascii(name.c_str(), "C");
    ascii = t ? t : "";
    g_free(t);
  } else {
    ascii = name;  // not UTF-8: every non-ASCII byte is replaced below
  }

  std::string out;
  out.reserve(ascii.size());
  for (const char ch : ascii) {
    const auto c = static_cast<unsigned char>(ch);
    const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                    c == '.' || c == '-' || c == '_';
    const char put = ok ? ch : '_';
    if (put == '_' && !out.empty() && out.back() == '_') continue;
    out.push_back(put);
  }
  const auto first = out.find_first_not_of("_.");
  if (first == std::string::npos) return {};
  const auto last = out.find_last_not_of("_.");
  return out.substr(first, last - first + 1);
}

std::string sanitizedViewTag(const std::vector<std::string>& path, const std::string& fallback)
{
  std::string tag;
  for (const auto& part : path) {
    const std::string s = sanitizeFilenamePart(part);
    if (s.empty()) continue;
    if (!tag.empty()) tag += "_";
    tag += s;
  }
  return tag.empty() && !path.empty() ? fallback : tag;
}

std::string uniqueFilename(const std::string& base, std::set<std::string>& used)
{
  std::string name = base;
  for (int n = 2; used.count(name); ++n) name = base + "_" + std::to_string(n);
  used.insert(name);
  return name;
}
