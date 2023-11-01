#include "Followers.h"
#include "JsonUtils.h"
#include "RuntimeData.h"
#include <algorithm>
#include "Positioning.h"

namespace Followers
{
	enum class Collision : uint32_t
	{
		Actor,
		Spell,
		None
	};

	// TODO: cylinder
	enum class Rounding : uint32_t
	{
		None,
		Plane,
		Sphere
	};

	struct Data
	{
		Positioning::Pattern pattern;  // 00

		Rounding rounding: 2;
		Collision collision: 2;
		float rounding_radius;  // if rounding
		float speed_mult;       // 0 for instant, default: 1

		explicit Data(const Json::Value& item) :
			pattern(item["Pattern"]), rounding(JsonUtils::mb_read_field<Rounding::None>(item, "rounding")),
			rounding_radius(rounding != Rounding::None ? JsonUtils::getFloat(item, "roundingR") : 0),
			collision(JsonUtils::mb_read_field<Collision::Actor>(item, "collision")),
			speed_mult(JsonUtils::mb_getFloat<1.0f>(item, "speed"))
		{}
	};
	static_assert(sizeof(Data) == 0x40);

	struct Storage
	{
		static void init(const Json::Value& HomingData)
		{
			data_static.clear();

			for (auto& key : HomingData.getMemberNames()) {
				read_json_entry(key, HomingData[key]);
			}
		}

		static void init_keys(const Json::Value& HomingData)
		{
			keys.init();

			for (auto& key : HomingData.getMemberNames()) {
				read_json_entry_keys(key, HomingData[key]);
			}
		}

		static const auto& get_data(uint32_t ind) { return data_static[ind - 1]; }

		static uint32_t get_key_ind(const std::string& key) { return keys.get(key); }

	private:
		static void read_json_entry(const std::string& key, const Json::Value& item)
		{
			[[maybe_unused]] uint32_t ind = keys.get(key);
			assert(ind == data_static.size() + 1);

			data_static.emplace_back(item);
		}

		static void read_json_entry_keys(const std::string& key, const Json::Value&) { keys.add(key); }

		static inline JsonUtils::KeysMap keys;
		static inline std::vector<Data> data_static;
	};

	uint32_t get_key_ind(const std::string& key) { return Storage::get_key_ind(key); }

	void set_follower_ind(RE::Projectile* proj, uint32_t ind) { ::set_follower_ind(proj, ind); }
	uint32_t get_follower_ind(RE::Projectile* proj) { return ::get_follower_ind(proj); }
	void set_follower_shape_ind(RE::Projectile* proj, uint32_t ind) { ::set_follower_shape_ind(proj, ind); }
	uint32_t get_follower_shape_ind(RE::Projectile* proj) { return ::get_follower_shape_ind(proj); }
	bool is_follower(RE::Projectile* proj) { return get_follower_ind(proj) != 0; }
	void disable_follower(RE::Projectile* proj) { set_follower_ind(proj, 0); }

	namespace Moving
	{
		auto get_target_point(RE::Projectile* proj)
		{
			auto& data = Storage::get_data(get_follower_ind(proj));

			RE::NiPoint3 final_vel;
			auto caster = proj->shooter.get().get()->As<RE::Actor>();
			RE::Projectile::ProjectileRot dir{ caster->GetAngleX(), caster->GetAngleZ() };

			RE::NiPoint3 spawn_center = caster->GetPosition();
			data.pattern.initCenter(spawn_center, dir, caster);
			RE::NiPoint3 cast_dir = data.pattern.getCastDir(dir);
			cast_dir.Unitize();

			return data.pattern.GetPosition(spawn_center, cast_dir, get_follower_shape_ind(proj));
		}

		RE::NiPoint2 rotate(RE::NiPoint2 P, float alpha)
		{
			float _cos = cosf(alpha);
			float _sin = sin(alpha);
			return { P.x * _cos - P.y * _sin, P.y * _cos + P.x * _sin };
		}

		const float CIRCLE_K = 0.01f;
		const float CIRCLE_K_BIG = 1.0f + CIRCLE_K;
		const float CIRCLE_K_SML = 1.0f - CIRCLE_K;

