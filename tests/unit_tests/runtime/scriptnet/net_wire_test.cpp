#ifdef WITH_RIVE_SCRIPTNET

#include "catch.hpp"
#include "rive/wasm/net_wire.hpp"

#include <vector>

using namespace rive;
using namespace rive::scriptnet;

namespace
{
std::vector<uint8_t> wireOf(const HttpRequest& request)
{
    std::vector<uint8_t> wire(NetWire::encodedSize(request));
    NetWire::encode(request, wire.data());
    return wire;
}

bool decodes(const std::vector<uint8_t>& wire, size_t length)
{
    HttpRequest request;
    return NetWire::decode(Span<const uint8_t>(wire.data(), length), request);
}

bool decodes(const std::vector<uint8_t>& wire)
{
    return decodes(wire, wire.size());
}

void putU32(std::vector<uint8_t>& wire, size_t at, uint32_t value)
{
    for (int i = 0; i < 4; i++)
    {
        wire[at + i] = (uint8_t)(value >> (8 * i));
    }
}

HttpRequest sampleRequest()
{
    HttpRequest request;
    request.timeoutMs = 1500;
    request.method = "POST";
    request.url = "https://example.com/data";
    request.headers.push_back({"X-Test", "1"});
    request.headers.push_back({"Accept", "*/*"});
    request.body = {'h', 'i'};
    return request;
}

// Where the header count sits: after the timeout, method and url runs.
size_t headerCountAt(const HttpRequest& request)
{
    return 4 + 4 + request.method.size() + 4 + request.url.size();
}
} // namespace

TEST_CASE("NetWire round trips a request", "[scriptnet]")
{
    HttpRequest sent = sampleRequest();
    std::vector<uint8_t> wire = wireOf(sent);
    HttpRequest request;
    REQUIRE(NetWire::decode(Span<const uint8_t>(wire.data(), wire.size()),
                            request));
    CHECK(request.timeoutMs == 1500);
    CHECK(request.method == "POST");
    CHECK(request.url == sent.url);
    REQUIRE(request.headers.size() == 2);
    CHECK(request.headers[1].name == "Accept");
    CHECK(request.headers[1].value == "*/*");
    CHECK(request.body == sent.body);
}

TEST_CASE("NetWire refuses every truncated request", "[scriptnet]")
{
    std::vector<uint8_t> wire = wireOf(sampleRequest());
    for (size_t length = 0; length < wire.size(); length++)
    {
        INFO("length " << length);
        CHECK_FALSE(decodes(wire, length));
    }
    wire.push_back(0);
    CHECK_FALSE(decodes(wire));
}

TEST_CASE("NetWire refuses hostile request lengths", "[scriptnet]")
{
    HttpRequest sent = sampleRequest();
    const std::vector<uint8_t> wire = wireOf(sent);
    size_t countAt = headerCountAt(sent);

    std::vector<uint8_t> method = wire;
    putU32(method, 4, 0xffffffff);
    CHECK_FALSE(decodes(method));

    std::vector<uint8_t> url = wire;
    putU32(url, 8 + sent.method.size(), (uint32_t)wire.size());
    CHECK_FALSE(decodes(url));

    // Far more headers than the stream could hold is refused before any
    // are reserved.
    std::vector<uint8_t> count = wire;
    putU32(count, countAt, 0x7fffffff);
    CHECK_FALSE(decodes(count));

    // One header more than sent passes that bound but runs out.
    std::vector<uint8_t> extra = wire;
    putU32(extra, countAt, 3);
    CHECK_FALSE(decodes(extra));

    std::vector<uint8_t> name = wire;
    putU32(name, countAt + 4, 0x80000000);
    CHECK_FALSE(decodes(name));

    std::vector<uint8_t> body = wire;
    putU32(body, wire.size() - 4 - sent.body.size(), 3);
    CHECK_FALSE(decodes(body));
}

#endif
