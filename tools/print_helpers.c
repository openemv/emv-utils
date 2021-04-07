/**
 * @file print_helpers.c
 * @brief Helper functions for command line output
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

#include "print_helpers.h"

#include "iso7816.h"
#include "iso7816_compact_tlv.h"
#include "iso7816_strings.h"

#include <stdio.h>

void print_buf(const char* buf_name, const void* buf, size_t length)
{
	const uint8_t* ptr = buf;
	printf("%s: ", buf_name);
	for (size_t i = 0; i < length; i++) {
		printf("%02X", ptr[i]);
	}
	printf("\n");
}

void print_atr(const struct iso7816_atr_info_t* atr_info)
{
	char str[1024];

	print_buf("ATR", atr_info->atr, atr_info->atr_len);

	// Print ATR info
	printf("  TS  = 0x%02X: %s\n", atr_info->TS, iso7816_atr_TS_get_string(atr_info));
	printf("  T0  = 0x%02X: %s\n", atr_info->T0, iso7816_atr_T0_get_string(atr_info, str, sizeof(str)));
	for (size_t i = 1; i < 5; ++i) {
		if (atr_info->TA[i] ||
			atr_info->TB[i] ||
			atr_info->TC[i] ||
			atr_info->TD[i] ||
			i < 3
		) {
			printf("  ----\n");
		}

		// Print TAi
		if (atr_info->TA[i]) {
			printf("  TA%zu = 0x%02X: %s\n", i, *atr_info->TA[i],
				iso7816_atr_TAi_get_string(atr_info, i, str, sizeof(str))
			);
		} else if (i < 3) {
			printf("  TA%zu absent: %s\n", i,
				iso7816_atr_TAi_get_string(atr_info, i, str, sizeof(str))
			);
		}

		// Print TBi
		if (atr_info->TB[i]) {
			printf("  TB%zu = 0x%02X: %s\n", i, *atr_info->TB[i],
				iso7816_atr_TBi_get_string(atr_info, i, str, sizeof(str))
			);
		} else if (i < 3) {
			printf("  TB%zu absent: %s\n", i,
				iso7816_atr_TBi_get_string(atr_info, i, str, sizeof(str))
			);
		}

		// Print TCi
		if (atr_info->TC[i]) {
			printf("  TC%zu = 0x%02X: %s\n", i, *atr_info->TC[i],
				iso7816_atr_TCi_get_string(atr_info, i, str, sizeof(str))
			);
		} else if (i < 3) {
			printf("  TC%zu absent: %s\n", i,
				iso7816_atr_TCi_get_string(atr_info, i, str, sizeof(str))
			);
		}

		if (atr_info->TD[i]) {
			printf("  TD%zu = 0x%02X: %s\n", i, *atr_info->TD[i],
				iso7816_atr_TDi_get_string(atr_info, i, str, sizeof(str))
			);
		}
	}
	if (atr_info->K_count) {
		printf("  ----\n");
		print_atr_historical_bytes(atr_info);

		if (atr_info->status_indicator_bytes) {
			printf("  ----\n");

			printf("  LCS = %02X: %s\n",
				atr_info->status_indicator.LCS,
				iso7816_lcs_get_string(atr_info->status_indicator.LCS)
			);

			if (atr_info->status_indicator.SW1 ||
				atr_info->status_indicator.SW2
			) {
				printf("  SW  = %02X%02X\n", atr_info->status_indicator.SW1, atr_info->status_indicator.SW2);
			}
		}
	}

	printf("  ----\n");
	printf("  TCK = 0x%02X\n", atr_info->TCK);
}

void print_atr_historical_bytes(const struct iso7816_atr_info_t* atr_info)
{
	int r;
	struct iso7816_compact_tlv_itr_t itr;
	struct iso7816_compact_tlv_t tlv;

	printf("  T1  = 0x%02X: %s\n", atr_info->T1,
		iso7816_atr_T1_get_string(atr_info)
	);

	r = iso7816_compact_tlv_itr_init(
		atr_info->historical_bytes,
		atr_info->historical_bytes_len,
		&itr
	);
	if (r) {
		printf("Failed to parse ATR historical bytes\n");
		return;
	}

	while ((r = iso7816_compact_tlv_itr_next(&itr, &tlv)) > 0) {
		printf("  %s: ", iso7816_compact_tlv_tag_get_string(tlv.tag));
		for (size_t i = 0; i < tlv.length; ++i) {
			printf("%s%02X", i ? " " : "", tlv.value[i]);
		}
		printf("\n");
	}
	if (r) {
		printf("Failed to parse ATR historical bytes\n");
		return;
	}
}
