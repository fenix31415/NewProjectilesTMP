#pragma once

#include <unordered_map>
#include <string>
#include "json/json.h"
#include "magic_enum.hpp"
#include "RE/P/Projectile.h"

template <typename T, T def>
T parse_impl(const std::string& s)
{
	return magic_enum::enum_cast<T>(s).value_or(def);
}

template <auto def>
auto parse_enum(const std::string& s)
{
	return parse_impl<decltype(def), def>(s);
}

template <auto def>
static auto parse_enum_ifIsMember(const Json::Value& item, std::string_view field_name)
{
	if constexpr (std::is_same<decltype(def), bool>::value) {
		if (item.isMember(field_name.data()))
			return item[field_name.data()].asBool();
		else
			return def;
	} else if constexpr (std::is_same<decltype(def), uint32_t>::value) {
		if (item.isMember(field_name.data()))
			return item[field_name.data()].asUInt();
		else
			return def;
	} else {
		if (item.isMember(field_name.data()))
			return parse_enum<def>(item[field_name.data()].asString());
		else
			return def;
	}
}

namespace JsonUtils
{
	RE::NiPoint3 get_point(const Json::Value& point);
	RE::Projectile::ProjectileRot get_point2(const Json::Value& point);

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

	RE::NiPoint3 readOrDefault3(const Json::Value& json_spawnGroup, std::string_view field_name);
	RE::Projectile::ProjectileRot readOrDefault2(const Json::Value& json_spawnGroup, std::string_view field_name);
}
