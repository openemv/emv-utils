/**
 * @file emv-dukpt.c
 * @brief Simple EMV encryption tool using DUKPT
 *
 * Copyright 2026 Leon Lynch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "emv.h"

#include <dukpt_tdes.h>
#include <dukpt_aes.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <argp.h>

// Helper functions
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state);

// argp option keys
enum emv_dukpt_param_t {
	EMV_DUKPT_VERSION = -255, // Negative value to avoid short options
};

// argp option structure
static struct argp_option argp_options[] = {
	{ "version", EMV_DUKPT_VERSION, NULL, 0, "Display emv-utils and dukpt versions" },

	{ 0 },
};

// argp configuration
static struct argp argp_config = {
	argp_options,
	argp_parser_helper,
	NULL,
	"Encrypt/decrypt EMV data using DUKPT",
};

// argp parser helper function
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state)
{
	switch (key) {
		case EMV_DUKPT_VERSION: {
			const char* version;
			const char* tdes_version;
			const char* aes_version;

			version = emv_lib_version_string();
			if (version) {
				printf("emv-utils %s\n", version);
			} else {
				printf("emv-utils Unknown\n");
			}

			// The dukpt project provides libdukpt_tdes and libdukpt_aes which
			// report the same project version. Report the dukpt project
			// version if the library versions match. Otherwise report the
			// library versions separately.
			tdes_version = dukpt_tdes_lib_version_string();
			aes_version = dukpt_aes_lib_version_string();
			if (tdes_version && aes_version &&
				strcmp(tdes_version, aes_version) == 0
			) {
				printf("dukpt %s\n", tdes_version);
			} else {
				printf("libdukpt_tdes %s\n", tdes_version ? tdes_version : "Unknown");
				printf("libdukpt_aes %s\n", aes_version ? aes_version : "Unknown");
			}

			exit(EXIT_SUCCESS);
			return 0;
		}

		default:
			return ARGP_ERR_UNKNOWN;
	}
}

int main(int argc, char** argv)
{
	int r;

	if (argc == 1) {
		// No command line arguments
		argp_help(&argp_config, stdout, ARGP_HELP_STD_HELP, argv[0]);
		return 1;
	}

	r = argp_parse(&argp_config, argc, argv, 0, 0, 0);
	if (r) {
		fprintf(stderr, "Failed to parse command line\n");
		return 1;
	}

	return 0;
}
