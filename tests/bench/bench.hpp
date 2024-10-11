/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/rive_types.hpp"
#include <functional>
#include <map>
#include <string>

// Base class of a microbenchmark.
class Bench
{
public:
    // Called once before run().
    virtual void setup() {}

    // Runs the benchmark and returns any int, ideally a value that discourages
    // the compiler from optimizing the benchmark away.
    virtual int run() const = 0;

    virtual ~Bench() {}

    using BenchMap = std::map<std::string, std::function<Bench*()>>;
    static BenchMap& Registry()
    {
        static BenchMap s_registry;
        return s_registry;
    }
};

#define REGISTER_BENCH(CLASS_NAME)                                             \
    extern bool g_register##CLASS_NAME;                                        \
    bool g_register##CLASS_NAME = [] {                                         \
        Bench::Registry()[#CLASS_NAME] = [] { return new CLASS_NAME; };        \
        return true;                                                           \
    }();
