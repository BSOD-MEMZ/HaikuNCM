#ifndef NCM_CRYPTO_H
#define NCM_CRYPTO_H

#include <string>

namespace ncm {

// Hex(lowercase) of MD5(input)
std::string Md5Hex(const std::string& input);

// Base64 of arbitrary bytes
std::string Base64Encode(const unsigned char* data, size_t length);

// AES-128-CBC with PKCS#7 padding, output base64
std::string AesCbcBase64(const std::string& plain, const std::string& key,
	const std::string& iv);

// AES-128-ECB with PKCS#7 padding, output uppercase hex
std::string AesEcbHexUpper(const std::string& plain, const std::string& key);

// Random string from [A-Za-z0-9]
std::string RandomKey(size_t length);

// Raw (no padding) RSA of the reversed secret key, 256 hex chars
std::string RsaNoPadHex(const std::string& reversedSecret);

struct WeapiForm {
	std::string params;
	std::string encSecKey;
};

// weapi: double AES-CBC + RSA protected key
WeapiForm Weapi(const std::string& jsonText);

// eapi: md5 digest + AES-128-ECB, returns uppercase hex for the "params" field
std::string EapiParams(const std::string& path, const std::string& jsonText);

} // namespace ncm

#endif
