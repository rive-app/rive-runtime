#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/text/text.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <vector>

static std::vector<rive::Text*> textsOf(rive::Artboard* artboard)
{
  std::vector<rive::Text*> texts;
  for (auto object : artboard->objects())
  {
    if (object != nullptr && object->is<rive::Text>())
    {
      texts.push_back(object->as<rive::Text>());
    }
  }
  return texts;
}

TEST_CASE("A cloned text sizes its run list like its source", "[text]")
{
  size_t runs = 0;
  for (const char* path :
       {"assets/new_text.riv",
        "assets/runtime_nested_text_runs.riv",
        "assets/modifier_to_run.riv",
        "assets/text_fit_test.riv"})
  {
    auto file = ReadRiveFile(path);
    for (size_t i = 0; i < file->artboardCount(); i++)
    {
      auto source = file->artboard(i);
      auto instance = source->instance();
      auto sourceTexts = textsOf(source);
      auto instanceTexts = textsOf(instance.get());
      REQUIRE(sourceTexts.size() == instanceTexts.size());
      for (size_t t = 0; t < sourceTexts.size(); t++)
      {
        const auto& sourceRuns = sourceTexts[t]->runs();
        const auto& instanceRuns = instanceTexts[t]->runs();
        CHECK(instanceRuns.size() == sourceRuns.size());
        // Sized once, not grown a run at a time.
        CHECK(instanceRuns.capacity() == sourceRuns.size());
        runs += sourceRuns.size();
      }
    }
  }
  CHECK(runs > 0);
}

TEST_CASE("Styled text sizes exactly once, then grows like appending", "[text]")
{
  rive::StyledText styled;
  styled.reserve(10, 2);
  CHECK(styled.unichars().capacity() == 10);
  CHECK(styled.runs().capacity() == 2);
  // Enough room already: nothing changes.
  styled.reserve(8, 1);
  CHECK(styled.unichars().capacity() == 10);
  CHECK(styled.runs().capacity() == 2);
  // Growing past it at least doubles, so growing a little at a time doesn't
  // reallocate every time.
  styled.reserve(11, 3);
  CHECK(styled.unichars().capacity() == 20);
  CHECK(styled.runs().capacity() == 4);
  styled.reserve(50, 3);
  CHECK(styled.unichars().capacity() == 50);
}
