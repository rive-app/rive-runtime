/*
 * Copyright 2024 Rive
 */

#include "common/test_harness.hpp"

#include "rive/rive_types.hpp"
#include "rive/math/math_types.hpp"
#include "tcp_client.hpp"
#include "png.h"
#include "zlib.h"
#include <vector>

#ifdef SYS_SIGNAL_H
#include <sys/signal.h>
const char* strsignal(int)
{
    return "(strsignal) not suported on this platform";
}
#else
#include <signal.h>
#endif

#if defined(_WIN32) && !defined(NO_REDIRECT_OUTPUT)
#include <io.h>
static int pipe(int pipefd[2]) { return _pipe(pipefd, 65536, 0); }
#endif

#ifdef RIVE_ANDROID
#include <android/log.h>
#endif

constexpr static uint32_t REQUEST_TYPE_IMAGE_UPLOAD = 0;
constexpr static uint32_t REQUEST_TYPE_CLAIM_GM_TEST = 1;
constexpr static uint32_t REQUEST_TYPE_FETCH_RIV_FILE = 2;
constexpr static uint32_t REQUEST_TYPE_GET_INPUT = 3;
constexpr static uint32_t REQUEST_TYPE_CANCEL_INPUT = 4;
#ifndef NO_REDIRECT_OUTPUT
constexpr static uint32_t REQUEST_TYPE_PRINT_MESSAGE = 5;
#endif
constexpr static uint32_t REQUEST_TYPE_DISCONNECT = 6;
constexpr static uint32_t REQUEST_TYPE_APPLICATION_CRASH = 7;

#ifdef _WIN32
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
#ifndef NO_SIGNAL_FORWARD
static void sig_handler(int signo)
{
    printf("Received signal %i (\"%s\")\n", signo, strsignal(signo));
    signal(signo, SIG_DFL);
    TestHarness::Instance().onApplicationCrash(strsignal(signo));
    abort();
}

static void check_early_exit()
{
    if (TestHarness::Instance().initialized())
    {
        // The tool should have called shutdown() before exit.
        TestHarness::Instance().onApplicationCrash("Early exit.");
    }
}
#endif

TestHarness& TestHarness::Instance()
{
    static TestHarness* instance = new TestHarness();
    return *instance;
}

TestHarness::TestHarness()
{
#ifndef NO_SIGNAL_FORWARD
    // Forward signals to the test harness.
    for (int i = 1; i <= SIGTERM; ++i)
    {
        signal(i, sig_handler);
    }

    // Check for if the app exits early (before calling
    // TestHarness::shutdown()).
    atexit(check_early_exit);
#endif
}

void TestHarness::init(std::unique_ptr<TCPClient> tcpClient,
                       size_t pngThreadCount)
{
    assert(!m_initialized);
    m_initialized = true;

    m_primaryTCPClient = std::move(tcpClient);

    initStdioThread();

    for (size_t i = 0; i < pngThreadCount; ++i)
    {
        m_encodeThreads.emplace_back(EncodePNGThread, this);
    }
}

void TestHarness::init(std::filesystem::path outputDir, size_t pngThreadCount)
{
    assert(!m_initialized);
    m_initialized = true;
    m_outputDir = outputDir;
    std::filesystem::create_directories(m_outputDir);

#ifdef RIVE_ANDROID
    // Still pipe stdout and sterr to the android logs.
    initStdioThread();
#endif

    for (size_t i = 0; i < pngThreadCount; ++i)
    {
        m_encodeThreads.emplace_back(EncodePNGThread, this);
    }
}

void TestHarness::initStdioThread()
{
#ifndef NO_REDIRECT_OUTPUT

#ifndef _WIN32
    // Make stdout & stderr line buffered. (This is not supported on Windows.)
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
#endif
    // Pipe stdout and sterr back to the server.
    m_savedStdout = dup(1);
    m_savedStderr = dup(2);
    pipe(m_stdioPipe.data());
    dup2(m_stdioPipe[1], 1);
    dup2(m_stdioPipe[1], 2);
    m_stdioThread = std::thread(MonitorStdIOThread, this);
#endif
}

