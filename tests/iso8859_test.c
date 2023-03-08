/**
 * @file is8859_test.c
 * @brief Unit tests for ISO/IEC 8859 processing
 *
 * Copyright (c) 2023 Leon Lynch
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
	uint8_t iso8859[96];
	size_t iso8859_ccs_len = 0;
	char utf8[sizeof(iso8859) * 4];

	// Populate ISO 8859 input string using the common character set
	iso8859_ccs_len = 95; // 95 characters in common character set
	for (size_t i = 0; i < iso8859_ccs_len; ++i) {
		iso8859[i] = 0x20 + i; // Common character set starts at 0x20
	}

	// Test ISO 8859 conversion of the common character set
	for (unsigned int codepage = 1; codepage <= 15; ++codepage) {
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

	// Populate ISO 8859 input string using the higher non-control characters
	for (size_t i = 0; i < sizeof(iso8859); ++i) {
		iso8859[i] = 0xA0 + i; // Interesting characters start at 0xA0
	}

	// Test ISO 8859 conversion of the higher non-control characters
	for (unsigned int codepage = 1; codepage <= 15; ++codepage) {
		if (!iso8859_is_supported(codepage)) {
				if (codepage == 12) {
				// ISO 8859-12 for Devanagari was officially abandoned in 1997
				continue;
			}

			fprintf(stderr, "iso8859_is_supported(%u) unexpectedly false\n", codepage);
			return 1;
		}

		memset(utf8, 0, sizeof(utf8));
		r = iso8859_to_utf8(codepage, iso8859, sizeof(iso8859), utf8, sizeof(utf8));
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

	printf("Success\n");
}
