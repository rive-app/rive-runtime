/*
 * Copyright 2025 Rive
 */

#include "common/stacktrace.hpp"
#include <assert.h>

#if defined(NO_REDIRECT_OUTPUT) || defined(__EMSCRIPTEN__)

namespace stacktrace
{
void replace_signal_handlers(SignalFunc signalFunc,
                             ExitFunc atExitFunc) noexcept
{}
}; // namespace stacktrace

#else

#ifdef _WIN32
#include <Windows.h>
#include <dbghelp.h>
#else
#include <inttypes.h>
#ifdef RIVE_ANDROID
#include <unwind.h>
#include <android/log.h>
#endif
#include <execinfo.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cxxabi.h>
#endif

#ifdef SYS_SIGNAL_H
#include <sys/signal.h>
#else
#include <signal.h>
#endif

#include <stdio.h>
#include <sstream>

static SignalFunc signalFunc;
static int signalRecurseLevel = 0;

// this is outside the namespace so we can use it as if it was defined by the
// c++ runtime
#if defined(SYS_SIGNAL_H) || defined(_WIN32)
const char* strsignal(int signo)
{
    switch (signo)
    {
        case SIGINT:
            return "SIGINT";
        case SIGILL:
            return "SIGILL";
        case SIGFPE:
            return "SIGFPE";
        case SIGSEGV:
            return "SIGSEGV";
        case SIGTERM:
            return "SIGTERM";
        case SIGBREAK:
            return "SIGBREAK";
        case SIGABRT:
            return "SIGABRT";
    }
    return "Unknown Signal";
}
#endif

namespace stacktrace
{
#if defined(_WIN32)
LONG WINAPI top_level_exception_handler(PEXCEPTION_POINTERS pExceptionInfo)
{
    std::stringstream f;

    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);

    // StackWalk64() may modify context record passed to it, so we will
    // use a copy.
    CONTEXT context_record = *pExceptionInfo->ContextRecord;
    // Initialize stack walking.
    STACKFRAME64 stack_frame;
    memset(&stack_frame, 0, sizeof(stack_frame));
#if defined(_WIN64)
    int machine_type = IMAGE_FILE_MACHINE_AMD64;
    stack_frame.AddrPC.Offset = context_record.Rip;
    stack_frame.AddrFrame.Offset = context_record.Rbp;
    stack_frame.AddrStack.Offset = context_record.Rsp;
#else
    int machine_type = IMAGE_FILE_MACHINE_I386;
    stack_frame.AddrPC.Offset = context_record.Eip;
    stack_frame.AddrFrame.Offset = context_record.Ebp;
    stack_frame.AddrStack.Offset = context_record.Esp;
#endif
    stack_frame.AddrPC.Mode = AddrModeFlat;
    stack_frame.AddrFrame.Mode = AddrModeFlat;
    stack_frame.AddrStack.Mode = AddrModeFlat;

    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;

    while (StackWalk64(machine_type,
                       GetCurrentProcess(),
                       GetCurrentThread(),
                       &stack_frame,
                       &context_record,
                       NULL,
                       &SymFunctionTableAccess64,
                       &SymGetModuleBase64,
                       NULL))
    {

        DWORD64 displacement = 0;

        if (!SymFromAddr(process,
                         (DWORD64)stack_frame.AddrPC.Offset,
                         &displacement,
                         pSymbol))
        {
            continue;
        }

        IMAGEHLP_MODULE64 moduleInfo;
        ZeroMemory(&moduleInfo, sizeof(IMAGEHLP_MODULE64));
        moduleInfo.SizeOfStruct = sizeof(moduleInfo);

        if (::SymGetModuleInfo64(process, pSymbol->ModBase, &moduleInfo))
            f << moduleInfo.ModuleName << ": ";

        f << pSymbol->Name << " + 0x" << std::hex << displacement << std::endl;
    }

    signalFunc(f.str().c_str());

    // match to SymInitialize
    SymCleanup(process);

    return EXCEPTION_CONTINUE_SEARCH;
}

void build_stack(int signo)
{
    CONTEXT context;
    ZeroMemory(&context, sizeof(CONTEXT));
    RtlCaptureContext(&context);
    EXCEPTION_POINTERS exceptionPointers;
    ZeroMemory(&exceptionPointers, sizeof(EXCEPTION_POINTERS));
    exceptionPointers.ContextRecord = &context;
    top_level_exception_handler(&exceptionPointers);
}

static void handle_signal(int signo) noexcept
{
    // this is because we can accidently spam the signal handler if we can't
    // communicate with the server
    if (signo == SIGABRT)
    {
        if (++signalRecurseLevel > 1)
        {
            return;
        }
    }

    printf("Received signal %i (\"%s\")\n", signo, strsignal(signo));

    build_stack(signo);
    // set the signal handler to the original
    signal(signo, SIG_DFL);
    // re raise the signal
    raise(signo);
}

