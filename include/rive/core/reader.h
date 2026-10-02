#include <stdlib.h>
#include <string.h>

static bool is_big_endian(void)
{
    union
    {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1;
}

/* Decode an unsigned int LEB128 at buf into r, returning the nr of bytes read.
 * Returns zero (overflow) when the buffer ends first, the encoding is longer
 * than a 64 bit value can need (where the shift would be undefined), or its
 * last group carries bits past bit 63.
 */
inline size_t decode_uint_leb(const uint8_t* buf,
                              const uint8_t* buf_end,
                              uint64_t* r)
{
    const uint8_t* p = buf;
    uint8_t shift = 0;
    uint64_t result = 0;
    uint8_t byte;

    do
    {
        if (p >= buf_end || shift >= 64)
        {
            return 0;
        }
        byte = *p++;
        // The tenth group lands on bit 63 and has room for one bit.
        if (shift == 63 && (byte & 0x7e) != 0)
        {
            return 0;
        }
        result |= ((uint64_t)(byte & 0x7f)) << shift;
        shift += 7;
    } while ((byte & 0x80) != 0);
    *r = result;
    return p - buf;
}

/* Decode an unsigned int LEB128 at buf into r, returning the nr of bytes read.
 * Returns zero (overflow) when the buffer ends first, the encoding is longer
 * than a 32 bit value can need (where the shift would be undefined), or its
 * last group carries bits past bit 31.
 */
inline size_t decode_uint_leb32(const uint8_t* buf,
                                const uint8_t* buf_end,
                                uint32_t* r)
{
    const uint8_t* p = buf;
    uint8_t shift = 0;
    uint32_t result = 0;
    uint8_t byte;

    do
    {
        if (p >= buf_end || shift >= 32)
        {
            return 0;
        }
        byte = *p++;
        // The fifth group lands on bit 28 and has room for four bits.
        if (shift == 28 && (byte & 0x70) != 0)
        {
            return 0;
        }
        result |= ((uint32_t)(byte & 0x7f)) << shift;
        shift += 7;
    } while ((byte & 0x80) != 0);
    *r = result;
    return p - buf;
}

/* Decodes a string of str_len bytes into char_buf, which must hold
 * str_len + 1 bytes. Returns zero bytes read on buffer overflow.
 */
inline uint64_t decode_string(uint64_t str_len,
                              const uint8_t* buf,
                              const uint8_t* buf_end,
                              char* char_buf)
{
    // Compare lengths rather than forming buf + str_len, which a file
    // controlled length can push past the end of the address space.
    if (buf_end < buf || str_len > (uint64_t)(buf_end - buf))
    {
        return 0;
    }
    memcpy(char_buf, buf, (size_t)str_len);
    // Add the null terminator
    char_buf[str_len] = '\0';
    return str_len;
}

/* Decodes a double (8 bytes)
 */
inline size_t decode_double(const uint8_t* buf,
                            const uint8_t* buf_end,
                            double* r)
{
    // Return zero bytes read on buffer overflow
    if (buf_end - buf < sizeof(double))
    {
        return 0;
    }
    if (is_big_endian())
    {
        uint8_t inverted[8] =
            {buf[7], buf[6], buf[5], buf[4], buf[3], buf[2], buf[1], buf[0]};
        memcpy(r, inverted, sizeof(double));
    }
    else
    {
        memcpy(r, buf, sizeof(double));
    }
    return sizeof(double);
}

/* Decodes a float (4 bytes)
 */
inline size_t decode_float(const uint8_t* buf, const uint8_t* buf_end, float* r)
{
    // Return zero bytes read on buffer overflow
    if (buf_end - buf < (unsigned)sizeof(float))
    {
        return 0;
    }
    if (is_big_endian())
    {
        uint8_t inverted[4] = {buf[3], buf[2], buf[1], buf[0]};
        memcpy(r, inverted, sizeof(float));
    }
    else
    {
        memcpy(r, buf, sizeof(float));
    }
    return sizeof(float);
}

/* Decodes a single byte
 */
inline size_t decode_uint_8(const uint8_t* buf,
                            const uint8_t* buf_end,
                            uint8_t* r)
{
    // Return zero bytes read on buffer overflow
    if (buf_end - buf < (unsigned)sizeof(uint8_t))
    {
        return 0;
    }
    memcpy(r, buf, sizeof(uint8_t));
    return sizeof(uint8_t);
}

/* Decodes a 16 bit unsigned integer.
 */
inline size_t decode_uint_16(const uint8_t* buf,
                             const uint8_t* buf_end,
                             uint16_t* r)
{
    // Return zero bytes read on buffer overflow
    if (buf_end - buf < (unsigned)sizeof(uint16_t))
    {
        return 0;
    }
    if (is_big_endian())
    {
        uint8_t inverted[2] = {buf[1], buf[0]};
        memcpy(r, inverted, sizeof(uint16_t));
    }
    else
    {
        memcpy(r, buf, sizeof(uint16_t));
    }
    return sizeof(uint16_t);
}

/* Decodes a 32 bit unsigned integer.
 */
inline size_t decode_uint_32(const uint8_t* buf,
                             const uint8_t* buf_end,
                             uint32_t* r)
{
    // Return zero bytes read on buffer overflow
    if (buf_end - buf < (unsigned)sizeof(uint32_t))
    {
        return 0;
    }
    if (is_big_endian())
    {
        uint8_t inverted[4] = {buf[3], buf[2], buf[1], buf[0]};
        memcpy(r, inverted, sizeof(uint32_t));
    }
    else
    {
        memcpy(r, buf, sizeof(uint32_t));
    }
    return sizeof(uint32_t);
}