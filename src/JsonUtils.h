#pragma once

#include <unordered_map>
#include <string>
#include "json/json.h"
#include "magic_enum.hpp"
#include "RE/P/Projectile.h"

namespace JsonUtils
{
	using namespace FenixUtils::Json;

	uint32_t get_formid(const std::string& name);

	class FormIDsMap
	{
		static inline std::unordered_map<std::string, uint32_t> formIDs;

	public:
		static void init(const Json::Value& json_root);

		// `key` must present and starts with "key_"
		static auto get(std::string key)
		{
			auto found = formIDs.find(key);
			assert(found != formIDs.end());
			return found->second;
		}
	};
	
	// Stores map key_... -> 1 ...
	class KeysMap
	{
		std::unordered_map<std::string, uint32_t> keys;

	public:
		void init() { keys.clear(); }

		// `key` must present and starts with "key_"
		auto get(const std::string& key)
		{
			auto found = keys.find(key);
			assert(found != keys.end());
			return (*found).second;
		}

		uint32_t add(const std::string& key)
		{
			auto found = keys.find(key);
			assert(found == keys.end());
			uint32_t new_key = static_cast<uint32_t>(keys.size()) + 1;
			keys.insert({ key, new_key });
			return new_key;
		}
	};
}
