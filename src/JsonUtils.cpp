#include "JsonUtils.h"

namespace JsonUtils
{
	RE::NiPoint3 get_point(const Json::Value& point) { return { point[0].asFloat(), point[1].asFloat(), point[2].asFloat() }; }

	RE::Projectile::ProjectileRot get_point2(const Json::Value& point) { return { point[0].asFloat(), point[1].asFloat() }; }

	int get_mod_index(const std::string_view& name)
	{
		auto esp = RE::TESDataHandler::GetSingleton()->LookupModByName(name);
		if (!esp)
			return -1;
		return !esp->IsLight() ? esp->compileIndex << 24 : (0xFE000 | esp->smallFileCompileIndex) << 12;
	}

	uint32_t get_formid(const std::string& name)
	{
		if (name.starts_with("key_")) {
			return FormIDsMap::get(name);
		}

		if (auto pos = name.find('|'); pos != std::string::npos) {
			auto ind = get_mod_index(name.substr(0, pos));
			return ind | std::stoul(name.substr(pos + 1), nullptr, 16);
		}

		return std::stoul(name, nullptr, 16);
	}

	void FormIDsMap::init(const Json::Value& json_root)
	{
		formIDs.clear();

		if (!json_root.isMember("FormIDs"))
			return;

		const auto& formids = json_root["FormIDs"];
		for (auto& key : formids.getMemberNames()) {
			formIDs.insert({ key, get_formid(formids[key].asString()) });
		}
	}
}


