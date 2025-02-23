/**
 * @file emv_capk.c
 * @brief EMV Certificate Authority Public Key (CAPK) helper functions
 *
 * Copyright 2025 Leon Lynch
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

#include "emv_capk.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CAPK_INIT(prefix, index) \
{ \
	prefix##_rid, \
	0x##index, \
	EMV_CAPK_HASH_SHA1, \
	prefix##_##index##_modulus, \
	sizeof(prefix##_##index##_modulus), \
	prefix##_##index##_exponent, \
	sizeof(prefix##_##index##_exponent), \
	prefix##_##index##_hash, \
	sizeof(prefix##_##index##_hash), \
}

// Visa CAPKs
static const uint8_t visa_rid[] = { 0xA0, 0x00, 0x00, 0x00, 0x03 };

static const uint8_t visa_92_modulus[] = {
	0x99, 0x6A, 0xF5, 0x6F, 0x56, 0x91, 0x87, 0xD0, 0x92, 0x93, 0xC1, 0x48, 0x10, 0x45, 0x0E, 0xD8,
	0xEE, 0x33, 0x57, 0x39, 0x7B, 0x18, 0xA2, 0x45, 0x8E, 0xFA, 0xA9, 0x2D, 0xA3, 0xB6, 0xDF, 0x65,
	0x14, 0xEC, 0x06, 0x01, 0x95, 0x31, 0x8F, 0xD4, 0x3B, 0xE9, 0xB8, 0xF0, 0xCC, 0x66, 0x9E, 0x3F,
	0x84, 0x40, 0x57, 0xCB, 0xDD, 0xF8, 0xBD, 0xA1, 0x91, 0xBB, 0x64, 0x47, 0x3B, 0xC8, 0xDC, 0x9A,
	0x73, 0x0D, 0xB8, 0xF6, 0xB4, 0xED, 0xE3, 0x92, 0x41, 0x86, 0xFF, 0xD9, 0xB8, 0xC7, 0x73, 0x57,
	0x89, 0xC2, 0x3A, 0x36, 0xBA, 0x0B, 0x8A, 0xF6, 0x53, 0x72, 0xEB, 0x57, 0xEA, 0x5D, 0x89, 0xE7,
	0xD1, 0x4E, 0x9C, 0x7B, 0x6B, 0x55, 0x74, 0x60, 0xF1, 0x08, 0x85, 0xDA, 0x16, 0xAC, 0x92, 0x3F,
	0x15, 0xAF, 0x37, 0x58, 0xF0, 0xF0, 0x3E, 0xBD, 0x3C, 0x5C, 0x2C, 0x94, 0x9C, 0xBA, 0x30, 0x6D,
	0xB4, 0x4E, 0x6A, 0x2C, 0x07, 0x6C, 0x5F, 0x67, 0xE2, 0x81, 0xD7, 0xEF, 0x56, 0x78, 0x5D, 0xC4,
	0xD7, 0x59, 0x45, 0xE4, 0x91, 0xF0, 0x19, 0x18, 0x80, 0x0A, 0x9E, 0x2D, 0xC6, 0x6F, 0x60, 0x08,
	0x05, 0x66, 0xCE, 0x0D, 0xAF, 0x8D, 0x17, 0xEA, 0xD4, 0x6A, 0xD8, 0xE3, 0x0A, 0x24, 0x7C, 0x9F,
};
static const uint8_t visa_92_exponent[] = { 0x03 };
static const uint8_t visa_92_hash[] = {
	0x42, 0x9C, 0x95, 0x4A, 0x38, 0x59, 0xCE, 0xF9, 0x12, 0x95, 0xF6, 0x63, 0xC9, 0x63, 0xE5, 0x82,
	0xED, 0x6E, 0xB2, 0x53,
};

static const uint8_t visa_94_modulus[] = {
	0xAC, 0xD2, 0xB1, 0x23, 0x02, 0xEE, 0x64, 0x4F, 0x3F, 0x83, 0x5A, 0xBD, 0x1F, 0xC7, 0xA6, 0xF6,
	0x2C, 0xCE, 0x48, 0xFF, 0xEC, 0x62, 0x2A, 0xA8, 0xEF, 0x06, 0x2B, 0xEF, 0x6F, 0xB8, 0xBA, 0x8B,
	0xC6, 0x8B, 0xBF, 0x6A, 0xB5, 0x87, 0x0E, 0xED, 0x57, 0x9B, 0xC3, 0x97, 0x3E, 0x12, 0x13, 0x03,
	0xD3, 0x48, 0x41, 0xA7, 0x96, 0xD6, 0xDC, 0xBC, 0x41, 0xDB, 0xF9, 0xE5, 0x2C, 0x46, 0x09, 0x79,
	0x5C, 0x0C, 0xCF, 0x7E, 0xE8, 0x6F, 0xA1, 0xD5, 0xCB, 0x04, 0x10, 0x71, 0xED, 0x2C, 0x51, 0xD2,
	0x20, 0x2F, 0x63, 0xF1, 0x15, 0x6C, 0x58, 0xA9, 0x2D, 0x38, 0xBC, 0x60, 0xBD, 0xF4, 0x24, 0xE1,
	0x77, 0x6E, 0x2B, 0xC9, 0x64, 0x80, 0x78, 0xA0, 0x3B, 0x36, 0xFB, 0x55, 0x43, 0x75, 0xFC, 0x53,
	0xD5, 0x7C, 0x73, 0xF5, 0x16, 0x0E, 0xA5, 0x9F, 0x3A, 0xFC, 0x53, 0x98, 0xEC, 0x7B, 0x67, 0x75,
	0x8D, 0x65, 0xC9, 0xBF, 0xF7, 0x82, 0x8B, 0x6B, 0x82, 0xD4, 0xBE, 0x12, 0x4A, 0x41, 0x6A, 0xB7,
	0x30, 0x19, 0x14, 0x31, 0x1E, 0xA4, 0x62, 0xC1, 0x9F, 0x77, 0x1F, 0x31, 0xB3, 0xB5, 0x73, 0x36,
	0x00, 0x0D, 0xFF, 0x73, 0x2D, 0x3B, 0x83, 0xDE, 0x07, 0x05, 0x2D, 0x73, 0x03, 0x54, 0xD2, 0x97,
	0xBE, 0xC7, 0x28, 0x71, 0xDC, 0xCF, 0x0E, 0x19, 0x3F, 0x17, 0x1A, 0xBA, 0x27, 0xEE, 0x46, 0x4C,
	0x6A, 0x97, 0x69, 0x09, 0x43, 0xD5, 0x9B, 0xDA, 0xBB, 0x2A, 0x27, 0xEB, 0x71, 0xCE, 0xEB, 0xDA,
	0xFA, 0x11, 0x76, 0x04, 0x64, 0x78, 0xFD, 0x62, 0xFE, 0xC4, 0x52, 0xD5, 0xCA, 0x39, 0x32, 0x96,
	0x53, 0x0A, 0xA3, 0xF4, 0x19, 0x27, 0xAD, 0xFE, 0x43, 0x4A, 0x2D, 0xF2, 0xAE, 0x30, 0x54, 0xF8,
	0x84, 0x06, 0x57, 0xA2, 0x6E, 0x0F, 0xC6, 0x17,
};
static const uint8_t visa_94_exponent[] = { 0x03 };
static const uint8_t visa_94_hash[] = {
	0xC4, 0xA3, 0xC4, 0x3C, 0xCF, 0x87, 0x32, 0x7D, 0x13, 0x6B, 0x80, 0x41, 0x60, 0xE4, 0x7D, 0x43,
	0xB6, 0x0E, 0x6E, 0x0F,
};

static const uint8_t visa_95_modulus[] = {
	0xBE, 0x9E, 0x1F, 0xA5, 0xE9, 0xA8, 0x03, 0x85, 0x29, 0x99, 0xC4, 0xAB, 0x43, 0x2D, 0xB2, 0x86,
	0x00, 0xDC, 0xD9, 0xDA, 0xB7, 0x6D, 0xFA, 0xAA, 0x47, 0x35, 0x5A, 0x0F, 0xE3, 0x7B, 0x15, 0x08,
	0xAC, 0x6B, 0xF3, 0x88, 0x60, 0xD3, 0xC6, 0xC2, 0xE5, 0xB1, 0x2A, 0x3C, 0xAA, 0xF2, 0xA7, 0x00,
	0x5A, 0x72, 0x41, 0xEB, 0xAA, 0x77, 0x71, 0x11, 0x2C, 0x74, 0xCF, 0x9A, 0x06, 0x34, 0x65, 0x2F,
	0xBC, 0xA0, 0xE5, 0x98, 0x0C, 0x54, 0xA6, 0x47, 0x61, 0xEA, 0x10, 0x1A, 0x11, 0x4E, 0x0F, 0x0B,
	0x55, 0x72, 0xAD, 0xD5, 0x7D, 0x01, 0x0B, 0x7C, 0x9C, 0x88, 0x7E, 0x10, 0x4C, 0xA4, 0xEE, 0x12,
	0x72, 0xDA, 0x66, 0xD9, 0x97, 0xB9, 0xA9, 0x0B, 0x5A, 0x6D, 0x62, 0x4A, 0xB6, 0xC5, 0x7E, 0x73,
	0xC8, 0xF9, 0x19, 0x00, 0x0E, 0xB5, 0xF6, 0x84, 0x89, 0x8E, 0xF8, 0xC3, 0xDB, 0xEF, 0xB3, 0x30,
	0xC6, 0x26, 0x60, 0xBE, 0xD8, 0x8E, 0xA7, 0x8E, 0x90, 0x9A, 0xFF, 0x05, 0xF6, 0xDA, 0x62, 0x7B,
};
static const uint8_t visa_95_exponent[] = { 0x03 };
static const uint8_t visa_95_hash[] = {
	0xEE, 0x15, 0x11, 0xCE, 0xC7, 0x10, 0x20, 0xA9, 0xB9, 0x04, 0x43, 0xB3, 0x7B, 0x1D, 0x5F, 0x6E,
	0x70, 0x30, 0x30, 0xF6,
};

static const uint8_t visa_99_modulus[] = {
	0xAB, 0x79, 0xFC, 0xC9, 0x52, 0x08, 0x96, 0x96, 0x7E, 0x77, 0x6E, 0x64, 0x44, 0x4E, 0x5D, 0xCD,
	0xD6, 0xE1, 0x36, 0x11, 0x87, 0x4F, 0x39, 0x85, 0x72, 0x25, 0x20, 0x42, 0x52, 0x95, 0xEE, 0xA4,
	0xBD, 0x0C, 0x27, 0x81, 0xDE, 0x7F, 0x31, 0xCD, 0x3D, 0x04, 0x1F, 0x56, 0x5F, 0x74, 0x73, 0x06,
	0xEE, 0xD6, 0x29, 0x54, 0xB1, 0x7E, 0xDA, 0xBA, 0x3A, 0x6C, 0x5B, 0x85, 0xA1, 0xDE, 0x1B, 0xEB,
	0x9A, 0x34, 0x14, 0x1A, 0xF3, 0x8F, 0xCF, 0x82, 0x79, 0xC9, 0xDE, 0xA0, 0xD5, 0xA6, 0x71, 0x0D,
	0x08, 0xDB, 0x41, 0x24, 0xF0, 0x41, 0x94, 0x55, 0x87, 0xE2, 0x03, 0x59, 0xBA, 0xB4, 0x7B, 0x75,
	0x75, 0xAD, 0x94, 0x26, 0x2D, 0x4B, 0x25, 0xF2, 0x64, 0xAF, 0x33, 0xDE, 0xDC, 0xF2, 0x8E, 0x09,
	0x61, 0x5E, 0x93, 0x7D, 0xE3, 0x2E, 0xDC, 0x03, 0xC5, 0x44, 0x45, 0xFE, 0x7E, 0x38, 0x27, 0x77,
};
static const uint8_t visa_99_exponent[] = { 0x03 };
static const uint8_t visa_99_hash[] = {
	0x4A, 0xBF, 0xFD, 0x6B, 0x1C, 0x51, 0x21, 0x2D, 0x05, 0x55, 0x2E, 0x43, 0x1C, 0x5B, 0x17, 0x00,
	0x7D, 0x2F, 0x5E, 0x6D,
};

// Mastercard CAPKs
static const uint8_t mastercard_rid[] = { 0xA0, 0x00, 0x00, 0x00, 0x04 };

static const uint8_t mastercard_EF_modulus[] = {
	0xA1, 0x91, 0xCB, 0x87, 0x47, 0x3F, 0x29, 0x34, 0x9B, 0x5D, 0x60, 0xA8, 0x8B, 0x3E, 0xAE, 0xE0,
	0x97, 0x3A, 0xA6, 0xF1, 0xA0, 0x82, 0xF3, 0x58, 0xD8, 0x49, 0xFD, 0xDF, 0xF9, 0xC0, 0x91, 0xF8,
	0x99, 0xED, 0xA9, 0x79, 0x2C, 0xAF, 0x09, 0xEF, 0x28, 0xF5, 0xD2, 0x24, 0x04, 0xB8, 0x8A, 0x22,
	0x93, 0xEE, 0xBB, 0xC1, 0x94, 0x9C, 0x43, 0xBE, 0xA4, 0xD6, 0x0C, 0xFD, 0x87, 0x9A, 0x15, 0x39,
	0x54, 0x4E, 0x09, 0xE0, 0xF0, 0x9F, 0x60, 0xF0, 0x65, 0xB2, 0xBF, 0x2A, 0x13, 0xEC, 0xC7, 0x05,
	0xF3, 0xD4, 0x68, 0xB9, 0xD3, 0x3A, 0xE7, 0x7A, 0xD9, 0xD3, 0xF1, 0x9C, 0xA4, 0x0F, 0x23, 0xDC,
	0xF5, 0xEB, 0x7C, 0x04, 0xDC, 0x8F, 0x69, 0xEB, 0xA5, 0x65, 0xB1, 0xEB, 0xCB, 0x46, 0x86, 0xCD,
	0x27, 0x47, 0x85, 0x53, 0x0F, 0xF6, 0xF6, 0xE9, 0xEE, 0x43, 0xAA, 0x43, 0xFD, 0xB0, 0x2C, 0xE0,
	0x0D, 0xAE, 0xC1, 0x5C, 0x7B, 0x8F, 0xD6, 0xA9, 0xB3, 0x94, 0xBA, 0xBA, 0x41, 0x9D, 0x3F, 0x6D,
	0xC8, 0x5E, 0x16, 0x56, 0x9B, 0xE8, 0xE7, 0x69, 0x89, 0x68, 0x8E, 0xFE, 0xA2, 0xDF, 0x22, 0xFF,
	0x7D, 0x35, 0xC0, 0x43, 0x33, 0x8D, 0xEA, 0xA9, 0x82, 0xA0, 0x2B, 0x86, 0x6D, 0xE5, 0x32, 0x85,
	0x19, 0xEB, 0xBC, 0xD6, 0xF0, 0x3C, 0xDD, 0x68, 0x66, 0x73, 0x84, 0x7F, 0x84, 0xDB, 0x65, 0x1A,
	0xB8, 0x6C, 0x28, 0xCF, 0x14, 0x62, 0x56, 0x2C, 0x57, 0x7B, 0x85, 0x35, 0x64, 0xA2, 0x90, 0xC8,
	0x55, 0x6D, 0x81, 0x85, 0x31, 0x26, 0x8D, 0x25, 0xCC, 0x98, 0xA4, 0xCC, 0x6A, 0x0B, 0xDF, 0xFF,
	0xDA, 0x2D, 0xCC, 0xA3, 0xA9, 0x4C, 0x99, 0x85, 0x59, 0xE3, 0x07, 0xFD, 0xDF, 0x91, 0x50, 0x06,
	0xD9, 0xA9, 0x87, 0xB0, 0x7D, 0xDA, 0xEB, 0x3B,
};
static const uint8_t mastercard_EF_exponent[] = { 0x03 };
static const uint8_t mastercard_EF_hash[] = {
	0x21, 0x76, 0x6E, 0xBB, 0x0E, 0xE1, 0x22, 0xAF, 0xB6, 0x5D, 0x78, 0x45, 0xB7, 0x3D, 0xB4, 0x6B,
	0xAB, 0x65, 0x42, 0x7A,
};

static const uint8_t mastercard_F1_modulus[] = {
	0xA0, 0xDC, 0xF4, 0xBD, 0xE1, 0x9C, 0x35, 0x46, 0xB4, 0xB6, 0xF0, 0x41, 0x4D, 0x17, 0x4D, 0xDE,
	0x29, 0x4A, 0xAB, 0xBB, 0x82, 0x8C, 0x5A, 0x83, 0x4D, 0x73, 0xAA, 0xE2, 0x7C, 0x99, 0xB0, 0xB0,
	0x53, 0xA9, 0x02, 0x78, 0x00, 0x72, 0x39, 0xB6, 0x45, 0x9F, 0xF0, 0xBB, 0xCD, 0x7B, 0x4B, 0x9C,
	0x6C, 0x50, 0xAC, 0x02, 0xCE, 0x91, 0x36, 0x8D, 0xA1, 0xBD, 0x21, 0xAA, 0xEA, 0xDB, 0xC6, 0x53,
	0x47, 0x33, 0x7D, 0x89, 0xB6, 0x8F, 0x5C, 0x99, 0xA0, 0x9D, 0x05, 0xBE, 0x02, 0xDD, 0x1F, 0x8C,
	0x5B, 0xA2, 0x0E, 0x2F, 0x13, 0xFB, 0x2A, 0x27, 0xC4, 0x1D, 0x3F, 0x85, 0xCA, 0xD5, 0xCF, 0x66,
	0x68, 0xE7, 0x58, 0x51, 0xEC, 0x66, 0xED, 0xBF, 0x98, 0x85, 0x1F, 0xD4, 0xE4, 0x2C, 0x44, 0xC1,
	0xD5, 0x9F, 0x59, 0x84, 0x70, 0x3B, 0x27, 0xD5, 0xB9, 0xF2, 0x1B, 0x8F, 0xA0, 0xD9, 0x32, 0x79,
	0xFB, 0xBF, 0x69, 0xE0, 0x90, 0x64, 0x29, 0x09, 0xC9, 0xEA, 0x27, 0xF8, 0x98, 0x95, 0x95, 0x41,
	0xAA, 0x67, 0x57, 0xF5, 0xF6, 0x24, 0x10, 0x4F, 0x6E, 0x1D, 0x3A, 0x95, 0x32, 0xF2, 0xA6, 0xE5,
	0x15, 0x15, 0xAE, 0xAD, 0x1B, 0x43, 0xB3, 0xD7, 0x83, 0x50, 0x88, 0xA2, 0xFA, 0xFA, 0x7B, 0xE7,
};
static const uint8_t mastercard_F1_exponent[] = { 0x03 };
static const uint8_t mastercard_F1_hash[] = {
	0xD8, 0xE6, 0x8D, 0xA1, 0x67, 0xAB, 0x5A, 0x85, 0xD8, 0xC3, 0xD5, 0x5E, 0xCB, 0x9B, 0x05, 0x17,
	0xA1, 0xA5, 0xB4, 0xBB,
};

static const uint8_t mastercard_F3_modulus[] = {
	0x98, 0xF0, 0xC7, 0x70, 0xF2, 0x38, 0x64, 0xC2, 0xE7, 0x66, 0xDF, 0x02, 0xD1, 0xE8, 0x33, 0xDF,
	0xF4, 0xFF, 0xE9, 0x2D, 0x69, 0x6E, 0x16, 0x42, 0xF0, 0xA8, 0x8C, 0x56, 0x94, 0xC6, 0x47, 0x9D,
	0x16, 0xDB, 0x15, 0x37, 0xBF, 0xE2, 0x9E, 0x4F, 0xDC, 0x6E, 0x6E, 0x8A, 0xFD, 0x1B, 0x0E, 0xB7,
	0xEA, 0x01, 0x24, 0x72, 0x3C, 0x33, 0x31, 0x79, 0xBF, 0x19, 0xE9, 0x3F, 0x10, 0x65, 0x8B, 0x2F,
	0x77, 0x6E, 0x82, 0x9E, 0x87, 0xDA, 0xED, 0xA9, 0xC9, 0x4A, 0x8B, 0x33, 0x82, 0x19, 0x9A, 0x35,
	0x0C, 0x07, 0x79, 0x77, 0xC9, 0x7A, 0xFF, 0x08, 0xFD, 0x11, 0x31, 0x0A, 0xC9, 0x50, 0xA7, 0x2C,
	0x3C, 0xA5, 0x00, 0x2E, 0xF5, 0x13, 0xFC, 0xCC, 0x28, 0x6E, 0x64, 0x6E, 0x3C, 0x53, 0x87, 0x53,
	0x5D, 0x50, 0x95, 0x14, 0xB3, 0xB3, 0x26, 0xE1, 0x23, 0x4F, 0x9C, 0xB4, 0x8C, 0x36, 0xDD, 0xD4,
	0x4B, 0x41, 0x6D, 0x23, 0x65, 0x40, 0x34, 0xA6, 0x6F, 0x40, 0x3B, 0xA5, 0x11, 0xC5, 0xEF, 0xA3,
};
static const uint8_t mastercard_F3_exponent[] = { 0x03 };
static const uint8_t mastercard_F3_hash[] = {
	0xA6, 0x9A, 0xC7, 0x60, 0x3D, 0xAF, 0x56, 0x6E, 0x97, 0x2D, 0xED, 0xC2, 0xCB, 0x43, 0x3E, 0x07,
	0xE8, 0xB0, 0x1A, 0x9A,
};

static const uint8_t mastercard_F4_modulus[] = {
	0x9E, 0x2F, 0x74, 0xBF, 0x4A, 0xB5, 0x21, 0x01, 0x97, 0x35, 0xBF, 0xC7, 0xE4, 0xCB, 0xC5, 0x6B,
	0x6F, 0x64, 0xAF, 0xF1, 0xED, 0x7B, 0x79, 0x99, 0x8E, 0xE5, 0xB3, 0xDF, 0xFE, 0x23, 0xDF, 0xC8,
	0xE2, 0xDD, 0x00, 0x25, 0x57, 0x5A, 0xF9, 0x4D, 0xE8, 0x14, 0x26, 0x45, 0x28, 0xAF, 0x6F, 0x80,
	0x05, 0xA5, 0x38, 0xB3, 0xD6, 0xAE, 0x88, 0x1B, 0x35, 0x0F, 0x89, 0x59, 0x55, 0x88, 0xE5, 0x1F,
	0x74, 0x23, 0xE7, 0x11, 0x10, 0x9D, 0xEC, 0x16, 0x9F, 0xDD, 0x56, 0x06, 0x02, 0xD8, 0x0E, 0xF4,
	0x6E, 0x58, 0x2C, 0x8C, 0x54, 0x6C, 0x89, 0x30, 0x39, 0x4B, 0xD5, 0x34, 0x41, 0x2A, 0x88, 0xCC,
	0x9F, 0xF4, 0xDF, 0xC0, 0x8A, 0xE7, 0x16, 0xA5, 0x95, 0xEF, 0x1A, 0xF7, 0xC3, 0x2E, 0xDF, 0xCF,
	0x99, 0x64, 0x33, 0xEB, 0x3C, 0x36, 0xBC, 0xE0, 0x93, 0xE4, 0x4E, 0x0B, 0xDE, 0x22, 0x8E, 0x02,
	0x99, 0xA0, 0xE3, 0x58, 0xBF, 0x28, 0x30, 0x8D, 0xB4, 0x73, 0x98, 0x15, 0xDD, 0x09, 0xF1, 0xE8,
	0x96, 0x54, 0xCC, 0x7C, 0xC1, 0x93, 0xE2, 0xAC, 0x17, 0xC4, 0xDA, 0x33, 0x5D, 0x90, 0x4B, 0x8E,
	0xC0, 0x6A, 0xCF, 0xBD, 0xE0, 0x83, 0xF7, 0x69, 0x33, 0xC9, 0x69, 0x67, 0x2E, 0x9A, 0xFE, 0xA3,
};
static const uint8_t mastercard_F4_exponent[] = { 0x03 };
static const uint8_t mastercard_F4_hash[] = {
	0xBF, 0x6B, 0x5B, 0x9C, 0x47, 0x13, 0x4E, 0x49, 0x45, 0x71, 0x73, 0x2A, 0x49, 0x03, 0xC9, 0x35,
	0x87, 0x46, 0x82, 0xB9,
};

static const uint8_t mastercard_F5_modulus[] = {
	0xA6, 0xE6, 0xFB, 0x72, 0x17, 0x95, 0x06, 0xF8, 0x60, 0xCC, 0xCA, 0x8C, 0x27, 0xF9, 0x9C, 0xEC,
	0xD9, 0x4C, 0x7D, 0x4F, 0x31, 0x91, 0xD3, 0x03, 0xBB, 0xEE, 0x37, 0x48, 0x1C, 0x7A, 0xA1, 0x5F,
	0x23, 0x3B, 0xA7, 0x55, 0xE9, 0xE4, 0x37, 0x63, 0x45, 0xA9, 0xA6, 0x7E, 0x79, 0x94, 0xBD, 0xC1,
	0xC6, 0x80, 0xBB, 0x35, 0x22, 0xD8, 0xC9, 0x3E, 0xB0, 0xCC, 0xC9, 0x1A, 0xD3, 0x1A, 0xD4, 0x50,
	0xDA, 0x30, 0xD3, 0x37, 0x66, 0x2D, 0x19, 0xAC, 0x03, 0xE2, 0xB4, 0xEF, 0x5F, 0x6E, 0xC1, 0x82,
	0x82, 0xD4, 0x91, 0xE1, 0x97, 0x67, 0xD7, 0xB2, 0x45, 0x42, 0xDF, 0xDE, 0xFF, 0x6F, 0x62, 0x18,
	0x55, 0x03, 0x53, 0x20, 0x69, 0xBB, 0xB3, 0x69, 0xE3, 0xBB, 0x9F, 0xB1, 0x9A, 0xC6, 0xF1, 0xC3,
	0x0B, 0x97, 0xD2, 0x49, 0xEE, 0xE7, 0x64, 0xE0, 0xBA, 0xC9, 0x7F, 0x25, 0xC8, 0x73, 0xD9, 0x73,
	0x95, 0x3E, 0x51, 0x53, 0xA4, 0x20, 0x64, 0xBB, 0xFA, 0xBF, 0xD0, 0x6A, 0x4B, 0xB4, 0x86, 0x86,
	0x0B, 0xF6, 0x63, 0x74, 0x06, 0xC9, 0xFC, 0x36, 0x81, 0x3A, 0x4A, 0x75, 0xF7, 0x5C, 0x31, 0xCC,
	0xA9, 0xF6, 0x9F, 0x8D, 0xE5, 0x9A, 0xDE, 0xCE, 0xF6, 0xBD, 0xE7, 0xE0, 0x78, 0x00, 0xFC, 0xBE,
	0x03, 0x5D, 0x31, 0x76, 0xAF, 0x84, 0x73, 0xE2, 0x3E, 0x9A, 0xA3, 0xDF, 0xEE, 0x22, 0x11, 0x96,
	0xD1, 0x14, 0x83, 0x02, 0x67, 0x7C, 0x72, 0x0C, 0xFE, 0x25, 0x44, 0xA0, 0x3D, 0xB5, 0x53, 0xE7,
	0xF1, 0xB8, 0x42, 0x7B, 0xA1, 0xCC, 0x72, 0xB0, 0xF2, 0x9B, 0x12, 0xDF, 0xEF, 0x4C, 0x08, 0x1D,
	0x07, 0x6D, 0x35, 0x3E, 0x71, 0x88, 0x0A, 0xAD, 0xFF, 0x38, 0x63, 0x52, 0xAF, 0x0A, 0xB7, 0xB2,
	0x8E, 0xD4, 0x9E, 0x1E, 0x67, 0x2D, 0x11, 0xF9,
};
static const uint8_t mastercard_F5_exponent[] = { 0x01, 0x00, 0x01 };
static const uint8_t mastercard_F5_hash[] = {
	0xC2, 0x23, 0x98, 0x04, 0xC8, 0x09, 0x81, 0x70, 0xBE, 0x52, 0xD6, 0xD5, 0xD4, 0x15, 0x9E, 0x81,
	0xCE, 0x84, 0x66, 0xBF,
};

static const uint8_t mastercard_F8_modulus[] = {
	0xA1, 0xF5, 0xE1, 0xC9, 0xBD, 0x86, 0x50, 0xBD, 0x43, 0xAB, 0x6E, 0xE5, 0x6B, 0x89, 0x1E, 0xF7,
	0x45, 0x9C, 0x0A, 0x24, 0xFA, 0x84, 0xF9, 0x12, 0x7D, 0x1A, 0x6C, 0x79, 0xD4, 0x93, 0x0F, 0x6D,
	0xB1, 0x85, 0x2E, 0x25, 0x10, 0xF1, 0x8B, 0x61, 0xCD, 0x35, 0x4D, 0xB8, 0x3A, 0x35, 0x6B, 0xD1,
	0x90, 0xB8, 0x8A, 0xB8, 0xDF, 0x04, 0x28, 0x4D, 0x02, 0xA4, 0x20, 0x4A, 0x7B, 0x6C, 0xB7, 0xC5,
	0x55, 0x19, 0x77, 0xA9, 0xB3, 0x63, 0x79, 0xCA, 0x3D, 0xE1, 0xA0, 0x8E, 0x69, 0xF3, 0x01, 0xC9,
	0x5C, 0xC1, 0xC2, 0x05, 0x06, 0x95, 0x92, 0x75, 0xF4, 0x17, 0x23, 0xDD, 0x5D, 0x29, 0x25, 0x29,
	0x05, 0x79, 0xE5, 0xA9, 0x5B, 0x0D, 0xF6, 0x32, 0x3F, 0xC8, 0xE9, 0x27, 0x3D, 0x6F, 0x84, 0x91,
	0x98, 0xC4, 0x99, 0x62, 0x09, 0x16, 0x6D, 0x9B, 0xFC, 0x97, 0x3C, 0x36, 0x1C, 0xC8, 0x26, 0xE1,
};
static const uint8_t mastercard_F8_exponent[] = { 0x03 };
static const uint8_t mastercard_F8_hash[] = {
	0xF0, 0x6E, 0xCC, 0x6D, 0x2A, 0xAE, 0xBF, 0x25, 0x9B, 0x7E, 0x75, 0x5A, 0x38, 0xD9, 0xA9, 0xB2,
	0x4E, 0x2F, 0xF3, 0xDD,
};

static const uint8_t mastercard_FA_modulus[] = {
	0xA9, 0x0F, 0xCD, 0x55, 0xAA, 0x2D, 0x5D, 0x99, 0x63, 0xE3, 0x5E, 0xD0, 0xF4, 0x40, 0x17, 0x76,
	0x99, 0x83, 0x2F, 0x49, 0xC6, 0xBA, 0xB1, 0x5C, 0xDA, 0xE5, 0x79, 0x4B, 0xE9, 0x3F, 0x93, 0x4D,
	0x44, 0x62, 0xD5, 0xD1, 0x27, 0x62, 0xE4, 0x8C, 0x38, 0xBA, 0x83, 0xD8, 0x44, 0x5D, 0xEA, 0xA7,
	0x41, 0x95, 0xA3, 0x01, 0xA1, 0x02, 0xB2, 0xF1, 0x14, 0xEA, 0xDA, 0x0D, 0x18, 0x0E, 0xE5, 0xE7,
	0xA5, 0xC7, 0x3E, 0x0C, 0x4E, 0x11, 0xF6, 0x7A, 0x43, 0xDD, 0xAB, 0x5D, 0x55, 0x68, 0x3B, 0x14,
	0x74, 0xCC, 0x06, 0x27, 0xF4, 0x4B, 0x8D, 0x30, 0x88, 0xA4, 0x92, 0xFF, 0xAA, 0xDA, 0xD4, 0xF4,
	0x24, 0x22, 0xD0, 0xE7, 0x01, 0x35, 0x36, 0xC3, 0xC4, 0x9A, 0xD3, 0xD0, 0xFA, 0xE9, 0x64, 0x59,
	0xB0, 0xF6, 0xB1, 0xB6, 0x05, 0x65, 0x38, 0xA3, 0xD6, 0xD4, 0x46, 0x40, 0xF9, 0x44, 0x67, 0xB1,
	0x08, 0x86, 0x7D, 0xEC, 0x40, 0xFA, 0xAE, 0xCD, 0x74, 0x0C, 0x00, 0xE2, 0xB7, 0xA8, 0x85, 0x2D,
};
static const uint8_t mastercard_FA_exponent[] = { 0x03 };
static const uint8_t mastercard_FA_hash[] = {
	0x5B, 0xED, 0x40, 0x68, 0xD9, 0x6E, 0xA1, 0x6D, 0x2D, 0x77, 0xE0, 0x3D, 0x60, 0x36, 0xFC, 0x7A,
	0x16, 0x0E, 0xA9, 0x9C,
};

static const uint8_t mastercard_FE_modulus[] = {
	0xA6, 0x53, 0xEA, 0xC1, 0xC0, 0xF7, 0x86, 0xC8, 0x72, 0x4F, 0x73, 0x7F, 0x17, 0x29, 0x97, 0xD6,
	0x3D, 0x1C, 0x32, 0x51, 0xC4, 0x44, 0x02, 0x04, 0x9B, 0x86, 0x5B, 0xAE, 0x87, 0x7D, 0x0F, 0x39,
	0x8C, 0xBF, 0xBE, 0x8A, 0x60, 0x35, 0xE2, 0x4A, 0xFA, 0x08, 0x6B, 0xEF, 0xDE, 0x93, 0x51, 0xE5,
	0x4B, 0x95, 0x70, 0x8E, 0xE6, 0x72, 0xF0, 0x96, 0x8B, 0xCD, 0x50, 0xDC, 0xE4, 0x0F, 0x78, 0x33,
	0x22, 0xB2, 0xAB, 0xA0, 0x4E, 0xF1, 0x37, 0xEF, 0x18, 0xAB, 0xF0, 0x3C, 0x7D, 0xBC, 0x58, 0x13,
	0xAE, 0xAE, 0xF3, 0xAA, 0x77, 0x97, 0xBA, 0x15, 0xDF, 0x7D, 0x5B, 0xA1, 0xCB, 0xAF, 0x7F, 0xD5,
	0x20, 0xB5, 0xA4, 0x82, 0xD8, 0xD3, 0xFE, 0xE1, 0x05, 0x07, 0x78, 0x71, 0x11, 0x3E, 0x23, 0xA4,
	0x9A, 0xF3, 0x92, 0x65, 0x54, 0xA7, 0x0F, 0xE1, 0x0E, 0xD7, 0x28, 0xCF, 0x79, 0x3B, 0x62, 0xA1,
};
static const uint8_t mastercard_FE_exponent[] = { 0x03 };
static const uint8_t mastercard_FE_hash[] = {
	0x9A, 0x29, 0x5B, 0x05, 0xFB, 0x39, 0x0E, 0xF7, 0x92, 0x3F, 0x57, 0x61, 0x8A, 0x9F, 0xDA, 0x29,
	0x41, 0xFC, 0x34, 0xE0,
};

// Amex CAPKs
static const uint8_t amex_rid[] = { 0xA0, 0x00, 0x00, 0x00, 0x25 };

static const uint8_t amex_04_modulus[] = {
	0xD0, 0xF5, 0x43, 0xF0, 0x3F, 0x25, 0x17, 0x13, 0x3E, 0xF2, 0xBA, 0x4A, 0x11, 0x04, 0x48, 0x67,
	0x58, 0x63, 0x0D, 0xCF, 0xE3, 0xA8, 0x83, 0xC7, 0x7B, 0x4E, 0x48, 0x44, 0xE3, 0x9A, 0x9B, 0xD6,
	0x36, 0x0D, 0x23, 0xE6, 0x64, 0x4E, 0x1E, 0x07, 0x1F, 0x19, 0x6D, 0xDF, 0x2E, 0x4A, 0x68, 0xB4,
	0xA3, 0xD9, 0x3D, 0x14, 0x26, 0x8D, 0x72, 0x40, 0xF6, 0xA1, 0x4F, 0x0D, 0x71, 0x4C, 0x17, 0x82,
	0x7D, 0x27, 0x9D, 0x19, 0x2E, 0x88, 0x93, 0x1A, 0xF7, 0x30, 0x07, 0x27, 0xAE, 0x9D, 0xA8, 0x0A,
	0x3F, 0x0E, 0x36, 0x6A, 0xEB, 0xA6, 0x17, 0x78, 0x17, 0x17, 0x37, 0x98, 0x9E, 0x1E, 0xE3, 0x09,
};
static const uint8_t amex_04_exponent[] = { 0x03 };
static const uint8_t amex_04_hash[] = {
	0xFD, 0xD7, 0x13, 0x9E, 0xC7, 0xE0, 0xC3, 0x31, 0x67, 0xFD, 0x61, 0xAD, 0x3C, 0xAD, 0xBD, 0x68,
	0xD6, 0x6E, 0x91, 0xC5,
};

static const uint8_t amex_65_modulus[] = {
	0xE5, 0x3E, 0xB4, 0x1F, 0x83, 0x9D, 0xDF, 0xB4, 0x74, 0xF2, 0x72, 0xCD, 0x0C, 0xBE, 0x37, 0x3D,
	0x54, 0x68, 0xEB, 0x3F, 0x50, 0xF3, 0x9C, 0x95, 0xBD, 0xF4, 0xD3, 0x9F, 0xA8, 0x2B, 0x98, 0xDA,
	0xBC, 0x94, 0x76, 0xB6, 0xEA, 0x35, 0x0C, 0x0D, 0xCE, 0x1C, 0xD9, 0x20, 0x75, 0xD8, 0xC4, 0x4D,
	0x1E, 0x57, 0x28, 0x31, 0x90, 0xF9, 0x6B, 0x35, 0x37, 0xD9, 0xE6, 0x32, 0xC4, 0x61, 0x81, 0x5E,
	0xBD, 0x2B, 0xAF, 0x36, 0x89, 0x1D, 0xF6, 0xBF, 0xB1, 0xD3, 0x0F, 0xA0, 0xB7, 0x52, 0xC4, 0x3D,
	0xCA, 0x02, 0x57, 0xD3, 0x5D, 0xFF, 0x4C, 0xCF, 0xC9, 0x8F, 0x84, 0x19, 0x8D, 0x51, 0x52, 0xEC,
	0x61, 0xD7, 0xB5, 0xF7, 0x4B, 0xD0, 0x93, 0x83, 0xBD, 0x0E, 0x2A, 0xA4, 0x22, 0x98, 0xFF, 0xB0,
	0x2F, 0x0D, 0x79, 0xAD, 0xB7, 0x0D, 0x72, 0x24, 0x3E, 0xE5, 0x37, 0xF7, 0x55, 0x36, 0xA8, 0xA8,
	0xDF, 0x96, 0x25, 0x82, 0xE9, 0xE6, 0x81, 0x2F, 0x3A, 0x0B, 0xE0, 0x2A, 0x43, 0x65, 0x40, 0x0D,
};
static const uint8_t amex_65_exponent[] = { 0x03 };
static const uint8_t amex_65_hash[] = {
	0x89, 0x4C, 0x5D, 0x08, 0xD4, 0xEA, 0x28, 0xBB, 0x79, 0xDC, 0x46, 0xCE, 0xAD, 0x99, 0x8B, 0x87,
	0x73, 0x22, 0xF4, 0x16,
};

static const uint8_t amex_A1_modulus[] = {
	0x99, 0xD1, 0x73, 0x96, 0x42, 0x1E, 0xE3, 0xF9, 0x19, 0xBA, 0x54, 0x9D, 0x95, 0x54, 0xBE, 0x0D,
	0x4F, 0x92, 0xCB, 0x8B, 0x53, 0xB4, 0x87, 0x8E, 0xD6, 0x0C, 0xC5, 0xB2, 0xDE, 0xED, 0xC7, 0x9B,
	0x85, 0xC8, 0xBD, 0x6F, 0xD2, 0xF2, 0x3C, 0x22, 0xE6, 0x8B, 0x38, 0x1A, 0xEE, 0xB7, 0x41, 0x53,
	0xAF, 0xB3, 0xC9, 0x6E, 0x6C, 0x96, 0xAD, 0x01, 0x8E, 0x73, 0xC2, 0x02, 0x5D, 0x1E, 0xE7, 0x76,
	0x22, 0xA7, 0x2B, 0xEE, 0x97, 0x3C, 0x1A, 0xF7, 0xB9, 0x08, 0x46, 0x8D, 0x74, 0xFD, 0xB5, 0x3D,
	0xCE, 0x83, 0x80, 0x52, 0x3E, 0x38, 0xC3, 0x0D, 0x0A, 0x8A, 0x22, 0x65, 0x29, 0x72, 0x68, 0x24,
	0xE2, 0x09, 0xE6, 0x68, 0xF4, 0x9F, 0x43, 0xB0, 0xE8, 0xCD, 0x2F, 0xE5, 0x27, 0xCE, 0x7C, 0xC4,
	0x1F, 0x33, 0xF4, 0x34, 0xF9, 0x5D, 0x6E, 0x2F, 0xE2, 0xF5, 0x89, 0x37, 0x20, 0x32, 0xF2, 0xD6,
	0x50, 0x43, 0x40, 0xF8, 0xC5, 0x42, 0xD2, 0x98, 0xB4, 0x99, 0xA5, 0x3D, 0x95, 0xAF, 0x40, 0x83,
};
static const uint8_t amex_A1_exponent[] = { 0x01, 0x00, 0x01 };
static const uint8_t amex_A1_hash[] = {
	0x6E, 0x8C, 0x1C, 0x5E, 0xF6, 0x6C, 0xC9, 0x2F, 0x40, 0xA1, 0x72, 0x19, 0x37, 0xA3, 0x38, 0x73,
	0xAF, 0x63, 0xBE, 0x5B,
};

static const uint8_t amex_C8_modulus[] = {
	0xBF, 0x0C, 0xFC, 0xED, 0x70, 0x8F, 0xB6, 0xB0, 0x48, 0xE3, 0x01, 0x43, 0x36, 0xEA, 0x24, 0xAA,
	0x00, 0x7D, 0x79, 0x67, 0xB8, 0xAA, 0x4E, 0x61, 0x3D, 0x26, 0xD0, 0x15, 0xC4, 0xFE, 0x78, 0x05,
	0xD9, 0xDB, 0x13, 0x1C, 0xED, 0x0D, 0x2A, 0x8E, 0xD5, 0x04, 0xC3, 0xB5, 0xCC, 0xD4, 0x8C, 0x33,
	0x19, 0x9E, 0x5A, 0x5B, 0xF6, 0x44, 0xDA, 0x04, 0x3B, 0x54, 0xDB, 0xF6, 0x02, 0x76, 0xF0, 0x5B,
	0x17, 0x50, 0xFA, 0xB3, 0x90, 0x98, 0xC7, 0x51, 0x1D, 0x04, 0xBA, 0xBC, 0x64, 0x94, 0x82, 0xDD,
	0xCF, 0x7C, 0xC4, 0x2C, 0x8C, 0x43, 0x5B, 0xAB, 0x8D, 0xD0, 0xEB, 0x1A, 0x62, 0x0C, 0x31, 0x11,
	0x1D, 0x1A, 0xAA, 0xF9, 0xAF, 0x65, 0x71, 0xEE, 0xBD, 0x4C, 0xF5, 0xA0, 0x84, 0x96, 0xD5, 0x7E,
	0x7A, 0xBD, 0xBB, 0x51, 0x80, 0xE0, 0xA4, 0x2D, 0xA8, 0x69, 0xAB, 0x95, 0xFB, 0x62, 0x0E, 0xFF,
	0x26, 0x41, 0xC3, 0x70, 0x2A, 0xF3, 0xBE, 0x0B, 0x0C, 0x13, 0x8E, 0xAE, 0xF2, 0x02, 0xE2, 0x1D,
};
static const uint8_t amex_C8_exponent[] = { 0x03 };
static const uint8_t amex_C8_hash[] = {
	0x33, 0xBD, 0x7A, 0x05, 0x9F, 0xAB, 0x09, 0x49, 0x39, 0xB9, 0x0A, 0x8F, 0x35, 0x84, 0x5C, 0x9D,
	0xC7, 0x79, 0xBD, 0x50,
};

static const uint8_t amex_C9_modulus[] = {
	0xB3, 0x62, 0xDB, 0x57, 0x33, 0xC1, 0x5B, 0x87, 0x97, 0xB8, 0xEC, 0xEE, 0x55, 0xCB, 0x1A, 0x37,
	0x1F, 0x76, 0x0E, 0x0B, 0xED, 0xD3, 0x71, 0x5B, 0xB2, 0x70, 0x42, 0x4F, 0xD4, 0xEA, 0x26, 0x06,
	0x2C, 0x38, 0xC3, 0xF4, 0xAA, 0xA3, 0x73, 0x2A, 0x83, 0xD3, 0x6E, 0xA8, 0xE9, 0x60, 0x2F, 0x66,
	0x83, 0xEE, 0xCC, 0x6B, 0xAF, 0xF6, 0x3D, 0xD2, 0xD4, 0x90, 0x14, 0xBD, 0xE4, 0xD6, 0xD6, 0x03,
	0xCD, 0x74, 0x42, 0x06, 0xB0, 0x5B, 0x4B, 0xAD, 0x0C, 0x64, 0xC6, 0x3A, 0xB3, 0x97, 0x6B, 0x5C,
	0x8C, 0xAA, 0xF8, 0x53, 0x95, 0x49, 0xF5, 0x92, 0x1C, 0x0B, 0x70, 0x0D, 0x5B, 0x0F, 0x83, 0xC4,
	0xE7, 0xE9, 0x46, 0x06, 0x8B, 0xAA, 0xAB, 0x54, 0x63, 0x54, 0x4D, 0xB1, 0x8C, 0x63, 0x80, 0x11,
	0x18, 0xF2, 0x18, 0x2E, 0xFC, 0xC8, 0xA1, 0xE8, 0x5E, 0x53, 0xC2, 0xA7, 0xAE, 0x83, 0x9A, 0x5C,
	0x6A, 0x3C, 0xAB, 0xE7, 0x37, 0x62, 0xB7, 0x0D, 0x17, 0x0A, 0xB6, 0x4A, 0xFC, 0x6C, 0xA4, 0x82,
	0x94, 0x49, 0x02, 0x61, 0x1F, 0xB0, 0x06, 0x1E, 0x09, 0xA6, 0x7A, 0xCB, 0x77, 0xE4, 0x93, 0xD9,
	0x98, 0xA0, 0xCC, 0xF9, 0x3D, 0x81, 0xA4, 0xF6, 0xC0, 0xDC, 0x6B, 0x7D, 0xF2, 0x2E, 0x62, 0xDB,
};
static const uint8_t amex_C9_exponent[] = { 0x03 };
static const uint8_t amex_C9_hash[] = {
	0x8E, 0x8D, 0xFF, 0x44, 0x3D, 0x78, 0xCD, 0x91, 0xDE, 0x88, 0x82, 0x1D, 0x70, 0xC9, 0x8F, 0x06,
	0x38, 0xE5, 0x1E, 0x49,
};

static const uint8_t amex_CA_modulus[] = {
	0xC2, 0x3E, 0xCB, 0xD7, 0x11, 0x9F, 0x47, 0x9C, 0x2E, 0xE5, 0x46, 0xC1, 0x23, 0xA5, 0x85, 0xD6,
	0x97, 0xA7, 0xD1, 0x0B, 0x55, 0xC2, 0xD2, 0x8B, 0xEF, 0x0D, 0x29, 0x9C, 0x01, 0xDC, 0x65, 0x42,
	0x0A, 0x03, 0xFE, 0x52, 0x27, 0xEC, 0xDE, 0xCB, 0x80, 0x25, 0xFB, 0xC8, 0x6E, 0xEB, 0xC1, 0x93,
	0x52, 0x98, 0xC1, 0x75, 0x3A, 0xB8, 0x49, 0x93, 0x67, 0x49, 0x71, 0x95, 0x91, 0x75, 0x8C, 0x31,
	0x5F, 0xA1, 0x50, 0x40, 0x07, 0x89, 0xBB, 0x14, 0xFA, 0xDD, 0x6E, 0xAE, 0x2A, 0xD6, 0x17, 0xDA,
	0x38, 0x16, 0x31, 0x99, 0xD1, 0xBA, 0xD5, 0xD3, 0xF8, 0xF6, 0xA7, 0xA2, 0x0A, 0xEF, 0x42, 0x0A,
	0xDF, 0xE2, 0x40, 0x4D, 0x30, 0xB2, 0x19, 0x35, 0x9C, 0x6A, 0x49, 0x52, 0x56, 0x5C, 0xCC, 0xA6,
	0xF1, 0x1E, 0xC5, 0xBE, 0x56, 0x4B, 0x49, 0xB0, 0xEA, 0x5B, 0xF5, 0xB3, 0xDC, 0x8C, 0x5C, 0x64,
	0x01, 0x20, 0x8D, 0x00, 0x29, 0xC3, 0x95, 0x7A, 0x8C, 0x59, 0x22, 0xCB, 0xDE, 0x39, 0xD3, 0xA5,
	0x64, 0xC6, 0xDE, 0xBB, 0x6B, 0xD2, 0xAE, 0xF9, 0x1F, 0xC2, 0x7B, 0xB3, 0xD3, 0x89, 0x2B, 0xEB,
	0x96, 0x46, 0xDC, 0xE2, 0xE1, 0xEF, 0x85, 0x81, 0xEF, 0xFA, 0x71, 0x21, 0x58, 0xAA, 0xEC, 0x54,
	0x1C, 0x0B, 0xBB, 0x4B, 0x3E, 0x27, 0x9D, 0x7D, 0xA5, 0x4E, 0x45, 0xA0, 0xAC, 0xC3, 0x57, 0x0E,
	0x71, 0x2C, 0x9F, 0x7C, 0xDF, 0x98, 0x5C, 0xFA, 0xFD, 0x38, 0x2A, 0xE1, 0x3A, 0x3B, 0x21, 0x4A,
	0x9E, 0x8E, 0x1E, 0x71, 0xAB, 0x1E, 0xA7, 0x07, 0x89, 0x51, 0x12, 0xAB, 0xC3, 0xA9, 0x7D, 0x0F,
	0xCB, 0x0A, 0xE2, 0xEE, 0x5C, 0x85, 0x49, 0x2B, 0x6C, 0xFD, 0x54, 0x88, 0x5C, 0xDD, 0x63, 0x37,
	0xE8, 0x95, 0xCC, 0x70, 0xFB, 0x32, 0x55, 0xE3,
};
static const uint8_t amex_CA_exponent[] = { 0x03 };
static const uint8_t amex_CA_hash[] = {
	0x6B, 0xDA, 0x32, 0xB1, 0xAA, 0x17, 0x14, 0x44, 0xC7, 0xE8, 0xF8, 0x80, 0x75, 0xA7, 0x4F, 0xBF,
	0xE8, 0x45, 0x76, 0x5F,
};

static const struct emv_capk_t capk_list[] = {
	// Visa CAPKs (test)
	// RID: A000000003
	CAPK_INIT(visa, 92),
	CAPK_INIT(visa, 94),
	CAPK_INIT(visa, 95),
	CAPK_INIT(visa, 99),

	// Mastercard CAPKs (test)
	// RID: A000000004
	CAPK_INIT(mastercard, EF),
	CAPK_INIT(mastercard, F1),
	CAPK_INIT(mastercard, F3),
	CAPK_INIT(mastercard, F4),
	CAPK_INIT(mastercard, F5),
	CAPK_INIT(mastercard, F8),
	CAPK_INIT(mastercard, FA),
	CAPK_INIT(mastercard, FE),

	// Amex CAPKs (test)
	// RID: A000000025
	CAPK_INIT(amex, 04),
	CAPK_INIT(amex, 65),
	CAPK_INIT(amex, A1),
	CAPK_INIT(amex, C8),
	CAPK_INIT(amex, C9),
	CAPK_INIT(amex, CA),
};

const struct emv_capk_t* emv_capk_lookup(const uint8_t* rid, uint8_t index)
{
	for (size_t i = 0; i < sizeof(capk_list) / sizeof(capk_list[0]); ++i) {
		if (index == capk_list[i].index &&
			memcmp(rid, capk_list[i].rid, EMV_CAPK_RID_LEN) == 0
		) {
			return &capk_list[i];
		}
	}

	return NULL;
}

int emv_capk_itr_init(struct emv_capk_itr_t* itr)
{
	if (!itr) {
		return -1;
	}

	memset(itr, 0, sizeof(*itr));
	return 0;
}

const struct emv_capk_t* emv_capk_itr_next(struct emv_capk_itr_t* itr)
{
	const struct emv_capk_t* capk;

	if (!itr) {
		return NULL;
	}
	if (itr->idx >= sizeof(capk_list) / sizeof(capk_list[0])) {
		return NULL;
	}

	capk = &capk_list[itr->idx];
	itr->idx++;
	return capk;
}
