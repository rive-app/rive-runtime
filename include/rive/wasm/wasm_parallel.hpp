#ifndef _RIVE_WASM_PARALLEL_HPP_
#define _RIVE_WASM_PARALLEL_HPP_

#if defined(WITH_RIVE_SCRIPTING_WASM) && !defined(__EMSCRIPTEN__)

// Hosts that carry rive_parallel_v1.run; elsewhere it stays unlinked.
#define RIVE_WASM_PARALLEL

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rive
{
/// A VM's parallel calls. Members are extra instances of its module that
/// share its memory, kept for the VM's life, and each call runs its parts on
/// them and on the caller's thread through a process wide team of threads.
class WasmParallel
{
public:
  struct Outcome
  {
    enum class Kind
    {
      ok,
      // A part trapped; message is the trap's text.
      trapped,
      // A part threw; message holds the bytes the module handed over.
      threw,
      // A part ran out of fuel.
      budget,
      // The caller's watchdog fired while it waited for the parts.
      interrupted,
      // The caller's own part failed first, which left its exception set.
      callerFailed,
    };
    Kind kind = Kind::ok;
    std::string message;
    /// The threads that ran parts, the caller's included.
    uint32_t threads = 1;
  };

  explicit WasmParallel(void* instance);
  ~WasmParallel();

  /// The threads a call may run its parts on, the caller's included.
  static uint32_t size();

  /// How many cores the OS calls fast, or UINT32_MAX where it does not say.
  /// Left to the default pool, a call runs on no more.
  static uint32_t performanceCores();

  /// Runs the call whose control block lies at control on the instance's
  /// exec env, its parts one after another there when oneThread. A call
  /// that asks for every part at once gets one part per thread it runs on.
  Outcome run(void* execEnv, uint32_t control, bool oneThread);

  /// Hands a trap raised without a VM, and the thrown message it carries,
  /// to the call the member on this thread runs.
  static void noteMemberTrap(
      const char* text,
      const char* message,
      uint32_t length);

  /// Traps the member part on this thread that asked for a parallel call.
  static void refuseMemberCall();

  /// Whether this thread runs a call's parts, when the memory may not grow.
  static bool runningParts();

#if defined(WITH_RIVE_TOOLS) || defined(TESTING)
  /// Tests give every VM at most this many members, as if making more
  /// failed.
  static void capMembers(uint32_t most);
  /// Tests fail making each VM's member at index first or later, and count
  /// the failures.
  static void failMembersFrom(uint32_t first);
  static uint32_t memberFailures();
#endif

private:
  struct Member;
  struct Call;
  uint32_t members(void* execEnv, uint32_t wanted, uint32_t size);
  bool callPart(void* execEnv, void* function, const Call& call, uint32_t part);

  void* m_instance;
  void* m_function = nullptr;
  // A stopped call left its members' interrupts set.
  bool m_interruptedMembers = false;
  std::vector<std::unique_ptr<Member>> m_members;
  // How many members there were when making one failed, kept until the
  // memory's pages or size() change so each call does not fail again.
  uint32_t m_memberLimit = UINT32_MAX;
  uint32_t m_limitPages = 0;
  uint32_t m_limitSize = 0;
};
} // namespace rive

#endif
#endif
