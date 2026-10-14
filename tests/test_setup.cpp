#include <doctest/doctest.h>

#include <windows.h>

#include "core/Settings.h"
#include "providers/Setup.h"
#include "providers/Registry.h"

using namespace aia;

TEST_CASE("discovery refuses cloud URLs and loopback lookalikes") {
  for (auto url : {"http://localhost:11434/v1", "http://127.0.0.1:1234/v1", "http://[::1]:1234/v1"}) CHECK(IsLoopbackUrl(url));
  for (auto url : {"https://localhost.example.com", "http://127.0.0.1.evil.test", "https://example.com", "file:///localhost", "http://user:secret@localhost", "bad url"}) CHECK_FALSE(IsLoopbackUrl(url));
  CHECK_FALSE(DiscoverLocalModels("ollama", "https://example.com/v1").error.empty());
}

class FakeSetupProvider : public Provider {
 public:
  ChatRequest received;
  StreamResult result{true};
  std::string output = "Hello!";
  ModelListResult ListModels(CancelToken&) override { return {}; }
  StreamResult Stream(const ChatRequest& request, const ModelInfo&, const StreamSink& sink, CancelToken&) override {
    received = request;
    sink.text(output);
    return result;
  }
};

TEST_CASE("setup verifies an actual isolated text answer") {
  FakeSetupProvider provider;
  CancelToken cancel;
  auto answer = TestSetupAnswer(provider, {"test-model", "Test"}, cancel);
  CHECK(answer.ok);
  CHECK(answer.answer == "Hello!");
  CHECK(provider.received.model == "test-model");
  REQUIRE(provider.received.messages.size() == 1);
  CHECK_FALSE(provider.received.messages[0].image.has_value());
  CHECK(provider.received.messages[0].raw.is_null());
  CHECK(provider.received.messages[0].text == "Say hello in one short sentence.");
  SUBCASE("empty text is not a connection success") {
    provider.output = " \n\t";
    CHECK_FALSE(TestSetupAnswer(provider, {}, cancel).ok);
  }
  SUBCASE("provider failures are retained") {
    provider.result = {false, false, "Sign-in expired"};
    CHECK(TestSetupAnswer(provider, {}, cancel).error == "Sign-in expired");
  }
  SUBCASE("cancellation is not success even after receiving text") {
    cancel.Cancel();
    CHECK_FALSE(TestSetupAnswer(provider, {}, cancel).ok);
  }
  SUBCASE("long Unicode answers remain valid UTF-8") {
    provider.output = std::string(4095, 'a') + "🙂";
    auto longAnswer = TestSetupAnswer(provider, {}, cancel);
    CHECK(longAnswer.answer.size() == 4095);
    CHECK_NOTHROW(json(longAnswer.answer).dump());
  }
}

TEST_CASE("local discovery and a real HTTP provider complete a text-only setup test") {
  // scripts/test-setup-http.mjs provides a synthetic loopback server. No paid API is called.
  char endpoint[4096];
  if (!GetEnvironmentVariableA("AIASSISTANT_SETUP_TEST_ENDPOINT", endpoint, sizeof(endpoint))) return;
  auto models = DiscoverLocalModels("ollama", endpoint);
  INFO(models.error);
  REQUIRE(models.error.empty());
  REQUIRE(models.models.size() == 1);
  CHECK(models.models[0].id == "synthetic-model");
  for (auto suffix : {"/empty", "/malformed", "/redirect"}) {
    CHECK_FALSE(DiscoverLocalModels("ollama", std::string(endpoint) + suffix).error.empty());
  }
  auto provider = CreateProvider("ollama", {{"baseUrl", endpoint}}, "");
  REQUIRE(provider);
  CancelToken cancel;
  auto answer = TestSetupAnswer(*provider, models.models[0], cancel);
  CHECK(answer.ok);
  CHECK(answer.answer == "Hello from the synthetic server!");
}

TEST_CASE("setup completion is persisted in normalized settings") {
  Settings settings;
  CHECK_FALSE(settings.Data()["setupCompleted"].get<bool>());
  settings.Apply({{"setupCompleted", true}});
  CHECK(settings.Data()["setupCompleted"].get<bool>());
}