		RE::NiPoint3 get_target_point_rounding_sphere(RE::Projectile* proj, RE::NiPoint3* dV)
		{
			auto& data = Storage::get_data(get_follower_ind(proj));

			auto target_pos = get_target_point(proj);
			float D2 = proj->GetPosition().GetSquaredDistance(target_pos);

			float dtime = sqrtf(dV->SqrLength() / proj->linearVelocity.SqrLength());
			float speed_origin = proj->linearVelocity.Length();
			float R = data.rounding_radius;
			float R2 = R * R;

			auto cast_dir = proj->linearVelocity.UnitCross(target_pos - proj->GetPosition());
			Positioning::Plane plane(target_pos, cast_dir);

			//draw_circle0(target_pos, R, cast_dir);

			if (D2 > R2 * CIRCLE_K_BIG * CIRCLE_K_BIG) {
				// Out of circle
				// Move to tangent
				RE::NiPoint2 P = plane.project(proj->GetPosition() - target_pos);
				RE::NiPoint2 vel = plane.project(proj->linearVelocity);

				RE::NiPoint2 Q = { -P.y, P.x };
				auto d2 = P.SqrLength();
				float a = R2 / d2;
				float b = R / d2 * sqrtf(d2 - R2);
				if (P.Cross(vel) < 0)
					b *= -1;
				auto T3 = plane.unproject(P * a + Q * b);
				auto V = T3 - proj->GetPosition();
				float len = V.Unitize();
				auto my_len = speed_origin * dtime;
				float circle_len = my_len - len;
				if (circle_len > 0) {
					return FenixUtils::Geom::rotate(T3, circle_len / R, target_pos, cast_dir);
				} else {
					return V * my_len + proj->GetPosition();
				}
			} else if (D2 < R2 * CIRCLE_K_SML * CIRCLE_K_SML) {
				// Inside of circle
				// Slightly rotate velocity to tangent

				RE::NiPoint3 proj_dir_final = (proj->GetPosition() - target_pos).UnitCross(cast_dir);
				if (proj_dir_final.Dot(proj->linearVelocity) < 0) {
					proj_dir_final *= -1;
				} else {
				}

				proj->linearVelocity =
					FenixUtils::Geom::rotateVel(proj->linearVelocity, dtime * speed_origin / R, proj_dir_final);

				return proj->GetPosition() + proj->linearVelocity * dtime;
			} else {
				// On circle
				// Move around the circle

				float phi = dtime * speed_origin / R;
				RE::NiPoint3 proj_dir_final = (proj->GetPosition() - target_pos).UnitCross(cast_dir);
				if (proj_dir_final.Dot(proj->linearVelocity) < 0) {
					proj_dir_final *= -1;
				} else {
					phi *= -1;
				}

				proj->linearVelocity = proj_dir_final * speed_origin;
				return FenixUtils::Geom::rotate(proj->GetPosition(), phi, target_pos, cast_dir);
			}
		}

		RE::NiPoint3 get_target_point_rounding_plane(RE::Projectile* proj, RE::NiPoint3* dV)
		{
			auto& data = Storage::get_data(get_follower_ind(proj));

			auto target_pos = get_target_point(proj);

			float dtime = sqrtf(dV->SqrLength() / proj->linearVelocity.SqrLength());
			float speed_origin = proj->linearVelocity.Length();
			float R = data.rounding_radius;
			float R2 = R * R;

			auto caster = proj->shooter.get().get()->As<RE::Actor>();
			RE::NiPoint3 cast_dir = data.pattern.getCastDir({ caster->GetAngleX(), caster->GetAngleZ() });
			cast_dir.Unitize();
			Positioning::Plane plane(target_pos, cast_dir);

			float dir_z = -cast_dir.Dot(proj->GetPosition() - target_pos);
			RE::NiPoint2 P = plane.project(proj->GetPosition() - target_pos);
			RE::NiPoint2 vel = plane.project(proj->linearVelocity);

			float D2 = P.SqrLength();

			if (D2 > R2 * CIRCLE_K_BIG * CIRCLE_K_BIG) {
				// Out of cylinder
				// Move to tangent
				
				RE::NiPoint2 Q = { -P.y, P.x };
				float a = R2 / D2;
				float b = R / D2 * sqrtf(D2 - R2);
				if (P.Cross(vel) < 0)
					b *= -1;
				auto T3 = plane.unproject(P * a + Q * b);
				auto V = T3 - proj->GetPosition();
				float len = V.Unitize();
				auto my_len = speed_origin * dtime;
				float circle_len = my_len - len;
				if (circle_len > 0) {
					return FenixUtils::Geom::rotate(T3, circle_len / R, target_pos, cast_dir);
				} else {
					return V * my_len + proj->GetPosition();
				}
			} else if (D2 < R2 * CIRCLE_K_SML * CIRCLE_K_SML) {
				// Inside of cylinder
				// Slightly rotate velocity to tangent

				RE::NiPoint3 proj_dir_final = (proj->GetPosition() - target_pos).UnitCross(cast_dir);
				if (proj_dir_final.Dot(proj->linearVelocity) < 0) {
					proj_dir_final *= -1;
				}

				proj->linearVelocity =
					FenixUtils::Geom::rotateVel(proj->linearVelocity, dtime * speed_origin / R, proj_dir_final);

				return proj->GetPosition() + proj->linearVelocity * dtime;
			} else {
				// On cylinder
				// Move around the cylinder

				float L = dtime * speed_origin;
				float H = abs(dir_z);
				float t;

				if (L > H + 0.0001f) {
					t = std::min(1.0f, H / sqrtf(L * L - H * H));
				} else {
					t = 1;
				}

				float dl = L / sqrtf(1 + t * t);
				float dh = dl * t;
				float df = dl / R;
				if (P.Cross(vel) >= 0)
					df *= -1;
				auto ans = FenixUtils::Geom::rotate(proj->GetPosition(), df, target_pos, cast_dir);
				if (dir_z < 0)
					dh *= -1;
				ans += cast_dir * dh;
				return ans;
			}
		}

