#include "HttpRangeSource.h"

#include <curl/curl.h>

#include <Autolock.h>

#include <cstring>
#include <list>

namespace {

size_t WriteToVector(char* data, size_t size, size_t count, void* userData)
{
	auto* out = static_cast<std::vector<char>*>(userData);
	const size_t total = size * count;
	out->insert(out->end(), data, data + total);
	return total;
}

} // namespace

HttpRangeSource::HttpRangeSource(const std::string& url,
	long long expectedSize, std::string* error)
	: fUrl(url)
	, fSize(expectedSize)
{
	CURL* handle = curl_easy_init();
	if (handle == nullptr) {
		if (error != nullptr)
			*error = "curl 初始化失败";
		return;
	}

	// 先用一个 1 字节的 Range 探测：确认 CDN 支持 206，顺便拿到总大小
	std::vector<char> body;
	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "Range: bytes=0-0");

	curl_easy_setopt(handle, CURLOPT_URL, fUrl.c_str());
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteToVector);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(handle, CURLOPT_USERAGENT,
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 15L);
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);

	const CURLcode code = curl_easy_perform(handle);
	long status = 0;
	curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
	curl_slist_free_all(headers);
	curl_easy_cleanup(handle);

	if (code != CURLE_OK) {
		if (error != nullptr)
			*error = std::string("连接失败: ") + curl_easy_strerror(code);
		return;
	}

	if (status == 206) {
		fRangeSupported = true;
		// Content-Range: bytes 0-0/<total>
		double downloaded = 0.0;
		curl_off_t total = 0;
		curl_easy_getinfo(handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &total);
		downloaded = static_cast<double>(total);
		if (expectedSize <= 0)
			fSize = static_cast<long long>(downloaded);
	} else if (status == 200) {
		// 服务器无视 Range：只能整段拉。若第一段已经把整个文件带回来，
		// 存成第 0 块也能用
		if (expectedSize > 0
			&& body.size() >= static_cast<size_t>(expectedSize)) {
			fBlocks[0] = body;
			fRangeSupported = false;
		} else {
			if (error != nullptr)
				*error = "CDN 不支持 Range 请求";
			return;
		}
	} else {
		if (error != nullptr)
			*error = "HTTP " + std::to_string(status);
		return;
	}

	if (fSize <= 0) {
		if (error != nullptr)
			*error = "拿不到文件大小";
		return;
	}

	fOk = true;
}

HttpRangeSource::~HttpRangeSource()
{
}

ssize_t HttpRangeSource::ReadAt(off_t position, void* buffer, size_t size)
{
	if (!fOk || buffer == nullptr || size == 0)
		return 0;
	if (position < 0)
		return B_BAD_VALUE;
	if (static_cast<long long>(position) >= fSize)
		return 0;
	if (static_cast<long long>(position) + static_cast<long long>(size)
		> fSize) {
		size = static_cast<size_t>(fSize - position);
	}

	BAutolock _(fLock);

	char* out = static_cast<char*>(buffer);
	size_t copied = 0;
	while (copied < size) {
		const long long pos = position + copied;
		const long long blockStart = (pos / kBlockSize) * kBlockSize;
		const size_t offsetInBlock = static_cast<size_t>(pos - blockStart);

		auto found = fBlocks.find(blockStart);
		if (found == fBlocks.end()) {
			std::string error;
			if (!_FetchChunk(blockStart, &error))
				return copied > 0 ? static_cast<ssize_t>(copied) : B_ERROR;
			found = fBlocks.find(blockStart);
			if (found == fBlocks.end())
				return B_ERROR;
		}

		const std::vector<char>& block = found->second;
		if (offsetInBlock >= block.size())
			return copied > 0 ? static_cast<ssize_t>(copied) : B_ERROR;

		const size_t available = block.size() - offsetInBlock;
		const size_t want = size - copied;
		const size_t take = want < available ? want : available;
		memcpy(out + copied, block.data() + offsetInBlock, take);
		copied += take;
	}

	fPosition = position + copied;
	return static_cast<ssize_t>(copied);
}

ssize_t HttpRangeSource::WriteAt(off_t, const void*, size_t)
{
	return B_NOT_SUPPORTED;
}

off_t HttpRangeSource::Seek(off_t position, uint32 seekMode)
{
	switch (seekMode) {
		case SEEK_END:
			fPosition = static_cast<off_t>(fSize) + position;
			break;
		case SEEK_CUR:
			fPosition += position;
			break;
		default:
			fPosition = position;
			break;
	}
	return fPosition;
}

off_t HttpRangeSource::Position() const
{
	return fPosition;
}

bool HttpRangeSource::_FetchChunk(long long blockStart, std::string* error)
{
	// 不支持 Range 时只可能有第 0 块（整文件）
	if (!fRangeSupported && blockStart != 0)
		return false;

	CURL* handle = curl_easy_init();
	if (handle == nullptr) {
		if (error != nullptr)
			*error = "curl 初始化失败";
		return false;
	}

	const long long end = blockStart + kBlockSize - 1;
	std::string range = "Range: bytes=" + std::to_string(blockStart) + "-"
		+ std::to_string(end);

	std::vector<char> body;
	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, range.c_str());

	curl_easy_setopt(handle, CURLOPT_URL, fUrl.c_str());
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteToVector);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(handle, CURLOPT_USERAGENT,
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 15L);
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);

	const CURLcode code = curl_easy_perform(handle);
	long status = 0;
	curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
	curl_slist_free_all(headers);
	curl_easy_cleanup(handle);

	if (code != CURLE_OK || (status != 206 && status != 200)) {
		if (error != nullptr)
			*error = "分块拉取失败";
		return false;
	}

	if (body.empty())
		return false;

	// 200（无视 Range）时整段返回，存第 0 块
	if (status == 200 && blockStart != 0) {
		if (error != nullptr)
			*error = "CDN 不支持 Range 请求";
		return false;
	}

	if (!fRangeSupported && status == 200)
		blockStart = 0;

	fBlocks[blockStart] = std::move(body);

	// 把离当前位置远的块清掉，内存封顶
	if (fBlocks.size() > kMaxBlocks) {
		auto victim = fBlocks.begin();
		long long bestDistance = -1;
		for (auto it = fBlocks.begin(); it != fBlocks.end(); ++it) {
			const long long distance = it->first > blockStart
				? it->first - blockStart : blockStart - it->first;
			if (bestDistance < 0 || distance > bestDistance) {
				bestDistance = distance;
				victim = it;
			}
		}
		if (victim != fBlocks.end())
			fBlocks.erase(victim);
	}

	return true;
}
