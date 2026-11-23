/*
 * Copyright 2026 Rive
 */

// Runs the platform transport tests on Android, from adb shell. The transport
// needs a Java VM, which this process starts itself, and ART takes SIGSEGV
// for its own checks: so Catch leaves signals alone here, and these tests
// stay out of unit_tests, whose other tests count on their handlers.
#define CATCH_CONFIG_NO_POSIX_SIGNALS
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include "rive/scriptnet/net.hpp"

#include <cstdio>
#include <dlfcn.h>
#include <jni.h>

int main(int argc, char** argv)
{
  // As dalvikvm does: load ART behind libnativehelper, then ask it for a VM.
  // Looked up by name, since only API 31 and later offer these.
  void* helper = dlopen("libnativehelper.so", RTLD_NOW);
  auto create = reinterpret_cast<void* (*)()>(
      helper == nullptr ? nullptr : dlsym(helper, "JniInvocationCreate"));
  auto init = reinterpret_cast<bool (*)(void*, const char*)>(
      helper == nullptr ? nullptr : dlsym(helper, "JniInvocationInit"));
  auto createVm = reinterpret_cast<jint (*)(JavaVM**, JNIEnv**, void*)>(
      helper == nullptr ? nullptr : dlsym(helper, "JNI_CreateJavaVM"));
  if (create == nullptr || init == nullptr || createVm == nullptr)
  {
    std::printf("skipped: this Android cannot start a Java VM from a native "
                "process, which takes API 31\n");
    return 0;
  }
  JavaVMInitArgs args;
  args.version = JNI_VERSION_1_6;
  args.nOptions = 0;
  args.options = nullptr;
  args.ignoreUnrecognized = JNI_FALSE;
  JavaVM* vm = nullptr;
  JNIEnv* env = nullptr;
  if (!init(create(), nullptr) || createVm(&vm, &env, &args) != JNI_OK)
  {
    std::fprintf(stderr, "could not start a Java VM\n");
    return 1;
  }
  rive::scriptnet::setJavaVM(vm);
  return Catch::Session().run(argc, argv);
}
