#include "TriggerFunctions.h"
#include "JsonUtils.h"
#include "RuntimeData.h"

namespace Emitters
{
	enum class Functions : uint32_t
	{
		NewProjType,          // call triggers in NewProjsType
		AccelerateToMaxSpeed  // accelerate until max speed, during given time
	};
	static constexpr Functions Functions__DEFAULT = Functions::NewProjType;

	union FunctionData
	{
		TriggerFunctions::Functions NewProjsType;
		float time;
	};

	struct Data
	{
		Functions type: 4;
		uint32_t destroy_after: 1;
		float interval;
		FunctionData data;
	};

	struct Storage
	{
		static void init(const Json::Value& HomingData)
		{
			for (auto& key : HomingData.getMemberNames()) {
				read_json_entry(key, HomingData[key]);
			}
		}

		static void init_keys(const Json::Value& HomingData)
		{
			for (auto& key : HomingData.getMemberNames()) {
				read_json_entry_keys(key, HomingData[key]);
			}
		}

		static const auto& get_data(uint32_t ind) { return data_static[ind - 1]; }

		static void forget()
		{
			keys.init();
			data_static.clear();
		}

		static uint32_t get_key_ind(const std::string& key) { return keys.get(key); }

	private:
		static void read_json_entry(const std::string& key, const Json::Value& item)
		{
			uint32_t ind = keys.get(key);
			assert(ind == data_static.size() + 1);

			Data new_data = {};
			new_data.interval = item["interval"].asFloat();
			new_data.destroy_after = parse_enum_ifIsMember<false>(item, "destroyAfter"sv);
			new_data.type = parse_enum<Functions__DEFAULT>(item["type"].asString());
			switch (new_data.type) {
			case Functions::AccelerateToMaxSpeed:
				new_data.data.time = item["time"].asFloat();
				break;
			case Functions::NewProjType:
			default:
				new_data.data.NewProjsType = TriggerFunctions::Functions(item["NewProjsType"]);
				break;
			}

			data_static.push_back(new_data);
		}

		static void read_json_entry_keys(const std::string& key, const Json::Value&) {
			keys.add(key);
		}

		static inline JsonUtils::KeysMap keys;
		static inline std::vector<Data> data_static;
	};

	void forget() { Storage::forget(); }
	uint32_t get_key_ind(const std::string& key) { return Storage::get_key_ind(key); }

	void init(const Json::Value& json_root)
	{
		if (json_root.isMember("EmittersData")) {
			Storage::init(json_root["EmittersData"]);
		}
	}

	void init_keys(const Json::Value& json_root)
	{
		if (json_root.isMember("EmittersData")) {
			Storage::init_keys(json_root["EmittersData"]);
		}
	}

	void set_emitter_ind(RE::Projectile* proj, uint32_t ind) { ::set_emitter_ind(proj, ind); }
	uint32_t get_emitter_ind(RE::Projectile* proj) { return ::get_emitter_ind(proj); }
	bool is_emitter(RE::Projectile* proj) { return get_emitter_ind(proj) != 0; }
	void disable_emitter(RE::Projectile* proj) { set_emitter_ind(proj, 0); }

	void onCreated(RE::Projectile* proj, uint32_t ind)
	{
		if (proj->IsMissileProjectile()) {
			set_emitter_ind(proj, ind == static_cast<uint32_t>(-1) ? 0 : ind);
		}
	}

	void onUpdate(RE::Projectile* proj, float dtime)
	{
		auto emitter_ind = get_emitter_ind(proj);

		auto& data = Storage::get_data(emitter_ind);

		if (proj->livingTime < data.interval)
			return;

		proj->livingTime = 0.000001f;

		switch (data.type) {
		case Functions::NewProjType:
			data.data.NewProjsType.eval(proj);
			break;
		case Functions::AccelerateToMaxSpeed:
			{
				// y = a*e^bx, a = M/X, b = ln X / N
				//constexpr float X = 70.0f;
				constexpr float LN_X = 4.248495242049359f;  // ln 70
				float b = LN_X / data.data.time;
				float cur_speed = proj->linearVelocity.Length();
				float add_to_speed = exp(b * dtime);
				float new_speed = cur_speed * add_to_speed;
				proj->linearVelocity *= (new_speed / cur_speed);
			}
			break;
		default:
			break;
		}

		if (data.destroy_after) {
			_generic_foo_<42930, void(RE::Projectile*)>::eval(proj);
		}
	}

	namespace Hooks
	{
		class EmitterHook
		{
		public:
			static void Hook()
			{
				_CheckExplosion = SKSE::GetTrampoline().write_call<5>(REL::ID(42852).address() + 0x680,
					CheckExplosion);  // SkyrimSE.exe+745450
				_AddImpact = SKSE::GetTrampoline().write_call<5>(REL::ID(42547).address() + 0x56,
					AddImpact);  // SkyrimSE.exe+732456 -- disable on hit
				_BSSoundHandle__ClearFollowedObject = SKSE::GetTrampoline().write_call<5>(REL::ID(42930).address() + 0x21,
					BSSoundHandle__ClearFollowedObject);  // SkyrimSE.exe+74BC21 -- disable on kill
			}

		private:
			static RE::Explosion* CheckExplosion(RE::MissileProjectile* proj, float dtime)
			{
				if (is_emitter(proj)) {
					onUpdate(proj, dtime);
				}

				return _CheckExplosion(proj, dtime);
			}

			static void* AddImpact(RE::Projectile* proj, RE::TESObjectREFR* a2, RE::NiPoint3* a3, RE::NiPoint3* a_velocity,
				RE::hkpCollidable* a_collidable, uint32_t a6, char a7)
			{
				auto ans = _AddImpact(proj, a2, a3, a_velocity, a_collidable, a6, a7);
				if (is_emitter(proj)) {
					disable_emitter(proj);
				}
				return ans;
			}

			static void BSSoundHandle__ClearFollowedObject(char* sound)
			{
				_BSSoundHandle__ClearFollowedObject(sound);
				auto proj = reinterpret_cast<RE::Projectile*>(sound - 0x128);
				if (is_emitter(proj)) {
					disable_emitter(proj);
				}
			}

			static inline REL::Relocation<decltype(CheckExplosion)> _CheckExplosion;
			static inline REL::Relocation<decltype(AddImpact)> _AddImpact;
			static inline REL::Relocation<decltype(BSSoundHandle__ClearFollowedObject)> _BSSoundHandle__ClearFollowedObject;
		};
	}

	void install() { Hooks::EmitterHook::Hook(); }
}
