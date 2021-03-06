/*
 * Copyright (C) 2011-2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
* File:
*     sgx_rsa_encryption.cpp
* Description:
*     Wrapper for rsa operation functions
*
*/
#include "util.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sgx_error.h"
#include "ipp_wrapper.h"
#include "sgx_trts.h"

#define RSA_SEED_SIZE_SHA256 32

sgx_status_t sgx_create_rsa_key_pair(int n_byte_size, int e_byte_size, unsigned char *p_n, unsigned char *p_d, unsigned char *p_e,
    unsigned char *p_p, unsigned char *p_q, unsigned char *p_dmp1,
    unsigned char *p_dmq1, unsigned char *p_iqmp)
{
    if (n_byte_size <= 0 || e_byte_size <= 0 || p_n == NULL || p_d == NULL || p_e == NULL ||
        p_p == NULL || p_q == NULL || p_dmp1 == NULL || p_dmq1 == NULL || p_iqmp == NULL) {
        return SGX_ERROR_INVALID_PARAMETER;
    }

    IppsRSAPrivateKeyState *p_pri_key = NULL;
	IppsRSAPublicKeyState *p_pub_key = NULL;
    IppStatus error_code = ippStsNoErr;
    sgx_status_t ret_code = SGX_ERROR_UNEXPECTED;
    IppsPRNGState *p_rand = NULL;
    IppsPrimeState *p_prime = NULL;
    Ipp8u * scratch_buffer = NULL;
    int pri_size = 0;
    IppsBigNumState *bn_n = NULL, *bn_e = NULL, *bn_d = NULL, *bn_e_s = NULL, *bn_p = NULL, *bn_q = NULL, *bn_dmp1 = NULL, *bn_dmq1 = NULL, *bn_iqmp = NULL;
    int validate_keys = IPP_IS_INVALID;
    int size;
    IppsBigNumSGN sgn;

    do {

        //create a new PRNG
        //
        error_code = newPRNG(&p_rand);
        ERROR_BREAK(error_code);
        
        //create a new prime number generator
        //
        error_code = newPrimeGen(n_byte_size * 8 / 2, &p_prime);
        ERROR_BREAK(error_code);
        
        //allocate and init private key of type 2
        //
        error_code = ippsRSA_GetSizePrivateKeyType2(n_byte_size / 2 * 8, n_byte_size / 2 * 8, &pri_size);
        ERROR_BREAK(error_code);
        p_pri_key = (IppsRSAPrivateKeyState *)malloc(pri_size);
        NULL_BREAK(p_pri_key);
        error_code = ippsRSA_InitPrivateKeyType2(n_byte_size / 2 * 8, n_byte_size / 2 * 8, p_pri_key, pri_size);
        ERROR_BREAK(error_code);
        
        //allocate scratch buffer, to be used as temp buffer
        //
        scratch_buffer = (Ipp8u *)malloc(pri_size);
        NULL_BREAK(scratch_buffer);
        memset(scratch_buffer, 0, pri_size);

        //allocate and initialize RSA BNs
        //
        error_code = newBN((const Ipp32u*)p_e, e_byte_size, &bn_e_s);
        ERROR_BREAK(error_code);
        error_code = newBN(NULL, n_byte_size, &bn_n);
        ERROR_BREAK(error_code);
        error_code = newBN(NULL, e_byte_size, &bn_e);
        ERROR_BREAK(error_code);
        error_code = newBN(NULL, n_byte_size, &bn_d);
        ERROR_BREAK(error_code);
        error_code = newBN(NULL, n_byte_size / 2, &bn_p);
        ERROR_BREAK(error_code);
        error_code = newBN(NULL, n_byte_size / 2, &bn_q);
        ERROR_BREAK(error_code);
        error_code = newBN(NULL, n_byte_size / 2, &bn_dmp1);
        ERROR_BREAK(error_code);
        error_code = newBN(NULL, n_byte_size / 2, &bn_dmq1);
        ERROR_BREAK(error_code);
        error_code = newBN(NULL, n_byte_size / 2, &bn_iqmp);
        ERROR_BREAK(error_code);

        //generate RSA key components with n_byte_size modulus and p_e public exponent
        //
        error_code = ippsRSA_GenerateKeys(bn_e_s,
            bn_n,
            bn_e,
            bn_d,
            p_pri_key,
            scratch_buffer,
            1,
            p_prime,
            ippsPRNGen,
            p_rand);
        ERROR_BREAK(error_code);

        //extract private key components into BNs
        //
        error_code = ippsRSA_GetPrivateKeyType2(bn_p,
            bn_q,
            bn_dmp1,
            bn_dmq1,
            bn_iqmp,
            p_pri_key);
        ERROR_BREAK(error_code);

        //allocate and initialize public key
        //
        error_code = ippsRSA_GetSizePublicKey(n_byte_size * 8, e_byte_size * 8, &pri_size);
        ERROR_BREAK(error_code);
        p_pub_key = (IppsRSAPublicKeyState *)malloc(pri_size);
        NULL_BREAK(p_pub_key);
        error_code = ippsRSA_InitPublicKey(n_byte_size * 8, e_byte_size * 8, p_pub_key, pri_size);
        ERROR_BREAK(error_code);
        error_code = ippsRSA_SetPublicKey(bn_n, bn_e, p_pub_key);
        ERROR_BREAK(error_code);
        
        //validate generated keys
        //
        ippsRSA_ValidateKeys(&validate_keys, p_pub_key, p_pri_key, NULL, scratch_buffer, 10, p_prime, ippsPRNGen, p_rand);
        if (validate_keys != IPP_IS_VALID) {
            break;
        }

        //extract RSA components from BNs into output buffers
        //
        error_code = ippsGetSize_BN(bn_n, &size);
        ERROR_BREAK(error_code);
        error_code = ippsGet_BN(&sgn, &size, (Ipp32u*)p_n, bn_n);
        ERROR_BREAK(error_code);
        error_code = ippsGetSize_BN(bn_e, &size);
        ERROR_BREAK(error_code);
        error_code = ippsGet_BN(&sgn, &size, (Ipp32u*)p_e, bn_e);
        ERROR_BREAK(error_code);
        error_code = ippsGetSize_BN(bn_d, &size);
        ERROR_BREAK(error_code);
        error_code = ippsGet_BN(&sgn, &size, (Ipp32u*)p_d, bn_d);
        ERROR_BREAK(error_code);
        error_code = ippsGetSize_BN(bn_p, &size);
        ERROR_BREAK(error_code);
        error_code = ippsGet_BN(&sgn, &size, (Ipp32u*)p_p, bn_p);
        ERROR_BREAK(error_code);
        error_code = ippsGetSize_BN(bn_q, &size);
        ERROR_BREAK(error_code);
        error_code = ippsGet_BN(&sgn, &size, (Ipp32u*)p_q, bn_q);
        ERROR_BREAK(error_code);
        error_code = ippsGetSize_BN(bn_dmp1, &size);
        ERROR_BREAK(error_code);
        error_code = ippsGet_BN(&sgn, &size, (Ipp32u*)p_dmp1, bn_dmp1);
        ERROR_BREAK(error_code);
        error_code = ippsGetSize_BN(bn_dmq1, &size);
        ERROR_BREAK(error_code);
        error_code = ippsGet_BN(&sgn, &size, (Ipp32u*)p_dmq1, bn_dmq1);
        ERROR_BREAK(error_code);
        error_code = ippsGetSize_BN(bn_iqmp, &size);
        ERROR_BREAK(error_code);
        error_code = ippsGet_BN(&sgn, &size, (Ipp32u*)p_iqmp, bn_iqmp);
        ERROR_BREAK(error_code);

        ret_code = SGX_SUCCESS;

    } while (0);

    secure_free_BN(bn_e_s, e_byte_size);
    secure_free_BN(bn_e, e_byte_size);
    secure_free_BN(bn_d, n_byte_size);
    secure_free_BN(bn_n, n_byte_size);
    secure_free_BN(bn_p, n_byte_size / 2);
    secure_free_BN(bn_q, n_byte_size / 2);
    secure_free_BN(bn_dmp1, n_byte_size / 2);
    secure_free_BN(bn_dmq1, n_byte_size / 2);
    secure_free_BN(bn_iqmp, n_byte_size / 2);

    SAFE_FREE_MM(p_rand);
    SAFE_FREE_MM(p_prime);
    secure_free_rsa_pri2_key(n_byte_size / 2, p_pri_key);
    secure_free_rsa_pub_key(n_byte_size, e_byte_size, p_pub_key);
    SAFE_FREE_MM(scratch_buffer);

    return ret_code;
}

