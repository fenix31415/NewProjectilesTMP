#pragma once
#include "json/json.h"

namespace Followers
{
	enum class Collision : uint32_t
	{
		Actor,
		Spell,
		None
	};

	void install();
	void clear();
	void clear_keys();
	void init(const std::string& filename, const Json::Value& json_root);
	void init_keys(const std::string& filename, const Json::Value& json_root);
	uint32_t get_key_ind(const std::string& filename, const std::string& key);
	void apply(RE::Projectile* proj, uint32_t ind);
	void disable(RE::Projectile* proj, bool restore_speed = true);
	RE::COL_LAYER layer2layer(Collision l);

	using forEachRes = RE::BSContainer::ForEachResult;
	using forEachF = std::function<forEachRes(RE::Projectile* proj)>;
	void forEachFollower(RE::TESObjectREFR* a, const forEachF& func);
}
