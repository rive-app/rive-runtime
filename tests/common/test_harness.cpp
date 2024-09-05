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
#include <signal.h>

#ifdef _WIN32
#include <io.h>
static int pipe(int pipefd[2]) { return _pipe(pipefd, 65536, 0); }
#endif

#ifdef RIVE_ANDROID
#include <android/log.h>
#endif

constexpr static uint32_t REQUEST_TYPE_IMAGE_UPLOAD = 0;
constexpr static uint32_t REQUEST_TYPE_CLAIM_GM_TEST = 1;
constexpr static uint32_t REQUEST_TYPE_CONSOLE_MESSAGE = 2;
constexpr static uint32_t REQUEST_TYPE_DISCONNECT = 3;
constexpr static uint32_t REQUEST_TYPE_APPLICATION_CRASH = 4;

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

static void sig_handler(int signo)
{
    signal(signo, SIG_DFL);
    TestHarness::Instance().onApplicationCrash(strsignal(signo));
    raise(signo);
}

static void check_early_exit()
{
    if (TestHarness::Instance().initialized())
    {
        // The tool should have called shutdown() before exit.
        TestHarness::Instance().onApplicationCrash("Early exit.");
    }
}

TestHarness& TestHarness::Instance()
{
    static TestHarness* instance = new TestHarness();
    return *instance;
}

TestHarness::TestHarness()
{
    // Forward signals to the test harness.
    for (int i = 1; i < NSIG; ++i)
    {
        signal(i, sig_handler);
    }

    // Check for if the app exits early (before calling TestHarness::shutdown()).
    atexit(check_early_exit);
}

void TestHarness::init(const char* output, const char* toolName, size_t pngThreadCount)
{
    assert(!m_initialized);
    m_initialized = true;

    // First check if "output" is a png server.
    m_primaryTCPClient = TCPClient::Connect(output);
    if (m_primaryTCPClient != nullptr)
    {
        m_outputDir = ".";

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
    }
    else
    {
        m_outputDir = output;
    }

    for (size_t i = 0; i < pngThreadCount; ++i)
    {
        m_encodeThreads.emplace_back(EncodePNGThread, this);
    }
}

void TestHarness::monitorStdIOThread()
{
    assert(m_initialized);
    assert(m_primaryTCPClient != nullptr);
    std::unique_ptr<TCPClient> threadTCPClient = m_primaryTCPClient->clone();

    char buff[1024];
    size_t readSize;
    while ((readSize = read(m_stdioPipe[0], buff, std::size(buff) - 1)) > 0)
    {
        buff[readSize] = '\0';
        threadTCPClient->send4(REQUEST_TYPE_CONSOLE_MESSAGE);
        threadTCPClient->sendString(buff);
#ifdef RIVE_ANDROID
        __android_log_write(ANDROID_LOG_DEBUG, "rive_android_tests", buff);
#endif
    }

    threadTCPClient->send4(REQUEST_TYPE_DISCONNECT);
    threadTCPClient->send4(false /* Don't shutdown the server */);
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
        auto destination = m_outputDir;
        destination /= args.name + ".png";
        destination.make_preferred();

        if (threadTCPClient == nullptr)
        {
            // We aren't connect to a test harness. Just save a file.
            WritePNGFile(args.pixels.data(),
                         args.width,
                         args.height,
                         true,
                         destination.generic_string().c_str(),
                         m_pngCompression);
            continue;
        }

        threadTCPClient->send4(REQUEST_TYPE_IMAGE_UPLOAD);
        threadTCPClient->sendString(destination.generic_string());

        auto png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png)
        {
            fprintf(stderr, "WritePNGFile: png_create_write_struct failed\n");
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
            fprintf(stderr, "WritePNGFile: png_create_info_struct failed\n");
            abort();
        }

        if (setjmp(png_jmpbuf(png)))
        {
            fprintf(stderr, "WritePNGFile: Error during init_io\n");
            abort();
        }

        png_set_write_fn(png, threadTCPClient.get(), &send_png_data_chunk, &flush_png_data);

        // Write header.
        if (setjmp(png_jmpbuf(png)))
        {
            fprintf(stderr, "WritePNGFile: Error during writing header\n");
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
            fprintf(stderr, "WritePNGFile: Error during writing bytes\n");
            abort();
        }

        std::vector<uint8_t*> rows(args.height);
        for (uint32_t y = 0; y < args.height; ++y)
        {
            rows[y] = args.pixels.data() + (args.height - 1 - y) * args.width * 4;
        }
        png_write_image(png, rows.data());

        // End write.
        if (setjmp(png_jmpbuf(png)))
        {
            fprintf(stderr, "WritePNGFile: Error during end of write");
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
        threadTCPClient->send4(false /* Don't shutdown the server */);
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

    if (m_primaryTCPClient != nullptr)
    {
        m_primaryTCPClient->send4(REQUEST_TYPE_DISCONNECT);
        m_primaryTCPClient->send4(true /* Shutdown the server */);
    }

    m_initialized = false;
}

void TestHarness::shutdownStdioThread()
{
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
}

void TestHarness::onApplicationCrash(const char* message)
{
    if (m_primaryTCPClient != nullptr)
    {
        // Buy monitorStdIOThread() some time to finish pumping any messages
        // related to this abort.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        shutdownStdioThread();
        m_primaryTCPClient->send4(REQUEST_TYPE_APPLICATION_CRASH);
        m_primaryTCPClient->sendString(message);
    }
}
