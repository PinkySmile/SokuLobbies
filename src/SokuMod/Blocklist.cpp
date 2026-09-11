#include "Blocklist.hpp"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <winsock2.h>
#include "nlohmann/json.hpp"

namespace {
	std::filesystem::path blocklistPath;
	std::filesystem::path legacyNamesPath;
	std::filesystem::path legacyIpsPath;
	std::unordered_map<std::string, std::unordered_set<std::string>> blockedEntries;
	std::unordered_set<uint32_t> blockedAddresses;
	std::mutex blocklistMutex;

	std::string stripLineEnding(std::string value)
	{
		while (!value.empty() && (value.back() == '\r' || value.back() == '\n'))
			value.pop_back();
		return value;
	}

	bool validIp(const std::string &ip)
	{
		auto address = inet_addr(ip.c_str());
		return address != INADDR_NONE && (address & htonl(0xFF000000)) != htonl(0x7F000000);
	}

	void rebuildAddressesLocked()
	{
		blockedAddresses.clear();
		for (const auto &[name, ips] : blockedEntries) {
			(void)name;
			for (const auto &ip : ips) {
				auto address = inet_addr(ip.c_str());
				if (address != INADDR_NONE)
					blockedAddresses.insert(address);
			}
		}
	}

	void saveLocked()
	{
		nlohmann::json root;
		root["version"] = 1;
		root["entries"] = nlohmann::json::array();
		std::vector<std::string> names;
		for (const auto &[name, ips] : blockedEntries) {
			(void)ips;
			names.push_back(name);
		}
		std::sort(names.begin(), names.end());
		for (const auto &name : names) {
			std::vector<std::string> ips(blockedEntries[name].begin(), blockedEntries[name].end());
			std::sort(ips.begin(), ips.end());
			root["entries"].push_back({{"name", name}, {"ips", ips}});
		}
		auto temporary = blocklistPath;
		temporary += ".tmp";
		{
			std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
			file << root.dump(2) << '\n';
		}
		std::error_code error;
		std::filesystem::remove(blocklistPath, error);
		error.clear();
		std::filesystem::rename(temporary, blocklistPath, error);
	}
}

void Blocklist::initialize(const std::filesystem::path &moduleFolder)
{
	std::lock_guard<std::mutex> lock(blocklistMutex);
	blocklistPath = moduleFolder / "blocklist.json";
	legacyNamesPath = moduleFolder / "blocked-users.txt";
	legacyIpsPath = moduleFolder / "blocked-ips.txt";
	blockedEntries.clear();
	bool migrated = false;
	bool loadedJson = false;
	std::ifstream jsonFile(blocklistPath, std::ios::binary);
	if (jsonFile) {
		try {
			nlohmann::json root;
			jsonFile >> root;
			loadedJson = root.value("version", 0) == 1 && root.contains("entries") && root["entries"].is_array();
			if (!loadedJson)
				throw std::runtime_error("Unsupported blocklist format");
			for (const auto &entry : root.value("entries", nlohmann::json::array())) {
				auto name = entry.value("name", std::string{});
				if (name.empty())
					continue;
				auto &ips = blockedEntries[name];
				for (const auto &ip : entry.value("ips", std::vector<std::string>{}))
					if (validIp(ip))
						ips.insert(ip);
			}
		} catch (...) {
			blockedEntries.clear();
			loadedJson = false;
		}
	}
	if (!loadedJson) {
		std::ifstream namesFile(legacyNamesPath, std::ios::binary);
		std::string line;
		while (std::getline(namesFile, line)) {
			line = stripLineEnding(std::move(line));
			if (!line.empty()) {
				blockedEntries[line];
				migrated = true;
			}
		}
		std::ifstream ipsFile(legacyIpsPath, std::ios::binary);
		while (std::getline(ipsFile, line)) {
			line = stripLineEnding(std::move(line));
			auto separator = line.find('\t');
			auto ip = line.substr(0, separator);
			auto name = separator == std::string::npos ? std::string{} : line.substr(separator + 1);
			if (validIp(ip) && !name.empty()) {
				blockedEntries[name].insert(ip);
				migrated = true;
			}
		}
	}
	rebuildAddressesLocked();
	if (migrated)
		saveLocked();
}

bool Blocklist::contains(const std::string &name)
{
	std::lock_guard<std::mutex> lock(blocklistMutex);
	return blockedEntries.find(name) != blockedEntries.end();
}

bool Blocklist::add(const std::string &name)
{
	std::lock_guard<std::mutex> lock(blocklistMutex);
	if (name.empty() || !blockedEntries.emplace(name, std::unordered_set<std::string>{}).second)
		return false;
	saveLocked();
	return true;
}

bool Blocklist::remove(const std::string &name)
{
	std::lock_guard<std::mutex> lock(blocklistMutex);
	if (!blockedEntries.erase(name))
		return false;
	rebuildAddressesLocked();
	saveLocked();
	return true;
}

std::vector<std::string> Blocklist::list()
{
	std::lock_guard<std::mutex> lock(blocklistMutex);
	std::vector<std::string> result;
	for (const auto &[name, ips] : blockedEntries) {
		(void)ips;
		result.push_back(name);
	}
	std::sort(result.begin(), result.end());
	return result;
}

std::vector<Blocklist::Entry> Blocklist::entries()
{
	std::lock_guard<std::mutex> lock(blocklistMutex);
	std::vector<Entry> result;
	for (const auto &[name, addresses] : blockedEntries) {
		Entry entry{name, {addresses.begin(), addresses.end()}};
		std::sort(entry.ips.begin(), entry.ips.end());
		result.push_back(std::move(entry));
	}
	std::sort(result.begin(), result.end(), [](const Entry &a, const Entry &b) { return a.name < b.name; });
	return result;
}

bool Blocklist::addIp(const std::string &name, const std::string &ip)
{
	std::lock_guard<std::mutex> lock(blocklistMutex);
	if (name.empty() || !validIp(ip))
		return false;
	auto &ips = blockedEntries[name];
	auto inserted = ips.insert(ip).second;
	blockedAddresses.insert(inet_addr(ip.c_str()));
	if (inserted)
		saveLocked();
	return inserted;
}

bool Blocklist::containsIp(uint32_t address)
{
	std::lock_guard<std::mutex> lock(blocklistMutex);
	return blockedAddresses.find(address) != blockedAddresses.end();
}
