/**
 * @file emv_date.h
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

#ifndef EMV_DATE_H
#define EMV_DATE_H

#include <sys/cdefs.h>
#include <stdbool.h>
#include <stdint.h>

__BEGIN_DECLS

// Forward declarations
struct emv_tlv_t;

/**
 * Determine whether a date in EMV format "n" with a layout of MMYY is expired
 * when compared to the Transaction Date (see @ref EMV_TAG_9A_TRANSACTION_DATE).
 *
 * @param txn_date Transaction Date (field 9A)
 * @param mmyy Date in EMV format "n" with a layout of MMYY. Must be 2 bytes.
 *
 * @return Boolean indicating whether date is expired.
 */
bool emv_date_mmyy_is_expired(
	const struct emv_tlv_t* txn_date,
	const uint8_t* mmyy
);

/**
 * Determine whether the Transaction Date (see @ref EMV_TAG_9A_TRANSACTION_DATE)
 * is less than the Application Effective Date (see
 * @ref EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE), therefore indicating that the
 * application is not yet effective.
 *
 * @param txn_date Transaction date (field 9A)
 * @param effective_date Application Effective Date (field 5F25)
 *
 * @return Boolean indicate whether application is not effective by date
 */
bool emv_date_is_not_effective(
	const struct emv_tlv_t* txn_date,
	const struct emv_tlv_t* effective_date
);

/**
 * Determine whether the Transaction Date (see @ref EMV_TAG_9A_TRANSACTION_DATE)
 * is greater than the Application Expiration Date (see
 * @ref EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE), therefore indicating that
 * the application is expired.
 *
 * @param txn_date Transaction Date (field 9A)
 * @param expiration_date Application Expiration Date (field 5F24)
 *
 * @return Boolean indicate whether application is expired by date
 */
bool emv_date_is_expired(
	const struct emv_tlv_t* txn_date,
	const struct emv_tlv_t* expiration_date
);

__END_DECLS

#endif