sgx_status_t sgx_create_rsa_priv2_key(int prime_size, int exp_size, const unsigned char *g_rsa_key_e, const unsigned char *g_rsa_key_p, const unsigned char *g_rsa_key_q,
    const unsigned char *g_rsa_key_dmp1, const unsigned char *g_rsa_key_dmq1, const unsigned char *g_rsa_key_iqmp,
    void **new_pri_key2)
{
    (void)(exp_size);
    (void)(g_rsa_key_e);
    IppsRSAPrivateKeyState *p_rsa2 = NULL;
    IppsBigNumState *p_p = NULL, *p_q = NULL, *p_dmp1 = NULL, *p_dmq1 = NULL, *p_iqmp = NULL;
    int rsa2_size = 0;
    sgx_status_t ret_code = SGX_ERROR_UNEXPECTED;
    if (prime_size <= 0 || g_rsa_key_p == NULL || g_rsa_key_q == NULL || g_rsa_key_dmp1 == NULL || g_rsa_key_dmq1 == NULL || g_rsa_key_iqmp == NULL || new_pri_key2 == NULL) {
        return SGX_ERROR_INVALID_PARAMETER;
    }

    IppStatus error_code = ippStsNoErr;
    do {

        //generate and assign RSA components BNs
        //
        error_code = newBN((const Ipp32u*)g_rsa_key_p, prime_size / 2, &p_p);
        ERROR_BREAK(error_code);
        error_code = newBN((const Ipp32u*)g_rsa_key_q, prime_size / 2, &p_q);
        ERROR_BREAK(error_code);
        error_code = newBN((const Ipp32u*)g_rsa_key_dmp1, prime_size / 2, &p_dmp1);
        ERROR_BREAK(error_code);
        error_code = newBN((const Ipp32u*)g_rsa_key_dmq1, prime_size / 2, &p_dmq1);
        ERROR_BREAK(error_code);
        error_code = newBN((const Ipp32u*)g_rsa_key_iqmp, prime_size / 2, &p_iqmp);
        ERROR_BREAK(error_code);
        
        //allocate and initialize private key of type 2
        //
        error_code = ippsRSA_GetSizePrivateKeyType2(prime_size / 2 * 8, prime_size / 2 * 8, &rsa2_size);
        ERROR_BREAK(error_code);
        p_rsa2 = (IppsRSAPrivateKeyState *)malloc(rsa2_size);
        NULL_BREAK(p_rsa2);
        error_code = ippsRSA_InitPrivateKeyType2(prime_size / 2 * 8, prime_size / 2 * 8, p_rsa2, rsa2_size);
        ERROR_BREAK(error_code);
        
        //setup private key with values of input components
        //
        error_code = ippsRSA_SetPrivateKeyType2(p_p, p_q, p_dmp1, p_dmq1, p_iqmp, p_rsa2);
        ERROR_BREAK(error_code);
        *new_pri_key2 = (void*)p_rsa2;

        ret_code = SGX_SUCCESS;
    } while (0);

    secure_free_BN(p_p, prime_size / 2);
    secure_free_BN(p_q, prime_size / 2);
    secure_free_BN(p_dmp1, prime_size / 2);
    secure_free_BN(p_dmq1, prime_size / 2);
    secure_free_BN(p_iqmp, prime_size / 2);

    if (ret_code != SGX_SUCCESS) {
        secure_free_rsa_pri2_key(prime_size, p_rsa2);
    }
    return ret_code;
}