void replace_signal_handlers(SignalFunc inSignalFunc,
                             ExitFunc atExitFunc) noexcept
{
    assert(inSignalFunc);
    assert(atExitFunc);

    // signals can only use global vars
    signalFunc = inSignalFunc;

    SetUnhandledExceptionFilter(top_level_exception_handler);

    for (int i = 1; i <= SIGTERM; ++i)
    {
        signal(i, handle_signal);
    }
    // for windows SIGABRT is a higher number then SIGTERM, so make sure to
    // intercept it as well
    signal(SIGABRT, handle_signal);

    atexit(atExitFunc);
}
#else
static void get_symbol_line(void* instructionPointer,
                            uint32_t i,
                            char* buffer,
                            std::size_t bufferSize)
{
    Dl_info dlInfo;
    if (dladdr(instructionPointer, &dlInfo) && dlInfo.dli_sname != nullptr)
    {
        char* demangled = nullptr;
        int status;
        demangled = abi::__cxa_demangle(dlInfo.dli_sname, nullptr, 0, &status);
        snprintf(buffer,
                 bufferSize,
                 "%u %s\n",
                 i,
                 (status == 0 && demangled != nullptr) ? demangled
                                                       : dlInfo.dli_sname);
        free(demangled);
    }
    else
    {
        snprintf(buffer,
                 bufferSize,
                 "%u <unknown symbol @%p>)\n",
                 i,
                 instructionPointer);
    }

    // snprintf does not null terminate if we hit the limit so add a null
    // terminator here to be safe.
    buffer[bufferSize - 1] = '\0';
}

template <std::size_t N>
static void get_symbol_line(void* instructionPointer,
                            uint32_t i,
                            char (&buffer)[N])
{
    get_symbol_line(instructionPointer, i, buffer, N);
}

#ifdef RIVE_ANDROID
#define LOG(...)                                                               \
    __android_log_print(ANDROID_LOG_ERROR, "rive_android_tests", __VA_ARGS__)
static void handle_signal(int sigNum, siginfo_t* signalInfo, void* userContext)
{
    LOG("Received signal %i (\"%s\")\n", sigNum, strsignal(sigNum));

    LOG("Stack trace:\n");
    struct State
    {
        uint32_t i = 0;
        std::stringstream f;
    };

    State st{};
    _Unwind_Backtrace(
        [](struct _Unwind_Context* context, void* stateVoid) {
            auto state = static_cast<State*>(stateVoid);
            void* ip = reinterpret_cast<void*>(_Unwind_GetIP(context));
            if (ip != nullptr)
            {
                char buffer[1024];
                get_symbol_line(ip, state->i, buffer);
                LOG("%s", buffer);
                state->f << buffer;
            }
            state->i++;
            return _URC_NO_REASON;
        },
        &st);

    signal(sigNum, SIG_DFL);
    signalFunc(st.f.str().c_str());
    raise(sigNum);
}

#else // we are unix

static void handle_signal(int sigNum, siginfo_t* signalInfo, void* userContext)
{
    printf("Received signal %i (\"%s\")\n", sigNum, strsignal(sigNum));
    // this is because we can accidently spam the signal handler if we can't
    // communicate with the server
    if (sigNum == SIGABRT)
    {
        if (++signalRecurseLevel > 1)
        {
            return;
        }
    }

    std::stringstream f;

    void* stacktrace[1024];
    char** symbols;
    char stringBuff[1024];
    // this is technically unsafe since it uses malloc behind the scene but it's
    // way simpler then the alternative and this is just for testing anyway so
    // it's fine
    int numFrames = backtrace(stacktrace, 1024);
    symbols = backtrace_symbols(stacktrace, numFrames);
    for (int i = 1; i < numFrames; i++)
    {
        get_symbol_line(stacktrace[i], i, stringBuff, std::size(stringBuff));
        f << stringBuff << symbols[i] << "\n";
    }

    free(symbols);

    // send stack to server
    signalFunc(f.str().c_str());

    exit(sigNum);
}
#endif
void replace_signal_handlers(SignalFunc inSignalFunc,
                             ExitFunc atExitFunc) noexcept
{
    assert(inSignalFunc);
    assert(atExitFunc);

    signalFunc = inSignalFunc;

    struct sigaction action;
    struct sigaction oldAction;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
#if defined(__LP64__) && defined(__APPLE__)
    action.sa_flags |= SA_64REGSET;
#endif
    action.sa_sigaction = &handle_signal;

    for (int i = 1; i <= SIGTERM; ++i)
    {
        sigaction(i, &action, &oldAction);
    }

    atexit(atExitFunc);
}

#endif // defined _WIN32

}; // namespace stacktrace
#endif // defined(NO_REDIRECT_OUTPUT) || defined(__EMSCRIPTEN__)
