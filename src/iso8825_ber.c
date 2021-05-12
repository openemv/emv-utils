/**
 * @file iso8825_ber.c
 * @brief Basic Encoding Rules (BER) implementation
 *        (see ISO/IEC 8825-1 or ITU-T Rec X.690 07/2002)
 *
 * Copyright (c) 2021 Leon Lynch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "iso8825_ber.h"

int iso8825_ber_decode(const void* ptr, size_t len, struct iso8825_tlv_t* tlv)
{
	const uint8_t* buf = ptr;
	size_t offset = 0;

	if (!ptr || !tlv) {
		return -1;
	}
	if (!len) {
		// End of encoded data
		return 0;
	}

	// Ensure minimum encoded data length
	// 1 byte tag and 1 byte length
	if (len < 2) {
		return -2;
	}

	// Decode tag octets
	if ((buf[offset] & ISO8825_BER_TAG_NUMBER_MASK) == ISO8825_BER_TAG_HIGH_FORM) {
		// High tag number form
		// See ISO 8825-1:2003, 8.1.2.4
		tlv->tag = buf[offset];
		++offset;

		do {
			// Shift next octet into tag
			tlv->tag <<= 8;
			tlv->tag |= buf[offset];
			++offset;

			// Read octets while highest bit is set
			// See ISO 8825-1:2003, 8.1.2.4.2
			if (!(buf[offset-1] & ISO8825_BER_TAG_HIGH_FORM_MORE)) {
				break;
			}

			if (offset >= sizeof(tlv->tag)) {
				// Decoded tag field size is too small for next high tag
				// number form octet
				return -3;
			}
		} while (1);
	} else {
		// Low tag number form
		tlv->tag = buf[offset];
		++offset;
	}

	// Decode length octets
	if (buf[offset] == ISO8825_BER_LEN_INDEFINITE_FORM) {
		// Indefinite length form
		++offset;
		tlv->length = 0;
		tlv->value = &buf[offset];

		// Ensure that this is a constructed type
		if ((buf[0] & ISO8825_BER_CONSTRUCTED) == 0) {
			return -4;
		}

		// BER decode content octets to find end-of-content
		do {
			int r;
			struct iso8825_tlv_t inner_tlv;

			r = iso8825_ber_decode(tlv->value + tlv->length, len - offset - tlv->length, &inner_tlv);
			if (r < 0) {
				// Error while decoding inner TLV
				return -5;
			}
			if (r == 0) {
				// Unexpected end-of-data before end-of-content
				return -6;
			}

			// Check for end-of-content
			if (inner_tlv.tag == ASN1_EOC) {
				break;
			}

			// Add consumed bytes to length
			// Exclude end-of-content from length
			tlv->length += r;
		} while (true);

	} else if (buf[offset] & ISO8825_BER_LEN_LONG_FORM) {
		// Long length form

		// Remaining bits indicate number of length octets
		size_t octet_count = buf[offset] & ISO8825_BER_LEN_LONG_FORM_COUNT_MASK;
		++offset;

		if (octet_count > sizeof(tlv->length)) {
			// Decoded length field size is too small for long length
			// form octets
			return -8;
		}

		// Validate length octet count
		if (offset + octet_count > len) {
			return -9;
		}

		// Shift length octets into length field
		// See ISO 8825-1:2003, 8.1.3.5
		tlv->length = 0;
		for (size_t i = 0; i < octet_count; ++i) {
			// Each subsequent octet is the next 8 bits of the length value
			tlv->length <<= 8;
			tlv->length |= buf[offset];
			++offset;
		}
	} else {
		// Short length form
		// Remaining bits indicate number of content octets
		// See ISO 8825-1:2003, 8.1.3.4
		tlv->length = buf[offset];
		++offset;
	}

	// Validate tag length
	if (offset + tlv->length > len) {
		return -10;
	}

	// Consume content
	tlv->value = &buf[offset];
	offset += tlv->length;

	// Capture class and primitive/constructed bits as flags
	tlv->flags = buf[0] & (ISO8825_BER_CLASS_MASK | ISO8825_BER_CONSTRUCTED);

	return offset;
}

int iso8825_ber_itr_init(const void* ptr, size_t len, struct iso8825_ber_itr_t* itr)
{
	if (!ptr || !itr) {
		return -1;
	}

	itr->ptr = ptr;
	itr->len = len;

	return 0;
}

int iso8825_ber_itr_next(struct iso8825_ber_itr_t* itr, struct iso8825_tlv_t* tlv)
{
	int r;

	if (!itr || !tlv) {
		return -1;
	}

	r = iso8825_ber_decode(itr->ptr, itr->len, tlv);
	if (r > 0) {
		// Advance iterator
		itr->ptr += r;
		itr->len -= r;
	}

	return r;
}