sgx_status_t sgx_create_rsa_pub1_key(int prime_size, int exp_size, const unsigned char *le_n, const unsigned char *le_e, void **new_pub_key1)
{
    if (new_pub_key1 == NULL || prime_size <= 0 || exp_size <= 0 || le_n == NULL || le_e == NULL) {
        return SGX_ERROR_INVALID_PARAMETER;
    }

    IppsRSAPublicKeyState *p_pub_key = NULL;
    IppsBigNumState *p_n = NULL, *p_e = NULL;
    int rsa_size = 0;
    sgx_status_t ret_code = SGX_ERROR_UNEXPECTED;
    IppStatus error_code = ippStsNoErr;
    do {

        //generate and assign RSA components BNs
        //
        error_code = newBN((const Ipp32u*)le_n, prime_size, &p_n);
        ERROR_BREAK(error_code);
        error_code = newBN((const Ipp32u*)le_e, exp_size, &p_e);
        ERROR_BREAK(error_code);

        //allocate and initialize public key
        //
        error_code = ippsRSA_GetSizePublicKey(prime_size * 8, exp_size * 8, &rsa_size);
        ERROR_BREAK(error_code);
        p_pub_key = (IppsRSAPublicKeyState *)malloc(rsa_size);
        NULL_BREAK(p_pub_key);
        error_code = ippsRSA_InitPublicKey(prime_size * 8, exp_size * 8, p_pub_key, rsa_size);
        ERROR_BREAK(error_code);

        //setup public key with values of input components
        //
        error_code = ippsRSA_SetPublicKey(p_n, p_e, p_pub_key);
        ERROR_BREAK(error_code);

        *new_pub_key1 = (void*)p_pub_key;
        ret_code = SGX_SUCCESS;
    } while (0);

    secure_free_BN(p_n, prime_size);
    secure_free_BN(p_e, exp_size);

    if (ret_code != SGX_SUCCESS) {
        secure_free_rsa_pub_key(prime_size, exp_size, p_pub_key);
    }

    return ret_code;
}

