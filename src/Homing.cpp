#include "Homing.h"
#include "RuntimeData.h"
#include "JsonUtils.h"

#ifdef DEBUG
#include "Triggers.h"
#endif  // DEBUG

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
		Hostile,  // Find nearest hostile target
		Cursor    // Find closest to cursor (within radius) target
	};
	static constexpr TargetTypes TargetTypes__DEFAULT = TargetTypes::Hostile;

	struct Data
	{
		HomingTypes type: 1;
		TargetTypes target: 2;
		float detection_angle;  // valid for target == cursor
		float val1;             // rotation time (ConstSpeed) or acceleration (ConstAccel)
	};
	static_assert(sizeof(Data) == 12);

	struct Storage
	{
		static void init(const Json::Value& HomingData)
		{
			for (auto& key : HomingData.getMemberNames()) {
				read_json_entry(key, HomingData[key]);
			}
		}

		static const auto& get_data(uint32_t ind) { return data_static[ind - 1]; }

		static void forget() {
			keys.init();
			data_static.clear();
			data_dynamic.clear();
		}

		static uint32_t get_key_ind(const std::string& key) { return keys.get(key); }

	private:
		static void read_json_entry(const std::string& key, const Json::Value& item)
		{
			uint32_t ind = keys.add(key);
			assert(ind == data_static.size() + 1);
			
			auto type = parse_enum<HomingTypes__DEFAULT>(item["type"].asString());
			auto target = parse_enum_ifIsMember<TargetTypes__DEFAULT>(item, "target"sv);

			float detection_angle = 0.0f;
			if (target == TargetTypes::Cursor) {
				detection_angle = item["cursorAngle"].asFloat();
			}

			float val1 = 0.0f;
			switch (type) {
			case HomingTypes::ConstAccel:
				val1 = item["acceleration"].asFloat();
				break;
			case HomingTypes::ConstSpeed:
				val1 = item["rotationTime"].asFloat();
				break;
			default:
				assert(false);
				break;
			}

			data_static.emplace_back(type, target, detection_angle, val1);
		}

		static inline JsonUtils::KeysMap keys;
		static inline std::vector<Data> data_static;
		static inline std::vector<Data> data_dynamic;
	};

	void forget() { Storage::forget(); }
	uint32_t get_key_ind(const std::string& key) { return Storage::get_key_ind(key); }

	void set_homing_ind(RE::Projectile* proj, uint32_t ind) { ::set_homing_ind(proj, ind); }
	uint32_t get_homing_ind(RE::Projectile* proj) { return ::get_homing_ind(proj); }
	bool is_homing(RE::Projectile* proj) { return get_homing_ind(proj) != 0; }
	void disable_homing(RE::Projectile* proj) { set_homing_ind(proj, 0); }

	namespace Targeting
	{
		RE::NiPoint3 get_victim_pos(RE::Actor* target, float dtime = 0.0f)
		{
			RE::NiPoint3 ans, eye_pos;
			target->GetLinearVelocity(ans);
			ans *= dtime;
			FenixUtils::Actor__get_eye_pos(target, eye_pos, 3);
			ans += eye_pos;
			return ans;
		}

		bool is_hostile(RE::TESObjectREFR* refr, RE::TESObjectREFR* _caster)
		{
			auto target = refr->As<RE::Actor>();
			auto caster = _caster->As<RE::Actor>();
			if (!target || !caster)
				return false;

			return target->currentCombatTarget.get().get() == caster;
		}

		RE::TESObjectREFR* find_nearest_target(RE::TESObjectREFR* caster, RE::TESObjectREFR* origin, bool onlyHostile,
			float within_dist2)
		{
			float mindist2 = 1.0E15f;
			RE::TESObjectREFR* refr = nullptr;
			RE::TES::GetSingleton()->ForEachReference([=, &mindist2, &refr](RE::TESObjectREFR& _refr) {
				if (!_refr.IsDisabled() && !_refr.IsDead() && _refr.GetFormType() == RE::FormType::ActorCharacter &&
					_refr.formID != caster->formID && (!onlyHostile || is_hostile(&_refr, caster))) {
					float curdist2 = origin->GetPosition().GetSquaredDistance(_refr.GetPosition());
					if (curdist2 < within_dist2 && curdist2 < mindist2) {
						mindist2 = curdist2;
						refr = &_refr;
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});

			if (!refr)
				return nullptr;

			return refr;
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

				FenixUtils::Actor__get_eye_pos(caster, caster_pos, 2);
				FenixUtils::Actor__get_eye_pos(target, target_pos, 3);

				caster_sight = caster_pos;
				caster_sight += FenixUtils::rotate(1, caster->data.angle);

				return is_anglebetween_less(caster_pos, caster_sight, target_pos, angle);
			}

			enum class LineOfSightLocation : std::uint32_t
			{
				kNone = 0,
				kEyes = 1,   // Eye level
				kHead = 2,   // 85%
				kTorso = 3,  // 50%
				kFeet = 4    // 15%
			};
			static_assert(sizeof(LineOfSightLocation) == 0x4);

			static LineOfSightLocation IsActorInLineOfSight(RE::Actor* caster, RE::Actor* target, float viewCone = 100)
			{
				return _generic_foo_<36752, decltype(IsActorInLineOfSight)>::eval(caster, target, viewCone);
			}

			RE::TESObjectREFR* find_cursor_target(RE::TESObjectREFR* _caster, float angle)
			{
				if (!_caster->IsPlayerRef())
					return nullptr;

				auto caster = _caster->As<RE::Actor>();
				std::vector<std::pair<RE::Actor*, float>> targets;

				RE::TES::GetSingleton()->ForEachReference([caster, &targets, angle](RE::TESObjectREFR& _refr) {
					if (auto refr = _refr.As<RE::Actor>();
						refr && !_refr.IsDisabled() && !_refr.IsDead() && _refr.GetFormType() == RE::FormType::ActorCharacter &&
						_refr.formID != caster->formID && is_near_to_cursor(caster, refr, angle)) {
						if (IsActorInLineOfSight(caster, refr) != LineOfSightLocation::kNone) {
							targets.push_back({ refr, caster->GetPosition().GetSquaredDistance(refr->GetPosition()) });
						}
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
		}

		RE::Actor* findTarget(RE::TESObjectREFR* origin, TargetTypes target_type, float detection_angle)
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

			RE::TESObjectREFR* refr;
			switch (target_type) {
			case TargetTypes::Nearest:
			case TargetTypes::Hostile:
				{
					float within_dist =
						proj && (proj->IsFlameProjectile() || proj->IsBeamProjectile()) ? proj->range * proj->range : 1.0E15f;
					refr = find_nearest_target(caster, proj ? proj : caster, target_type == TargetTypes::Hostile, within_dist);
					break;
				}
			case TargetTypes::Cursor:
				refr = Cursor::find_cursor_target(caster, detection_angle);
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
		// SkyrimSE.exe+74DC20
		float get_proj_speed(RE::Projectile* proj) { return _generic_foo_<42958, decltype(get_proj_speed)>::eval(proj); }

		bool get_shoot_dir(RE::Projectile* proj, RE::Actor* target, float dtime, RE::NiPoint3& ans)
		{
			RE::NiPoint3 target_dir;
			target->GetLinearVelocity(target_dir);
			double target_speed = target_dir.Length();

			double proj_speed = get_proj_speed(proj);

			auto target_pos = Targeting::get_victim_pos(target, dtime);
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
			auto old_dir = proj->linearVelocity;
			old_dir.Unitize();

			float max_angle = get_rotation_speed(proj, param) * dtime;
			float angle = acos(old_dir.Dot(final_dir));
			auto axis = old_dir.UnitCross(final_dir);

			float phi = fmin(max_angle, angle);
			float cos_phi = cos(phi);
			float sin_phi = sin(phi);
			float one_cos_phi = 1 - cos_phi;
			RE::NiMatrix3 R = { { cos_phi + one_cos_phi * axis.x * axis.x, axis.x * axis.y * one_cos_phi - axis.z * sin_phi,
									axis.x * axis.z * one_cos_phi + axis.y * sin_phi },
				{ axis.y * axis.x * one_cos_phi + axis.z * sin_phi, cos_phi + axis.y * axis.y * one_cos_phi,
					axis.y * axis.z * one_cos_phi - axis.x * sin_phi },
				{ axis.z * axis.x * one_cos_phi - axis.y * sin_phi, axis.z * axis.y * one_cos_phi + axis.x * sin_phi,
					cos_phi + axis.z * axis.z * one_cos_phi } };

			proj->linearVelocity = (R * old_dir) * proj->linearVelocity.Length();
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
			speed = get_proj_speed(proj);
			proj->linearVelocity += V;
			float newspeed = proj->linearVelocity.Length();
			proj->linearVelocity *= speed / newspeed;
		}

		void SetRotationMatrix(RE::NiMatrix3& a_matrix, float sacb, float cacb, float sb)
		{
			float cb = std::sqrtf(1 - sb * sb);
			float ca = cacb / cb;
			float sa = -sacb / cb;
			a_matrix.entry[0][0] = ca;
			a_matrix.entry[0][1] = sacb;
			a_matrix.entry[0][2] = sa * sb;
			a_matrix.entry[1][0] = sa;
			a_matrix.entry[1][1] = cacb;
			a_matrix.entry[1][2] = -ca * sb;
			a_matrix.entry[2][0] = 0.0;
			a_matrix.entry[2][1] = sb;
			a_matrix.entry[2][2] = cb;
		}

		void update_node_rotation(RE::Projectile* proj)
		{
			RE::NiPoint3 proj_dir = proj->linearVelocity;
			proj_dir.Unitize();

			proj->data.angle.x = asin(proj_dir.z);
			proj->data.angle.z = atan2(proj_dir.x, proj_dir.y);

			if (proj_dir.x < 0.0) {
				proj->data.angle.x += 3.1415926f;
			}

			if (proj->data.angle.z < 0.0) {
				proj->data.angle.z += 3.1415926f;
			}

			SetRotationMatrix(proj->Get3D2()->local.rotate, proj_dir.x, proj_dir.y, proj_dir.z);
		}

		void change_direction_linVel(RE::Projectile* proj, float dtime)
		{
			RE::NiPoint3 final_vel;
			auto& data = Storage::get_data(get_homing_ind(proj));
			if (auto target = Targeting::findTarget(proj, data.target, data.detection_angle); target && get_shoot_dir(proj, target, dtime, final_vel)) {
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

			update_node_rotation(proj);

#ifdef DEBUG
			{
				auto proj_dir = proj->linearVelocity;
				proj_dir.Unitize();
				draw_line<Colors::RED>(proj->GetPosition(), proj->GetPosition() + proj_dir);
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
			static bool ShouldUseDesiredTarget(RE::Projectile* proj)
			{
				bool ans = _ShouldUseDesiredTarget(proj);
				auto target = proj->desiredTarget.get().get();
				return ans || target;
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

		namespace Debug
		{
			uint32_t get_cursor_ind(RE::PlayerCharacter* a) {
				if (auto obj = a->GetEquippedObject(false))
					if (auto spel = obj->As<RE::SpellItem>())
						if (auto mgef = FenixUtils::getAVEffectSetting(spel))
							if (auto ind = Triggers::Triggers::get_homing_ind(mgef->data.projectileBase)) {
								const auto& data = Storage::get_data(ind);
								if (data.target == TargetTypes::Cursor)
									return ind;
							}
				
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
						if (auto target = Targeting::Cursor::find_cursor_target(a, data.detection_angle))
							draw_line0(a->GetPosition(), target->GetPosition());
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
						FenixUtils::Actor__get_eye_pos(a, origin, 2);

						const float circle_dist = 2000;
						caster_dir = FenixUtils::rotate(1, a->data.angle);

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

							draw_line(old, cur_p, 5, 0);
							old = cur_p;
						}
					}
				}

				static inline REL::Relocation<decltype(Update)> _Update;
			};
		}
	}

	auto rot_at(RE::NiPoint3 dir)
	{
		RE::Projectile::ProjectileRot rot;
		auto len = dir.Unitize();
		if (len == 0) {
			rot = { 0, 0 };
		} else {
			float polar_angle = _generic_foo_<68820, float(RE::NiPoint3 * p)>::eval(&dir);  // SkyrimSE.exe+c51f70
			rot = { -asin(dir.z), polar_angle };
		}

		return rot;
	}

	auto rot_at(const RE::NiPoint3& from, const RE::NiPoint3& to) { return rot_at(to - from); }

	void onCreated(RE::Projectile* proj, uint32_t ind)
	{
		auto& data = Storage::get_data(ind);
		auto caster = proj->shooter.get().get();
		if (!caster)
			return;

		if (proj->IsMissileProjectile()) {
			set_homing_ind(proj, ind);
		}

		if (proj->IsBeamProjectile()) {
			if (auto target = Targeting::findTarget(proj, data.target, data.detection_angle)) {
				auto dir = rot_at(proj->GetPosition(), Targeting::get_victim_pos(target));

				_generic_foo_<19362, void(RE::TESObjectREFR * refr, float rot_Z)>::eval(proj, dir.z);
				_generic_foo_<19360, void(RE::TESObjectREFR * refr, float rot_X)>::eval(proj, dir.x);
			}
		}

		if (proj->IsFlameProjectile()) {
			Targeting::findTarget(proj, data.target, data.detection_angle);
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

	void init(const Json::Value& HomingData) { Storage::init(HomingData); }
}