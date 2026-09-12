#include "Blocklist.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <winsock2.h>
#include "nlohmann/json.hpp"

namespace {
	std::filesystem::path blocklistPath;
	std::filesystem::path legacyJsonPath;
	std::filesystem::path legacyNamesPath;
	std::filesystem::path legacyIpsPath;
	std::unordered_map<std::string, std::unordered_set<std::string>> blockedEntries;
	std::unordered_set<uint32_t> blockedAddresses;
	std::mutex blocklistMutex;
	constexpr std::array<unsigned char, 8> FILE_MAGIC{0x91, 0x4D, 0xE7, 0x2A, 0x63, 0xB8, 0x05, 0xCF};
	constexpr std::array<unsigned char, 16> FILE_KEY{0x37, 0xC2, 0x5B, 0xA1, 0xE8, 0x0D, 0x74, 0x96, 0x43, 0xFA, 0x28, 0xBD, 0x61, 0x8C, 0x15, 0xD0};

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

	uint32_t checksum(const std::string &value)
	{
		uint32_t result = 2166136261U;
		for (auto c : value) {
			result ^= static_cast<unsigned char>(c);
			result *= 16777619U;
		}
		return result;
	}

	void transform(std::string &value)
	{
		for (size_t i = 0; i < value.size(); i++)
			value[i] ^= static_cast<char>(FILE_KEY[(i * 7 + value.size()) % FILE_KEY.size()] ^ ((i * 31 + 0x5A) & 0xFF));
	}

	void write32(std::ostream &stream, uint32_t value)
	{
		for (unsigned i = 0; i < 4; i++)
			stream.put(static_cast<char>(value >> (i * 8)));
	}

	bool read32(std::istream &stream, uint32_t &value)
	{
		value = 0;
		for (unsigned i = 0; i < 4; i++) {
			auto c = stream.get();
			if (c == std::char_traits<char>::eof())
				return false;
			value |= static_cast<uint32_t>(static_cast<unsigned char>(c)) << (i * 8);
		}
		return true;
	}

	bool saveLocked()
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
		auto payload = root.dump();
		auto payloadChecksum = checksum(payload);
		transform(payload);
		auto temporary = blocklistPath;
		temporary += ".tmp";
		{
			std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
			if (!file)
				return false;
			file.write(reinterpret_cast<const char *>(FILE_MAGIC.data()), FILE_MAGIC.size());
			write32(file, static_cast<uint32_t>(payload.size()));
			write32(file, payloadChecksum);
			file.write(payload.data(), payload.size());
			if (!file)
				return false;
		}
		std::error_code error;
		std::filesystem::remove(blocklistPath, error);
		error.clear();
		std::filesystem::rename(temporary, blocklistPath, error);
		return !error;
	}

	bool loadBinaryLocked()
	{
		std::ifstream file(blocklistPath, std::ios::binary);
		if (!file)
			return false;
		std::array<unsigned char, FILE_MAGIC.size()> magic{};
		file.read(reinterpret_cast<char *>(magic.data()), magic.size());
		uint32_t size = 0;
		uint32_t expectedChecksum = 0;
		if (!file || magic != FILE_MAGIC || !read32(file, size) || !read32(file, expectedChecksum) || size > 4 * 1024 * 1024)
			return false;
		std::string payload(size, '\0');
		file.read(payload.data(), payload.size());
		if (!file)
			return false;
		transform(payload);
		if (checksum(payload) != expectedChecksum)
			return false;
		auto root = nlohmann::json::parse(payload);
		if (root.value("version", 0) != 1 || !root.contains("entries") || !root["entries"].is_array())
			return false;
		for (const auto &entry : root["entries"]) {
			auto name = entry.value("name", std::string{});
			if (name.empty())
				continue;
			auto &ips = blockedEntries[name];
			for (const auto &ip : entry.value("ips", std::vector<std::string>{}))
				if (validIp(ip))
					ips.insert(ip);
		}
		return true;
	}
}

void Blocklist::initialize(const std::filesystem::path &moduleFolder)
{
	std::lock_guard<std::mutex> lock(blocklistMutex);
	blocklistPath = moduleFolder / "blocklist.dat";
	legacyJsonPath = moduleFolder / "blocklist.json";
	legacyNamesPath = moduleFolder / "blocked-users.txt";
	legacyIpsPath = moduleFolder / "blocked-ips.txt";
	blockedEntries.clear();
	bool migrated = false;
	bool loadedBinary = false;
	try {
		loadedBinary = loadBinaryLocked();
	} catch (...) {
		blockedEntries.clear();
	}
	bool loadedJson = false;
	std::ifstream jsonFile(legacyJsonPath, std::ios::binary);
	if (!loadedBinary && jsonFile) {
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
			migrated = true;
		} catch (...) {
			blockedEntries.clear();
			loadedJson = false;
		}
	}
	if (!loadedBinary && !loadedJson) {
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
	if (migrated && saveLocked()) {
		std::error_code error;
		std::filesystem::remove(legacyJsonPath, error);
		std::filesystem::remove(legacyNamesPath, error);
		std::filesystem::remove(legacyIpsPath, error);
	}
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
