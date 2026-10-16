#pragma once

#include <set>
#include <string>
#include <vector>

// Turn a user-supplied name (view() path element, camera(name=…)) into a portable
// filename fragment: diacritics transliterated to ASCII ("Přední díl" -> "Predni_dil"),
// everything outside [A-Za-z0-9._-] replaced by '_', runs of '_' collapsed and
// leading/trailing '_' / '.' trimmed. May return an empty string (caller falls back
// to an index or similar).
std::string sanitizeFilenamePart(const std::string& name);

// Filename tag for a nested view() path: each element sanitized, empty ones
// dropped, joined with '_'. Returns `fallback` if nothing is left of a non-empty
// path; an empty path (the root) gives an empty tag.
std::string sanitizedViewTag(const std::vector<std::string>& path,
                             const std::string& fallback = "view");

// Return `base` if it is not in `used`, else the first free `base_2`, `base_3`, …
// The returned name is inserted into `used`.
std::string uniqueFilename(const std::string& base, std::set<std::string>& used);
