/**
 * @file isocodes_test.c
 * @brief Various tests related to iso-codes package
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

#include "isocodes_lookup.h"

#include <stdio.h>
#include <string.h>

#include <locale.h>
#include <libintl.h>

int main(void)
{
	int r;
	const char* currency;

	setlocale(LC_ALL, "nl_NL.UTF-8");
	printf("%s\n", dgettext("iso_639-2", "French"));
	printf("%s\n", dgettext("iso_3166-1", "France"));
	printf("%s\n", dgettext("iso_3166-3", "710")); // Cannot resolve numeric codes via libintl

	r = isocodes_init();
	if (r) {
		fprintf(stderr, "isocodes_init() failed; r=%d\n", r);
		return 1;
	}

	currency = isocodes_lookup_currency_by_alpha3("EUR");
	if (!currency) {
		fprintf(stderr, "isocodes_lookup_currency_by_alpha3() failed\n");
		return 1;
	}
	if (strcmp(currency, "Euro") != 0) {
		fprintf(stderr, "isocodes_lookup_currency_by_alpha3() found unexpected currency '%s'\n", currency);
		return 1;
	}

	currency = isocodes_lookup_currency_by_numeric(978);
	if (!currency) {
		fprintf(stderr, "isocodes_lookup_currency_by_numeric() failed\n");
		return 1;
	}
	if (strcmp(currency, "Euro") != 0) {
		fprintf(stderr, "isocodes_lookup_currency_by_numeric() found unexpected currency '%s'\n", currency);
		return 1;
	}

	return 0;
}