sgx_status_t sgx_rsa_pub_encrypt_sha256(void* rsa_key, unsigned char* pout_data, size_t* pout_len, const unsigned char* pin_data,
    const size_t pin_len) {
    (void)(pout_len);
    if (rsa_key == NULL || pin_data == NULL || pin_len < 1 || pin_len >= INT_MAX) {
        return SGX_ERROR_INVALID_PARAMETER;
    }

    uint8_t *p_scratch_buffer = NULL;
    Ipp8u seeds[RSA_SEED_SIZE_SHA256] = { 0 };
    int scratch_buff_size = 0;
    sgx_status_t ret_code = SGX_ERROR_UNEXPECTED;
    if (pout_data == NULL) {
        return SGX_SUCCESS;
    }

    do {
        
        //get scratch buffer size, to be used as temp buffer, and allocate it
        //
        if (ippsRSA_GetBufferSizePublicKey(&scratch_buff_size, (IppsRSAPublicKeyState*)rsa_key) != ippStsNoErr) {
            break;
        }
        p_scratch_buffer = (uint8_t *)malloc(8 * scratch_buff_size);
        NULL_BREAK(p_scratch_buffer);

        //get random seed
        //
        if (sgx_read_rand(seeds, RSA_SEED_SIZE_SHA256) != SGX_SUCCESS) {
            break;
        }

        //encrypt input data with public rsa_key and SHA256 padding
        //
        if (ippsRSAEncrypt_OAEP(pin_data, (int)pin_len, NULL, 0, seeds,
            pout_data, (IppsRSAPublicKeyState*)rsa_key, IPP_ALG_HASH_SHA256, p_scratch_buffer) != ippStsNoErr) {
            break;
        }

        ret_code = SGX_SUCCESS;
    } while (0);

    memset_s(seeds, RSA_SEED_SIZE_SHA256, 0, RSA_SEED_SIZE_SHA256);
    SAFE_FREE_MM(p_scratch_buffer);

    return ret_code;
}

