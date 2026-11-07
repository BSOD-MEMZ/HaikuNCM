#include "http.h"

#include <curl/curl.h>

#include <cstdio>
#include <mutex>

namespace ncm {

namespace {

size_t WriteToString(void* contents, size_t size, size_t count, void* userData)
{
	const size_t total = size * count;
	auto* body = static_cast<std::string*>(userData);
	body->append(static_cast<char*>(contents), total);
	return total;
}

struct curl_slist* BuildHeaders(const std::vector<std::string>& headers)
{
	struct curl_slist* list = nullptr;
	for (const std::string& header : headers)
		list = curl_slist_append(list, header.c_str());
	return list;
}

CURL* Prepare(const std::string& url, const std::vector<std::string>& headers)
{
	CURL* handle = curl_easy_init();
	if (handle == nullptr)
		return nullptr;

	curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(handle, CURLOPT_TIMEOUT, 60L);
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, BuildHeaders(headers));
	return handle;
}

} // namespace

void HttpInit()
{
	static std::once_flag once;
	std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

std::string UrlEncode(const std::string& value)
{
	CURL* handle = curl_easy_init();
	if (handle == nullptr)
		return value;

	char* encoded = curl_easy_escape(handle, value.c_str(),
		static_cast<int>(value.size()));
	std::string result = encoded != nullptr ? encoded : value;
	if (encoded != nullptr)
		curl_free(encoded);
	curl_easy_cleanup(handle);
	return result;
}

Response PostForm(const std::string& url, const std::string& body,
	const std::vector<std::string>& headers)
{
	HttpInit();

	Response response;
	CURL* handle = Prepare(url, headers);
	if (handle == nullptr) {
		response.error = "curl_easy_init failed";
		return response;
	}

	curl_easy_setopt(handle, CURLOPT_POST, 1L);
	curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE,
		static_cast<long>(body.size()));
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteToString);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response.body);

	const CURLcode result = curl_easy_perform(handle);
	if (result != CURLE_OK)
		response.error = curl_easy_strerror(result);
	else
		curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status);

	curl_easy_cleanup(handle); // frees the slist attached as HTTPHEADER too
	return response;
}

bool DownloadToFile(const std::string& url, const std::string& path,
	const std::vector<std::string>& headers, std::string* error)
{
	HttpInit();

	FILE* file = fopen(path.c_str(), "wb");
	if (file == nullptr) {
		if (error != nullptr)
			*error = "cannot open " + path;
		return false;
	}

	CURL* handle = Prepare(url, headers);
	if (handle == nullptr) {
		fclose(file);
		if (error != nullptr)
			*error = "curl_easy_init failed";
		return false;
	}

	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, nullptr);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, file);

	const CURLcode result = curl_easy_perform(handle);
	long status = 0;
	curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);

	bool success = result == CURLE_OK && status >= 200 && status < 300;
	if (!success && error != nullptr) {
		*error = result != CURLE_OK ? curl_easy_strerror(result)
									: "HTTP " + std::to_string(status);
	}

	curl_easy_cleanup(handle);
	fclose(file);
	return success;
}

} // namespace ncm
