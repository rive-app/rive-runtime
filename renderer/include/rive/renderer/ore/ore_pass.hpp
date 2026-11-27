/*
 * Copyright 2026 Rive
 */

#pragma once

namespace rive::ore
{

class Context;

// What a context's open pass ledger needs of a render or compute pass.
class Pass
{
public:
  virtual void finish() = 0;
  bool isFinished() const { return m_finished; }

  virtual ~Pass() = default;

  Pass(const Pass&) = delete;
  Pass& operator=(const Pass&) = delete;

protected:
  friend class Context;

  Pass() = default;
  explicit Pass(Context* context) : m_context(context) {}

  bool m_finished = false;

  // Where errors are reported. Weak ref, null once the context died.
  Context* m_context = nullptr;
};

} // namespace rive::ore
