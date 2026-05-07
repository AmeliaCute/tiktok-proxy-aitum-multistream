#pragma once

#include <string>
#include <vector>
#include <functional>

struct TikTokCategory 
{
	std::string id;
	std::string name;
};

struct TikTokStreamInfo 
{
	std::string server;
	std::string key;
	std::string error;
	bool success = false;
};

struct TikTokAccountInfo 
{
	std::string username;
	std::string status;
	bool can_go_live = false;
	std::string error;
};

class TikTokStreamlabs 
{
	public:
	static std::vector<TikTokCategory> searchCategories(const std::string &token, const std::string &query);

	static TikTokAccountInfo getAccountInfo(const std::string &token);

	static TikTokStreamInfo startStream(const std::string &token, const std::string &title, const std::string &gameMaskId, bool mature = false);
	static bool endStream(const std::string &token);

	private:
	static std::string httpGet(const std::string &url, const std::string &token);
	static std::string httpPost(const std::string &url, const std::string &token, const std::string &body);
};
