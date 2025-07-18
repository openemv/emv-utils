/**
 * @file emv_ttl.h
 * @brief EMV Terminal Transport Layer (TTL)
 *
 * Copyright 2021, 2024-2025 Leon Lynch
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

#ifndef EMV_TTL_H
#define EMV_TTL_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

/// Maximum length of C-APDU data field in bytes
#define EMV_CAPDU_DATA_MAX (255)

/// Maximum length of C-APDU buffer in bytes
#define EMV_CAPDU_MAX (6 + EMV_CAPDU_DATA_MAX)

/// Maximum length of R-APDU data field in bytes
#define EMV_RAPDU_DATA_MAX (256)

/// Maximum length of R-APDU buffer in bytes
#define EMV_RAPDU_MAX (EMV_RAPDU_DATA_MAX + 2)

/// Card reader mode
enum emv_cardreader_mode_t {
	EMV_CARDREADER_MODE_APDU = 1,               ///< Card reader is in APDU mode
	EMV_CARDREADER_MODE_TPDU,                   ///< Card reader is in TPDU mode
};

/// Card reader transceive function type
typedef int (*emv_cardreader_trx_t)(
	void* ctx,
	const void* tx_buf,
	size_t tx_buf_len,
	void* rx_buf,
	size_t* rx_buf_len
);

/**
 * EMV Terminal Transport Layer (TTL) abstraction for card reader
 * @note the card reader mode determines whether the @ref trx function
 * operates on TPDU frames or APDU frames. Typically PC/SC card readers use
 * APDU mode.
 */
struct emv_cardreader_t {
	enum emv_cardreader_mode_t mode;            ///< Card reader mode (TPDU vs APDU)
	void* ctx;                                  ///< Card reader transceive function context
	emv_cardreader_trx_t trx;                   ///< Card reader transceive function
};

/// EMV Terminal Transport Layer context
struct emv_ttl_t {
	struct emv_cardreader_t cardreader;
};

/**
 * @name Generate Application Cryptogram (GENAC) reference control parameter
 *       bit values
 * @remark See EMV 4.4 Book 3, 6.5.5.2, Table 12
 * @anchor emv-ttl-genac-ref-ctrl
 */
/// @{
#define EMV_TTL_GENAC_TYPE_MASK                 (0xC0) ///< Reference control parameter mask for Application Cryptogram (AC) type
#define EMV_TTL_GENAC_TYPE_AAC                  (0x00) ///< Application Cryptogram (AC) type: Application Authentication Cryptogram (AAC)
#define EMV_TTL_GENAC_TYPE_TC                   (0x40) ///< Application Cryptogram (AC) type: Transaction Certificate (TC)
#define EMV_TTL_GENAC_TYPE_ARQC                 (0x80) ///< Application Cryptogram (AC) type: Authorisation Request Cryptogram (ARQC)
#define EMV_TTL_GENAC_SIG_MASK                  (0x18) ///< Reference control parameter mask for requested signature
#define EMV_TTL_GENAC_SIG_NONE                  (0x00) ///< Requested signature: No CDA/XDA signature requested
#define EMV_TTL_GENAC_SIG_CDA                   (0x10) ///< Requested signature: CDA signature requested
#define EMV_TTL_GENAC_SIG_XDA                   (0x08) ///< Requested signature: XDA signature requested
/// @}

/**
 * EMV Terminal Transport Layer (TTL) transceive function for sending a
 * Command Application Protocol Data Unit (C-APDU) and receiving a
 * Response Application Protocol Data Unit (R-APDU)
 * @param ctx EMV Terminal Transport Layer context
 * @param c_apdu Command Application Protocol Data Unit (C-APDU) buffer
 * @param c_apdu_len Length of C-APDU buffer in bytes
 * @param r_apdu Response Application Protocol Data Unit (R-APDU) buffer
 * @param r_apdu_len Length of R-APDU buffer in bytes
 * @param sw1sw2 Status bytes (SW1-SW2) output in host endianness
 * @return Zero for success. Less than zero for error. Greater than zero for invalid reader response.
 */
int emv_ttl_trx(
	struct emv_ttl_t* ctx,
	const void* c_apdu,
	size_t c_apdu_len,
	void* r_apdu,
	size_t* r_apdu_len,
	uint16_t* sw1sw2
);

/**
 * SELECT (0xA4) the first or only application by Dedicated File (DF) name
 * and provide File Control Information (FCI) template.
 * @remark See EMV 4.4 Book 1, 11.3
 * @remark See ISO 7816-4:2005, 7.1.1
 *
 * @param ctx EMV Terminal Transport Layer context
 * @param df_name Dedicated File (DF) name
 * @param df_name_len Length of Dedicated File (DF) name in bytes, without NULL-termination
 * @param fci File Control Information (FCI) template output
 * @param fci_len Length of File Control Information (FCI) template in bytes
 * @param sw1sw2 Status bytes (SW1-SW2) output
 * @return Zero for success. Less than zero for error. Greater than zero for invalid reader response.
 */
