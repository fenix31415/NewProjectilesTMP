#include "Homing.h"
#include "JsonUtils.h"
#include "RuntimeData.h"

namespace Homing
{
	enum class HomingTypes : uint32_t
	{
		ConstSpeed,  // Projectile has constant speed
		ConstAccel   // Projectile has constant rotation time
	};

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

	struct Storage
	{
		static void clear_keys() { keys.clear(); }
		static void clear()
		{
			clear_keys();
			data_static.clear();
		}

		static void init(const std::string& filename, const Json::Value& HomingData)
		{
			for (auto& key : HomingData.getMemberNames()) {
				read_json_entry(filename, key, HomingData[key]);
			}
		}

		static void init_keys(const std::string& filename, const Json::Value& HomingData)
		{
			for (auto& key : HomingData.getMemberNames()) {
				read_json_entry_keys(filename, key, HomingData[key]);
			}
		}

		static const auto& get_data(uint32_t ind) { return data_static[ind - 1]; }

		static uint32_t get_key_ind(const std::string& filename, const std::string& key) { return keys.get(filename, key); }

	private:
		static void read_json_entry(const std::string& filename, const std::string& key, const Json::Value& item)
		{
			[[maybe_unused]] uint32_t ind = keys.get(filename, key);
			assert(ind == data_static.size() + 1);

			auto type = JsonUtils::read_enum<HomingTypes>(item, "type");
			auto target = JsonUtils::mb_read_field<TargetTypes__DEFAULT>(item, "target");
			bool check_los = JsonUtils::mb_read_field<false>(item, "checkLOS");
			auto aggressive = JsonUtils::mb_read_field<AggressiveTypes__DEFAULT>(item, "aggressive");

			float detection_angle = 0.0f;
			if (target == TargetTypes::Cursor) {
				detection_angle = JsonUtils::getFloat(item, "cursorAngle");
			}

			float val1 = 0.0f;
			switch (type) {
			case HomingTypes::ConstAccel:
				val1 = JsonUtils::getFloat(item, "acceleration");
				break;
			case HomingTypes::ConstSpeed:
				val1 = JsonUtils::getFloat(item, "rotationTime");
				break;
			default:
				assert(false);
				break;
			}

			data_static.emplace_back(type, target, check_los, aggressive, detection_angle, val1);
		}

		static void read_json_entry_keys(const std::string& filename, const std::string& key, const Json::Value&)
		{
			keys.add(filename, key);
		}

		static inline JsonUtils::KeysMap keys;
		static inline std::vector<Data> data_static;
	};

	// used in multicast evenly
	const Data& get_data(uint32_t ind) { return Storage::get_data(ind); }

	uint32_t get_key_ind(const std::string& filename, const std::string& key) { return Storage::get_key_ind(filename, key); }

	void set_homing_ind(RE::Projectile* proj, uint32_t ind) { ::set_homing_ind(proj, ind); }
	uint32_t get_homing_ind(RE::Projectile* proj) { return ::get_homing_ind(proj); }
	bool is_homing(RE::Projectile* proj) { return get_homing_ind(proj) != 0; }
	void disable_homing(RE::Projectile* proj) { set_homing_ind(proj, 0); }

	namespace Targeting
	{
		constexpr float WITHIN_DIST2 = 4.0E7f;

		using FenixUtils::Geom::Actor::AnticipatePos;

		bool is_hostile(RE::TESObjectREFR* refr, RE::TESObjectREFR* _caster)
		{
			auto target = refr->As<RE::Actor>();
			auto caster = _caster->As<RE::Actor>();
			if (!target || !caster)
				return false;
			return target->currentCombatTarget.get().get() == caster;
		}

		bool filter_target_base(RE::TESObjectREFR& _refr, RE::TESObjectREFR* caster)
		{
			return !_refr.IsDisabled() && !_refr.IsDead() && _refr.GetFormType() == RE::FormType::ActorCharacter &&
			       _refr.formID != caster->formID;
		}

