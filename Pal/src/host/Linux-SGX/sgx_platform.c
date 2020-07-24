/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright (C) 2019, Texas A&M University.
 *               2020, Intel Labs.
 */

#include <asm/errno.h>
#include <linux/un.h>
#include <stdbool.h>

#include "gsgx.h"
#include "quote/aesm.pb-c.h"
#include "sgx_attest.h"
#include "sgx_internal.h"

#ifdef SGX_DCAP
/* hard-coded production attestation key of Intel reference QE (the only supported one) */
/* FIXME: should allow other attestation keys from non-Intel QEs */
static const sgx_ql_att_key_id_t g_default_ecdsa_p256_att_key_id = {
    .id               = 0,
    .version          = 0,
    .mrsigner_length  = 32,
    .mrsigner         = { 0x8c, 0x4f, 0x57, 0x75, 0xd7, 0x96, 0x50, 0x3e,
                          0x96, 0x13, 0x7f, 0x77, 0xc6, 0x8a, 0x82, 0x9a,
                          0x00, 0x56, 0xac, 0x8d, 0xed, 0x70, 0x14, 0x0b,
                          0x08, 0x1b, 0x09, 0x44, 0x90, 0xc5, 0x7b, 0xff,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    .prod_id          = 1,
    .extended_prod_id = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    .config_id        = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    .family_id        = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    .algorithm_id     = SGX_QL_ALG_ECDSA_P256,
};
#endif

/*
 * Connect to the AESM service to interact with the architectural enclave. Must reconnect
 * for each request to the AESM service.
 *
 * Some older versions of AESM service use a UNIX socket at "\0sgx_aesm_socket_base".
 * The latest AESM service binds the socket at "/var/run/aesmd/aesm.socket". This function
 * tries to connect to either of the paths to ensure connectivity.
 */
static int connect_aesm_service(void) {
    int sock = INLINE_SYSCALL(socket, 3, AF_UNIX, SOCK_STREAM, 0);
    if (IS_ERR(sock))
        return -ERRNO(sock);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void)strcpy_static(addr.sun_path, "\0sgx_aesm_socket_base", sizeof(addr.sun_path));

    int ret = INLINE_SYSCALL(connect, 3, sock, &addr, sizeof(addr));
    if (!IS_ERR(ret))
        return sock;
    if (ERRNO(ret) != ECONNREFUSED)
        goto err;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void)strcpy_static(addr.sun_path, "/var/run/aesmd/aesm.socket", sizeof(addr.sun_path));

    ret = INLINE_SYSCALL(connect, 3, sock, &addr, sizeof(addr));
    if (!IS_ERR(ret))
        return sock;

err:
    INLINE_SYSCALL(close, 1, sock);
    return -ERRNO(ret);
}

/*
 * A wrapper for both creating a connection to the AESM service and submitting a request
 * to the service. Upon success, the function returns a response from the AESM service
 * back to the caller.
 */
static int request_aesm_service(Request* req, Response** res) {
    int aesm_socket = connect_aesm_service();
    if (aesm_socket < 0)
        return aesm_socket;

    uint32_t req_len = (uint32_t) request__get_packed_size(req);
    uint8_t* req_buf = __alloca(req_len);
    request__pack(req, req_buf);

    int ret = INLINE_SYSCALL(write, 3, aesm_socket, &req_len, sizeof(req_len));
    if (IS_ERR(ret))
        goto err;

    ret = INLINE_SYSCALL(write, 3, aesm_socket, req_buf, req_len);
    if (IS_ERR(ret))
        goto err;

    uint32_t res_len;
    ret = INLINE_SYSCALL(read, 3, aesm_socket, &res_len, sizeof(res_len));
    if (IS_ERR(ret))
        goto err;

    uint8_t* res_buf = __alloca(res_len);
    ret = INLINE_SYSCALL(read, 3, aesm_socket, res_buf, res_len);
    if (IS_ERR(ret))
        goto err;

    *res = response__unpack(NULL, res_len, res_buf);
    ret = *res == NULL ? -EINVAL : 0;
err:
    INLINE_SYSCALL(close, 1, aesm_socket);
    return -ERRNO(ret);
}

int init_quoting_enclave_targetinfo(sgx_target_info_t* qe_target_info) {
	static sgx_target_info_t dummy = {0};
	memcpy(qe_target_info, &dummy, sizeof(*qe_target_info));
	return 0;
}

int retrieve_quote(const sgx_spid_t* spid, bool linkable, const sgx_report_t* report,
                   const sgx_quote_nonce_t* nonce, char** quote, size_t* quote_len) {
__UNUSED(spid);
__UNUSED(linkable);
__UNUSED(report);
__UNUSED(nonce);
static char dummy[256] = {0};
char* mmapped = (char*)INLINE_SYSCALL(mmap, 6, NULL, ALLOC_ALIGN_UP(sizeof(dummy)),
		  PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
memcpy(mmapped, &dummy, sizeof(dummy));
*quote = mmapped;
*quote_len = sizeof(dummy);
return 0;

}
