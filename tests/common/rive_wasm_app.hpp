/*
 * Copyright 2025 Rive
 */

#pragma once

// Primary entrypoint for rive wasm tests. Each individual test is expected
// to implement this method in wasm (instead of "main()").
extern int rive_wasm_main(int argc, const char* const* argv);
