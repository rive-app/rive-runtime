//
//  main.m
//  ios_tests
//
//  Created by Chris Dalton on 6/7/24.
//

#import <UIKit/UIKit.h>

#include "common/tcp_client.hpp"
#include <thread>

static int tool_argc;
static const char** tool_argv;

static void execute_tool()
{
    const char* pngServer = nullptr;
    for (int i = 1; i < tool_argc - 1; ++i)
    {
        if (strcmp(tool_argv[i], "--output") == 0 || strcmp(tool_argv[i], "-o") == 0)
        {
            pngServer = tool_argv[i + 1];
        }
    }

    // If there's a PNG server, wait until the app is granted local network permissions and can
    // connect.
    if (pngServer != nullptr)
    {
        auto tcpCheck = TCPClient::Connect(pngServer);
        while (tcpCheck == nullptr)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            tcpCheck = TCPClient::Connect(pngServer);
            printf("Ensure the device is connected to WiFi, is on the same local network as the "
                   "host, and app has local network permissions.\n");
        }
    }

    if (strcmp(tool_argv[0], "gms") == 0)
    {
        extern int gms_ios_main(int argc, const char* argv[]);
        exit(gms_ios_main(tool_argc, tool_argv));
    }

    if (strcmp(tool_argv[0], "goldens") == 0)
    {
        extern int goldens_ios_main(int argc, const char* argv[]);
        exit(goldens_ios_main(tool_argc, tool_argv));
    }
}

int main(int argc, char* argv[])
{
    if (argc <= 1)
    {
        printf("No arguments supplied.");
        return 0;
    }

    tool_argc = argc - 1;
    tool_argv = const_cast<const char**>(argv + 1);
    std::thread thread(execute_tool);

    return UIApplicationMain(argc, argv, nil, nil);
}
