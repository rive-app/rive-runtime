/*
 * pointer_log_replay.hpp
 *
 * Loads a pointer-interaction log recorded by the Rive viewer (see
 * packages/viewer InteractionRecorder) and replays it against a
 * SerializingFactory to drive a "silver" test at runtime -- an alternative to
 * generating C++ ahead of time with tools/pointer_log_to_silver.js.
 *
 * The replay follows the SAME cadence + quantization rules as that generator,
 * so a JSON-driven test and a generated-code test produce the identical .sriv
 * stream (and therefore the same golden).
 *
 * Usage:
 *   SerializingFactory silver;
 *   auto file = ReadRiveFile("assets/foo.riv", &silver);
 *   auto artboard = file->artboardDefault();
 *   silver.frameSize(artboard->width(), artboard->height());
 *   auto sm = artboard->stateMachineAt(0);
 *   // ... bind view model, prime, initial draw (mirror the generator preamble)
 *   auto renderer = silver.makeRenderer();
 *   artboard->draw(renderer.get());
 *   replayPointerLog("assets/foo.json", silver, artboard.get(), sm.get(),
 *                    renderer.get(), {.stride = 1});
 *   CHECK(silver.matches("foo"));
 */

#ifndef _RIVE_POINTER_LOG_REPLAY_HPP_
#define _RIVE_POINTER_LOG_REPLAY_HPP_

#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/renderer.hpp"
#include "utils/serializing_factory.hpp"

#include "rive_testing.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace rive
{
namespace pointer_log
{

// ---------------------------------------------------------------------------
// Minimal JSON value + parser (sufficient for the recorder's schema: an object
// with a "meta" object and an "entries" array of flat objects whose values are
// strings, numbers, and booleans). Not a general-purpose JSON library.
//
// Entry ops:
//   {op:"pointer", type:"down|move|up|exit", x, y, id}
//   {op:"advance", dt}                    -- one frame
//   {op:"multiAdvance", count, dt}        -- `count` frames of `dt` each
// ---------------------------------------------------------------------------
struct Json
{
    enum class Type
    {
        null,
        boolean,
        number,
        string,
        array,
        object
    };

    Type type = Type::null;
    bool boolean = false;
    double number = 0;
    std::string str;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;

    const Json* find(const char* key) const
    {
        for (const auto& kv : object)
        {
            if (kv.first == key)
            {
                return &kv.second;
            }
        }
        return nullptr;
    }

    double numberOr(double fallback) const
    {
        return type == Type::number ? number : fallback;
    }
    bool boolOr(bool fallback) const
    {
        return type == Type::boolean ? boolean : fallback;
    }
    std::string stringOr(const char* fallback) const
    {
        return type == Type::string ? str : std::string(fallback);
    }
};

class Parser
{
public:
    Parser(const char* begin, const char* end) : m_p(begin), m_end(end) {}

    bool parse(Json& out)
    {
        skipWs();
        if (!parseValue(out))
        {
            return false;
        }
        skipWs();
        return m_ok;
    }

private:
    const char* m_p;
    const char* m_end;
    bool m_ok = true;

    bool atEnd() const { return m_p >= m_end; }
    char peek() const { return atEnd() ? '\0' : *m_p; }

    void skipWs()
    {
        while (!atEnd())
        {
            char c = *m_p;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                m_p++;
            }
            else
            {
                break;
            }
        }
    }

    bool expect(char c)
    {
        if (peek() != c)
        {
            m_ok = false;
            return false;
        }
        m_p++;
        return true;
    }

    bool parseValue(Json& out)
    {
        skipWs();
        char c = peek();
        switch (c)
        {
            case '{':
                return parseObject(out);
            case '[':
                return parseArray(out);
            case '"':
                return parseString(out);
            case 't':
            case 'f':
                return parseBool(out);
            case 'n':
                return parseNull(out);
            default:
                return parseNumber(out);
        }
    }

    bool parseObject(Json& out)
    {
        out.type = Json::Type::object;
        if (!expect('{'))
        {
            return false;
        }
        skipWs();
        if (peek() == '}')
        {
            m_p++;
            return true;
        }
        while (true)
        {
            skipWs();
            Json key;
            if (!parseString(key))
            {
                return false;
            }
            skipWs();
            if (!expect(':'))
            {
                return false;
            }
            Json value;
            if (!parseValue(value))
            {
                return false;
            }
            out.object.emplace_back(key.str, std::move(value));
            skipWs();
            if (peek() == ',')
            {
                m_p++;
                continue;
            }
            return expect('}');
        }
    }

    bool parseArray(Json& out)
    {
        out.type = Json::Type::array;
        if (!expect('['))
        {
            return false;
        }
        skipWs();
        if (peek() == ']')
        {
            m_p++;
            return true;
        }
        while (true)
        {
            Json value;
            if (!parseValue(value))
            {
                return false;
            }
            out.array.push_back(std::move(value));
            skipWs();
            if (peek() == ',')
            {
                m_p++;
                continue;
            }
            return expect(']');
        }
    }

    bool parseString(Json& out)
    {
        out.type = Json::Type::string;
        if (!expect('"'))
        {
            return false;
        }
        std::string s;
        while (!atEnd())
        {
            char c = *m_p++;
            if (c == '"')
            {
                out.str = std::move(s);
                return true;
            }
            if (c == '\\')
            {
                if (atEnd())
                {
                    break;
                }
                char e = *m_p++;
                switch (e)
                {
                    case '"':
                        s.push_back('"');
                        break;
                    case '\\':
                        s.push_back('\\');
                        break;
                    case '/':
                        s.push_back('/');
                        break;
                    case 'n':
                        s.push_back('\n');
                        break;
                    case 't':
                        s.push_back('\t');
                        break;
                    case 'r':
                        s.push_back('\r');
                        break;
                    case 'b':
                        s.push_back('\b');
                        break;
                    case 'f':
                        s.push_back('\f');
                        break;
                    case 'u':
                        // Skip the 4 hex digits; the recorder never emits
                        // these.
                        for (int i = 0; i < 4 && !atEnd(); i++)
                        {
                            m_p++;
                        }
                        s.push_back('?');
                        break;
                    default:
                        s.push_back(e);
                        break;
                }
            }
            else
            {
                s.push_back(c);
            }
        }
        m_ok = false;
        return false;
    }

    bool parseNumber(Json& out)
    {
        const char* start = m_p;
        if (peek() == '-')
        {
            m_p++;
        }
        bool any = false;
        while (!atEnd())
        {
            char c = *m_p;
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' ||
                c == '+' || c == '-')
            {
                any = true;
                m_p++;
            }
            else
            {
                break;
            }
        }
        if (!any)
        {
            m_ok = false;
            return false;
        }
        out.type = Json::Type::number;
        out.number = std::strtod(std::string(start, m_p).c_str(), nullptr);
        return true;
    }

    bool parseBool(Json& out)
    {
        if (matchLiteral("true"))
        {
            out.type = Json::Type::boolean;
            out.boolean = true;
            return true;
        }
        if (matchLiteral("false"))
        {
            out.type = Json::Type::boolean;
            out.boolean = false;
            return true;
        }
        m_ok = false;
        return false;
    }

    bool parseNull(Json& out)
    {
        if (matchLiteral("null"))
        {
            out.type = Json::Type::null;
            return true;
        }
        m_ok = false;
        return false;
    }

    bool matchLiteral(const char* literal)
    {
        const char* q = m_p;
        while (*literal)
        {
            if (q >= m_end || *q != *literal)
            {
                return false;
            }
            q++;
            literal++;
        }
        m_p = q;
        return true;
    }
};

} // namespace pointer_log

