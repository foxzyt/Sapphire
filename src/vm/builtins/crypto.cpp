// crypto.cpp — Sapphire Crypto Module (v1.0.9)
// Provides cryptographic primitives using OpenSSL (already linked via OpenSSL::Crypto).
//
// Exposed as Sapphire global functions:
//   Crypto.sha256(str)                -> hex string
//   Crypto.sha1(str)                  -> hex string
//   Crypto.md5(str)                   -> hex string
//   Crypto.hmacSha256(key, data)      -> hex string
//   Crypto.base64Encode(str)          -> base64 string
//   Crypto.base64Decode(str)          -> decoded string
//   Crypto.randomBytes(n)             -> string of n raw bytes (crypto secure)
//   Crypto.randomHex(n)               -> hex string of n random bytes
//   Crypto.uuid4()                    -> UUID v4 string e.g. "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
//   Crypto.aesEncrypt(key, iv, data)  -> encrypted string (AES-256-CBC)
//   Crypto.aesDecrypt(key, iv, data)  -> decrypted string (AES-256-CBC)
#include "builtins.h"
#include "../object.h"
#include "../value.h"

#ifdef OPENSSL_FOUND
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#endif

#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <string>

#ifdef OPENSSL_FOUND
// ─────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────

static std::string bytes_to_hex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; i++) {
        oss << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    return oss.str();
}

static std::string evp_digest(const EVP_MD* type, const std::string& input) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    if (EVP_DigestInit_ex(ctx, type, nullptr) != 1 ||
        EVP_DigestUpdate(ctx, input.data(), input.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    EVP_MD_CTX_free(ctx);
    return bytes_to_hex(digest, digest_len);
}

// ─────────────────────────────────────────────
// Hash functions
// ─────────────────────────────────────────────

SapphireValue native_crypto_sha256(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string input = static_cast<ObjString*>(args[0].as.obj)->chars;
    return new_string(g_current_vm, evp_digest(EVP_sha256(), input));
}

SapphireValue native_crypto_sha1(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string input = static_cast<ObjString*>(args[0].as.obj)->chars;
    return new_string(g_current_vm, evp_digest(EVP_sha1(), input));
}

SapphireValue native_crypto_md5(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string input = static_cast<ObjString*>(args[0].as.obj)->chars;
    return new_string(g_current_vm, evp_digest(EVP_md5(), input));
}

// ─────────────────────────────────────────────
// HMAC
// ─────────────────────────────────────────────

SapphireValue native_crypto_hmac_sha256(int arg_count, SapphireValue* args) {
    if (arg_count != 2 || !is_obj_type(args[0], OBJ_STRING) || !is_obj_type(args[1], OBJ_STRING))
        return {};
    std::string key  = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string data = static_cast<ObjString*>(args[1].as.obj)->chars;

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  digest_len = 0;

    unsigned char* result = HMAC(
        EVP_sha256(),
        reinterpret_cast<const unsigned char*>(key.data()),
        static_cast<int>(key.size()),
        reinterpret_cast<const unsigned char*>(data.data()),
        static_cast<int>(data.size()),
        digest,
        &digest_len
    );

    if (!result) return {};
    return new_string(g_current_vm, bytes_to_hex(digest, digest_len));
}

// ─────────────────────────────────────────────
// Base64
// ─────────────────────────────────────────────

SapphireValue native_crypto_base64_encode(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string input = static_cast<ObjString*>(args[0].as.obj)->chars;

    BIO* b64  = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    b64 = BIO_push(b64, bmem);

    BIO_write(b64, input.data(), static_cast<int>(input.size()));
    BIO_flush(b64);

    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(b64, &bptr);

    std::string output(bptr->data, bptr->length);
    BIO_free_all(b64);
    return new_string(g_current_vm, output);
}

SapphireValue native_crypto_base64_decode(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    std::string input = static_cast<ObjString*>(args[0].as.obj)->chars;

    BIO* b64  = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new_mem_buf(input.data(), static_cast<int>(input.size()));
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    b64 = BIO_push(b64, bmem);

    std::vector<char> buf(input.size());
    int decoded_len = BIO_read(b64, buf.data(), static_cast<int>(buf.size()));
    BIO_free_all(b64);

    if (decoded_len < 0) return {};
    return new_string(g_current_vm, std::string(buf.data(), static_cast<size_t>(decoded_len)));
}

// ─────────────────────────────────────────────
// Random bytes / hex / UUID v4
// ─────────────────────────────────────────────

SapphireValue native_crypto_random_bytes(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_NUMBER) return {};
    int n = static_cast<int>(args[0].as.number);
    if (n <= 0 || n > 4096) return {};

    std::vector<unsigned char> buf(static_cast<size_t>(n));
    if (RAND_bytes(buf.data(), n) != 1) return {};

    return new_string(g_current_vm, std::string(reinterpret_cast<char*>(buf.data()), static_cast<size_t>(n)));
}

SapphireValue native_crypto_random_hex(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_NUMBER) return {};
    int n = static_cast<int>(args[0].as.number);
    if (n <= 0 || n > 4096) return {};

    std::vector<unsigned char> buf(static_cast<size_t>(n));
    if (RAND_bytes(buf.data(), n) != 1) return {};

    return new_string(g_current_vm, bytes_to_hex(buf.data(), static_cast<size_t>(n)));
}

