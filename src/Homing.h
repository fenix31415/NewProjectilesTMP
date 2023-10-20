#pragma once

#include "json/json.h"

namespace Homing
{
	enum class HomingTypes : uint32_t
	{
		ConstSpeed,  // Projectile has constant speed
		ConstAccel   // Projectile has constant rotation time
	};
	static constexpr HomingTypes HomingTypes__DEFAULT = HomingTypes::ConstSpeed;

	enum class TargetTypes : uint32_t
	{
		Nearest,  // Find nearest target
		Cursor    // Find closest to cursor (within radius) target
	};
	static constexpr TargetTypes TargetTypes__DEFAULT = TargetTypes::Nearest;

	enum class AggressiveTypes : uint32_t
	{
		Aggressive,  // Accept only aggressive to caster at the moment targets
		Hostile,     // Accept only hostile targets
		Any,         // Accept any target
	};
	static constexpr AggressiveTypes AggressiveTypes__DEFAULT = AggressiveTypes::Hostile;

	struct Data
	{
		HomingTypes type: 1;
		TargetTypes target: 2;
		uint32_t check_LOS: 1;
		AggressiveTypes hostile_filter: 2;
		float detection_angle;  // valid for target == cursor
		float val1;             // rotation time (ConstSpeed) or acceleration (ConstAccel)
	};
	static_assert(sizeof(Data) == 12);

	const Data& get_data(uint32_t ind);

	void applyRotate(RE::Projectile* proj, uint32_t ind, RE::Actor* targetOverride);
	void apply(RE::Projectile* proj, uint32_t ind, RE::Actor* targetOverride);

	void install();
	void init(const Json::Value& json_root);
	void init_keys(const Json::Value& json_root);
	uint32_t get_key_ind(const std::string& key);

	namespace Targeting
	{
		constexpr float WITHIN_DIST2 = 4.0E7f;

		RE::NiPoint3 get_victim_pos(RE::Actor* target, float dtime = 0.0f);

		std::vector<RE::Actor*> get_nearest_targets(RE::TESObjectREFR* caster, const RE::NiPoint3& origin_pos,
			const Data& data, float within_dist2 = WITHIN_DIST2);

		RE::Actor* find_nearest_target(RE::TESObjectREFR* caster, const RE::NiPoint3& origin_pos, const Data& data,
			float within_dist2 = WITHIN_DIST2);

		namespace Cursor
		{
			std::vector<RE::Actor*> get_cursor_targets(RE::TESObjectREFR* _caster, const Data& data,
				float within_dist2 = WITHIN_DIST2);
			RE::Actor* find_cursor_target(RE::TESObjectREFR* _caster, const Data& data,
				float within_dist2 = WITHIN_DIST2);
		}
	}
}
