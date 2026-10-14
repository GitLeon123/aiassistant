#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

#include "net/Sse.h"

using namespace aia;

namespace {
using Events = std::vector<std::pair<std::string, std::string>>;

Events Parse(const std::vector<std::string>& chunks, bool finish = true) {
  Events out;
  SseParser p([&](const std::string& e, const std::string& d) { out.push_back({e, d}); });
  for (const auto& c : chunks) p.Feed(c);
  if (finish) p.Finish();
  return out;
}
}  // namespace

TEST_CASE("SSE: single event") {
  auto e = Parse({"event: ping\ndata: {\"a\":1}\n\n"});
  REQUIRE(e.size() == 1);
  CHECK(e[0].first == "ping");
  CHECK(e[0].second == "{\"a\":1}");
}

TEST_CASE("SSE: events split across arbitrary chunks") {
  std::string stream = "data: hello\n\nevent: x\ndata: world\n\n";
  for (size_t split = 1; split < stream.size(); ++split) {
    auto e = Parse({stream.substr(0, split), stream.substr(split)});
    REQUIRE(e.size() == 2);
    CHECK(e[0].second == "hello");
    CHECK(e[1].first == "x");
    CHECK(e[1].second == "world");
  }
}

TEST_CASE("SSE: CRLF and CR line endings, CRLF split between chunks") {
  auto e = Parse({"data: a\r", "\n\r\n", "data: b\r\rdata: c\n\n"});
  REQUIRE(e.size() == 3);
  CHECK(e[0].second == "a");
  CHECK(e[1].second == "b");
  CHECK(e[2].second == "c");
}

TEST_CASE("SSE: multi-line data, comments, no space after colon") {
  auto e = Parse({": keep-alive\n", "data:line1\ndata: line2\n\n"});
  REQUIRE(e.size() == 1);
  CHECK(e[0].second == "line1\nline2");
}

TEST_CASE("SSE: trailing event without blank line is dispatched on Finish") {
  CHECK(Parse({"data: tail"}, false).empty());
  auto e = Parse({"data: tail"});
  REQUIRE(e.size() == 1);
  CHECK(e[0].second == "tail");
}

TEST_CASE("SSE: blank lines without data do not dispatch") {
  CHECK(Parse({"\n\n\nevent: only\n\n"}).empty());
}

TEST_CASE("LineSplitter handles partial lines and CRLF") {
  std::vector<std::string> lines;
  LineSplitter s([&](std::string_view l) { lines.emplace_back(l); });
  s.Feed("{\"a\":1}\r\n{\"b\"");
  s.Feed(":2}\n\n");
  s.Feed("tail");
  s.Finish();
  REQUIRE(lines.size() == 3);
  CHECK(lines[0] == "{\"a\":1}");
  CHECK(lines[1] == "{\"b\":2}");
  CHECK(lines[2] == "tail");
}
