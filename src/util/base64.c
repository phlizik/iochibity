//******************************************************************
//
// Copyright 2015 Samsung Electronics All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "base64.h"

#include <stdio.h>

#if EXPORT_INTERFACE
#include <stdint.h>
#include <stdlib.h>
#endif	/* INTERFACE */

/**
 * @file
 * An implementation of Base 64 encoding. As defined in RFC4648 section 4.
 *
 * This enables encoding/decoding binary data into a 65-character subset
 * of US-ASCII (64 characters plus a padding character).
 *
 * @note Only Base 64 encoding is implemented. Other encoding types
 * defined in RFC4648 are not implemented. Base 64 encoding with URL
 * and filename safe alphabet, Bass 32, and Base 16 Encoding are not
 * implemented.
 */

#if INTERFACE
/**
 * Macro to calculate the size of 'output' buffer required for
 * a 'input' buffer of length x during Base64 encoding operation.
 */
#define B64ENCODE_OUT_SAFESIZE(x) ((((x) + 3 - 1)/3) * 4 + 1)

/**
 * Macro to calculate the size of 'output' buffer required for
 * a 'input' buffer of length x during Base64 decoding operation.
 */
#define B64DECODE_OUT_SAFESIZE(x) (((x)*3)/4)

/**
 * Result code of base64 functions.
 */
typedef enum
{
    /// The encode, decode operation was successful
    B64_OK = 0,
    /// Invalid input parameter.
    B64_INVALID_PARAM,
    /// The output buffer is not large enough for the encoded or decoded data.
    B64_OUTPUT_BUFFER_TOO_SMALL,
    /// Indicates error encoding or decoding.
    B64_ERROR
} B64Result;
#endif	/* INTERFACE */

