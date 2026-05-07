#include "tiktok-streamlabs.hpp"

#include <curl/curl.h>
#include <obs-data.h>
#include <sstream>
#include <cstring>

static size_t curlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	auto *buf = static_cast<std::string *>(userdata);
	buf->append(ptr, size * nmemb);
	return size * nmemb;
}

static CURL *makeCurl(const std::string &url, const std::string &token, std::string &response, curl_slist **headers_out)
{
	CURL *curl = curl_easy_init();
	if (!curl) return nullptr;

	curl_slist *headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");

	if (!token.empty()) 
	{
		std::string auth = "Authorization: Bearer " + token;
		headers = curl_slist_append(headers, auth.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

	if (headers_out) *headers_out = headers;

	return curl;
}

std::string TikTokStreamlabs::httpGet(const std::string &url, const std::string &token)
{
	std::string response;
	curl_slist *headers = nullptr;
	CURL *curl = makeCurl(url, token, response, &headers);
	if (!curl) return "";

	curl_easy_perform(curl);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return response;
}

std::string TikTokStreamlabs::httpPost(const std::string &url, const std::string &token, const std::string &body)
{
	std::string response;
	curl_slist *headers = nullptr;
	CURL *curl = makeCurl(url, token, response, &headers);
	if (!curl)
		return "";

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

	curl_easy_perform(curl);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return response;
}

static std::string jsonString(const std::string &s)
{
	std::string out = "\"";
	for (char c : s) 
	{
		if (c == '"') out += "\\\"";
		else if (c == '\\') out += "\\\\";
		else out += c;
	}
	out += "\"";
	return out;
}

std::vector<TikTokCategory> TikTokStreamlabs::searchCategories(const std::string &token, const std::string &query)
{
	std::vector<TikTokCategory> results;
	if (token.empty()) return results;

	CURL *curl = curl_easy_init();
	if (!curl) return results;

	char *encoded = curl_easy_escape(curl, query.c_str(), (int)query.size());
	std::string q = encoded ? std::string(encoded) : query;
	if (encoded) curl_free(encoded);

	curl_easy_cleanup(curl);

	std::string url ="https://streamlabs.com/api/v5/platforms/tiktok/search?type=game&page=1&q=" + q;
	auto raw = httpGet(url, token);
	if (raw.empty()) return results;

	auto *root = obs_data_create_from_json(raw.c_str());
	if (!root) return results;

	auto *arr = obs_data_get_array(root, "data");
	if (arr) 
	{
		size_t count = obs_data_array_count(arr);
		for (size_t i = 0; i < count; i++) 
		{
			auto *item = obs_data_array_item(arr, i);
			TikTokCategory cat;

			cat.id = obs_data_get_string(item, "game_mask_id");
			cat.name = obs_data_get_string(item, "full_name");

			if (!cat.id.empty() && !cat.name.empty()) results.push_back(cat);
			obs_data_release(item);
		}
		obs_data_array_release(arr);
	}
	obs_data_release(root);

	return results;
}

TikTokAccountInfo TikTokStreamlabs::getAccountInfo(const std::string &token)
{
	TikTokAccountInfo info;
	if (token.empty()) 
	{
		info.error = "No token";
		return info;
	}

	auto raw = httpGet("https://streamlabs.com/api/v5/platforms/tiktok/info", token);
	if (raw.empty()) 
	{
		info.error = "Network error";
		return info;
	}

	auto *root = obs_data_create_from_json(raw.c_str());
	if (!root) 
	{
		info.error = "Parse error";
		return info;
	}

	auto *user = obs_data_get_obj(root, "user");
	if (user) 
	{
		info.username = obs_data_get_string(user, "username");
		obs_data_release(user);
	}

	auto *status = obs_data_get_obj(root, "application_status");
	if (status) 
	{
		info.status = obs_data_get_string(status, "status");
		obs_data_release(status);
	}

	info.can_go_live = obs_data_get_bool(root, "can_be_live");
	obs_data_release(root);

	return info;
}

TikTokStreamInfo TikTokStreamlabs::startStream(const std::string &token, const std::string &title, const std::string &gameMaskId, bool mature)
{
	TikTokStreamInfo result;
	if (token.empty()) 
	{
		result.error = "No token configured";
		return result;
	}

	std::string body = "{\"title\":" + jsonString(title) + ",\"game_mask_id\":" + jsonString(gameMaskId) + ",\"audience_type\":\"" + (mature ? "1" : "0") + "\"}";

	auto raw = httpPost("https://streamlabs.com/api/v5/platforms/tiktok/stream", token, body);
	if (raw.empty()) 
	{
		result.error = "Network error";
		return result;
	}

	auto *root = obs_data_create_from_json(raw.c_str());
	if (!root) 
	{
		result.error = "Parse error";
		return result;
	}

	result.server = obs_data_get_string(root, "server");
	result.key = obs_data_get_string(root, "stream_key");

	if (!result.server.empty() && !result.key.empty()) 
		result.success = true;
	else 
	{
		const char *msg = obs_data_get_string(root, "message");
		result.error = (msg && strlen(msg)) ? msg : "Unknown error from Streamlabs";
	}

	obs_data_release(root);
	return result;
}

bool TikTokStreamlabs::endStream(const std::string &token)
{
	if (token.empty()) return false;

	auto raw = httpPost("https://streamlabs.com/api/v5/platforms/tiktok/stream/end", token, "{}");
	if (raw.empty()) return false;

	auto *root = obs_data_create_from_json(raw.c_str());
	if (!root) return false;

	bool ok = obs_data_get_bool(root, "success");
	obs_data_release(root);
	return ok;
}


static size_t curlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	auto *buf = static_cast<std::string *>(userdata);
	buf->append(ptr, size * nmemb);
	return size * nmemb;
}

static CURL *makeCurl(const std::string &url, const std::string &token, std::string &response, curl_slist **headers_out)
{
	CURL *curl = curl_easy_init();
	if (!curl) return nullptr;

	curl_slist *headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");

	if (!token.empty()) 
	{
		std::string auth = "Authorization: Bearer " + token;
		headers = curl_slist_append(headers, auth.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	if (headers_out) *headers_out = headers;

	return curl;
}

std::string TikTokStreamlabs::httpGet(const std::string &url, const std::string &token)
{
	std::string response;
	curl_slist *headers = nullptr;
	CURL *curl = makeCurl(url, token, response, &headers);
	if (!curl) return "";

	curl_easy_perform(curl);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return response;
}

std::string TikTokStreamlabs::httpPost(const std::string &url, const std::string &token,
					const std::string &body)
{
	std::string response;
	curl_slist *headers = nullptr;
	CURL *curl = makeCurl(url, token, response, &headers);
	if (!curl)
		return "";

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

	curl_easy_perform(curl);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return response;
}

std::vector<TikTokCategory> TikTokStreamlabs::searchCategories(const std::string &token, const std::string &query)
{
	std::vector<TikTokCategory> results;
	if (token.empty()) return results;

	CURL *curl = curl_easy_init();
	if (!curl) return results;

	char *encoded = curl_easy_escape(curl, query.c_str(), (int)query.size());
	std::string q = encoded ? std::string(encoded) : query;
	if (encoded) curl_free(encoded);
	curl_easy_cleanup(curl);

	std::string url = "https://streamlabs.com/api/v5/platforms/tiktok/search?type=game&page=1&q=" + q;
	auto raw = httpGet(url, token);
	if (raw.empty()) return results;

	try 
	{
		auto j = json::parse(raw);
		if (!j.contains("data") || !j["data"].is_array()) return results;
		for (auto &item : j["data"]) 
		{
			TikTokCategory cat;
			cat.id = item.value("game_mask_id", "");
			cat.name = item.value("full_name", "");
			if (!cat.id.empty() && !cat.name.empty()) results.push_back(cat);
		}
	} catch (...) { }

	return results;
}

TikTokAccountInfo TikTokStreamlabs::getAccountInfo(const std::string &token)
{
	TikTokAccountInfo info;
	if (token.empty()) 
	{
		info.error = "No token";
		return info;
	}

	auto raw = httpGet("https://streamlabs.com/api/v5/platforms/tiktok/info", token);
	if (raw.empty())
	{
		info.error = "Network error";
		return info;
	}

	try 
	{
		auto j = json::parse(raw);
		if (j.contains("user") && j["user"].is_object()) info.username = j["user"].value("username", "");
		if (j.contains("application_status") && j["application_status"].is_object()) info.status = j["application_status"].value("status", "");
		info.can_go_live = j.value("can_be_live", false);
	} catch (...) { info.error = "Parse error"; }

	return info;
}

TikTokStreamInfo TikTokStreamlabs::startStream(const std::string &token, const std::string &title, const std::string &gameMaskId, bool mature)
{
	TikTokStreamInfo result;
	if (token.empty()) 
	{
		result.error = "No token configured";
		return result;
	}

	json body;
	body["title"] = title;
	body["game_mask_id"] = gameMaskId;
	body["audience_type"] = mature ? "1" : "0";

	auto raw = httpPost("https://streamlabs.com/api/v5/platforms/tiktok/stream", token, body.dump());
	if (raw.empty()) 
	{
		result.error = "Network error";
		return result;
	}

	try {
		auto j = json::parse(raw);
		result.server = j.value("server", "");
		result.key = j.value("stream_key", "");
		if (!result.server.empty() && !result.key.empty()) result.success = true;
		else result.error = j.value("message", "Unknown error from Streamlabs");
	} catch (...) { result.error = "Parse error"; }

	return result;
}

bool TikTokStreamlabs::endStream(const std::string &token)
{
	if (token.empty()) return false;
	auto raw = httpPost("https://streamlabs.com/api/v5/platforms/tiktok/stream/end", token, "{}");
	if (raw.empty()) return false;
	try 
	{
		auto j = json::parse(raw);
		return j.value("success", false);
	} 
	catch (...) { return false; }
}
