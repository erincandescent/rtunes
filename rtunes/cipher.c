/* $Id: cipher.c,v 1.9 2009/06/27 10:10:32 mbalmer Exp $ */

/*
 * Copyright (c) 2006 Marcus Glocker <marcus@nazgul.ch>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"
#include "proto.h"

/*
 * cipher_ aes_sendsample()
 *	AES encrypt audio sample
 * Return:
 *	<total bytes of encrypted sample> = success, -1 = error
 */ 
size_t
cipher_aes_sendsample(unsigned char *buf, size_t len)
{
	size_t			len_total, len_encrypt, i;
	unsigned short		j;
	unsigned char		aesiv_copy[16];

	/* sample header */
	static const unsigned char header[16] = {
	    0x24, 0x00, 0x00, 0x00, 0xF0, 0xFF, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	
	/* make copy of original IV */
	memcpy(aesiv_copy, aesiv, 16L);

	/* padding */
	i = len % AES_BLOCK_SIZE;
	if (i) {
		i = AES_BLOCK_SIZE - i;
		len_encrypt = len + i;
		for (i = len; i < len_encrypt; i++)
			buf[i] = '\0';
	} else
		len_encrypt = len;

	/* prepend sample header */
	memmove(buf + 16, buf, len_encrypt);
	memcpy(buf, header, 16L);
	/* write sample size into header */
	j = len_encrypt + 12;
	j = j >> 8;
	buf[2] = j;
	j = len_encrypt + 12;
	j = j << 8;
	j = j >> 8;
	buf[3] = j;

	/* sum header length and data length to total  */
	len_total = 16 + len_encrypt;

	/* encrypt */
	AES_cbc_encrypt(buf + 16, buf + 16, len_encrypt, &aeskey, aesiv_copy,
	    AES_ENCRYPT);

	return (len_total);
}

/*
 * rsa_encrypt_aeskey()
 *	RSA encrypt the AES session key with the AirPorts public RSA key
 * Return:
 *	<pointer to key> = success, NULL = error
 */
unsigned char *
cipher_rsa_encrypt_aeskey(unsigned char *aeskey)
{
	RSA		*key;
	unsigned char	*to = NULL;
	unsigned char	 n_bin[256], e_bin[3];

	/* RSA public key modulus */
	static const unsigned char *n = (unsigned char *)
	    "59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUtwC"
	    "5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6DZHJ2YCCLlDR"
	    "KSKv6kDqnw4UwPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuB"
	    "OitnZ/bDzPHrTOZz0Dew0uowxf/+sG+NCK3eQJVxqcaJ/vEHKIVd2M+5qL71yJ"
	    "Q+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/UAaHqn9JdsBWLUEpVviYnh"
	    "imNVvYFZeCXg/IdTQ+x4IRdiXNv5hEew==";
	/* RSA public key exponent */
	static const unsigned char *e = (unsigned char *)"AQAB";
	/* random string */
	static const char *rnd_seed = "the randomness maybe with you";

	/* seed random */
	RAND_seed(rnd_seed, strlen(rnd_seed));

	/* convert base64 encoded RSA public key back to binary */
	if (cipher_base64_decode(n_bin, n, sizeof(n_bin),
	    strlen((char *)n)) < 1)
		return (NULL);
	if (cipher_base64_decode(e_bin, e, sizeof(e_bin),
	    strlen((char *)e)) < 1)
		return (NULL);

	/* initialize RSA public key */
	key = RSA_new();
	RSA_set0_key(key,
		BN_bin2bn(n_bin, 256, NULL),
		BN_bin2bn(e_bin, 3,   NULL),
		NULL);
	
	/* RSA encrypt */
	to = malloc(256);
	if (RSA_public_encrypt(16, aeskey, to, key, RSA_PKCS1_OAEP_PADDING) < 1)
		return (NULL);

	return (to);
}

/*
 * aes_generate_key()
 *	generates a 128-bit key used for AES encryption
 * Return:
 *	<pointer to key> = success, NULL = error
 * Note:
 *	return pointer needs to be freed if not needed anymore 
 */ 
unsigned char *
cipher_aes_generate_key(void)
{
	unsigned char	*p = NULL;

	p = malloc(16);
	RAND_bytes(p, 16);
	
	return (p);
}

/*
 * base64_encode()
 *	encode plain data to base64
 * Return:
 *	<plain bytes encoded> = success, -1 = error
 */
int
cipher_base64_encode(unsigned char *encode, const unsigned char *plain,
    int len_e, int len_p)
{
	int		i, j;
	int		offset_e = 0, offset_p = 0;
	unsigned char	in[3], out[4];

	static const unsigned char table[64] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	for (;;) {
		in[0] = in[1] = in[2] = 0;

		/* read 3 bytes plain data */
		for (i = 0; i < 3; i++) {
			if (offset_p == len_p)
				break;
			in[i] = plain[offset_p];
			offset_p++;
		}

		/* encode: split the 24-bit block to four 6-bit blocks */
		out[0] = table[(in[0] >> 2)];
		out[1] = table[((in[0] & 3) << 4) | (in[1] >> 4)];
		out[2] = i < 2 ? '=' : table[((in[1] & 15) << 2) |
		    (in[2] >> 6)];
		out[3] = i < 3 ? '=' : table[(in[2] & 63)];

		/* write 4 bytes encrypted data to buffer */
		for (j = 0; j < 4; j++) {
			if (offset_e == len_e)
				return (-1); /* write buffer full */
			encode[offset_e] = out[j];
			offset_e++;
		}

		/* done */
		if (i != 3)
			break;
	}

	return (offset_p);
}

/*
 * base64_decode()
 *	deocde base64 data to plain
 * Return:
 *	<plain bytes decoded> = success, -1 = error
 */
int
cipher_base64_decode(unsigned char *plain, const unsigned char *encode,
    int len_p, int len_d)
{
	int		i, j, ch;
	int		offset_d, offset_p;
	char		out[3];
	unsigned char	in_a[4], in_b[4];

	static const unsigned char table[256] = {
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
	 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
	255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
	255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};

	offset_d = offset_p = 0;

	for (;;) {
		if (encode[offset_d] == '\0')
			break;

		/* read 4 bytes encrypted data */
		for (i = 0; i < 4; i++) {
			ch = encode[offset_d];
			offset_d++;

			/* ignore if char is not base64 */
			if (table[ch] == 255) {
				i--;
				continue;
			}

			/* save it */
			in_a[i] = ch;
			in_b[i] = table[ch];
		}

		/* decode: split the 24-bit block to three 8-bit blocks */
		out[0] = (in_b[0] << 2) | (in_b[1] >> 4);
		out[1] = (in_b[1] << 4) | (in_b[2] >> 2);
		out[2] = (in_b[2] << 6) |  in_b[3];

		/* padding */
		i = in_a[2] == '=' ? 1 : (in_a[3] == '=' ? 2 : 3);

		/* write 3 bytes plain data to buffer */
		for (j = 0; j < i; j++) {
			if (offset_p == len_p)
				return (-1); /* write buffer full */
			plain[offset_p] = out[j];
			offset_p++;
		}
	}

	return (offset_p);
}

/*
 * cipher_base64_rp()
 *	removes padding
 * Return:
 *	 0 = success
 */
int
cipher_base64_rp(char *buf, int len)
{
	int	i;

	for (i = 0; i < len; i++)
		if (buf[i] == '=')
			buf[i] = '\0';

	return (0);
}
