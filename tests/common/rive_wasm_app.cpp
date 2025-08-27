/*
 * Copyright 2025 Rive
 */

#if defined(__EMSCRIPTEN__)

#include "common/rive_wasm_app.hpp"

#include "common/test_harness.hpp"
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <sstream>
#include <vector>

EM_JS(char*, get_window_location, (), {
    var jsString = window.location.href;
    var lengthBytes = lengthBytesUTF8(jsString) + 1;
    var stringOnWasmHeap = _malloc(lengthBytes);
    stringToUTF8(jsString, stringOnWasmHeap, lengthBytes);
    return stringOnWasmHeap;
});

EM_JS(char*, get_window_location_hash, (), {
    var jsString = window.location.hash.substring(1);
    var lengthBytes = lengthBytesUTF8(jsString) + 1;
    var stringOnWasmHeap = _malloc(lengthBytes);
    stringToUTF8(jsString, stringOnWasmHeap, lengthBytes);
    return stringOnWasmHeap;
});

static void split_string(const std::string& str,
                         const std::string& delimiter,
                         std::vector<std::string>& result)
{
    size_t start = 0;
    size_t end;

    while ((end = str.find(delimiter, start)) != std::string::npos)
    {
        if (end > start)
        {
            result.push_back(str.substr(start, end - start));
        }
        start = end + delimiter.length();
    }

    if (str.length() > start)
    {
        // Add the last piece
        result.push_back(str.substr(start));
    }
}

int main()
{
    char* location = get_window_location();
    char* hash = get_window_location_hash();

    // Command line arguments are passed via the window location's hash string.
    //
    //   e.g., "http://localhost/my_app.html#--backend%20gl%20--another%20arg"
    //
    std::vector<std::string> hashStrs = {location};
    split_string(hash, "%20", hashStrs);

    free(hash);
    free(location);

    std::vector<const char*> hashArgs;
    for (const std::string& str : hashStrs)
    {
        hashArgs.push_back(str.c_str());
    }

    return rive_wasm_main(hashArgs.size(), hashArgs.data());
}

extern "C" void rive_print_message_on_server(const char* msg)
{
    // stdout and stderr get redirected here and forwarded to the server for
    // logging.
    TestHarness::Instance().printMessageOnServer(msg);
}

#endif
