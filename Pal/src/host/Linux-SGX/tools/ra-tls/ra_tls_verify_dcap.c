/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright (C) 2018-2020 Intel Labs */

/*!
 * \file
 *
 * This file contains the implementation of verification callbacks for TLS libraries. The callbacks
 * verify the correctness of a self-signed RA-TLS certificate with an SGX quote embedded in it. The
 * callbacks call into the `libsgx_dcap_quoteverify` DCAP library for ECDSA-based verification. A
 * callback ra_tls_verify_callback() can be used directly in mbedTLS, and a more generic version
 * ra_tls_verify_callback_der() should be used for other TLS libraries.
 *
 * This file is part of the RA-TLS verification library which is typically linked into client
 * applications. This library is *not* thread-safe.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>

#include "attestation.h"
#include "ra_tls.h"
#include "sgx_arch.h"
#include "sgx_attest.h"
#include "util.h"

extern verify_measurements_cb_t g_verify_measurements_cb;

/* we cannot include libsgx_dcap_verify headers because they conflict with Graphene SGX headers,
 * so we declare the used types and functions below */

/* QL stands for Quoting Library; QV stands for Quote Verification */
#define SGX_QL_QV_MK_ERROR(x) (0x0000A000|(x))
typedef enum _sgx_ql_qv_result_t {
    /* quote verification passed and is at the latest TCB level */
    SGX_QL_QV_RESULT_OK = 0x0000,
    /* quote verification passed and the platform is patched to the latest TCB level but additional
     * configuration of the SGX platform may be needed */
    SGX_QL_QV_RESULT_CONFIG_NEEDED = SGX_QL_QV_MK_ERROR(0x0001),
    /* quote is good but TCB level of the platform is out of date; platform needs patching to be at
     * the latest TCB level */
    SGX_QL_QV_RESULT_OUT_OF_DATE = SGX_QL_QV_MK_ERROR(0x0002),
    /* quote is good but the TCB level of the platform is out of date and additional configuration
     * of the SGX platform at its current patching level may be needed; platform needs patching to
     * be at the latest TCB level */
    SGX_QL_QV_RESULT_OUT_OF_DATE_CONFIG_NEEDED = SGX_QL_QV_MK_ERROR(0x0003),
    /* signature over the application report is invalid */
    SGX_QL_QV_RESULT_INVALID_SIGNATURE = SGX_QL_QV_MK_ERROR(0x0004),
    /* attestation key or platform has been revoked */
    SGX_QL_QV_RESULT_REVOKED = SGX_QL_QV_MK_ERROR(0x0005),
    /* quote verification failed due to an error in one of the input */
    SGX_QL_QV_RESULT_UNSPECIFIED = SGX_QL_QV_MK_ERROR(0x0006),
    /* TCB level of the platform is up to date, but SGX SW hardening is needed */
    SGX_QL_QV_RESULT_SW_HARDENING_NEEDED = SGX_QL_QV_MK_ERROR(0x0007),
    /* TCB level of the platform is up to date, but additional configuration of the platform at its
     * current patching level may be needed; moreover, SGX SW hardening is also needed */
    SGX_QL_QV_RESULT_CONFIG_AND_SW_HARDENING_NEEDED = SGX_QL_QV_MK_ERROR(0x0008),
} sgx_ql_qv_result_t;

int sgx_qv_get_quote_supplemental_data_size(uint32_t* p_data_size);
int sgx_qv_verify_quote(const uint8_t* p_quote, uint32_t quote_size, void* p_quote_collateral,
                        const time_t expiration_check_date,
                        uint32_t* p_collateral_expiration_status,
                        sgx_ql_qv_result_t* p_quote_verification_result, void* p_qve_report_info,
                        uint32_t supplemental_data_size, uint8_t* p_supplemental_data);


int ra_tls_verify_callback(void* data, mbedtls_x509_crt* crt, int depth, uint32_t* flags) {
	*flags = 0;
	return 0;
}
