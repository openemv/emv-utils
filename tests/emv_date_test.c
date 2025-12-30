/**
 * @file emv_date_test.c
 * @brief Unit tests for EMV helper functions related to validation of dates
 *
 * Copyright 2025 Leon Lynch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "emv_date.h"
#include "emv_tlv.h"
#include "emv_tags.h"

#include <stdint.h>
#include <stdio.h>

int main(void)
{
	bool expired;
	bool not_effective;
	uint8_t mmyy[2] = { 0x04, 0x22 };
	struct emv_tlv_t txn_date = {
		{ {
			EMV_TAG_9A_TRANSACTION_DATE,
			3,
			(uint8_t[]){ 0x21, 0x05, 0x01 },
			0
		} },
		NULL
	};
	struct emv_tlv_t effective_date = {
		{ {
			EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE,
			3,
			(uint8_t[]){ 0x21, 0x05, 0x01 },
			0
		} },
		NULL
	};
	struct emv_tlv_t expiration_date = {
		{ {
			EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE,
			3,
			(uint8_t[]){ 0x21, 0x05, 0x01 },
			0
		} },
		NULL
	};

	// Test NULL parameters
	expired = emv_date_mmyy_is_expired(NULL, mmyy);
	printf("emv_date_mmyy_is_expired(NULL, %02X/%02X)=%u\n",
		mmyy[0], mmyy[1],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}
	expired = emv_date_mmyy_is_expired(&txn_date, NULL);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, NULL)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test zero dates
	txn_date.value = (uint8_t[]){ 0x21, 0x00, 0x01 };
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}
	txn_date.value = (uint8_t[]){ 0x21, 0x05, 0x01 };
	mmyy[0] = 0;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test invalid transaction date
	txn_date.value = (uint8_t[]){ 0x21, 0x13, 0x01 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x22;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test MMYY expired in previous year
	txn_date.value = (uint8_t[]){ 0x23, 0x05, 0x01 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x22;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test MMYY expires in next year
	txn_date.value = (uint8_t[]){ 0x21, 0x05, 0x01 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x22;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test MMYY expires before Y2K
	txn_date.value = (uint8_t[]){ 0x00, 0x05, 0x01 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x99;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test MMYY expires after Y2K
	txn_date.value = (uint8_t[]){ 0x99, 0x05, 0x01 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x00;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(19%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test MMYY expired in previous month
	txn_date.value = (uint8_t[]){ 0x22, 0x05, 0x01 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x22;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test MMYY expires in next month
	txn_date.value = (uint8_t[]){ 0x22, 0x03, 0x31 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x22;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test MMYY expires in current month
	txn_date.value = (uint8_t[]){ 0x22, 0x04, 0x15 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x22;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test MMYY expires on next day
	txn_date.value = (uint8_t[]){ 0x22, 0x04, 0x29 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x22;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test MMYY expires on exact day
	txn_date.value = (uint8_t[]){ 0x22, 0x04, 0x30 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x22;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test MMYY expires before transaction date beyond month end
	txn_date.value = (uint8_t[]){ 0x22, 0x04, 0x31 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x22;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (expired) { // Because transaction date is considered to be month end
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test MMYY expires before invalid transaction date
	txn_date.value = (uint8_t[]){ 0x22, 0x04, 0x32 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x22;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test MMYY expires after invalid transaction date
	txn_date.value = (uint8_t[]){ 0x21, 0x04, 0x32 };
	mmyy[0] = 0x04;
	mmyy[1] = 0x22;
	expired = emv_date_mmyy_is_expired(&txn_date, mmyy);
	printf("emv_date_mmyy_is_expired(20%02X-%02X-%02X, %02X/%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		mmyy[0], mmyy[1],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test NULL parameters
	not_effective = emv_date_is_not_effective(NULL, &effective_date);
	printf("emv_date_is_not_effective(NULL, 20%02X-%02X-%02X)=%u\n",
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (!not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}
	not_effective = emv_date_is_not_effective(&txn_date, NULL);
	printf("emv_date_is_not_effective(20%02X-%02X-%02X, NULL)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		not_effective
	);
	if (!not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test zero dates
	txn_date.value = (uint8_t[]){ 0x21, 0x00, 0x01 };
	effective_date.value = (uint8_t[]){ 0x20, 0x05, 0x01 };
	not_effective = emv_date_is_not_effective(&txn_date, &effective_date);
	printf("emv_date_is_not_effective(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (!not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}
	txn_date.value = (uint8_t[]){ 0x21, 0x05, 0x01 };
	effective_date.value = (uint8_t[]){ 0x20, 0x05, 0x00 };
	not_effective = emv_date_is_not_effective(&txn_date, &effective_date);
	printf("emv_date_is_not_effective(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (!not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test invalid dates
	txn_date.value = (uint8_t[]){ 0x21, 0x13, 0x01 };
	effective_date.value = (uint8_t[]){ 0x20, 0x05, 0x01 };
	not_effective = emv_date_is_not_effective(&txn_date, &effective_date);
	printf("emv_date_is_not_effective(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (!not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}
	txn_date.value = (uint8_t[]){ 0x21, 0x12, 0x01 };
	effective_date.value = (uint8_t[]){ 0x20, 0x13, 0x01 };
	not_effective = emv_date_is_not_effective(&txn_date, &effective_date);
	printf("emv_date_is_not_effective(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (!not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test effective in previous year
	txn_date.value = (uint8_t[]){ 0x21, 0x12, 0x01 };
	effective_date.value = (uint8_t[]){ 0x20, 0x05, 0x01 };
	not_effective = emv_date_is_not_effective(&txn_date, &effective_date);
	printf("emv_date_is_not_effective(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test effective in next year
	txn_date.value = (uint8_t[]){ 0x21, 0x12, 0x01 };
	effective_date.value = (uint8_t[]){ 0x22, 0x05, 0x01 };
	not_effective = emv_date_is_not_effective(&txn_date, &effective_date);
	printf("emv_date_is_not_effective(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (!not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test effective before Y2K
	txn_date.value = (uint8_t[]){ 0x00, 0x05, 0x01 };
	effective_date.value = (uint8_t[]){ 0x99, 0x12, 0x31 };
	not_effective = emv_date_is_not_effective(&txn_date, &effective_date);
	printf("emv_date_is_not_effective(20%02X-%02X-%02X, 19%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test effective after Y2K
	txn_date.value = (uint8_t[]){ 0x99, 0x12, 0x31 };
	effective_date.value = (uint8_t[]){ 0x00, 0x05, 0x01 };
	not_effective = emv_date_is_not_effective(&txn_date, &effective_date);
	printf("emv_date_is_not_effective(19%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (!not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test effective in previous month
	txn_date.value = (uint8_t[]){ 0x22, 0x06, 0x01 };
	effective_date.value = (uint8_t[]){ 0x22, 0x05, 0x20 };
	not_effective = emv_date_is_not_effective(&txn_date, &effective_date);
	printf("emv_date_is_not_effective(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test effective in next month
	txn_date.value = (uint8_t[]){ 0x22, 0x08, 0x20 };
	effective_date.value = (uint8_t[]){ 0x22, 0x09, 0x05 };
	not_effective = emv_date_is_not_effective(&txn_date, &effective_date);
	printf("emv_date_is_not_effective(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (!not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test effective on next day
	txn_date.value = (uint8_t[]){ 0x22, 0x08, 0x04 };
	effective_date.value = (uint8_t[]){ 0x22, 0x08, 0x05 };
	not_effective = emv_date_is_not_effective(&txn_date, &effective_date);
	printf("emv_date_is_not_effective(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (!not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test effective on exact day
	txn_date.value = (uint8_t[]){ 0x22, 0x09, 0x13 };
	effective_date.value = (uint8_t[]){ 0x22, 0x09, 0x13 };
	not_effective = emv_date_is_not_effective(&txn_date, &effective_date);
	printf("emv_date_is_not_effective(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		effective_date.value[0], effective_date.value[1], effective_date.value[2],
		not_effective
	);
	if (not_effective) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test NULL parameters
	expired = emv_date_is_expired(NULL, &expiration_date);
	printf("emv_date_is_expired(NULL, 20%02X-%02X-%02X)=%u\n",
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}
	expired = emv_date_is_expired(&txn_date, NULL);
	printf("emv_date_is_expired(20%02X-%02X-%02X, NULL)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test zero dates
	txn_date.value = (uint8_t[]){ 0x20, 0x00, 0x01 };
	expiration_date.value = (uint8_t[]){ 0x21, 0x05, 0x01 };
	expired = emv_date_is_expired(&txn_date, &expiration_date);
	printf("emv_date_is_expired(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}
	txn_date.value = (uint8_t[]){ 0x20, 0x05, 0x01 };
	expiration_date.value = (uint8_t[]){ 0x21, 0x05, 0x00 };
	expired = emv_date_is_expired(&txn_date, &expiration_date);
	printf("emv_date_is_expired(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test invalid dates
	txn_date.value = (uint8_t[]){ 0x20, 0x13, 0x01 };
	expiration_date.value = (uint8_t[]){ 0x21, 0x05, 0x01 };
	expired = emv_date_is_expired(&txn_date, &expiration_date);
	printf("emv_date_is_expired(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}
	txn_date.value = (uint8_t[]){ 0x20, 0x12, 0x01 };
	expiration_date.value = (uint8_t[]){ 0x21, 0x13, 0x01 };
	expired = emv_date_is_expired(&txn_date, &expiration_date);
	printf("emv_date_is_expired(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test expires in previous year
	txn_date.value = (uint8_t[]){ 0x21, 0x12, 0x01 };
	expiration_date.value = (uint8_t[]){ 0x20, 0x05, 0x01 };
	expired = emv_date_is_expired(&txn_date, &expiration_date);
	printf("emv_date_is_expired(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test expires in next year
	txn_date.value = (uint8_t[]){ 0x21, 0x12, 0x01 };
	expiration_date.value = (uint8_t[]){ 0x22, 0x05, 0x01 };
	expired = emv_date_is_expired(&txn_date, &expiration_date);
	printf("emv_date_is_expired(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test expires before Y2K
	txn_date.value = (uint8_t[]){ 0x00, 0x05, 0x01 };
	expiration_date.value = (uint8_t[]){ 0x99, 0x12, 0x31 };
	expired = emv_date_is_expired(&txn_date, &expiration_date);
	printf("emv_date_is_expired(20%02X-%02X-%02X, 19%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test expires after Y2K
	txn_date.value = (uint8_t[]){ 0x99, 0x12, 0x31 };
	expiration_date.value = (uint8_t[]){ 0x00, 0x05, 0x01 };
	expired = emv_date_is_expired(&txn_date, &expiration_date);
	printf("emv_date_is_expired(19%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test expires in previous month
	txn_date.value = (uint8_t[]){ 0x22, 0x06, 0x01 };
	expiration_date.value = (uint8_t[]){ 0x22, 0x05, 0x20 };
	expired = emv_date_is_expired(&txn_date, &expiration_date);
	printf("emv_date_is_expired(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (!expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test expires in next month
	txn_date.value = (uint8_t[]){ 0x22, 0x08, 0x20 };
	expiration_date.value = (uint8_t[]){ 0x22, 0x09, 0x05 };
	expired = emv_date_is_expired(&txn_date, &expiration_date);
	printf("emv_date_is_expired(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test expires on next day
	txn_date.value = (uint8_t[]){ 0x22, 0x08, 0x04 };
	expiration_date.value = (uint8_t[]){ 0x22, 0x08, 0x05 };
	expired = emv_date_is_expired(&txn_date, &expiration_date);
	printf("emv_date_is_expired(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	// Test expires on exact day
	txn_date.value = (uint8_t[]){ 0x22, 0x09, 0x13 };
	expiration_date.value = (uint8_t[]){ 0x22, 0x09, 0x13 };
	expired = emv_date_is_expired(&txn_date, &expiration_date);
	printf("emv_date_is_expired(20%02X-%02X-%02X, 20%02X-%02X-%02X)=%u\n",
		txn_date.value[0], txn_date.value[1], txn_date.value[2],
		expiration_date.value[0], expiration_date.value[1], expiration_date.value[2],
		expired
	);
	if (expired) {
		fprintf(stderr, "Failed!\n");
		return 1;
	}

	printf("Success\n");
	return 0;
}
