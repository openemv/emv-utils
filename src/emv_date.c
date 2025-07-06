/**
 * @file emv_date.c
 * @brief EMV helper functions for validation of dates
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

#include "emv_date.h"
#include "emv_tlv.h"

static inline uint8_t emv_date_format_n_to_uint8(uint8_t x)
{
	return (x >> 4) * 10 + (x & 0xF);
}

static bool emv_date_is_valid(const struct emv_tlv_t* date)
{
	return
		date &&
		date->length == 3 &&
		date->value &&
		(date->value[0] >> 4) <= 9 &&
		(date->value[0] & 0xF) <= 9 &&
		date->value[1] != 0 &&
		date->value[2] != 0 &&
		emv_date_format_n_to_uint8(date->value[1]) <= 12 &&
		emv_date_format_n_to_uint8(date->value[2]) <= 31;
}

static inline unsigned int emv_date_format_n_to_year(uint8_t yy)
{
	unsigned int year = emv_date_format_n_to_uint8(yy);

	// See EMV 4.4 Book 4, 6.7.3
	if (year < 50) {
		return 2000 + year;
	} else {
		return 1900 + year;
	}
}

bool emv_date_mmyy_is_expired(
	const struct emv_tlv_t* txn_date,
	const uint8_t* mmyy
)
{
	if (!emv_date_is_valid(txn_date)) {
		return true;
	}

	if (!mmyy || !mmyy[0]) {
		return true;
	}

	// If the years differ...
	if (emv_date_format_n_to_year(mmyy[1]) <
		emv_date_format_n_to_year(txn_date->value[0])
	) {
		return true;
	}
	if (emv_date_format_n_to_year(mmyy[1]) >
		emv_date_format_n_to_year(txn_date->value[0])
	) {
		return false;
	}

	// If the years are the same, but the months differ...
	if (mmyy[0] < txn_date->value[1]) {
		return true;
	}
	if (mmyy[0] > txn_date->value[1]) {
		return false;
	}

	// If the years and months are the same, then the last day of the month
	// specified by MMYY will always be equal or later than the current date,
	// and therefore not expired.
	return false;
}

bool emv_date_is_not_effective(
	const struct emv_tlv_t* txn_date,
	const struct emv_tlv_t* effective_date
)
{
	if (!emv_date_is_valid(txn_date)) {
		return true;
	}

	if (!emv_date_is_valid(effective_date)) {
		return true;
	}

	// If the years differ...
	if (emv_date_format_n_to_year(txn_date->value[0]) <
		emv_date_format_n_to_year(effective_date->value[0])
	) {
		return true;
	}
	if (emv_date_format_n_to_year(txn_date->value[0]) >
		emv_date_format_n_to_year(effective_date->value[0])
	) {
		return false;
	}

	// If the years are the same, but the months differ...
	if (txn_date->value[1] < effective_date->value[1]) {
		return true;
	}
	if (txn_date->value[1] > effective_date->value[1]) {
		return false;
	}

	// If the years and months are the same, compare the dates...
	if (txn_date->value[2] < effective_date->value[2]) {
		return true;
	} else {
		// If the dates are exactly the same, the application is effective
		return false;
	}
}

bool emv_date_is_expired(
	const struct emv_tlv_t* txn_date,
	const struct emv_tlv_t* expiration_date
)
{
	if (!emv_date_is_valid(txn_date)) {
		return true;
	}

	if (!emv_date_is_valid(expiration_date)) {
		return true;
	}

	// If the years differ...
	if (emv_date_format_n_to_year(txn_date->value[0]) >
		emv_date_format_n_to_year(expiration_date->value[0])
	) {
		return true;
	}
	if (emv_date_format_n_to_year(txn_date->value[0]) <
		emv_date_format_n_to_year(expiration_date->value[0])
	) {
		return false;
	}

	// If the years are the same, but the months differ...
	if (txn_date->value[1] > expiration_date->value[1]) {
		return true;
	}
	if (txn_date->value[1] < expiration_date->value[1]) {
		return false;
	}

	// If the years and months are the same, compare the dates...
	if (txn_date->value[2] > expiration_date->value[2]) {
		return true;
	} else {
		// If the dates are exactly the same, the application has not expired
		return false;
	}
}