		bool filter_target_los(RE::TESObjectREFR& _refr, RE::TESObjectREFR* caster, bool check_los)
		{
			return !check_los || !caster->As<RE::Actor>() || !_refr.As<RE::Actor>() ||
			       FenixUtils::Geom::Actor::ActorInLOS(caster->As<RE::Actor>(), _refr.As<RE::Actor>(), 100);
		}

		bool filter_target_aggressive(RE::TESObjectREFR& _refr, RE::TESObjectREFR* caster, AggressiveTypes type)
		{
			return type == AggressiveTypes::Any || (type == AggressiveTypes::Aggressive && is_hostile(&_refr, caster)) ||
			       (type == AggressiveTypes::Hostile && _refr.As<RE::Actor>() && caster->As<RE::Actor>() &&
					   _refr.As<RE::Actor>()->IsHostileToActor(caster->As<RE::Actor>()));
		}

		bool filter_target_dist(RE::TESObjectREFR& _refr, const RE::NiPoint3& origin_pos, float within_dist2)
		{
			return origin_pos.GetSquaredDistance(_refr.GetPosition()) < within_dist2;
		}

		bool filter_target(RE::TESObjectREFR& _refr, RE::TESObjectREFR* caster, const RE::NiPoint3& origin_pos,
			AggressiveTypes type, bool check_los, float within_dist2)
		{
			return filter_target_base(_refr, caster) && filter_target_dist(_refr, origin_pos, within_dist2) &&
			       filter_target_los(_refr, caster, check_los) && filter_target_aggressive(_refr, caster, type);
		}