/// Cadence options; mirror the tools/pointer_log_to_silver.js flags. Every
/// advance is always applied at its (quantized) recorded dt so animation state
/// stays faithful; these only control how often a frame is recorded.
struct PointerLogReplayOptions
{
    /// Seconds between snapshots. When > 0 this takes precedence over `stride`.
    float interval = 0.0f;
    /// Convenience: when > 0, interval = 1 / fps.
    float fps = 0.0f;
    /// Count-based cadence: snapshot every Nth advance (used when interval <=
    /// 0).
    int stride = 1;
    /// dt bucket size for grouping near-equal advances. <= 0 disables grouping
    /// (each distinct exact dt is emitted as-is; only identical dts group).
    float quantize = 0.001f;
};

/// Replays the entries of a recorded pointer log against `silver`, driving
/// `sm` / `artboard` and recording frames into `renderer`.
static inline void replayPointerLog(const char* jsonPath,
                                    SerializingFactory& silver,
                                    Artboard* artboard,
                                    StateMachineInstance* sm,
                                    Renderer* renderer,
                                    const PointerLogReplayOptions& opts = {})
{
    // Read the whole file.
    std::ifstream stream(jsonPath, std::ios::binary);
    REQUIRE(stream.good());
    std::stringstream buffer;
    buffer << stream.rdbuf();
    std::string text = buffer.str();

    pointer_log::Json root;
    pointer_log::Parser parser(text.data(), text.data() + text.size());
    REQUIRE(parser.parse(root));

    const pointer_log::Json* entries = root.find("entries");
    REQUIRE(entries != nullptr);
    REQUIRE(entries->type == pointer_log::Json::Type::array);

    // Resolve cadence.
    float interval = opts.interval;
    if (opts.fps > 0.0f)
    {
        interval = 1.0f / opts.fps;
    }
    const bool useInterval = interval > 0.0f;
    const int stride = opts.stride < 1 ? 1 : opts.stride;
    const float quantize = opts.quantize;

    // Bucket a dt (> 0) into a grouping key and the float we emit for it.
    // `q` is guaranteed > 0 so `interval / q` can never divide by zero:
    //   - quantize > 0: round to the nearest bucket, clamped to >= 1 bucket so
    //     a tiny dt still yields q == quantize rather than 0.
    //   - quantize <= 0: grouping disabled -- use the exact dt (always > 0
    //     here); only advances with identical dt group together.
    auto bucketFor = [&](double dt) -> std::pair<double, float> {
        if (quantize > 0.0f)
        {
            long bucket = std::lround(dt / quantize);
            if (bucket < 1)
            {
                bucket = 1;
            }
            return {(double)bucket, (float)bucket * quantize};
        }
        return {dt, (float)dt};
    };

    // Open advance group state: a run of consecutive advances sharing
    // `groupKey` (and thus the emitted `groupQ`).
    double groupKey = 0;
    float groupQ = 0.0f;
    bool haveGroup = false;
    int groupCount = 0;

    auto flushGroup = [&]() {
        if (!haveGroup || groupCount == 0)
        {
            haveGroup = false;
            groupCount = 0;
            return;
        }
        const float q = groupQ; // always > 0
        const int step =
            useInterval ? std::max(1, (int)std::lround(interval / q)) : stride;
        const int periods = groupCount / step;
        const int remainder = groupCount - periods * step;

        for (int p = 0; p < periods; p++)
        {
            silver.addFrame();
            for (int s = 0; s < step; s++)
            {
                sm->advanceAndApply(q);
            }
            artboard->draw(renderer);
        }
        // Trailing advances that don't complete a snapshot period: apply time
        // without recording a frame.
        for (int r = 0; r < remainder; r++)
        {
            sm->advanceAndApply(q);
        }

        haveGroup = false;
        groupCount = 0;
    };

    // Feed one advance of `dt` into the grouping state (a `multiAdvance` entry
    // calls this `count` times).
    auto handleAdvance = [&](double dt) {
        if (dt <= 0)
        {
            // Zero-length advance (drain): apply without a frame.
            flushGroup();
            sm->advanceAndApply(0.0f);
            return;
        }
        const auto bq = bucketFor(dt);
        const double key = bq.first;
        const float q = bq.second;
        if (!haveGroup)
        {
            haveGroup = true;
            groupKey = key;
            groupQ = q;
            groupCount = 1;
        }
        else if (key == groupKey)
        {
            groupCount++;
        }
        else
        {
            flushGroup();
            haveGroup = true;
            groupKey = key;
            groupQ = q;
            groupCount = 1;
        }
    };

    for (const auto& e : entries->array)
    {
        const pointer_log::Json* opJson = e.find("op");
        if (opJson == nullptr)
        {
            continue;
        }
        const std::string op = opJson->stringOr("");

        if (op == "pointer")
        {
            flushGroup();
            const pointer_log::Json* typeJson = e.find("type");
            const std::string type =
                typeJson ? typeJson->stringOr("") : std::string();
            const float x = (float)(e.find("x") ? e.find("x")->numberOr(0) : 0);
            const float y = (float)(e.find("y") ? e.find("y")->numberOr(0) : 0);
            const int id = (int)(e.find("id") ? e.find("id")->numberOr(0) : 0);
            const Vec2D pos(x, y);
            if (type == "down")
            {
                sm->pointerDown(pos, id);
            }
            else if (type == "up")
            {
                sm->pointerUp(pos, id);
            }
            else if (type == "move")
            {
                sm->pointerMove(pos, 0, id);
            }
            else if (type == "exit")
            {
                sm->pointerExit(pos, id);
            }
        }
        else if (op == "advance")
        {
            handleAdvance(e.find("dt") ? e.find("dt")->numberOr(0) : 0);
        }
        else if (op == "multiAdvance")
        {
            // A run of identical-delta advances: {count, dt}.
            const double dt = e.find("dt") ? e.find("dt")->numberOr(0) : 0;
            const long count =
                (long)(e.find("count") ? e.find("count")->numberOr(0) : 0);
            for (long i = 0; i < count; i++)
            {
                handleAdvance(dt);
            }
        }
    }
    flushGroup();
}

} // namespace rive

#endif
