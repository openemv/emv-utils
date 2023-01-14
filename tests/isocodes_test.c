/**
 * @file isocodes_test.c
 * @brief Various tests related to iso-codes package
 *
 * Copyright (c) 2021, 2023 Leon Lynch
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

#ifdef HAVE_LIBINTL_H
#include <locale.h>
#include <libintl.h>
#endif

int main(void)
{
	int r;
	const char* country;
	const char* currency;
	const char* language;

#ifdef HAVE_LIBINTL_H
	setlocale(LC_ALL, "nl_NL.UTF-8");
	printf("%s\n", dgettext("iso_639-2", "French"));
	printf("%s\n", dgettext("iso_3166-1", "France"));
	printf("%s\n", dgettext("iso_3166-3", "710")); // Cannot resolve numeric codes via libintl
#endif

	r = isocodes_init(NULL);
	if (r) {
		fprintf(stderr, "isocodes_init() failed; r=%d\n", r);
		return 1;
	}

	country = isocodes_lookup_country_by_alpha2("NL");
	if (!country) {
		fprintf(stderr, "isocodes_lookup_country_by_alpha2() failed\n");
		return 1;
	}
	if (strcmp(country, "Netherlands") != 0) {
		fprintf(stderr, "isocodes_lookup_country_by_alpha2() found unexpected country '%s'\n", country);
		return 1;
	}

	country = isocodes_lookup_country_by_alpha3("NLD");
	if (!country) {
		fprintf(stderr, "isocodes_lookup_country_by_alpha3() failed\n");
		return 1;
	}
	if (strcmp(country, "Netherlands") != 0) {
		fprintf(stderr, "isocodes_lookup_country_by_alpha3() found unexpected country '%s'\n", country);
		return 1;
	}

	country = isocodes_lookup_country_by_numeric(528);
	if (!country) {
		fprintf(stderr, "isocodes_lookup_country_by_numeric() failed\n");
		return 1;
	}
	if (strcmp(country, "Netherlands") != 0) {
		fprintf(stderr, "isocodes_lookup_country_by_numeric() found unexpected country '%s'\n", country);
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

	language = isocodes_lookup_language_by_alpha2("fr");
	if (!language) {
		fprintf(stderr, "isocodes_lookup_language_by_alpha2() failed\n");
		return 1;
	}
	if (strcmp(language, "French") != 0) {
		fprintf(stderr, "isocodes_lookup_language_by_alpha2() found unexpected language '%s'\n", language);
		return 1;
	}

	language = isocodes_lookup_language_by_alpha3("frr");
	if (!language) {
		fprintf(stderr, "isocodes_lookup_language_by_alpha3() failed\n");
		return 1;
	}
	if (strcmp(language, "Northern Frisian") != 0) {
		fprintf(stderr, "isocodes_lookup_language_by_alpha3() found unexpected language '%s'\n", language);
		return 1;
	}

	return 0;
}