sgx_status_t sgx_rsa_priv_decrypt_sha256(void* rsa_key, unsigned char* pout_data, size_t* pout_len, const unsigned char* pin_data,
    const size_t pin_len) {
    (void)(pin_len);
    if (rsa_key == NULL || pout_len == NULL || pin_data == NULL) {
        return SGX_ERROR_INVALID_PARAMETER;
    }
    sgx_status_t ret_code = SGX_ERROR_UNEXPECTED;
    uint8_t *p_scratch_buffer = NULL;
    int scratch_buff_size = 0;
    if (pout_data == NULL) {
        return SGX_SUCCESS;
    }
    do {

        //get scratch buffer size, to be used as temp buffer, and allocate it
        //
        if (ippsRSA_GetBufferSizePrivateKey(&scratch_buff_size, (IppsRSAPrivateKeyState*)rsa_key) != ippStsNoErr) {
            break;
        }
        p_scratch_buffer = (uint8_t *)malloc(scratch_buff_size);
        NULL_BREAK(p_scratch_buffer);

        //decrypt input ciphertext using private key rsa_key
        if (ippsRSADecrypt_OAEP(pin_data, NULL, 0, pout_data, (int*)pout_len, (IppsRSAPrivateKeyState*)rsa_key,
            IPP_ALG_HASH_SHA256, p_scratch_buffer) != ippStsNoErr) {
            break;
        }
        ret_code = SGX_SUCCESS;

    } while (0);
    SAFE_FREE_MM(p_scratch_buffer);

    return ret_code;
}

sgx_status_t sgx_create_rsa_priv1_key(int n_byte_size, int e_byte_size, int d_byte_size, const unsigned char *le_n, const unsigned char *le_e,
    const unsigned char *le_d, void **new_pri_key1)
{
    if (n_byte_size <= 0 || e_byte_size <= 0 || d_byte_size <= 0 || new_pri_key1 == NULL ||
        le_n == NULL || le_e == NULL || le_d == NULL) {
        return SGX_ERROR_INVALID_PARAMETER;
    }

    IppsRSAPrivateKeyState *p_rsa1 = NULL;
    IppsBigNumState *p_n = NULL, *p_d = NULL;
    int rsa1_size = 0;
    sgx_status_t ret_code = SGX_ERROR_UNEXPECTED;

    IppStatus error_code = ippStsNoErr;
    do {

        //generate and assign RSA components BNs
        //
        error_code = newBN((const Ipp32u*)le_n, n_byte_size, &p_n);
        ERROR_BREAK(error_code);
        error_code = newBN((const Ipp32u*)le_d, d_byte_size, &p_d);
        ERROR_BREAK(error_code);

        //allocate and init private key of type 1
        //
        error_code = ippsRSA_GetSizePrivateKeyType1(n_byte_size * 8, d_byte_size * 8, &rsa1_size);
        ERROR_BREAK(error_code);
        p_rsa1 = (IppsRSAPrivateKeyState *)malloc(rsa1_size);
        NULL_BREAK(p_rsa1);
        error_code = ippsRSA_InitPrivateKeyType1(n_byte_size * 8, d_byte_size * 8, p_rsa1, rsa1_size);
        ERROR_BREAK(error_code);
        
        //setup private key with values of input components
        //
        error_code = ippsRSA_SetPrivateKeyType1(p_n, p_d, p_rsa1);
        ERROR_BREAK(error_code);

        *new_pri_key1 = p_rsa1;
        ret_code = SGX_SUCCESS;

    } while (0);

    secure_free_BN(p_n, n_byte_size);
    secure_free_BN(p_d, d_byte_size);
    if (ret_code != SGX_SUCCESS) {
        secure_free_rsa_pri1_key(n_byte_size, d_byte_size, p_rsa1);
    }

    return ret_code;
}


