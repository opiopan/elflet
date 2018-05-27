#pragma once

#include <stdint.h>
#include <mbedtls/error.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

class Digest {
public:
    virtual void update(const uint8_t* data, size_t len) = 0;
    virtual void finish() = 0;
    virtual mbedtls_md_type_t getAlgolithm() = 0;
    virtual const uint8_t* getHash() = 0;
    virtual size_t getHashLength() = 0;
};

class PK : private mbedtls_pk_context {
public:
    PK(){
	mbedtls_pk_init(this);
    };

    ~PK(){
	mbedtls_pk_free(this);
    };

    int parsePublicKeyfile(const char* keypath){
	return mbedtls_pk_parse_public_keyfile(this, keypath);
    };

    int verify(Digest* digest, const uint8_t* sig, size_t sig_len){
	return mbedtls_pk_verify(this,
				 digest->getAlgolithm(),
				 digest->getHash(),
				 digest->getHashLength(),
				 sig, sig_len);
    };
};

class SHA256 : private mbedtls_sha256_context, public Digest {
private:
    uint8_t hash[32];
    
public:
    SHA256(){
	mbedtls_sha256_init(this);
	mbedtls_sha256_starts(this, 0);
    };

    ~SHA256(){
	mbedtls_sha256_free(this);
    };

    void update(const uint8_t* data, size_t len){
	mbedtls_sha256_update(this, data, len);
    };

    void finish(){
	mbedtls_sha256_finish(this, hash);
    };

    mbedtls_md_type_t getAlgolithm(){
	return MBEDTLS_MD_SHA256;
    }
    
    const uint8_t* getHash(){
	return hash;
    };

    size_t getHashLength(){
	return sizeof(hash);
    };
};
