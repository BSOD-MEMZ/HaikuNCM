#include "crypto.h"

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/md5.h>

#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>

namespace ncm {

namespace {

const char* kPresetKey = "0CoJUm6Qyw8W8jud";
const char* kIv = "0102030405060708";
const char* kEapiKey = "e82ckenh8dichen8";

const char* kRsaModulus =
	"00e0b509f6259df8642dbc35662901477df22677ec152b5ff68ace615bb7b725152b3ab17a876aea8a5aa76d2e417629ec4"
	"ee341f56135fccf695280104e0312ecbda92557c93870114af6c9d05c4f7f0c3685b7a46bee255932575cce10b424d813c"
	"fe4875d3e82047b97ddef52741d546b8e289dc6935b3ece0462db0a22b8e7";

const long kRsaExponent = 65537;

std::string AesRaw(const std::string& plain, const std::string& key,
	const std::string& iv, bool cbc)
{
	if (key.size() != 16)
		throw std::runtime_error("AES-128 needs a 16 byte key");
	if (cbc && iv.size() != 16)
		throw std::runtime_error("AES-CBC needs a 16 byte iv");

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (ctx == nullptr)
		throw std::runtime_error("EVP_CIPHER_CTX_new failed");

	std::vector<unsigned char> out(plain.size() + 32);
	int produced = 0;
	int total = 0;

	const EVP_CIPHER* cipher = cbc ? EVP_aes_128_cbc() : EVP_aes_128_ecb();
	if (EVP_EncryptInit_ex(ctx, cipher, nullptr,
			reinterpret_cast<const unsigned char*>(key.data()),
			cbc ? reinterpret_cast<const unsigned char*>(iv.data()) : nullptr) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		throw std::runtime_error("EVP_EncryptInit_ex failed");
	}

	if (EVP_EncryptUpdate(ctx, out.data(), &produced,
			reinterpret_cast<const unsigned char*>(plain.data()),
			static_cast<int>(plain.size())) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		throw std::runtime_error("EVP_EncryptUpdate failed");
	}
	total = produced;

	if (EVP_EncryptFinal_ex(ctx, out.data() + total, &produced) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		throw std::runtime_error("EVP_EncryptFinal_ex failed");
	}
	total += produced;
	EVP_CIPHER_CTX_free(ctx);

	return std::string(reinterpret_cast<char*>(out.data()), total);
}

} // namespace

std::string Md5Hex(const std::string& input)
{
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int length = 0;

	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	if (ctx == nullptr)
		throw std::runtime_error("EVP_MD_CTX_new failed");

	std::string result;
	if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) == 1
		&& EVP_DigestUpdate(ctx, input.data(), input.size()) == 1
		&& EVP_DigestFinal_ex(ctx, digest, &length) == 1) {
		static const char* kHex = "0123456789abcdef";
		result.reserve(length * 2);
		for (unsigned int i = 0; i < length; i++) {
			result.push_back(kHex[digest[i] >> 4]);
			result.push_back(kHex[digest[i] & 0x0f]);
		}
	}
	EVP_MD_CTX_free(ctx);

	if (result.empty())
		throw std::runtime_error("MD5 failed");
	return result;
}

std::string Base64Encode(const unsigned char* data, size_t length)
{
	if (length == 0)
		return std::string();

	std::vector<unsigned char> buffer(4 * ((length + 2) / 3) + 1);
	int written = EVP_EncodeBlock(buffer.data(), data,
		static_cast<int>(length));
	if (written < 0)
		throw std::runtime_error("EVP_EncodeBlock failed");
	return std::string(reinterpret_cast<char*>(buffer.data()), written);
}

std::string AesCbcBase64(const std::string& plain, const std::string& key,
	const std::string& iv)
{
	const std::string raw = AesRaw(plain, key, iv, true);
	return Base64Encode(reinterpret_cast<const unsigned char*>(raw.data()),
		raw.size());
}

std::string AesEcbHexUpper(const std::string& plain, const std::string& key)
{
	const std::string raw = AesRaw(plain, key, std::string(), false);
	static const char* kHex = "0123456789ABCDEF";

	std::string result;
	result.reserve(raw.size() * 2);
	for (unsigned char byte : raw) {
		result.push_back(kHex[byte >> 4]);
		result.push_back(kHex[byte & 0x0f]);
	}
	return result;
}

std::string RandomKey(size_t length)
{
	static const char* kAlphabet =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

	std::random_device device;
	std::mt19937 generator(device());
	std::uniform_int_distribution<size_t> distribution(0,
		strlen(kAlphabet) - 1);

	std::string result;
	result.reserve(length);
	for (size_t i = 0; i < length; i++)
		result.push_back(kAlphabet[distribution(generator)]);
	return result;
}

std::string RsaNoPadHex(const std::string& reversedSecret)
{
	std::string result;

	BN_CTX* ctx = BN_CTX_new();
	BIGNUM* message = BN_bin2bn(
		reinterpret_cast<const unsigned char*>(reversedSecret.data()),
		static_cast<int>(reversedSecret.size()), nullptr);
	BIGNUM* modulus = nullptr;
	BIGNUM* exponent = BN_new();
	BIGNUM* cipher = BN_new();

	if (ctx != nullptr && message != nullptr && exponent != nullptr
		&& cipher != nullptr && BN_hex2bn(&modulus, kRsaModulus) > 0
		&& BN_set_word(exponent, kRsaExponent) == 1
		&& BN_mod_exp(cipher, message, exponent, modulus, ctx) == 1) {
		char* hex = BN_bn2hex(cipher);
		if (hex != nullptr) {
			result.assign(hex);
			OPENSSL_free(hex);
			for (char& c : result) {
				if (c >= 'A' && c <= 'F')
					c = static_cast<char>(c - 'A' + 'a');
			}
			// 1024 bit modulus -> 256 hex chars, left padded with zeros
			if (result.size() < 256)
				result.insert(0, 256 - result.size(), '0');
		}
	}

	BN_free(cipher);
	BN_free(exponent);
	BN_free(modulus);
	BN_free(message);
	BN_CTX_free(ctx);

	if (result.empty())
		throw std::runtime_error("RSA encryption failed");
	return result;
}

WeapiForm Weapi(const std::string& jsonText)
{
	const std::string secret = RandomKey(16);
	const std::string first = AesCbcBase64(jsonText, kPresetKey, kIv);
	const std::string second = AesCbcBase64(first, secret, kIv);

	WeapiForm form;
	form.params = second;
	form.encSecKey = RsaNoPadHex(std::string(secret.rbegin(), secret.rend()));
	return form;
}

std::string EapiParams(const std::string& path, const std::string& jsonText)
{
	const std::string digest = Md5Hex(
		"nobody" + path + "use" + jsonText + "md5forencrypt");
	const std::string joined = path + "-36cd479b6b5-" + jsonText
		+ "-36cd479b6b5-" + digest;
	return AesEcbHexUpper(joined, kEapiKey);
}

} // namespace ncm