sgx_status_t sgx_free_rsa_key(void *p_rsa_key, sgx_rsa_key_type_t key_type, int mod_size, int exp_size) {
	if (key_type == SGX_RSA_PRIVATE_KEY) {
		(void)(exp_size);
		secure_free_rsa_pri2_key(mod_size, (IppsRSAPrivateKeyState*)p_rsa_key);
	} else if (key_type == SGX_RSA_PUBLIC_KEY) {
		secure_free_rsa_pub_key(mod_size, exp_size, (IppsRSAPublicKeyState*)p_rsa_key);
	}

	return SGX_SUCCESS;
}


sgx_status_t sgx_calculate_ecdsa_priv_key(const unsigned char* hash_drg, int hash_drg_len,
    const unsigned char* sgx_nistp256_r_m1, int sgx_nistp256_r_m1_len,
    unsigned char* out_key, int out_key_len) {

    if (out_key == NULL || hash_drg_len <= 0 || sgx_nistp256_r_m1_len <= 0 ||
        out_key_len <= 0 || hash_drg == NULL || sgx_nistp256_r_m1 == NULL) {
        return SGX_ERROR_INVALID_PARAMETER;
    }

    sgx_status_t ret_code = SGX_ERROR_UNEXPECTED;
    IppStatus ipp_status = ippStsNoErr;
    IppsBigNumState *bn_d = NULL;
    IppsBigNumState *bn_m = NULL;
    IppsBigNumState *bn_o = NULL;
    IppsBigNumState *bn_one = NULL;
    Ipp32u i = 1;

    do {

        //allocate and initialize BNs
        //
        ipp_status = newBN(reinterpret_cast<const Ipp32u *>(hash_drg), hash_drg_len, &bn_d);
        ERROR_BREAK(ipp_status);
        
        //generate mod to be n-1 where n is order of ECC Group
        //
        ipp_status = newBN(reinterpret_cast<const Ipp32u *>(sgx_nistp256_r_m1), sgx_nistp256_r_m1_len, &bn_m);
        ERROR_BREAK(ipp_status);
        
        //allocate memory for output BN
        //
        ipp_status = newBN(NULL, sgx_nistp256_r_m1_len, &bn_o);
        ERROR_BREAK(ipp_status);
        
        //create big number with value of 1
        //
        ipp_status = newBN(&i, sizeof(Ipp32u), &bn_one);
        ERROR_BREAK(ipp_status);

        //calculate output's BN value
        ipp_status = ippsMod_BN(bn_d, bn_m, bn_o);
        ERROR_BREAK(ipp_status)

        //increase by 1
        //
        ipp_status = ippsAdd_BN(bn_o, bn_one, bn_o);
        ERROR_BREAK(ipp_status);

        /*Unmatched size*/
        if (sgx_nistp256_r_m1_len != sizeof(sgx_ec256_private_t)) {
            break;
        }

        //convert BN_o into octet string
        ipp_status = ippsGetOctString_BN(reinterpret_cast<Ipp8u *>(out_key), sgx_nistp256_r_m1_len, bn_o);//output data in bigendian order
        ERROR_BREAK(ipp_status);

        ret_code = SGX_SUCCESS;
    } while (0);

    if (NULL != bn_d) {
        secure_free_BN(bn_d, hash_drg_len);
    }
    if (NULL != bn_m) {
        secure_free_BN(bn_m, sgx_nistp256_r_m1_len);
    }
    if (NULL != bn_o) {
        secure_free_BN(bn_o, sgx_nistp256_r_m1_len);
    }
    if (NULL != bn_one) {
        secure_free_BN(bn_one, sizeof(uint32_t));
    }
    if (ret_code != SGX_SUCCESS) {
        (void)memset_s(out_key, out_key_len, 0, out_key_len);
    }

    return ret_code;
}
