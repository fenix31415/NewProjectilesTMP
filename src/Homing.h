#pragma once

#include "json/json.h"

namespace Homing
{
	void applyRotate(RE::Projectile* proj, uint32_t ind, RE::Actor* targetOverride);
	void apply(RE::Projectile* proj, uint32_t ind, RE::Actor* targetOverride);
	void disable(RE::Projectile* proj);

	void install();
	void init(const Json::Value& json_root);
	void init_keys(const Json::Value& json_root);
	uint32_t get_key_ind(const std::string& key);

	// For MC
	std::vector<RE::Actor*> get_targets(uint32_t homingInd, RE::TESObjectREFR* caster, const RE::NiPoint3& origin_pos);
}
