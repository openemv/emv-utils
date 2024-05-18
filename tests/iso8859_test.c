/**
 * @file is8859_test.c
 * @brief Unit tests for ISO/IEC 8859 processing
 *
 * Copyright 2023-2024 Leon Lynch
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

#include "iso8859.h"

#include <stdio.h>
#include <string.h>

#ifdef ISO8859_SIMPLE
const unsigned int codepage_max = 1;
#else
const unsigned int codepage_max = 15;
#endif

// WARNING: the UTF-8 strings below may contain characters that your editor
// cannot display!
static const char* utf8_verify[] = {
	"",
	u8" ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ",
	u8" Ą˘Ł¤ĽŚ§¨ŠŞŤŹ­ŽŻ°ą˛ł´ľśˇ¸šşťź˝žżŔÁÂĂÄĹĆÇČÉĘËĚÍÎĎĐŃŇÓÔŐÖ×ŘŮÚŰÜÝŢßŕáâăäĺćçčéęëěíîďđńňóôőö÷řůúűüýţ˙",
	u8" Ħ˘£¤Ĥ§¨İŞĞĴ­Ż°ħ²³´µĥ·¸ışğĵ½żÀÁÂÄĊĈÇÈÉÊËÌÍÎÏÑÒÓÔĠÖ×ĜÙÚÛÜŬŜßàáâäċĉçèéêëìíîïñòóôġö÷ĝùúûüŭŝ˙",
	u8" ĄĸŖ¤ĨĻ§¨ŠĒĢŦ­Ž¯°ą˛ŗ´ĩļˇ¸šēģŧŊžŋĀÁÂÃÄÅÆĮČÉĘËĖÍÎĪĐŅŌĶÔÕÖ×ØŲÚÛÜŨŪßāáâãäåæįčéęëėíîīđņōķôõö÷øųúûüũū˙",
	u8" ЁЂЃЄЅІЇЈЉЊЋЌ­ЎЏАБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдежзийклмнопрстуфхцчшщъыьэюя№ёђѓєѕіїјљњћќ§ўџ",
	u8" ¤،­؛؟ءآأؤإئابةتثجحخدذرزسشصضطظعغـفقكلمنهوىيًٌٍَُِّْ",
	u8" ‘’£€₯¦§¨©ͺ«¬­―°±²³΄΅Ά·ΈΉΊ»Ό½ΎΏΐΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩΪΫάέήίΰαβγδεζηθικλμνξοπρςστυφχψωϊϋόύώ",
	u8" ¢£¤¥¦§¨©×«¬­®¯°±²³´µ¶·¸¹÷»¼½¾‗אבגדהוזחטיךכלםמןנסעףפץצקרשת‎‏",
	u8" ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüışÿ",
	u8" ĄĒĢĪĨĶ§ĻĐŠŦŽ­ŪŊ°ąēģīĩķ·ļđšŧž―ūŋĀÁÂÃÄÅÆĮČÉĘËĖÍÎÏÐŅŌÓÔÕÖŨØŲÚÛÜÝÞßāáâãäåæįčéęëėíîïðņōóôõöũøųúûüýþĸ",
	u8" กขฃคฅฆงจฉชซฌญฎฏฐฑฒณดตถทธนบปผฝพฟภมยรฤลฦวศษสหฬอฮฯะัาำิีึืฺุู฿เแโใไๅๆ็่้๊๋์ํ๎๏๐๑๒๓๔๕๖๗๘๙๚๛",
	"", // ISO 8859-12 for Devanagari was officially abandoned in 1997
	u8" ”¢£¤„¦§Ø©Ŗ«¬­®Æ°±²³“µ¶·ø¹ŗ»¼½¾æĄĮĀĆÄÅĘĒČÉŹĖĢĶĪĻŠŃŅÓŌÕÖ×ŲŁŚŪÜŻŽßąįāćäåęēčéźėģķīļšńņóōõö÷ųłśūüżž’",
	u8" Ḃḃ£ĊċḊ§Ẁ©ẂḋỲ­®ŸḞḟĠġṀṁ¶ṖẁṗẃṠỳẄẅṡÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏŴÑÒÓÔÕÖṪØÙÚÛÜÝŶßàáâãäåæçèéêëìíîïŵñòóôõöṫøùúûüýŷÿ",
	u8" ¡¢£€¥Š§š©ª«¬­®¯°±²³Žµ¶·ž¹º»ŒœŸ¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ",
	//u8" ĄąŁ€„Š§š©Ș«Ź­źŻ°±ČłŽ”¶·žčș»ŒœŸżÀÁÂĂÄĆÆÇÈÉÊËÌÍÎÏĐŃÒÓÔŐÖŚŰÙÚÛÜĘȚßàáâăäćæçèéêëìíîïđńòóôőöśűùúûüęțÿ",
};

int main(void)
{
	int r;
	uint8_t iso8859[97]; // 96 characters plus null
	size_t iso8859_ccs_len;
	size_t iso8859_other_len;
	char utf8[sizeof(iso8859) * 4];

	// Populate ISO 8859 input string using the common character set
	iso8859_ccs_len = 95; // 95 characters in common character set
	for (size_t i = 0; i < iso8859_ccs_len; ++i) {
		iso8859[i] = 0x20 + i; // Common character set starts at 0x20
	}
	iso8859[iso8859_ccs_len] = 0; // Null terminate

	// Test ISO 8859 conversion of the common character set
	for (unsigned int codepage = 1; codepage <= codepage_max; ++codepage) {
		if (!iso8859_is_supported(codepage)) {
				if (codepage == 12) {
				// ISO 8859-12 for Devanagari was officially abandoned in 1997
				continue;
			}

			fprintf(stderr, "iso8859_is_supported(%u) unexpectedly false\n", codepage);
			return 1;
		}

		memset(utf8, 0, sizeof(utf8));
		r = iso8859_to_utf8(codepage, iso8859, iso8859_ccs_len, utf8, sizeof(utf8));
		if (r) {
			fprintf(stderr, "iso8859_to_utf8() failed; r=%d\n", r);
			return 1;
		}

		if (strncmp(utf8, (const char*)iso8859, iso8859_ccs_len) != 0) {
			fprintf(stderr, "ISO8859-%u: UTF-8 string verification failed()\n", codepage);

			return 1;
		}
	}

	// Test ISO 8859 conversion of the higher non-control characters
	for (unsigned int codepage = 1; codepage <= codepage_max; ++codepage) {
		if (!iso8859_is_supported(codepage)) {
				if (codepage == 12) {
				// ISO 8859-12 for Devanagari was officially abandoned in 1997
				continue;
			}

			fprintf(stderr, "iso8859_is_supported(%u) unexpectedly false\n", codepage);
			return 1;
		}

		// Populate ISO 8859 input string using the higher non-control characters
		// Interesting characters start at 0xA0
		iso8859_other_len = 0;
		for (size_t i = 0xA0; i <= 0xFF; ++i) {
			// Skip unassigned code points for each code page
			// See Wikipedia page on ISO 8859 for unassigned code points
			if (
				(codepage == 3 && (i == 0xA5 || i == 0xAE || i == 0xBE || i == 0xC3 || i == 0xD0 || i == 0xE3 || i == 0xF0)) ||
				(codepage == 6 && i != 0xA0 && i != 0xA4 && i != 0xAC && i != 0xAD && i != 0xBB && i != 0xBF && (i <= 0xC0 || (i >= 0xDB && i <= 0xDF) || i >= 0xF3)) ||
				(codepage == 7 && (i == 0xAE || i == 0xD2 || i == 0xFF)) ||
				(codepage == 8 && (i == 0xA1 || (i >= 0xBF && i <= 0xDE) || i == 0xFB || i == 0xFC || i == 0xFF)) ||
				(codepage == 11 && ((i >= 0xDB && i <= 0xDE) || i >= 0xFC))
			) {
				continue;
			}

			iso8859[iso8859_other_len++] = i;
		}
		iso8859[iso8859_other_len++] = 0; // Null terminate

		memset(utf8, 0, sizeof(utf8));
		r = iso8859_to_utf8(codepage, iso8859, iso8859_other_len, utf8, sizeof(utf8));
		if (r) {
			fprintf(stderr, "iso8859_to_utf8() failed; r=%d\n", r);
			return 1;
		}
		printf("ISO8859-%u:\t%s\n", codepage, utf8);

		if (strcmp(utf8, utf8_verify[codepage]) != 0) {
			fprintf(stderr, "UTF-8 string verification failed()\n");
			fprintf(stderr, "\t\t%s\n", utf8_verify[codepage]);

			fprintf(stderr, "\t\t");
			for (size_t j = 0; j < strlen(utf8); ++j) {
				fprintf(stderr, "%02X", (uint8_t)utf8[j]);
			}
			fprintf(stderr, "\n");
			fprintf(stderr, "\t\t");
			for (size_t j = 0; j < strlen(utf8_verify[codepage]); ++j) {
				fprintf(stderr, "%02X", (uint8_t)utf8_verify[codepage][j]);
			}
			fprintf(stderr, "\n");

			return 1;
		}
	}

	// Test ISO 8859 conversion of an empty string
	for (unsigned int codepage = 1; codepage <= codepage_max; ++codepage) {
		if (!iso8859_is_supported(codepage)) {
				if (codepage == 12) {
				// ISO 8859-12 for Devanagari was officially abandoned in 1997
				continue;
			}

			fprintf(stderr, "iso8859_is_supported(%u) unexpectedly false\n", codepage);
			return 1;
		}

		iso8859[0] = 0;
		memset(utf8, 0xFF, sizeof(utf8));
		r = iso8859_to_utf8(codepage, iso8859, 1, utf8, sizeof(utf8));
		if (r) {
			fprintf(stderr, "iso8859_to_utf8() failed; r=%d\n", r);
			return 1;
		}
		if (utf8[0] != 0) {
			fprintf(stderr, "Conversion of empty of size 1 failed\n");
			return 1;
		}
	}

	printf("Success\n");
}
