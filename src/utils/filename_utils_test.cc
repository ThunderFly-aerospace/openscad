#include "utils/filename_utils.h"

#include <catch2/catch_all.hpp>
#include <set>
#include <string>
#include <vector>

TEST_CASE("sanitizeFilenamePart transliterates diacritics and replaces spaces", "[filename]")
{
  CHECK(sanitizeFilenamePart("Přední díl") == "Predni_dil");
  CHECK(sanitizeFilenamePart("ŽLUŤOUČKÝ kůň") == "ZLUTOUCKY_kun");
  CHECK(sanitizeFilenamePart("Größe") == "Grosse");
}

TEST_CASE("sanitizeFilenamePart keeps safe characters", "[filename]")
{
  CHECK(sanitizeFilenamePart("front") == "front");
  CHECK(sanitizeFilenamePart("cam-1.v2_a") == "cam-1.v2_a");
}

TEST_CASE("sanitizeFilenamePart replaces separators and collapses underscores", "[filename]")
{
  CHECK(sanitizeFilenamePart("a/b\\c:d") == "a_b_c_d");
  CHECK(sanitizeFilenamePart("a   b") == "a_b");
  CHECK(sanitizeFilenamePart("a * ? b") == "a_b");
  CHECK(sanitizeFilenamePart("  x  ") == "x");
  CHECK(sanitizeFilenamePart("..hidden.") == "hidden");
}

TEST_CASE("sanitizeFilenamePart may return empty", "[filename]")
{
  CHECK(sanitizeFilenamePart("").empty());
  CHECK(sanitizeFilenamePart("   ").empty());
  CHECK(sanitizeFilenamePart("??").empty());
  CHECK(sanitizeFilenamePart("日本").empty());
}

TEST_CASE("sanitizeFilenamePart tolerates invalid UTF-8", "[filename]")
{
  CHECK(sanitizeFilenamePart(std::string("a\xff" "b")) == "a_b");
}

TEST_CASE("uniqueFilename appends a counter on collision", "[filename]")
{
  std::set<std::string> used;
  CHECK(uniqueFilename("out_Dil_A", used) == "out_Dil_A");
  CHECK(uniqueFilename("out_Dil_A", used) == "out_Dil_A_2");
  CHECK(uniqueFilename("out_Dil_A", used) == "out_Dil_A_3");
  CHECK(uniqueFilename("out_other", used) == "out_other");
}

TEST_CASE("sanitizedViewTag joins sanitized view path elements", "[filename]")
{
  CHECK(sanitizedViewTag({}).empty());
  CHECK(sanitizedViewTag({"Sestava", "Přední díl"}) == "Sestava_Predni_dil");
  CHECK(sanitizedViewTag({"a", "??", "b"}) == "a_b");
  CHECK(sanitizedViewTag({"日本"}) == "view");
}
