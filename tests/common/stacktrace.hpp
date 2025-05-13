typedef void (*SignalFunc)(const char*);
typedef void (*ExitFunc)(void);

namespace stacktrace
{
void replace_signal_handlers(SignalFunc signalFunc,
                             ExitFunc atExitFunc) noexcept;
} // namespace stacktrace
