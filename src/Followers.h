#pragma once
#include "json/json.h"

namespace Followers
{
	void install();
	void init(const Json::Value& json_root);
	void init_keys(const Json::Value& json_root);
	uint32_t get_key_ind(const std::string& key);
	void onCreated(RE::Projectile* proj, uint32_t ind);

	using forEachRes = RE::BSContainer::ForEachResult;
	using forEachF = std::function<forEachRes(RE::Projectile* proj)>;
	void forEachFollower(RE::Actor* a, const forEachF& func);
}