/** base character of Base64. */
static const char g_b64TransTbl[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"\
                "ghijklmnopqrstuvwxyz0123456789+/";

/**
 * base64 block encode function.
 *
 * @param in is the octet stream, max 3 byte.
 * @param out is the Base64 encoded stream, 4 byte.
 * @param len is the  byte-length of octet stream.
 *
 * @return ::B64_OK for Success, otherwise some error value.
 */
static B64Result b64EncodeBlk(const uint8_t* in, char* out, size_t len)
{
    if (NULL == in || NULL ==  out || 0 == len )
    {
        return B64_INVALID_PARAM;
    }

    out[0] = g_b64TransTbl[in[0] >> 2];

    if (1 == len)
    {
        out[1] = g_b64TransTbl[((in[0] & 0x03) << 4)];
    }
    else
    {
        out[1] = g_b64TransTbl[((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4)];
    }

    if (2 == len)
    {
        out[2] = g_b64TransTbl[((in[1] & 0x0f) << 2)];
    }
    else if (1 < len)
    {
        out[2] = g_b64TransTbl[((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6)];
    }
    else
    {
        out[2] = '=';
    }

    if (2 < len)
    {
        out[3] = g_b64TransTbl[in[2] & 0x3f];
    }
    else
    {
        out[3] = '=';
    }

    return B64_OK;
}

/**
 * Encode the plain message in base64.
 *
 * Example usage
 *
 *     const uint8_t* input = { 0x00, 0x11, 0x22, 0x33 };
 *     size_t b64Size = 0;
 *     size_t b64BufSize = B64ENCODE_OUT_SAFESIZE(sizeof(input) / sizeof(uint8_t*));
 *     char* b64Buf = (char*)OICCalloc(1, b64BufSize);
 *     if(B64_OK != b64Encode(input, sizeof(input) / sizeof(uint8_t*),
 *                            b64Buf, b64BufSize, &b64Size))
 *     {
 *         printf("Failure Base 64 encoding input\n");
 *     }
 *
 * @param[in]  in is the plain message to be converted.
 * @param[in]  inLen is the byte length of plain message.
 * @param[out] outBuf is the output buffer containing Base64 encoded message.
 * @param[in]  outBufSize is the size of output buffer.
 * @param[out] outLen is the byte length of encoded message.
 * @note `outBuf` adds a NULL to the string configuration.
 *
 * @return ::B64_OK for Success, otherwise some error value.
 *
 * @deprecated This implementation of base64 encode has been deprecated in favor
 * of the base64 implementation that is available as part of the mbedtls library.
 * @see mbedtls_base64_encode()
 *
 */
B64Result b64Encode(const uint8_t* in, const size_t inLen,
               char* outBuf, const size_t outBufSize, size_t* outLen)
{
    if (NULL == in || NULL == outBuf || NULL == outLen )
    {
        return B64_INVALID_PARAM;
    }

    *outLen = ((inLen / 3) * 3 == inLen) ?
              ((inLen / 3) * 4) :
              (((inLen / 3) + 1) * 4);
    size_t minBufSize = (*outLen + 1);
    if (outBufSize < minBufSize)
    {
        return B64_OUTPUT_BUFFER_TOO_SMALL;
    }

    size_t i;
    for (i = 0; i < inLen / 3; i++)
    {
        if(B64_OK != b64EncodeBlk(in + i * 3, outBuf + i * 4, 3))
        {
            return B64_INVALID_PARAM;
        }
    }

    if ((i * 3) != inLen)
    {
        if (B64_OK != b64EncodeBlk(in + i * 3, outBuf + i * 4, inLen - i * 3))
        {
            return B64_INVALID_PARAM;
        }
    }

    outBuf[*outLen] = '\0';

    return B64_OK;
}

/**
 * Get decoded value.
 *
 * @param c is the Base64 encoded character.
 *
 * @return decoded value, 6-bit.
 */
static uint32_t b64GetVal(char c)
{
    if (('A' <= c) && ('Z' >= c))
    {
        return c - 'A';
    }
    else if (('a' <= c) && ('z' >= c))
    {
        return c - 'a' + 26;
    }
    else if (('0' <= c) && ('9' >= c))
    {
        return c - '0' + 52;
    }
    else if ('+' == c)
    {
        return 62;
    }
    else if ('/' == c)
    {
        return 63;
    }
    else if ('=' == c)
    {
        return 0;
    }

    return 0;
}

/**
 * Check if the input string is a valid Base64 encoded string.
 * The string must have groups of 4 characters and only contain
 * the US-ASCII characters `[A-Z][a-z][0-9]+/` and (pad) `=`
 *
 * @param[in] in is the Base64 encoded message to validate.
 * @param[in] inLen is the byte length of the encoded message.
 *
 * @return
 *    - B64_OK    if it is valid Base64 encoding string.
 *    - B64_ERROR if string is not a valid Base64 encoded string.
 */
static B64Result isValidB64EncodedString(const char* in, const size_t inLen)
{
    // All valid Base64 encoded strings will be multiples of 4
    if (0 != (inLen % 4))
    {
        return B64_ERROR;
    }
    for (size_t i = 0; i < inLen; ++i)
    {
        // "[A-Z][a-z][0-9]+/=" are the only valid
        // letters in a Base64 encoded string
        if ((('A' > in[i]) || ('Z' < in[i])) &&
            (('a' > in[i]) || ('z' < in[i])) &&
            (('0' > in[i]) || ('9' < in[i])) &&
            ('+' != in[i]) &&
            ('/' != in[i]) &&
            ('=' != in[i]))
        {
            return B64_ERROR;
        }
        // Padding character "=" can only show up as last 2 characters
        if ('=' == in[i])
        {
            if (i < inLen - 2)
            {
                return B64_ERROR;
            }
            if (i == inLen - 2 && '=' != in[i+1])
            {
                return B64_ERROR;
            }
        }
    }
    return B64_OK;
}

/**
 * Base64 block decode function.
 *
 * @param in is the Base64 encoded stream, 4 bytes.
 * @param out is the Octet stream, 3 bytes.
 *
 * @return ::B64_OK for Success, otherwise some error value.
 */
static B64Result b64DecodeBlk(const char* in, uint8_t* out)
{
    if (NULL == in || NULL == out)
    {
        return B64_INVALID_PARAM;
    }

    uint32_t val = (b64GetVal(in[0]) << 18) | (b64GetVal(in[1]) << 12) |
          (b64GetVal(in[2]) << 6) | (b64GetVal(in[3]));

    out[0] = (val >> 16) & 0xff;

    if ('=' != in[2])
    {
        out[1] = (val >> 8) & 0xff;
    }
    if ('=' != in[3])
    {
        out[2] = val & 0xff;
    }

    return B64_OK;
}

/**
 * Decode the encoded message in base64.
 *
 * Example usage
 *
 *     const char* b64EncodedData = "Zm9vYmFy";
 *     size_t outLen;
 *     size_t outBufLen = B64DECODE_OUT_SAFESIZE(strlen(b64EncodedData));
 *     uint8_t* outBuf = (uint8_t*)OICCalloc(1, outBufSize);
 *     if (B64_OK != b64Decode(b64EncodedData, strlen(b64EncodedData), outBuf, outBufLen, &outLen));
 *     {
 *          printf("Failure decoding Base 64 encoded data.\n");
 *     }
 *
 * @param[in]  in is the Base64 encoded message to be converted.
 * @param[in]  inLen is the byte length of the encoded message.
 * @param[out] outBuf is the output buffer containing decoded message.
 * @param[in]  outBufSize is the size of output buffer.
 * @param[out] outLen is the byte length of decoded message.
 *
 * @return ::B64_OK for Success, otherwise some error value.
 *
 * @deprecated This implementation of base64 decode has been deprecated in favor
 * of the base64 implementation that is available as part of the mbedtls library.
 * @see mbedtls_base64_decode()
 */
B64Result b64Decode(const char* in, const size_t inLen,
               uint8_t* outBuf, size_t outBufSize, size_t* outLen)
{
    if (NULL == in || NULL == outBuf || NULL == outLen)
    {
        return B64_INVALID_PARAM;
    }

    if (B64_OK != isValidB64EncodedString(in, inLen))
    {
        return B64_INVALID_PARAM;
    }

    *outLen = (inLen / 4) * 3;
    size_t minBufSize = (inLen / 4) * 3;
    if (inLen != 0)
    {
        if ('=' == in[inLen - 1])
        {
            minBufSize--;
            (*outLen)--;
        }
        if ('=' == in[inLen - 2])
        {
            minBufSize--;
            (*outLen)--;
        }
    }
    if (outBufSize < minBufSize)
    {
        return B64_OUTPUT_BUFFER_TOO_SMALL;
    }

    for (size_t i = 0; i < inLen / 4; i++)
    {
        if (B64_OK != b64DecodeBlk(in + i * 4, outBuf + i * 3))
        {
            return B64_INVALID_PARAM;
        }
    }

    return B64_OK;
}