int emv_ttl_select_by_df_name(
	struct emv_ttl_t* ctx,
	const void* df_name,
	size_t df_name_len,
	void* fci,
	size_t* fci_len,
	uint16_t* sw1sw2
);

/**
 * SELECT (0xA4) the next application by Dedicated File (DF) name and
 * provide File Control Information (FCI) template.
 * @remark See EMV 4.4 Book 1, 11.3
 * @remark See ISO 7816-4:2005, 7.1.1
 *
 * @param ctx EMV Terminal Transport Layer context
 * @param df_name Dedicated File (DF) name
 * @param df_name_len Length of Dedicated File (DF) name in bytes, without NULL-termination
 * @param fci File Control Information (FCI) template output
 * @param fci_len Length of File Control Information (FCI) template in bytes
 * @param sw1sw2 Status bytes (SW1-SW2) output
 * @return Zero for success. Less than zero for error. Greater than zero for invalid reader response.
 */
int emv_ttl_select_by_df_name_next(
	struct emv_ttl_t* ctx,
	const void* df_name,
	size_t df_name_len,
	void* fci,
	size_t* fci_len,
	uint16_t* sw1sw2
);

/**
 * READ RECORD (0xB2) from current file
 * @remark See EMV 4.4 Book 1, 11.2
 * @remark See ISO 7816-4:2005, 7.3.3
 *
 * @param ctx EMV Terminal Transport Layer context
 * @param sfi Short File Identifier (SFI) of elementary file (EF)
 * @param record_number Record number to read
 * @param data Record data output
 * @param data_len Length of record data in bytes
 * @param sw1sw2 Status bytes (SW1-SW2) output
 * @return Zero for success. Less than zero for error. Greater than zero for invalid reader response.
 */
int emv_ttl_read_record(
	struct emv_ttl_t* ctx,
	uint8_t sfi,
	uint8_t record_number,
	void* data,
	size_t* data_len,
	uint16_t* sw1sw2
);

/**
 * GET PROCESSING OPTIONS (0xA8) for current application
 * @remark EMV 4.4 Book 3, 6.5.8
 *
 * @param ctx EMV Terminal Transport Layer context
 * @param data Command Template (field 83) according to Processing Options Data Object List (PDOL). NULL if no PDOL.
 * @param data_len Length of Command Template (field 83) in bytes. Zero if no PDOL.
 * @param response Response output
 * @param response_len Length of response output in bytes
 * @param sw1sw2 Status bytes (SW1-SW2) output
 * @return Zero for success. Less than zero for error. Greater than zero for invalid reader response.
 */
int emv_ttl_get_processing_options(
	struct emv_ttl_t* ctx,
	const void* data,
	size_t data_len,
	void* response,
	size_t* response_len,
	uint16_t* sw1sw2
);

/**
 * GET DATA (0xCA) for current application
 * @remark EMV 4.4 Book 3, 6.5.7
 * @remark EMV 4.4 Book 3, 7.3
 *
 * @param ctx EMV Terminal Transport Layer context
 * @param p1p2 P1-P2 in host endianness for the requested tag
 * @param response Response output
 * @param response_len Length of response output in bytes
 * @param sw1sw2 Status bytes (SW1-SW2) output
 * @return Zero for success. Less than zero for error. Greater than zero for invalid reader response.
 */
int emv_ttl_get_data(
	struct emv_ttl_t* ctx,
	uint16_t p1p2,
	void* response,
	size_t* response_len,
	uint16_t* sw1sw2
);

/**
 * INTERNAL AUTHENTICATE (0x88) for current application
 * @remark EMV 4.4 Book 3, 6.5.9
 *
 * @param ctx EMV Terminal Transport Layer context
 * @param data Authentication-related data
 * @param data_len Length of authentication-related data in bytes
 * @param response Response output
 * @param response_len Length of response output in bytes
 * @param sw1sw2 Status bytes (SW1-SW2) output
 * @return Zero for success. Less than zero for error. Greater than zero for invalid reader response.
 */
int emv_ttl_internal_authenticate(
	struct emv_ttl_t* ctx,
	const void* data,
	size_t data_len,
	void* response,
	size_t* response_len,
	uint16_t* sw1sw2
);

/**
 * GENERATE APPLICATION CRYPTOGRAM (0xAE) for current application
 * @remark EMV 4.4 Book 3, 6.5.5
 *
 * @param ctx EMV Terminal Transport Layer context
 * @param ref_ctrl Reference control parameter. See @ref emv-ttl-genac-ref-ctrl "bit values".
 * @param data Authentication-related data
 * @param data_len Length of authentication-related data in bytes
 * @param response Response output
 * @param response_len Length of response output in bytes
 * @param sw1sw2 Status bytes (SW1-SW2) output
 * @return Zero for success. Less than zero for error. Greater than zero for invalid reader response.
 */
int emv_ttl_genac(
	struct emv_ttl_t* ctx,
	uint8_t ref_ctrl,
	const void* data,
	size_t data_len,
	void* response,
	size_t* response_len,
	uint16_t* sw1sw2
);

__END_DECLS

#endif