		void change_direction_linVel(RE::Projectile* proj, const RE::NiPoint3& target_pos, float speed_mult)
		{
			auto dir = target_pos - proj->GetPosition();
			auto dist = dir.Unitize();
			auto speed_origin = FenixUtils::Projectile__GetSpeed(proj);
			auto speed_needed = dist * speed_mult;
			float speed = std::min(speed_origin, speed_needed);
			proj->linearVelocity = dir * speed;
		}
		
		void change_direction(RE::Projectile* proj, RE::NiPoint3*, float dtime)
		{
			auto target_pos = get_target_point(proj);

			auto& data = Storage::get_data(get_follower_ind(proj));
			switch (data.rounding) {
			case Rounding::Sphere:
				//change_direction_rounding_sphere(proj, target_pos, dtime);
				break;
			case Rounding::Plane:
				//change_direction_rounding_plane(proj, target_pos, dtime);
				break;
			case Rounding::None:
			default:
				change_direction_linVel(proj, target_pos, data.speed_mult);
				break;
			}

			// Smooth rotating
			RE::NiPoint3 proj_dir;
			if (proj->linearVelocity.SqrLength() > 30.0f) {
				proj_dir = proj->linearVelocity;
			} else {
				auto proj_dir_final = FenixUtils::Geom::angles2dir(proj->shooter.get().get()->data.angle);
				auto proj_dir_cur = FenixUtils::Geom::angles2dir(proj->data.angle);
				proj_dir = FenixUtils::Geom::rotateVel(proj_dir_cur, data.speed_mult * dtime, proj_dir_final);
			}
			FenixUtils::Geom::Projectile::update_node_rotation(proj, proj_dir);
		}

		void change_direction_instant(RE::Projectile* proj, RE::NiPoint3* dV)
		{
			auto& data = Storage::get_data(get_follower_ind(proj));

			RE::NiPoint3 P;
			RE::NiPoint3 proj_dir;
			if (data.speed_mult == 0) {
				P = get_target_point(proj);
				proj_dir = FenixUtils::Geom::angles2dir(proj->shooter.get().get()->data.angle);
				proj_dir.Unitize();
			} else if (data.rounding == Rounding::Sphere) {
				P = get_target_point_rounding_sphere(proj, dV);
			} else if (data.rounding == Rounding::Plane) {
				P = get_target_point_rounding_plane(proj, dV);
			} else {
				return;
			}

			if (data.rounding == Rounding::Sphere || data.rounding == Rounding::Plane) {
				proj_dir = P - proj->GetPosition();
				proj_dir.Unitize();
				proj->linearVelocity = proj_dir * proj->linearVelocity.Length();
			}

			FenixUtils::Geom::Projectile::update_node_rotation(proj, proj_dir);
			*dV = P - proj->GetPosition();
		}
	}

	namespace Hooks
	{
		// Make projectile follow the caster
		class FollowingHook
		{
		public:
			static void Hook()
			{
				_Projectile__apply_gravity = SKSE::GetTrampoline().write_call<5>(REL::ID(43006).address() + 0x69,
					change_direction);  // SkyrimSE.exe+751309
				_Projectile__MovePoint = SKSE::GetTrampoline().write_call<5>(REL::ID(43006).address() + 0x85,
					change_direction_instant);  // 140751325
			}

