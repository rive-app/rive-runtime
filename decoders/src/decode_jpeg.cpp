// Adapted from libjpeg-turbo's example:
// https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/example.c
#include "rive/decoders/bitmap_decoder.hpp"

#include "jpeglib.h"
#include "jerror.h"

#include <setjmp.h>
#include <algorithm>
#include <cassert>
#include <string.h>

struct my_error_mgr
{
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

typedef struct my_error_mgr* my_error_ptr;

void my_error_exit(j_common_ptr cinfo)
{
    // cinfo.err really points to a my_error_mgr struct, so coerce pointer
    my_error_ptr myerr = (my_error_ptr)cinfo->err;

    // Always display the message.
    // We could postpone this until after returning, if we chose.
    (*cinfo->err->output_message)(cinfo);

    // Return control to the setjmp point
    longjmp(myerr->setjmp_buffer, 1);
}

std::unique_ptr<Bitmap> DecodeJpeg(const uint8_t bytes[], size_t byteCount)
{
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr jerr;

    JSAMPARRAY buffer = nullptr;
    std::unique_ptr<const uint8_t[]> pixelBuffer;
    int row_stride;

    // Step 1: allocate and initialize JPEG decompression object.

    // We set up the normal JPEG error routines, then override error_exit.
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    // Establish the setjmp return context for my_error_exit to use.
    if (setjmp(jerr.setjmp_buffer))
    {
        // If we get here, the JPEG code has signaled an error.
        // We need to clean up the JPEG object, close the input file, and return.
        jpeg_destroy_decompress(&cinfo);
        return nullptr;
    }

    // Now we can initialize the JPEG decompression object.
    jpeg_create_decompress(&cinfo);

    // Step 2: specify data source
    jpeg_mem_src(&cinfo, bytes, byteCount);

    // Step 3: read file parameters with jpeg_read_header()

    jpeg_read_header(&cinfo, TRUE);

    // Step 4: set parameters for decompression

    // always want 8 bit RGB
    cinfo.data_precision = 8;
    cinfo.out_color_space = JCS_RGB;

    // Step 5: Start decompressor
    jpeg_start_decompress(&cinfo);

    /// Api worked as expected and gave us correct format even for jpeg 12 or 16
    assert(cinfo.data_precision == 8);
    assert(cinfo.output_components == 3);

    size_t pixelBufferSize = static_cast<size_t>(cinfo.output_width) *
                             static_cast<size_t>(cinfo.output_height) *
                             static_cast<size_t>(cinfo.output_components);
    pixelBuffer = std::make_unique<uint8_t[]>(pixelBufferSize);

    uint8_t* pixelWriteBuffer = (uint8_t*)pixelBuffer.get();
    const uint8_t* pixelWriteBufferEnd = pixelWriteBuffer + pixelBufferSize;

    // Samples per row in output buffer
    row_stride = cinfo.output_width * cinfo.output_components;
    // Make a one-row-high sample array that will go away when done with image
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    // Step 6: while (scan lines remain to be read)
    //           jpeg_read_scanlines(...);

    // Here we use the library's state variable cinfo->output_scanline as the
    // loop counter, so that we don't have to keep track ourselves.
    //
    while (cinfo.output_scanline < cinfo.output_height)
    {
        // jpeg_read_scanlines expects an array of pointers to scanlines.
        // Here the array is only one element long, but you could ask for
        // more than one scanline at a time if that's more convenient.
        jpeg_read_scanlines(&cinfo, buffer, 1);

        if (pixelWriteBuffer + row_stride > pixelWriteBufferEnd)
        {
            // memcpy would cause an overflow.
            jpeg_finish_decompress(&cinfo);
            jpeg_destroy_decompress(&cinfo);
            return nullptr;
        }
        memcpy(pixelWriteBuffer, buffer[0], row_stride);
        pixelWriteBuffer += row_stride;
    }

    // Step 7: Finish decompression
    jpeg_finish_decompress(&cinfo);

    // Step 8: Release JPEG decompression object
    jpeg_destroy_decompress(&cinfo);

    return std::make_unique<Bitmap>(cinfo.output_width,
                                    cinfo.output_height,
                                    Bitmap::PixelFormat::RGB,
                                    std::move(pixelBuffer));
}