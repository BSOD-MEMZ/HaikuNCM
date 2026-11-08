#ifndef NCM_HTTP_H
#define NCM_HTTP_H

#include <string>
#include <vector>

namespace ncm {

struct Response {
	long status = 0;
	std::string body;
	std::string headers;
	std::string error;

	bool Ok() const
	{
		return error.empty() && status >= 200 && status < 300;
	}
};

// Call once before using any request helper (thread safe).
void HttpInit();

std::string UrlEncode(const std::string& value);

Response HttpGet(const std::string& url,
	const std::vector<std::string>& headers);

Response PostForm(const std::string& url, const std::string& body,
	const std::vector<std::string>& headers);

bool DownloadToFile(const std::string& url, const std::string& path,
	const std::vector<std::string>& headers, std::string* error);

} // namespace ncm

#endif
