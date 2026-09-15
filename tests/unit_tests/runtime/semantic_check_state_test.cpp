/*
 * Copyright 2026 Rive
 */

// Check state is the one semantic state that is a *field* rather than a flag:
// a checkbox is tri-state, so isChecked occupies two bits of stateFlags at
// isCheckedBitOffset instead of a bit of its own.
//
// These cover the two halves that a per-bit model used to get for free:
// decoding a value out of the packed mask, and writing one in without
// disturbing the single-bit states packed either side of it.

#include <rive/generated/semantic/semantic_data_base.hpp>
#include <rive/semantic/semantic_data.hpp>
#include <rive/semantic/semantic_state.hpp>
#include <catch.hpp>

using namespace rive;

namespace
{
// Places a raw field value into an otherwise empty stateFlags mask.
constexpr uint32_t encode(uint32_t value)
{
    return value << SemanticDataBase::isCheckedBitOffset;
}
} // namespace

TEST_CASE("checkStateOf decodes each authored value", "[semantics][check]")
{
    CHECK(checkStateOf(encode(0)) == SemanticCheckState::Unchecked);
    CHECK(checkStateOf(encode(1)) == SemanticCheckState::Checked);
    CHECK(checkStateOf(encode(2)) == SemanticCheckState::Mixed);
}

TEST_CASE("checkStateOf reads the unassigned fourth value as mixed",
          "[semantics][check]")
{
    // The field is two bits, so 3 is representable but has no meaning. It
    // reads as mixed rather than falling through to an unnamed state, which
    // is also what both historical Checked/Mixed bits set used to mean.
    CHECK(checkStateOf(encode(3)) == SemanticCheckState::Mixed);
}

TEST_CASE("checkStateOf ignores the states packed around the field",
          "[semantics][check]")
{
    // Selected is the bit directly below the field and Toggled the bit
    // directly above; neither may leak into the decoded value.
    const uint32_t neighbours = static_cast<uint32_t>(SemanticState::Selected) |
                                static_cast<uint32_t>(SemanticState::Toggled) |
                                static_cast<uint32_t>(SemanticState::Hidden);

    CHECK(checkStateOf(neighbours) == SemanticCheckState::Unchecked);
    CHECK(checkStateOf(neighbours | encode(2)) == SemanticCheckState::Mixed);
}

TEST_CASE("isChecked writes the field without disturbing its neighbours",
          "[semantics][check]")
{
    SemanticData sd;
    sd.stateFlags(static_cast<uint32_t>(SemanticState::Selected) |
                  static_cast<uint32_t>(SemanticState::Toggled));

    sd.isChecked(2);
    CHECK(sd.isChecked() == 2);
    CHECK(checkStateOf(sd.stateFlags()) == SemanticCheckState::Mixed);
    CHECK(hasSemanticState(sd.stateFlags(), SemanticState::Selected));
    CHECK(hasSemanticState(sd.stateFlags(), SemanticState::Toggled));

    // Writing a narrower value clears the high bit of the field rather than
    // OR-ing into it, which a flag-shaped setter would have got wrong.
    sd.isChecked(1);
    CHECK(sd.isChecked() == 1);
    CHECK(checkStateOf(sd.stateFlags()) == SemanticCheckState::Checked);

    sd.isChecked(0);
    CHECK(sd.isChecked() == 0);
    CHECK(checkStateOf(sd.stateFlags()) == SemanticCheckState::Unchecked);
    CHECK(hasSemanticState(sd.stateFlags(), SemanticState::Selected));
    CHECK(hasSemanticState(sd.stateFlags(), SemanticState::Toggled));
}

TEST_CASE("the check field occupies the bits the two flags used to",
          "[semantics][check]")
{
    // The encoding is deliberately unchanged from the era of independent
    // Checked (bit 2) and Mixed (bit 3) flags, so a reader of stateFlags sees
    // the same bytes.
    static_assert(SemanticDataBase::isCheckedBitOffset == 2);
    static_assert(SemanticDataBase::isCheckedFieldMask == 0xCu);
    CHECK(encode(1) == 1u << 2);
    CHECK(encode(2) == 1u << 3);
}
