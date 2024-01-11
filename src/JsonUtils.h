#pragma once

#include <unordered_map>
#include <string>
#include "json/json.h"
#include "magic_enum.hpp"
#include "RE/P/Projectile.h"

namespace JsonUtils
{
	using namespace FenixUtils::Json;

	uint32_t get_formid(const std::string& filename, const std::string& name);

	class FormIDsMap
	{
		static inline std::unordered_map<std::string, uint32_t> formIDs;

	public:
		static void init(const std::string& filename, const Json::Value& json_root);
		static void clear();

		// `key` must present and starts with "key_"
		static auto get(const std::string& filename, const std::string& key)
		{
			auto found = formIDs.find(filename + key);
			assert(found != formIDs.end());
			return found->second;
		}
	};
	
	// Stores map key_... -> 1 ...
	class KeysMap
	{
		std::unordered_map<std::string, uint32_t> keys;

	public:
		void clear() { keys.clear(); }

		// `key` must present and starts with "key_"
		auto get(const std::string& filename, const std::string& key)
		{
			auto found = keys.find(filename + key);
			assert(found != keys.end());
			return (*found).second;
		}

		uint32_t add(const std::string& filename, const std::string& key)
		{
			auto finalkey = filename + key;
			auto found = keys.find(finalkey);
			assert(found == keys.end());
			uint32_t new_key = static_cast<uint32_t>(keys.size()) + 1;
			keys.insert({ finalkey, new_key });
			return new_key;
		}
	};
}