SapphireValue native_crypto_uuid4(int arg_count, SapphireValue* args) {
    unsigned char b[16];
    if (RAND_bytes(b, 16) != 1) return {};

    // RFC 4122 version 4
    b[6] = (b[6] & 0x0F) | 0x40; // version 4
    b[8] = (b[8] & 0x3F) | 0x80; // variant bits

    char uuid[37];
    snprintf(uuid, sizeof(uuid),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        b[0],  b[1],  b[2],  b[3],
        b[4],  b[5],
        b[6],  b[7],
        b[8],  b[9],
        b[10], b[11], b[12], b[13], b[14], b[15]);

    return new_string(g_current_vm, std::string(uuid));
}

// ─────────────────────────────────────────────
// AES-256-CBC encryption / decryption
// ─────────────────────────────────────────────

SapphireValue native_crypto_aes_encrypt(int arg_count, SapphireValue* args) {
    if (arg_count != 3 ||
        !is_obj_type(args[0], OBJ_STRING) ||
        !is_obj_type(args[1], OBJ_STRING) ||
        !is_obj_type(args[2], OBJ_STRING)) return {};

    std::string key  = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string iv   = static_cast<ObjString*>(args[1].as.obj)->chars;
    std::string data = static_cast<ObjString*>(args[2].as.obj)->chars;

    // Pad key/iv to required lengths
    unsigned char k[32] = {0};
    unsigned char i[16] = {0};
    std::memcpy(k, key.data(), std::min(key.size(), sizeof(k)));
    std::memcpy(i, iv.data(),  std::min(iv.size(),  sizeof(i)));

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    std::vector<unsigned char> out(data.size() + 32);
    int out_len1 = 0, out_len2 = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, k, i) != 1 ||
        EVP_EncryptUpdate(ctx,
            out.data(), &out_len1,
            reinterpret_cast<const unsigned char*>(data.data()),
            static_cast<int>(data.size())) != 1 ||
        EVP_EncryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    EVP_CIPHER_CTX_free(ctx);

    return new_string(g_current_vm,
        std::string(reinterpret_cast<char*>(out.data()),
                    static_cast<size_t>(out_len1 + out_len2)));
}

SapphireValue native_crypto_aes_decrypt(int arg_count, SapphireValue* args) {
    if (arg_count != 3 ||
        !is_obj_type(args[0], OBJ_STRING) ||
        !is_obj_type(args[1], OBJ_STRING) ||
        !is_obj_type(args[2], OBJ_STRING)) return {};

    std::string key  = static_cast<ObjString*>(args[0].as.obj)->chars;
    std::string iv   = static_cast<ObjString*>(args[1].as.obj)->chars;
    std::string data = static_cast<ObjString*>(args[2].as.obj)->chars;

    unsigned char k[32] = {0};
    unsigned char i[16] = {0};
    std::memcpy(k, key.data(), std::min(key.size(), sizeof(k)));
    std::memcpy(i, iv.data(),  std::min(iv.size(),  sizeof(i)));

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    std::vector<unsigned char> out(data.size() + 16);
    int out_len1 = 0, out_len2 = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, k, i) != 1 ||
        EVP_DecryptUpdate(ctx,
            out.data(), &out_len1,
            reinterpret_cast<const unsigned char*>(data.data()),
            static_cast<int>(data.size())) != 1 ||
        EVP_DecryptFinal_ex(ctx, out.data() + out_len1, &out_len2) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    EVP_CIPHER_CTX_free(ctx);

    return new_string(g_current_vm,
        std::string(reinterpret_cast<char*>(out.data()),
                    static_cast<size_t>(out_len1 + out_len2)));
}

#endif // OPENSSL_FOUND

// ─────────────────────────────────────────────
// Stubs when OpenSSL is not available
// ─────────────────────────────────────────────

#ifndef OPENSSL_FOUND
SapphireValue native_crypto_sha256(int arg_count, SapphireValue* args) { return {}; }
SapphireValue native_crypto_sha1(int arg_count, SapphireValue* args) { return {}; }
SapphireValue native_crypto_md5(int arg_count, SapphireValue* args) { return {}; }
SapphireValue native_crypto_hmac_sha256(int arg_count, SapphireValue* args) { return {}; }
SapphireValue native_crypto_base64_encode(int arg_count, SapphireValue* args) { return {}; }
SapphireValue native_crypto_base64_decode(int arg_count, SapphireValue* args) { return {}; }
SapphireValue native_crypto_random_bytes(int arg_count, SapphireValue* args) { return {}; }
SapphireValue native_crypto_random_hex(int arg_count, SapphireValue* args) { return {}; }
SapphireValue native_crypto_uuid4(int arg_count, SapphireValue* args) { return {}; }
SapphireValue native_crypto_aes_encrypt(int arg_count, SapphireValue* args) { return {}; }
SapphireValue native_crypto_aes_decrypt(int arg_count, SapphireValue* args) { return {}; }
#endif

// ─────────────────────────────────────────────
// Module registration
// ─────────────────────────────────────────────

void register_crypto_module(VM* vm) {
#ifdef OPENSSL_FOUND
    vm->define_native("crypto_sha256", native_crypto_sha256);
    vm->define_native("crypto_sha1", native_crypto_sha1);
    vm->define_native("crypto_md5", native_crypto_md5);
    vm->define_native("crypto_hmac_sha256", native_crypto_hmac_sha256);
    vm->define_native("crypto_base64_encode", native_crypto_base64_encode);
    vm->define_native("crypto_base64_decode", native_crypto_base64_decode);
    vm->define_native("crypto_random_bytes", native_crypto_random_bytes);
    vm->define_native("crypto_random_hex", native_crypto_random_hex);
    vm->define_native("crypto_uuid4", native_crypto_uuid4);
    vm->define_native("crypto_aes_encrypt", native_crypto_aes_encrypt);
    vm->define_native("crypto_aes_decrypt", native_crypto_aes_decrypt);
#endif
}