		RE::Actor* find_nearest_target(RE::TESObjectREFR* caster, const RE::NiPoint3& origin_pos, const Data& data,
			float within_dist2 = WITHIN_DIST2)
		{
			bool check_los = data.check_LOS;
			auto hostile_filter = data.hostile_filter;

			float mindist2 = 1.0E15f;
			RE::TESObjectREFR* refr = nullptr;
			RE::TES::GetSingleton()->ForEachReference([=, &mindist2, &refr](RE::TESObjectREFR& _refr) {
				if (filter_target(_refr, caster, origin_pos, hostile_filter, check_los, within_dist2)) {
					float curdist2 = origin_pos.GetSquaredDistance(_refr.GetPosition());
					if (curdist2 < mindist2) {
						mindist2 = curdist2;
						refr = &_refr;
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});

			if (!refr)
				return nullptr;

			return refr->As<RE::Actor>();
		}

		std::vector<RE::Actor*> get_nearest_targets(RE::TESObjectREFR* caster, const RE::NiPoint3& origin_pos, const Data& data,
			float within_dist2 = WITHIN_DIST2)
		{
			bool check_los = data.check_LOS;
			auto hostile_filter = data.hostile_filter;

			std::vector<RE::Actor*> ans;

			RE::TES::GetSingleton()->ForEachReference([=, &ans](RE::TESObjectREFR& _refr) {
				if (filter_target(_refr, caster, origin_pos, hostile_filter, check_los, within_dist2)) {
					ans.push_back(_refr.As<RE::Actor>());
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});

			return ans;
		}

		namespace Cursor
		{
			static bool is_anglebetween_less(const RE::NiPoint3& A, const RE::NiPoint3& B1, const RE::NiPoint3& B2,
				float angle_deg)
			{
				auto AB1 = B1 - A;
				auto AB2 = B2 - A;
				AB1.Unitize();
				AB2.Unitize();
				return acos(AB1.Dot(AB2)) < angle_deg / 180.0f * 3.1415926f;
			}

			static bool is_near_to_cursor(RE::Actor* caster, RE::Actor* target, float angle)
			{
				RE::NiPoint3 caster_pos, caster_sight, target_pos;

				caster_pos = FenixUtils::Geom::Actor::CalculateLOSLocation(caster, FenixUtils::LineOfSightLocation::kHead);
				target_pos = FenixUtils::Geom::Actor::CalculateLOSLocation(target, FenixUtils::LineOfSightLocation::kTorso);

				caster_sight = caster_pos;
				caster_sight += FenixUtils::Geom::angles2dir(caster->data.angle);

				return is_anglebetween_less(caster_pos, caster_sight, target_pos, angle);
			}

			bool filter_target_cursor(RE::TESObjectREFR& _refr, RE::Actor* caster, AggressiveTypes type, bool check_los,
				float angle, float within_dist2)
			{
				auto refr = _refr.As<RE::Actor>();
				return filter_target(_refr, caster, caster->GetPosition(), type, check_los, within_dist2) && refr &&
				       is_near_to_cursor(caster, refr, angle);
			}

			RE::Actor* find_cursor_target(RE::TESObjectREFR* _caster, const Data& data, float within_dist2 = WITHIN_DIST2)
			{
				if (!_caster->IsPlayerRef())
					return nullptr;

				auto caster = _caster->As<RE::Actor>();
				std::vector<std::pair<RE::Actor*, float>> targets;

				auto angle = data.detection_angle;
				bool check_los = data.check_LOS;
				auto hostile_filter = data.hostile_filter;

				RE::TES::GetSingleton()->ForEachReference([=, &targets](RE::TESObjectREFR& _refr) {
					if (filter_target_cursor(_refr, caster, hostile_filter, check_los, angle, within_dist2)) {
						auto caster_pos =
							FenixUtils::Geom::Actor::CalculateLOSLocation(caster, FenixUtils::LineOfSightLocation::kHead);
						auto target_pos =
							FenixUtils::Geom::Actor::CalculateLOSLocation(&_refr, FenixUtils::LineOfSightLocation::kTorso);

						auto caster_sight = caster_pos;
						caster_sight += FenixUtils::Geom::angles2dir(caster->data.angle);

						auto AB1 = caster_sight - caster_pos;
						auto AB2 = target_pos - caster_pos;
						AB1.Unitize();
						AB2.Unitize();

						targets.push_back(
							//{ _refr.As<RE::Actor>(), caster->GetPosition().GetSquaredDistance(_refr.GetPosition()) });
							{ _refr.As<RE::Actor>(), abs(acos(AB1.Dot(AB2))) });
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});

				if (!targets.size())
					return nullptr;

				return (*std::min_element(targets.begin(), targets.end(),
							[](const std::pair<RE::Actor*, float>& a, const std::pair<RE::Actor*, float>& b) {
								return a.second < b.second;
							}))
				    .first;
			}

			std::vector<RE::Actor*> get_cursor_targets(RE::TESObjectREFR* _caster, const Data& data,
				float within_dist2 = WITHIN_DIST2)
			{
				std::vector<RE::Actor*> ans;

				if (!_caster->IsPlayerRef())
					return ans;

				auto caster = _caster->As<RE::Actor>();

				auto angle = data.detection_angle;
				bool check_los = data.check_LOS;
				auto hostile_filter = data.hostile_filter;

				RE::TES::GetSingleton()->ForEachReference([=, &ans](RE::TESObjectREFR& _refr) {
					if (filter_target_cursor(_refr, caster, hostile_filter, check_los, angle, within_dist2)) {
						auto refr = _refr.As<RE::Actor>();
						if (caster->GetPosition().GetSquaredDistance(refr->GetPosition()) < within_dist2) {
							ans.push_back(refr);
						}
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});

				return ans;
			}
		}

		RE::Actor* findTarget(RE::TESObjectREFR* origin, const Data& data)
		{
			auto proj = origin->As<RE::Projectile>();

			if (proj) {
				auto target = proj->desiredTarget.get().get();
				if (target)
					return target->As<RE::Actor>();
			}

			auto caster = proj ? proj->shooter.get().get() : origin;
			if (!caster)
				return nullptr;

			if (auto caster_npc = caster->As<RE::Actor>(); caster_npc && !caster_npc->IsPlayerRef()) {
				return caster_npc->currentCombatTarget.get().get();
			}

			auto target_type = data.target;
			float within_dist =
				proj && (proj->IsFlameProjectile() || proj->IsBeamProjectile()) ? proj->range * proj->range : WITHIN_DIST2;

			RE::TESObjectREFR* refr;
			switch (target_type) {
			case TargetTypes::Nearest:
				{
					refr = find_nearest_target(caster, (proj ? proj : caster)->GetPosition(), data, within_dist);
					break;
				}
			case TargetTypes::Cursor:
				refr = Cursor::find_cursor_target(caster, data, within_dist);
				break;
			default:
				refr = nullptr;
				break;
			}

			if (!refr)
				return nullptr;

#ifdef DEBUG
			FenixUtils::notification("Target found: %s", refr->GetName());
#endif  // DEBUG

			if (proj)
				proj->desiredTarget = refr->GetHandle();
			return refr->As<RE::Actor>();
		}
	}

	namespace Moving
	{
		bool get_shoot_dir(RE::Projectile* proj, RE::Actor* target, float dtime, RE::NiPoint3& ans)
		{
			RE::NiPoint3 target_dir;
			target->GetLinearVelocity(target_dir);
			double target_speed = target_dir.Length();

			double proj_speed = FenixUtils::Projectile__GetSpeed(proj);

			auto target_pos = Targeting::AnticipatePos(target, dtime);
			auto strait_dir = target_pos - proj->GetPosition();

			double a = proj_speed * proj_speed - target_speed * target_speed;

			double strait_len = strait_dir.Unitize();
			double c = -(strait_len * strait_len);
			double b;

			if (target_speed > 0.0001) {
				target_dir.Unitize();
				double cos_phi = -target_dir.Dot(strait_dir);
				b = 2 * strait_len * target_speed * cos_phi;
			} else {
				b = 0.0;
			}

			double D = b * b - 4 * a * c;
			if (D < 0)
				return false;

			D = sqrt(D);
			double t1 = (-b + D) / a * 0.5;
			double t2 = (-b - D) / a * 0.5;

			if (t1 <= 0 && t2 <= 0)
				return false;

			double t = t1;
			if (t2 > 0 && t2 < t1)
				t = t2;

			ans = target_dir * (float)target_speed + strait_dir * (float)(strait_len / t);
			return true;
		}

		// constant speed, limited rotation angle
		void change_direction_1(RE::Projectile* proj, float dtime, const RE::NiPoint3& final_vel, float param)
		{
			auto get_rotation_speed = []([[maybe_unused]] RE::Projectile* proj, float param) {
				// param1 / 100 = time to rotate at 180
				// 250 350 500 norm
				return 3.1415926f / param;
			};

			auto final_dir = final_vel;
			final_dir.Unitize();

			proj->linearVelocity =
				FenixUtils::Geom::rotateVel(proj->linearVelocity, get_rotation_speed(proj, param) * dtime, final_dir);
		}

		// constant acceleration length
		void change_direction_2(RE::Projectile* proj, [[maybe_unused]] float dtime, const RE::NiPoint3& final_vel, float param)
		{
			auto get_acceleration = []([[maybe_unused]] RE::Projectile* proj, float param) {
				// param1 / 10 = acceleration vector length
				// 50 100 500
				return param;
			};

			auto V = final_vel;
			auto speed = proj->linearVelocity.Length();
			V.Unitize();
			V *= speed;
			V -= proj->linearVelocity;
			V.Unitize();
			V *= get_acceleration(proj, param);
			speed = FenixUtils::Projectile__GetSpeed(proj);
			proj->linearVelocity += V;
			float newspeed = proj->linearVelocity.Length();
			proj->linearVelocity *= speed / newspeed;
		}

		void change_direction_linVel(RE::Projectile* proj, float dtime)
		{
			RE::NiPoint3 final_vel;
			auto& data = Storage::get_data(get_homing_ind(proj));
			if (auto target = Targeting::findTarget(proj, data); target && get_shoot_dir(proj, target, dtime, final_vel)) {
				auto val1 = data.val1;
				auto type = data.type;
				switch (type) {
				case HomingTypes::ConstSpeed:
					change_direction_1(proj, dtime, final_vel, val1);
					break;
				case HomingTypes::ConstAccel:
					change_direction_2(proj, dtime, final_vel, val1);
					break;
				default:
					break;
				}
			} else {
				disable_homing(proj);
			}
		}

		void change_direction(RE::Projectile* proj, RE::NiPoint3*, float dtime)
		{
			change_direction_linVel(proj, dtime);

			FenixUtils::Geom::Projectile::update_node_rotation(proj);

#ifdef DEBUG
			{
				auto proj_dir = proj->linearVelocity;
				proj_dir.Unitize();
				draw_line(proj->GetPosition(), proj->GetPosition() + proj_dir, Colors::RED);
			}
#endif  // DEBUG
		}
	}

	namespace Hooks
	{
		// Change ShouldUseDesiredTarget vfunc
		class HomingFlamesHook
		{
		public:
			static void Hook()
			{
				_ShouldUseDesiredTarget =
					REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_FlameProjectile[0])).write_vfunc(0xC1, ShouldUseDesiredTarget);
			}

		private:
			// TODO: update if dist too far away
			static bool ShouldUseDesiredTarget(RE::Projectile* proj)
			{
				bool ans = _ShouldUseDesiredTarget(proj);
				if (auto target = proj->desiredTarget.get().get()) {
					draw_point(target->GetPosition(), Colors::RED, 0);
					draw_point(proj->GetPosition(), Colors::BLU, 0);

					auto range = proj->GetProjectileBase()->data.range;

					if (target->IsDead() || target->GetPosition().GetSquaredDistance(proj->GetPosition()) > range * range) {
						proj->desiredTarget = {};
						return ans;
					}

					return true;
				}
				return ans;
			}

			static inline REL::Relocation<decltype(ShouldUseDesiredTarget)> _ShouldUseDesiredTarget;
		};

		// Make projectile move to the target
		class HomingMissilesHook
		{
		public:
			static void Hook()
			{
				_Projectile__ApplyGravity = SKSE::GetTrampoline().write_call<5>(REL::ID(43006).address() + 0x69,
					change_direction);  // SkyrimSE.exe+751309
			}

		private:
			static bool change_direction(RE::Projectile* proj, RE::NiPoint3* dV, float dtime)
			{
				bool ans = _Projectile__ApplyGravity(proj, dV, dtime);
				if (is_homing(proj)) {
					Moving::change_direction(proj, dV, dtime);
				}
				return ans;
			}

			static inline REL::Relocation<decltype(change_direction)> _Projectile__ApplyGravity;
		};

#ifdef DEBUG
		namespace Debug
		{
			// TODO
			uint32_t get_cursor_ind(RE::PlayerCharacter*)
			{
				/*if (auto obj = a->GetEquippedObject(false))
					if (auto spel = obj->As<RE::SpellItem>())
						if (auto mgef = FenixUtils::getAVEffectSetting(spel))
							if (auto ind = Triggers::get_homing_ind(mgef->data.projectileBase)) {
								const auto& data = Storage::get_data(ind);
								if (data.target == TargetTypes::Cursor)
									return ind;
							}
							*/
				return 0;
			}

			// Draw debug lines to captured targets
			class CursorDetectedHook
			{
			public:
				static void Hook()
				{
					_Update = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_PlayerCharacter[0])).write_vfunc(0xad, Update);
				}

			private:
				static void Update(RE::PlayerCharacter* a, float delta)
				{
					_Update(a, delta);

					if (auto ind = get_cursor_ind(a)) {
						const auto& data = Storage::get_data(ind);
						if (auto target = Targeting::Cursor::find_cursor_target(a, data, Targeting::WITHIN_DIST2))
							draw_line(a->GetPosition(), target->GetPosition(), Colors::RED, 0);
					}
				}

				static inline REL::Relocation<decltype(Update)> _Update;
			};

			// Draw debug circle of target capturing
			class CursorCircleHook
			{
			public:
				static void Hook()
				{
					_Update = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_PlayerCharacter[0])).write_vfunc(0xad, Update);
				}

			private:
				static void Update(RE::PlayerCharacter* a, float delta)
				{
					_Update(a, delta);

					if (auto ind = get_cursor_ind(a)) {
						const auto& data = Storage::get_data(ind);
						float alpha_max = data.detection_angle;
						alpha_max = alpha_max / 180.0f * 3.1415926f;

						RE::NiPoint3 origin, caster_dir;
						origin = FenixUtils::Geom::Actor::CalculateLOSLocation(a, FenixUtils::LineOfSightLocation::kHead);

						const float circle_dist = 2000;
						caster_dir = FenixUtils::Geom::angles2dir(a->data.angle);

						float circle_r = circle_dist * tan(alpha_max);
						RE::NiPoint3 right_dir = RE::NiPoint3(0, 0, -1).UnitCross(caster_dir);
						if (right_dir.SqrLength() == 0)
							right_dir = { 1, 0, 0 };
						right_dir *= circle_r;
						RE::NiPoint3 up_dir = right_dir.Cross(caster_dir);

						origin += caster_dir * circle_dist;

						RE::NiPoint3 old = origin + right_dir;
						const int N = 31;
						for (int i = 1; i <= N; i++) {
							float alpha = 2 * 3.1415926f / N * i;

							auto cur_p = origin + right_dir * cos(alpha) + up_dir * sin(alpha);

							draw_line(old, cur_p, Colors::RED, 0);
							old = cur_p;
						}
					}
				}

				static inline REL::Relocation<decltype(Update)> _Update;
			};
		}
#endif  // DEBUG
	}

	std::vector<RE::Actor*> get_targets(uint32_t homingInd, RE::TESObjectREFR* caster, const RE::NiPoint3& origin_pos)
	{
		auto& homing_data = get_data(homingInd);
		return homing_data.target == TargetTypes::Cursor ? Targeting::Cursor::get_cursor_targets(caster, homing_data) :
		                                                   Targeting::get_nearest_targets(caster, origin_pos, homing_data);
	}

	void applyRotate(RE::Projectile* proj, uint32_t ind, RE::Actor* targetOverride)
	{
		auto caster = proj->shooter.get().get();
		if (!caster)
			return;

		if (proj->IsMissileProjectile() || proj->IsBeamProjectile()) {
			auto& data = Storage::get_data(ind);

			if (!targetOverride)
				targetOverride = Targeting::findTarget(proj, data);

			if (targetOverride) {
				FenixUtils::Geom::Projectile::aimToPoint(proj, Targeting::AnticipatePos(targetOverride));
			}
		}
	}

	void disable(RE::Projectile* proj)
	{
		if (proj->IsMissileProjectile()) {
			disable_homing(proj);
		}
	}

	void apply(RE::Projectile* proj, uint32_t ind, RE::Actor* targetOverride)
	{
		auto caster = proj->shooter.get().get();
		if (!caster)
			return;

		assert(ind > 0);

		if (proj->IsMissileProjectile()) {
			set_homing_ind(proj, ind);
		}

		auto& data = Storage::get_data(ind);

		if (!targetOverride)
			targetOverride = Targeting::findTarget(proj, data);
		else
			proj->desiredTarget = targetOverride->GetHandle();

		if (!targetOverride)
			return;

		if (proj->IsBeamProjectile()) {
			auto dir = FenixUtils::Geom::rot_at(proj->GetPosition(), Targeting::AnticipatePos(targetOverride));

			FenixUtils::TESObjectREFR__SetAngleOnReferenceZ(proj, dir.z);
			FenixUtils::TESObjectREFR__SetAngleOnReferenceX(proj, dir.x);
		}
	}

	void install()
	{
		using namespace Hooks;

		HomingFlamesHook::Hook();
		HomingMissilesHook::Hook();

#ifdef DEBUG
		Debug::CursorDetectedHook::Hook();
		Debug::CursorCircleHook::Hook();
#endif
	}

	void clear() { Storage::clear(); }
	void clear_keys() { Storage::clear_keys(); }

	void init(const std::string& filename, const Json::Value& json_root)
	{
		if (json_root.isMember("HomingData")) {
			Storage::init(filename, json_root["HomingData"]);
		}
	}

	void init_keys(const std::string& filename, const Json::Value& json_root)
	{
		if (json_root.isMember("HomingData")) {
			Storage::init_keys(filename, json_root["HomingData"]);
		}
	}
}
