// Explicitly opt-in: these tests invoke signed-in CLIs and consume subscription usage.
// All prompts and images are synthetic; no desktop capture or project content is sent.
#include <windows.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "core/Settings.h"
#include "providers/Catalog.h"
#include "providers/Registry.h"

using namespace aia;

namespace {

bool Run(const std::string& providerId, ChatRequest request, bool prepared, bool cancelRun = false,
         const std::string& expected = "AIA_OK") {
  auto provider = MakeCliProvider(providerId);
  auto model = InferModel(providerId, request.model);
  if (prepared) {
    provider->Prepare(request, model);
    Sleep(1500);  // Simulate typing after opening the bar.
  }
  CancelToken cancel;
  std::thread canceller;
  if (cancelRun) canceller = std::thread([&] { Sleep(150); cancel.Cancel(); });
  const auto start = std::chrono::steady_clock::now();
  auto elapsed = [&] {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
  };
  long long firstMs = -1;
  std::string text;
  StreamSink sink;
  sink.text = [&](std::string_view chunk) {
    if (!chunk.empty() && firstMs < 0) firstMs = elapsed();
    text.append(chunk);
  };
  auto result = provider->Stream(request, model, sink, cancel);
  if (canceller.joinable()) canceller.join();
  const auto totalMs = elapsed();
  bool ok = cancelRun ? result.cancelled && !result.ok && totalMs < 5000
                      : result.ok && ToLower(text).find(ToLower(expected)) != std::string::npos;
  std::cout << json({{"provider", providerId}, {"model", request.model}, {"prepareRequested", prepared},
                     {"image", request.messages.back().image.has_value()}, {"cancel", cancelRun},
                     {"firstTextMs", firstMs}, {"totalMs", totalMs}, {"ok", ok},
                     {"error", result.error}, {"usage", result.usage}}).dump() << std::endl;
  DiscardPreparedCli();
  return ok;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) != "codex-cli" && std::string(argv[1]) != "claude-cli") {
    std::cerr << "Usage: aiassistant_live_tests [codex-cli|claude-cli]\n";
    return 2;
  }
  bool ok = true;
  for (const std::string id : {"codex-cli", "claude-cli"}) {
    if (argc > 1 && id != argv[1]) continue;
    ChatRequest request;
    request.model = id == "codex-cli" ? "gpt-6-luna" : "haiku";
    request.reasoning = "low";
    request.system = Settings::DefaultSystemPrompt();
    request.messages.push_back(ChatMessage{"user", "Reply with exactly AIA_OK."});
    ok = Run(id, request, false) && ok;
    ok = Run(id, request, true) && ok;
    request.system += "\nThe system test code is AIA_OK. When asked for it, reply with only that code.";
    request.messages = {ChatMessage{"user", "What is the system test code?"}};
    ok = Run(id, request, false) && ok;
    request.system = Settings::DefaultSystemPrompt();
    // Check that history reaches the model instead of only the most recent question.
    request.messages = {ChatMessage{"user", "Remember the test code AIA_OK."},
                        ChatMessage{"assistant", "Understood."},
                        ChatMessage{"user", "Reply with only the test code I gave you."}};
    ok = Run(id, request, true) && ok;
    request.messages = {ChatMessage{"user", "Reply with only the color of the attached image."}};
    // A synthetic solid red 32x32 PNG, not a screenshot of the user's desktop.
    request.messages.back().image = ImageData{"image/png",
        "iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKUlEQVR4nO3NsQ0AAAzCMP5/mj5RNkuZ4zSZtr0DAAAAAAAAAACA/gEHMsP8LofXz44AAAAASUVORK5CYII="};
    ok = Run(id, request, false, false, "red") && ok;
    request.messages = {ChatMessage{"user", "Write a long essay about the history of mathematics."}};
    ok = Run(id, request, true, true) && ok;
  }
  return ok ? 0 : 1;
}