void TestHarness::monitorStdIOThread()
{
#ifndef NO_REDIRECT_OUTPUT
    assert(m_initialized);

    std::unique_ptr<TCPClient> threadTCPClient;
    if (m_primaryTCPClient != nullptr)
    {
        threadTCPClient = m_primaryTCPClient->clone();
    }

    char buff[1024];
    size_t readSize;
    while ((readSize = read(m_stdioPipe[0], buff, std::size(buff) - 1)) > 0)
    {
        buff[readSize] = '\0';
        if (threadTCPClient != nullptr)
        {
            threadTCPClient->send4(REQUEST_TYPE_PRINT_MESSAGE);
            threadTCPClient->sendString(buff);
        }
        else if (FILE* f = nullptr/*
                     fopen((m_outputDir / "rive_log.txt").string().c_str(),
                           "a")*/)
        {
            // Sometimes it can help to also save a log file (e.g., when
            // ANDROID_LOG_DEBUG gets lost on browserstack).
            // DANGER: Writing this file may also cause failures, so turn the
            // if() back on with caution.
            fwrite(buff, 1, strlen(buff), f);
            fclose(f);
        }
#ifdef RIVE_ANDROID
        __android_log_write(ANDROID_LOG_DEBUG, "rive_android_tests", buff);
#endif
    }

    if (threadTCPClient != nullptr)
    {
        threadTCPClient->send4(REQUEST_TYPE_DISCONNECT);
        threadTCPClient->send4(false /* Don't shutdown the server yet */);
    }
#endif
}

void send_png_data_chunk(png_structp png, png_bytep data, png_size_t length)
{
    auto tcpClient = reinterpret_cast<TCPClient*>(png_get_io_ptr(png));
    tcpClient->send4(rive::math::lossless_numeric_cast<uint32_t>(length));
    tcpClient->sendall(data, length);
}

void flush_png_data(png_structp png) {}

void TestHarness::encodePNGThread()
{
    assert(m_initialized);

    std::unique_ptr<TCPClient> threadTCPClient;
    if (m_primaryTCPClient != nullptr)
    {
        threadTCPClient = m_primaryTCPClient->clone();
    }

    for (;;)
    {
        ImageSaveArgs args;
        m_encodeQueue.pop(args);
        if (args.quit)
        {
            break;
        }
        assert(args.width > 0);
        assert(args.height > 0);
        std::string pngName = args.name + ".png";

        if (threadTCPClient == nullptr)
        {
            // We aren't connect to a test harness. Just save a file.
            auto destination = m_outputDir;
            destination /= pngName;
            destination.make_preferred();
            WritePNGFile(args.pixels.data(),
                         args.width,
                         args.height,
                         true,
                         destination.generic_string().c_str(),
                         m_pngCompression);
            continue;
        }

        threadTCPClient->send4(REQUEST_TYPE_IMAGE_UPLOAD);
        threadTCPClient->sendString(pngName);

        auto png =
            png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png)
        {
            fprintf(stderr, "TestHarness: png_create_write_struct failed\n");
            abort();
        }

        // RLE with SUB gets best performance with our content.
        png_set_compression_level(png, 6);
        png_set_compression_strategy(png, Z_RLE);
        png_set_compression_strategy(png, Z_RLE);
        png_set_filter(png, 0, PNG_FILTER_SUB);

        auto info = png_create_info_struct(png);
        if (!info)
        {
            fprintf(stderr, "TestHarness: png_create_info_struct failed\n");
            abort();
        }

        if (setjmp(png_jmpbuf(png)))
        {
            fprintf(stderr, "TestHarness: Error during init_io\n");
            abort();
        }

        png_set_write_fn(png,
                         threadTCPClient.get(),
                         &send_png_data_chunk,
                         &flush_png_data);

        // Write header.
        if (setjmp(png_jmpbuf(png)))
        {
            fprintf(stderr, "TestHarness: Error during writing header\n");
            abort();
        }

        png_set_IHDR(png,
                     info,
                     args.width,
                     args.height,
                     8,
                     PNG_COLOR_TYPE_RGB_ALPHA,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE,
                     PNG_FILTER_TYPE_BASE);

        png_write_info(png, info);

        // Write bytes.
        if (setjmp(png_jmpbuf(png)))
        {
            fprintf(stderr, "TestHarness: Error during writing bytes\n");
            abort();
        }

        std::vector<uint8_t*> rows(args.height);
        for (uint32_t y = 0; y < args.height; ++y)
        {
            rows[y] =
                args.pixels.data() + (args.height - 1 - y) * args.width * 4;
        }
        png_write_image(png, rows.data());

        // End write.
        if (setjmp(png_jmpbuf(png)))
        {
            fprintf(stderr, "TestHarness: Error during end of write");
            abort();
        }

        png_write_end(png, NULL);
        png_destroy_write_struct(&png, &info);

        threadTCPClient->sendHandshake();
        threadTCPClient->recvHandshake();
    }

    if (threadTCPClient != nullptr)
    {
        threadTCPClient->send4(REQUEST_TYPE_DISCONNECT);
        threadTCPClient->send4(false /* Don't shutdown the server yet */);
    }
}