		private:
			static bool change_direction(RE::Projectile* proj, RE::NiPoint3* dV, float dtime)
			{
				bool ans = _Projectile__apply_gravity(proj, dV, dtime);
				if (is_follower(proj)) {
					Moving::change_direction(proj, dV, dtime);
				}
				return ans;
			}

			static void change_direction_instant(RE::Projectile* proj, RE::NiPoint3* dV)
			{
				if (is_follower(proj)) {
					Moving::change_direction_instant(proj, dV);
				}

				_Projectile__MovePoint(proj, dV);
			}

			static inline REL::Relocation<decltype(change_direction)> _Projectile__apply_gravity;
			static inline REL::Relocation<decltype(change_direction_instant)> _Projectile__MovePoint;
		};

		class NoCollisionHook
		{
		public:
			static void Hook()
			{
				{
					// InitHavok
					struct Code : Xbyak::CodeGenerator
					{
						Code(uintptr_t func_addr)
						{
							// rdi == proj
							mov(rdx, rdi);
							mov(rax, func_addr);
							jmp(rax);
						}
					} xbyakCode{ uintptr_t(InitHavok__GetCollisionLayer) };

					_InitHavok__GetCollisionLayer = add_trampoline<5, 42934, 0x9d, true>(&xbyakCode);  // SkyrimSE.exe+74beed
				}

				{
					// TargetPick
					struct Code : Xbyak::CodeGenerator
					{
						Code(uintptr_t func_addr)
						{
							// r14 == proj
							mov(rdx, r14);
							mov(rax, func_addr);
							jmp(rax);
						}
					} xbyakCode{ uintptr_t(TargetPick__GetCollisionLayer) };

					_TargetPick__GetCollisionLayer = add_trampoline<5, 42982, 0x187, true>(&xbyakCode);  // SkyrimSE.exe+74ee97
				}

				{
					// PlayerSmth
					struct Code : Xbyak::CodeGenerator
					{
						Code(uintptr_t func_addr)
						{
							// rdi == proj
							mov(rdx, rdi);
							mov(rax, func_addr);
							jmp(rax);
						}
					} xbyakCode{ uintptr_t(PlayerSmth__GetCollisionLayer) };

					_PlayerSmth__GetCollisionLayer = add_trampoline<5, 37684, 0x680, true>(&xbyakCode);  // SkyrimSE.exe+62b0e0
				}

				{
					// MissileImpale
					struct Code : Xbyak::CodeGenerator
					{
						Code(uintptr_t func_addr)
						{
							// rsi == proj
							mov(rdx, rsi);
							mov(rax, func_addr);
							jmp(rax);
						}
					} xbyakCode{ uintptr_t(MissileImpale__GetCollisionLayer) };

					_MissileImpale__GetCollisionLayer = add_trampoline<5, 42855, 0xd0f, true>(&xbyakCode);  // SkyrimSE.exe+7467ef
				}
			}

		private:
			static RE::COL_LAYER GetCollisionLayer(RE::Projectile* proj, RE::COL_LAYER origin)
			{
				if (is_follower(proj)) {
					auto& data = Storage::get_data(get_follower_ind(proj));
					switch (data.collision) {
					case Collision::Actor:
						return RE::COL_LAYER(54);
					case Collision::None:
						return RE::COL_LAYER::kNonCollidable;
					case Collision::Spell:
					default:
						return origin;
					}
				}

				return origin;
			}

			static RE::COL_LAYER InitHavok__GetCollisionLayer(RE::BGSProjectile* bproj, RE::Projectile* proj)
			{
				return GetCollisionLayer(proj, _InitHavok__GetCollisionLayer(bproj));
			}
			static RE::COL_LAYER TargetPick__GetCollisionLayer(RE::BGSProjectile* bproj, RE::Projectile* proj)
			{
				return GetCollisionLayer(proj, _TargetPick__GetCollisionLayer(bproj));
			}
			static RE::COL_LAYER PlayerSmth__GetCollisionLayer(RE::BGSProjectile* bproj, RE::Projectile* proj)
			{
				return GetCollisionLayer(proj, _PlayerSmth__GetCollisionLayer(bproj));
			}
			static RE::COL_LAYER MissileImpale__GetCollisionLayer(RE::BGSProjectile* bproj, RE::Projectile* proj)
			{
				return GetCollisionLayer(proj, _MissileImpale__GetCollisionLayer(bproj));
			}
			
