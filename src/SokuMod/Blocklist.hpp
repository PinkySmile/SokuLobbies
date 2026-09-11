#pragma once

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Blocklist {
	struct Entry {
		std::string name;
		std::vector<std::string> ips;
	};

	void initialize(const std::filesystem::path &moduleFolder);
	bool contains(const std::string &name);
	bool add(const std::string &name);
	bool remove(const std::string &name);
	std::vector<std::string> list();
	std::vector<Entry> entries();
	bool addIp(const std::string &name, const std::string &ip);
	bool containsIp(uint32_t address);
}