bool TestHarness::claimGMTest(const std::string& name)
{
    if (m_primaryTCPClient != nullptr)
    {
        m_primaryTCPClient->send4(REQUEST_TYPE_CLAIM_GM_TEST);
        m_primaryTCPClient->sendString(name);
        return m_primaryTCPClient->recv4();
    }
    return true;
}

bool TestHarness::fetchRivFile(std::string& name, std::vector<uint8_t>& bytes)
{
    if (m_primaryTCPClient == nullptr)
    {
        return false;
    }
    m_primaryTCPClient->send4(REQUEST_TYPE_FETCH_RIV_FILE);
    uint32_t nameLength = m_primaryTCPClient->recv4();
    if (nameLength == TCPClient::SHUTDOWN_TOKEN)
    {
        return false;
    }
    name.resize(nameLength);
    m_primaryTCPClient->recvall(name.data(), nameLength);
    uint32_t fileSize = m_primaryTCPClient->recv4();
    bytes.resize(fileSize);
    m_primaryTCPClient->recvall(bytes.data(), fileSize);
    return true;
}

void TestHarness::inputPumpThread()
{
    assert(m_initialized);

    if (m_primaryTCPClient == nullptr)
    {
        return;
    }

    std::unique_ptr<TCPClient> threadTCPClient = m_primaryTCPClient->clone();

    for (std::vector<char> keys; m_initialized;)
    {
        threadTCPClient->send4(REQUEST_TYPE_GET_INPUT);
        uint32_t len = threadTCPClient->recv4();
        if (len == TCPClient::SHUTDOWN_TOKEN)
        {
            return;
        }
        keys.resize(len);
        threadTCPClient->recvall(keys.data(), len);
        for (char key : keys)
        {
            m_inputQueue.push(char(key));
        }
    }

    threadTCPClient->send4(REQUEST_TYPE_DISCONNECT);
    threadTCPClient->send4(false /* Don't shutdown the server yet */);
}

bool TestHarness::peekChar(char& key)
{
    if (m_primaryTCPClient == nullptr)
    {
        return false;
    }
    if (!m_inputPumpThread.joinable())
    {
        m_inputPumpThread = std::thread(InputPumpThread, this);
    }
    return m_inputQueue.try_pop(key);
}

void TestHarness::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    for (std::thread& thread RIVE_MAYBE_UNUSED : m_encodeThreads)
    {
        m_encodeQueue.push({.quit = true});
    }
    for (std::thread& thread : m_encodeThreads)
    {
        thread.join();
    }
    m_encodeThreads.clear();

    shutdownStdioThread();
    shutdownInputPumpThread();

    if (m_primaryTCPClient != nullptr)
    {
        m_primaryTCPClient->send4(REQUEST_TYPE_DISCONNECT);
        m_primaryTCPClient->send4(true /* Shutdown the server */);
        m_primaryTCPClient = nullptr;
    }

    m_initialized = false;
}

void TestHarness::shutdownStdioThread()
{
#ifndef NO_REDIRECT_OUTPUT
    if (m_savedStdout != 0 || m_savedStderr != 0)
    {
        // Restore stdout and stderr.
        dup2(m_savedStdout, 1);
        dup2(m_savedStderr, 2);
        close(m_savedStdout);
        close(m_savedStderr);
        m_savedStdout = m_savedStderr = 0;

        // Shutdown the stdio-monitoring thread and pipe.
        close(m_stdioPipe[1]);
        m_stdioThread.join();
        close(m_stdioPipe[0]);
        m_stdioPipe = {0, 0};
    }
#endif
}

void TestHarness::shutdownInputPumpThread()
{
    if (m_inputPumpThread.joinable())
    {
        m_primaryTCPClient->send4(REQUEST_TYPE_CANCEL_INPUT);
        m_inputPumpThread.join();
    }
}

void TestHarness::onApplicationCrash(const char* message)
{
    if (m_primaryTCPClient != nullptr)
    {
        // Buy monitorStdIOThread() some time to finish pumping any messages
        // related to this abort.

        // std::this_thread::sleep_for causes weird link issues in unreal. just
        // use sleep instead
#if defined(RIVE_UNREAL) && defined(_WIN32)
        Sleep(100);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
        shutdownStdioThread();
        shutdownInputPumpThread();
        m_primaryTCPClient->send4(REQUEST_TYPE_APPLICATION_CRASH);
        m_primaryTCPClient->sendString(message);
    }
}