			using func_t = RE::COL_LAYER(RE::BGSProjectile*);
			static inline REL::Relocation<func_t> _InitHavok__GetCollisionLayer;
			static inline REL::Relocation<func_t> _TargetPick__GetCollisionLayer;
			static inline REL::Relocation<func_t> _PlayerSmth__GetCollisionLayer;
			static inline REL::Relocation<func_t> _MissileImpale__GetCollisionLayer;
		};
	}

	void get_unused_shape_ind(RE::Projectile* proj, std::set<uint32_t>& ans, const RE::BSTArray<RE::ProjectileHandle>& a)
	{
		auto baseType = get_follower_ind(proj);
		for (auto& i : a) {
			if (auto _proj = i.get().get(); _proj && get_follower_ind(_proj) == baseType && _proj->formID != proj->formID) {
				uint32_t ind = get_follower_shape_ind(_proj);
				ans.erase(ind);
			}
		}
	}

	uint32_t get_unused_shape_ind(RE::Projectile* proj)
	{
		std::set<uint32_t> ans;
		{
			auto& data = Storage::get_data(get_follower_ind(proj));
			int tmp = 0;
			std::generate_n(std::inserter(ans, ans.begin()), data.pattern.getSize(), [&tmp]() { return tmp++; });
		}

		auto manager = RE::Projectile::Manager::GetSingleton();
		get_unused_shape_ind(proj, ans, manager->limited);
		get_unused_shape_ind(proj, ans, manager->pending);
		get_unused_shape_ind(proj, ans, manager->unlimited);
		return ans.begin() == ans.end() ? -1 : *ans.begin();
	}

	forEachRes forEachFollower(RE::TESObjectREFR* a, const RE::BSTArray<RE::ProjectileHandle>& arr,
		const forEachF& func)
	{
		for (auto& i : arr) {
			if (auto proj = i.get().get(); proj && proj->shooter.get().get()) {
				if (proj->shooter.get().get()->formID == a->formID && is_follower(proj)) {
					if (func(proj) == forEachRes::kStop)
						return forEachRes::kStop;
				}
			}
		}
		return forEachRes::kContinue;
	}

	void forEachFollower(RE::TESObjectREFR* a, const forEachF& func)
	{
		auto manager = RE::Projectile::Manager::GetSingleton();
		if (forEachFollower(a, manager->limited, func) == forEachRes::kStop)
			return;
		if (forEachFollower(a, manager->pending, func) == forEachRes::kStop)
			return;
		if (forEachFollower(a, manager->unlimited, func) == forEachRes::kStop)
			return;
	}

	void disable(RE::Projectile* proj, bool restore_speed)
	{
		if (is_follower(proj)) {
			if (restore_speed) {
				auto& data = Storage::get_data(get_follower_ind(proj));
				if (data.rounding == Followers::Rounding::None) {
					float speed = FenixUtils::Projectile__GetSpeed(proj);
					proj->linearVelocity *= speed / proj->linearVelocity.Length();
				}
			}
			FenixUtils::Projectile__set_collision_layer(proj, RE::COL_LAYER::kSpell);
			disable_follower(proj);
		}
	}

	void apply(RE::Projectile* proj, uint32_t ind)
	{
		if (proj->IsMissileProjectile() && proj->shooter.get().get() && proj->shooter.get().get()->As<RE::Actor>()) {
			assert(ind > 0);
			
			set_follower_ind(proj, ind);

			auto& data = Storage::get_data(ind);

			if (!data.pattern.isShapeless()) {
				// TODO: optimize for MC
				auto new_ind = get_unused_shape_ind(proj);
				if (new_ind == -1 || new_ind >= Storage::get_data(ind).pattern.getSize())
					new_ind = 0;

				set_follower_shape_ind(proj, new_ind);
			}
		}
	}

	void install()
	{
		using namespace Hooks;
		FollowingHook::Hook();
		NoCollisionHook::Hook();
	}

	void init(const Json::Value& json_root)
	{
		if (json_root.isMember("FollowersData")) {
			Storage::init(json_root["FollowersData"]);
		}
	}

	void init_keys(const Json::Value& json_root)
	{
		if (json_root.isMember("FollowersData")) {
			Storage::init_keys(json_root["FollowersData"]);
		}
	}
}
