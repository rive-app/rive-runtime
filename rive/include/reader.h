#include <stdlib.h>
#include <string.h>

/* Decode an unsigned int LEB128 at buf into r, returning the nr of bytes read.
 */
inline size_t decode_uint_leb(const uint8_t *buf, const uint8_t *buf_end, uint64_t *r)
{
	const uint8_t *p = buf;
	uint8_t shift = 0;
	uint64_t result = 0;
	uint8_t byte;

	while (1)
	{
		if (p >= buf_end)
			return 0;

		byte = *p++;
		result |= ((uint64_t)(byte & 0x7f)) << shift;
		if ((byte & 0x80) == 0)
			break;
		shift += 7;
	}
	*r = result;
	return p - buf;
}

/* Decode a signed int LEB128 at buf into r, returning the nr of bytes read.
 */
inline size_t decode_int_leb(const uint8_t *buf, const uint8_t *buf_end, int64_t *r)
{
	const uint8_t *p = buf;
	uint8_t shift = 0;
	int64_t result = 0;
	uint8_t byte;

	do
	{
		if (p >= buf_end)
			return 0;

		byte = *p++;
		result |= ((uint64_t)(byte & 0x7f)) << shift;
		shift += 7;
	} while ((byte & 0x80) != 0);
	if (shift < (sizeof(*r) * 8) && (byte & 0x40) != 0)
		result |= -(((uint64_t)1) << shift);

	*r = result;
	return p - buf;
}

/* Decodes a string
 */
inline size_t decode_string(uint8_t str_len, const uint8_t *buf, const uint8_t *buf_end, char *char_buf)
{
	if (buf_end - buf < str_len)
		return 0;
	const uint8_t *p = buf;
	for (int i = 0; i < str_len; i++)
	{
		char_buf[i] = *p++;
	}
	// Add the null terminator
	char_buf[str_len] = '\0';
	return str_len;
}

/* Decodes a double (8 bytes)
 */
inline size_t decode_double(const uint8_t *buf, const uint8_t *buf_end, double *r)
{
	if (buf_end - buf < sizeof(double))
		return 0;
	memcpy(r, buf, sizeof(double));
	return sizeof(double);
}

/* Decodes a float (4 bytes)
 */
inline size_t decode_float(const uint8_t *buf, const uint8_t *buf_end, float *r)
{
	if (buf_end - buf < sizeof(float))
		return 0;
	memcpy(r, buf, sizeof(float));
	return sizeof(float);
}