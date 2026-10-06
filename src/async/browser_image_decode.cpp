#ifdef __EMSCRIPTEN__

#include "rive/async/browser_image_decode.hpp"

#include <emscripten.h>
#include <cstdlib>
#include <unordered_map>
#include <utility>

namespace
{
std::unordered_map<uint32_t, rive::BrowserImageDecoded> s_pendingDecodes;
uint32_t s_nextDecodeId = 1;

uint32_t nextDecodeId()
{
    uint32_t id = s_nextDecodeId++;
    // On wraparound, skip ids that are still in flight.
    while (id == 0 || s_pendingDecodes.count(id) != 0)
    {
        id = s_nextDecodeId++;
    }
    return id;
}

// Removes the entry first so done may start or cancel other decodes.
rive::BrowserImageDecoded takeDecode(uint32_t id)
{
    auto it = s_pendingDecodes.find(id);
    if (it == s_pendingDecodes.end())
    {
        return nullptr;
    }
    auto done = std::move(it->second);
    s_pendingDecodes.erase(it);
    return done;
}
} // namespace

// createImageBitmap is async; the callback calls back into C++ between frames.
// Internals go by their bare names, which the closure compiler keeps.
EM_JS(void,
      wasm_start_image_decode,
      (uint32_t requestId, const uint8_t* data, int dataLen),
      {
          // Copy from WASM heap (SharedArrayBuffer can't be used for Blob).
          var sourceView = new Uint8Array(wasmMemory.buffer, data, dataLen);
          var buffer = new Uint8Array(dataLen);
          buffer.set(sourceView);

          // A decoder that throws as it starts fails the decode, not librive.
          Promise.resolve()
              .then(
                  function() { return createImageBitmap(new Blob([buffer])); })
              .then(function(bmp) {
                  // Draw to OffscreenCanvas to extract raw RGBA pixels.
                  var canvas = new OffscreenCanvas(bmp.width, bmp.height);
                  var ctx2d = canvas.getContext("2d");
                  ctx2d.drawImage(bmp, 0, 0);
                  var imageData =
                      ctx2d.getImageData(0, 0, bmp.width, bmp.height);

                  // Allocate WASM memory and copy pixels.
                  var numBytes = imageData.data.length;
                  var ptr = _wasm_image_decode_alloc(numBytes);
                  if (!ptr)
                  {
                      _wasm_image_decode_error(requestId);
                      return;
                  }
                  // malloc may grow memory, so view the buffer after it.
                  new Uint8Array(wasmMemory.buffer, ptr, numBytes)
                      .set(imageData.data);

                  _wasm_image_decode_complete(requestId,
                                              bmp.width,
                                              bmp.height,
                                              ptr,
                                              numBytes);
              })
              .catch(function() { _wasm_image_decode_error(requestId); });
      });

extern "C"
{
    // Our own export, so the glue needs no EM_JS_DEPS on malloc, which Unity's
    // oldest emsdk predates.
    EMSCRIPTEN_KEEPALIVE
    uint8_t* wasm_image_decode_alloc(int numBytes)
    {
        return static_cast<uint8_t*>(malloc(numBytes));
    }

    EMSCRIPTEN_KEEPALIVE
    void wasm_image_decode_complete(uint32_t requestId,
                                    int width,
                                    int height,
                                    uint8_t* pixels,
                                    int numBytes)
    {
        auto done = takeDecode(requestId);
        if (done)
        {
            // getImageData returns straight RGBA, and both scripting lanes
            // deliver premultiplied RGBA8.
            for (int i = 0; i < numBytes; i += 4)
            {
                uint8_t a = pixels[i + 3];
                if (a < 255)
                {
                    pixels[i + 0] = (uint16_t(pixels[i + 0]) * a + 127) / 255;
                    pixels[i + 1] = (uint16_t(pixels[i + 1]) * a + 127) / 255;
                    pixels[i + 2] = (uint16_t(pixels[i + 2]) * a + 127) / 255;
                }
            }
            done(requestId,
                 width,
                 height,
                 rive::Span<const uint8_t>(pixels, numBytes),
                 nullptr);
        }
        free(pixels);
    }

    // The native decoders' message, so a script sees one on every platform.
    EMSCRIPTEN_KEEPALIVE
    void wasm_image_decode_error(uint32_t requestId)
    {
        auto done = takeDecode(requestId);
        if (done)
        {
            done(requestId,
                 0,
                 0,
                 rive::Span<const uint8_t>(),
                 "failed to decode image data");
        }
    }
}

namespace rive
{
uint32_t startBrowserImageDecode(const uint8_t* bytes,
                                 uint32_t byteCount,
                                 BrowserImageDecoded done)
{
    uint32_t id = nextDecodeId();
    s_pendingDecodes[id] = std::move(done);
    wasm_start_image_decode(id, bytes, (int)byteCount);
    return id;
}

void cancelBrowserImageDecode(uint32_t id) { s_pendingDecodes.erase(id); }
} // namespace rive

#endif
