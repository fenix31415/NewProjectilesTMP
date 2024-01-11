#include "JsonUtils.h"
#include "TriggerFunctions.h"
#include "Triggers.h"
#include "Homing.h"
#include "Positioning.h"
#include <random>

namespace Multicast
{
	using ProjectileRot = RE::Projectile::ProjectileRot;

	using Positioning::Shape;

	// Projectiles already placed to cast. Determine SP initial direction.
	enum class LaunchDir : uint32_t
	{
		Parallel,
		ToSight,
		ToCenter,
		FromCenter,

		// TODO: implement
		ToTarget
	};

	enum class SoundType : uint32_t
	{
		Every,
		Single,
		None
	};

	// A blueprint for setting projectiles
	struct SpawnGroupData
	{
		Positioning::Pattern pattern;  // 00

		// "NPC R UpperArm [RUar]" and "NPC L UpperArm [LUar]" is cool yeah.
		RE::NiPoint3 pos_rnd;      // 30 rnd offset for every individual proj
		ProjectileRot rot_offset;  // 3C offset of SP rotation from actual cast rotation
		ProjectileRot rot_rnd;     // 44 rnd rotation offset for every individual proj

		LaunchDir rot: 3;              // 48:00
		SoundType sound: 2;            // 48:03
		uint32_t rotation_target: 27;  // 48:05 used if rotation == ToTarget

		SpawnGroupData(const std::string& filename, const Json::Value& item) :
			pattern(item["Pattern"]), rot(JsonUtils::mb_read_field<LaunchDir::Parallel>(item, "rotation")),
			sound(JsonUtils::mb_read_field<SoundType::Single>(item, "sound")), pos_rnd(JsonUtils::mb_getPoint3(item, "posRnd")),
			rot_offset(JsonUtils::mb_getPoint2(item, "rotOffset")), rot_rnd(JsonUtils::mb_getPoint2(item, "rotRnd")),
			rotation_target(
				rot == LaunchDir::ToTarget ? Homing::get_key_ind(filename, JsonUtils::getString(item, "rotationTarget")) : 0)
		{}
	};
	static_assert(sizeof(SpawnGroupData) == 0x50);

	struct SpawnGroupStorage
	{
		static void clear_keys()
		{
			keys.clear();
		}
		static void clear()
		{
			clear_keys();
			data.clear();
		}

		static void init(const std::string& filename, const Json::Value& SpawnGroups)
		{
			for (auto& key : SpawnGroups.getMemberNames()) {
				read_json_entry(filename, key, SpawnGroups[key]);
			}
		}

		static void init_keys(const std::string& filename, const Json::Value& SpawnGroups)
		{
			for (auto& key : SpawnGroups.getMemberNames()) {
				read_json_entry_keys(filename, key, SpawnGroups[key]);
			}
		}

		static const auto& get_data(uint32_t ind) { return data[ind - 1]; }

		static uint32_t get_key_ind(const std::string& filename, const std::string& key) { return keys.get(filename, key); }

	private:
		static void read_json_entry(const std::string& filename, const std::string& key, const Json::Value& item)
		{
			[[maybe_unused]] uint32_t ind = get_key_ind(filename, key);
			assert(ind == data.size() + 1);

			data.emplace_back(filename, item);
		}

		static void read_json_entry_keys(const std::string& filename, const std::string& key, const Json::Value&)
		{
			keys.add(filename, key);
		}

		static inline JsonUtils::KeysMap keys;
		static inline std::vector<SpawnGroupData> data;
	};

	struct SpellData
	{
		uint32_t spellID;  // -1 = Current
	};

	struct ArrowData
	{
		uint32_t weapID;   // -1 = Current
		uint32_t arrowID;  // -1 = Current
	};

	struct SpellArrowData
	{
		std::variant<SpellData, ArrowData> data;

		static uint32_t currentOrID(const std::string& filename, const Json::Value& item, const std::string& field)
		{
			auto spellID = JsonUtils::getString(item, field);
			if (spellID == "Current")
				return CURRENT;
			else
				return JsonUtils::get_formid(filename, spellID);
		}

		static constexpr uint32_t CURRENT = static_cast<uint32_t>(-1);
	};

	enum class HomingDetectionType : uint32_t
	{
		Individual,
		Evenly
	};

	struct Data
	{
		Data(SpellArrowData origin_formIDs, TriggerFunctions::Functions functions, uint32_t pattern_ind,
			HomingDetectionType homing_setting, bool call_triggers) :
			origin_formIDs(std::move(origin_formIDs)),
			functions(std::move(functions)), pattern_ind(pattern_ind), homing_setting(homing_setting),
			call_triggers(call_triggers)
		{}

		SpellArrowData origin_formIDs;
		TriggerFunctions::Functions functions;
		uint32_t pattern_ind;                   // for SpawnGroupData
		HomingDetectionType homing_setting: 3;  // if new type is homing, how to chose targets
		uint32_t call_triggers: 1;
	};

	struct Storage
	{
		static void clear_keys() { keys.clear(); }
		static void clear()
		{
			clear_keys();
			data.clear();
		}

		static void init(const std::string& filename, const Json::Value& MulticastData)
		{
			for (auto& key : MulticastData.getMemberNames()) {
				read_json_entry(filename, key, MulticastData[key]);
			}
		}

		static void init_keys(const std::string& filename, const Json::Value& MulticastData)
		{
			for (auto& key : MulticastData.getMemberNames()) {
				read_json_entry_keys(filename, key, MulticastData[key]);
			}
		}

		static const auto& get_data(uint32_t ind) { return data[ind - 1]; }

		static uint32_t get_key_ind(const std::string& filename, const std::string& key) { return keys.get(filename, key); }

	private:
		static void read_json_entry(const std::string& filename, const std::string& key, const Json::Value& item)
		{
			[[maybe_unused]] uint32_t ind = get_key_ind(filename, key);
			assert(ind == data.size() + 1);

			data.push_back(std::vector<Data>());
			auto& new_data = data.back();

			for (int i = 0; i < (int)item.size(); i++) {
				read_json_entry_item(filename, new_data, item[i]);
			}
		}

		static void read_json_entry_keys(const std::string& filename, const std::string& key, const Json::Value&)
		{
			keys.add(filename, key);
		}

		static void read_json_entry_item(const std::string& filename, std::vector<Data>& new_data, const Json::Value& item)
		{
			SpellArrowData origin_formIDs;
			if (item.isMember("spellID")) {
				origin_formIDs.data = SpellData{};
				auto& spelldata = std::get<SpellData>(origin_formIDs.data);

				spelldata.spellID = SpellArrowData::currentOrID(filename, item, "spellID");
			} else if (item.isMember("weapID")) {
				origin_formIDs.data = ArrowData{};
				auto& arrowdata = std::get<ArrowData>(origin_formIDs.data);

				arrowdata.weapID = SpellArrowData::currentOrID(filename, item, "weapID");

				if (item.isMember("arrowID")) {
					arrowdata.arrowID = SpellArrowData::currentOrID(filename, item, "arrowID");
				} else {
					arrowdata.arrowID = SpellArrowData::CURRENT;
				}
			} else {
				assert(false);
			}

			TriggerFunctions::Functions functions;
			HomingDetectionType homing_detection =
				JsonUtils::mb_read_field<HomingDetectionType::Individual>(item, "HomingDetection");

			if (item.isMember("TriggerFunctions")) {
				functions = TriggerFunctions::Functions(filename, item["TriggerFunctions"]);
			}

			auto pattern_ind = SpawnGroupStorage::get_key_ind(filename, item["spawn_group"].asString());
			auto call_triggers = JsonUtils::mb_read_field<false>(item, "callTriggers");

			new_data.emplace_back(origin_formIDs, functions, pattern_ind, homing_detection, call_triggers);
		}

		static inline JsonUtils::KeysMap keys;
		static inline std::vector<std::vector<Data>> data;
	};

	uint32_t get_key_ind(const std::string& filename, const std::string& key) { return Storage::get_key_ind(filename, key); }

	namespace Sounds
	{
		RE::BGSSoundDescriptorForm* EffectSetting__get_sndr(RE::EffectSetting* a1, RE::MagicSystem::SoundID sid)
		{
			return _generic_foo_<11001, decltype(EffectSetting__get_sndr)>::eval(a1, sid);
		}

		void PlaySound_func3_140BEDB10(RE::BSSoundHandle* a1, RE::NiAVObject* source_node)
		{
			return _generic_foo_<66375, decltype(PlaySound_func3_140BEDB10)>::eval(a1, source_node);
		}

		char set_sound_position(RE::BSSoundHandle* shandle, float x, float y, float z)
		{
			return _generic_foo_<66370, decltype(set_sound_position)>::eval(shandle, x, y, z);
		}

		RE::BSSoundHandle tmpsound;

		void prepare(RE::BSSoundHandle& shandle, RE::MagicItem* spel, RE::TESObjectREFR* caster)
		{
			auto sid = RE::MagicSystem::SoundID::kRelease;
			auto eff = FenixUtils::getAVEffectSetting(spel);
			auto sndr = EffectSetting__get_sndr(eff, sid);
			// Release
			_generic_foo_<66382, bool(RE::BSSoundHandle&)>::eval(shandle);
			RE::BSAudioManager::GetSingleton()->BuildSoundDataFromDescriptor(shandle, sndr, 0);
			shandle.SetObjectToFollow(caster->Get3D());
		}

		void play_cast_sound(RE::TESObjectREFR* caster, RE::MagicItem* spel, const RE::NiPoint3& start_pos)
		{
			RE::BSSoundHandle shandle;

			auto sid = RE::MagicSystem::SoundID::kRelease;
			if (auto eff = FenixUtils::getAVEffectSetting(spel)) {
				if (auto sndr = EffectSetting__get_sndr(eff, sid)) {
					RE::BSAudioManager::GetSingleton()->BuildSoundDataFromDescriptor(shandle, sndr, 16);

					//shandle.SetPosition();
					//const auto& start_pos = caster->GetPosition();
					if (_generic_foo_<66370, bool(RE::BSSoundHandle&, float x, float y, float z)>::eval(shandle, start_pos.x,
							start_pos.y, start_pos.z)) {
						shandle.SetObjectToFollow(caster->Get3D());
						if (shandle.Play())
							logger::info("Q");
					}
				}
			}
		}

		void play_cast_sound__(RE::TESObjectREFR* caster, RE::MagicItem* spel, const RE::NiPoint3& start_pos)
		{
			//RE::BSSoundHandle shandle;
			RE::BSSoundHandle& shandle = tmpsound;
			if (!shandle.IsValid()) {
				prepare(shandle, spel, caster);
			}

			if (shandle.IsValid()) {
				if (shandle.IsPlaying())
					shandle.Stop();
				//shandle.SetPosition();
				const auto& start_pos_ = RE::PlayerCharacter::GetSingleton()->GetPosition();
				_generic_foo_<66382, bool(RE::BSSoundHandle&, float x, float y, float z)>::eval(shandle, start_pos_.x,
					start_pos_.y, start_pos_.z);
				start_pos;
				shandle.Play();
			}
		}

		void play_cast_sound_(RE::TESObjectREFR* caster, RE::MagicItem* spel, const RE::NiPoint3& start_pos)
		{
			if (auto root = caster->Get3D2()) {
				RE::BSSoundHandle shandle;
				auto sid = RE::MagicSystem::SoundID::kRelease;
				if (auto eff = FenixUtils::getAVEffectSetting(spel)) {
					if (auto sndr = EffectSetting__get_sndr(eff, sid)) {
						RE::BSAudioManager::GetSingleton()->BuildSoundDataFromDescriptor(shandle, sndr, 0);
						if (shandle.IsValid()) {
							PlaySound_func3_140BEDB10(&shandle, root);

							set_sound_position(&shandle, start_pos.x, start_pos.y, start_pos.z);
							shandle.Play();
						}
					}
				}
			}
		}
	}

	namespace Casting
	{
		struct CastData
		{
			ProjectileRot parallel_rot;
			RE::NiPoint3 start_pos;

			struct SpellData
			{
				RE::MagicItem* spel;
			};

			struct ArrowData
			{
				RE::TESObjectWEAP* weap;
				RE::TESAmmo* ammo;
			};

			std::variant<SpellData, ArrowData> spellarrow_data;
		};

		namespace Rotation
		{
			float add_rot_x(float val, float d)
			{
				const float PI = 3.1415926f;
				// -pi/2..pi/2
				d = d * PI / 180.0f;
				val += d;
				val = std::max(val, -PI / 2);
				val = std::min(val, PI / 2);
				return val;
			}

			float add_rot_z(float val, float d)
			{
				const float PI = 3.1415926f;
				// -pi/2..pi/2
				d = d * PI / 180.0f;
				val += d;
				while (val < 0) val += 2 * PI;
				while (val > 2 * PI) val -= 2 * PI;
				return val;
			}

			auto add_rot(ProjectileRot rot, ProjectileRot delta)
			{
				rot.x = add_rot_x(rot.x, delta.x);
				rot.z = add_rot_z(rot.z, delta.z);
				return rot;
			}

			auto add_rot_rnd(ProjectileRot rot, ProjectileRot rnd)
			{
				using FenixUtils::Random::FloatNeg1To1;

				if (rnd.x == 0 && rnd.z == 0)
					return rot;

				rot.x = add_rot_x(rot.x, rnd.x * FloatNeg1To1());
				rot.z = add_rot_z(rot.z, rnd.z * FloatNeg1To1());
				return rot;
			}

			auto add_point_rnd(const RE::NiPoint3& rnd)
			{
				using FenixUtils::Random::FloatNeg1To1;

				if (rnd.x == 0 && rnd.y == 0 && rnd.z == 0)
					return RE::NiPoint3{ 0, 0, 0 };

				return RE::NiPoint3{ rnd.x * FloatNeg1To1(), rnd.y * FloatNeg1To1(), rnd.z * FloatNeg1To1() };
			}
		}

		auto get_SPItem_rot(LaunchDir rot, const RE::NiPoint3& item_pos, const RE::NiPoint3& SP_center,
			const RE::NiPoint3& cast_dir, RE::TESObjectREFR* caster, RE::TESObjectREFR* target)
		{
			using FenixUtils::Geom::rot_at;

			switch (rot) {
			case LaunchDir::ToTarget:
				{
					if (target)
						return rot_at(item_pos, target->As<RE::Actor>() ?
													FenixUtils::Geom::Actor::AnticipatePos(target->As<RE::Actor>()) :
													target->GetPosition());
					else
						break;
				}
			case LaunchDir::FromCenter:
				return rot_at(SP_center, item_pos);
			case LaunchDir::ToCenter:
				return rot_at(item_pos, SP_center);
			case LaunchDir::ToSight:
				if (caster->As<RE::Actor>())
					return rot_at(item_pos, FenixUtils::Geom::Actor::raycast(caster->As<RE::Actor>()));
				else
					break;
			case LaunchDir::Parallel:
			default:
				break;
			}

			return rot_at(cast_dir);
		}

		// Given cur_proj pos, SP's CD
		// 1. Determine rotation
		//    1. Initial rotation determined right after blueprint constructed
		//    2. Added rot_offset
		//    3. Added rot_rnd
		// 2. Add rnd_offset to pos
		// 3. Launch the proj either as spell or as arrow
		auto multiCastGroupItem(RE::NiPoint3 pos, const Data& data, const CastData& SP_CD, bool withSound,
			const RE::NiPoint3& cast_dir, RE::TESObjectREFR* caster, RE::TESObjectREFR* target)
		{
			auto& pattern_data = SpawnGroupStorage::get_data(data.pattern_ind);

			ProjectileRot item_rot = get_SPItem_rot(pattern_data.rot, pos, SP_CD.start_pos, cast_dir, caster, target);

			item_rot = Rotation::add_rot(item_rot, pattern_data.rot_offset);
			item_rot = Rotation::add_rot_rnd(item_rot, pattern_data.rot_rnd);

			RE::NiPoint3 rnd_offset = Rotation::add_point_rnd(pattern_data.pos_rnd);
			rnd_offset = pattern_data.pattern.rotateDependsX(rnd_offset, SP_CD.parallel_rot);
			pos += rnd_offset;

			auto type = SP_CD.spellarrow_data.index();
			RE::ProjectileHandle handle;
			// SpellData
			if (type == 0) {
				auto spel = std::get<CastData::SpellData>(SP_CD.spellarrow_data).spel->As<RE::SpellItem>();
				assert(spel);

				RE::Projectile::LaunchSpell(&handle, caster, spel, pos, item_rot);

				if (withSound) {
					if (auto proj = handle.get().get())
						Sounds::play_cast_sound(proj, spel, pos);
				}
			}
			// ArrowData
			if (type == 1) {
				auto& arrow_data = std::get<CastData::ArrowData>(SP_CD.spellarrow_data);
				RE::Projectile::LaunchArrow(&handle, caster, arrow_data.ammo, arrow_data.weap, pos, item_rot);

				if (auto proj = handle.get().get()) {
					if (proj->power > 0) {
						proj->weaponDamage /= proj->power;
						proj->power = 0.75f;
						proj->weaponDamage *= proj->power;
					}

					// TODO: sound
				}
			}

			return handle;
		}
		
		// SP_CD has info about cast. Copied, because every SP has info itself.
		void multiCastGroup(CastData SP_CD, const Data& data, RE::TESObjectREFR* origin, RE::TESObjectREFR* caster)
		{
			const auto& spellarrow_data = data.origin_formIDs;

			auto type = spellarrow_data.data.index();
			// SpellData
			if (type == 0) {
				auto& spell_data = std::get<SpellData>(spellarrow_data.data);
				auto spell_id = spell_data.spellID;

				assert(SP_CD.spellarrow_data.index() == 0 || spell_id != SpellArrowData::CURRENT);

				if (SP_CD.spellarrow_data.index() != 0)
					SP_CD.spellarrow_data = CastData::SpellData{};

				if (spell_id != SpellArrowData::CURRENT)
					std::get<CastData::SpellData>(SP_CD.spellarrow_data).spel = RE::TESForm::LookupByID<RE::SpellItem>(spell_id);
			}
			// ArrowData
			if (type == 1) {
				auto& arrow_data = std::get<ArrowData>(spellarrow_data.data);

				auto arrow_id = arrow_data.arrowID;
				auto weap_id = arrow_data.weapID;

				assert(SP_CD.spellarrow_data.index() == 1 ||
					   (weap_id != SpellArrowData::CURRENT && arrow_id != SpellArrowData::CURRENT));

				if (SP_CD.spellarrow_data.index() != 1)
					SP_CD.spellarrow_data = CastData::ArrowData{};

				auto& arrowdata = std::get<CastData::ArrowData>(SP_CD.spellarrow_data);
				if (weap_id != SpellArrowData::CURRENT) {
					arrowdata.weap = RE::TESForm::LookupByID<RE::TESObjectWEAP>(weap_id);
				}
				if (arrow_id != SpellArrowData::CURRENT) {
					arrowdata.ammo = RE::TESForm::LookupByID<RE::TESAmmo>(arrow_id);
				}
			}

			auto& pattern_data = SpawnGroupStorage::get_data(data.pattern_ind);

			pattern_data.pattern.initCenter(SP_CD.start_pos, SP_CD.parallel_rot, origin);
			RE::NiPoint3 cast_dir = pattern_data.pattern.getCastDir(SP_CD.parallel_rot);
			cast_dir.Unitize();

			// Homing::Evenly and ToTarget support
			std::vector<RE::Actor*> targets;
			uint32_t homingInd = pattern_data.rotation_target;
			if (!homingInd)
				homingInd = data.functions.get_homing_ind(false);

			if (!homingInd)
				homingInd = data.functions.get_homing_ind(true);

			// homingInd may be 0 for ToTarget
			if (homingInd) {
				targets = Homing::get_targets(homingInd, caster, SP_CD.start_pos);

				std::random_device rd;
				std::mt19937 g(rd());
				std::shuffle(targets.begin(), targets.end(), g);
			}

			bool needsound_every = pattern_data.sound == SoundType::Every;
			bool needsound_single = type == 0 && pattern_data.sound == SoundType::Single;
			size_t target_ind = 0;

			Positioning::Plane plane(SP_CD.start_pos, cast_dir);
			for (size_t i = 0; i < pattern_data.pattern.getSize(); i++) {
				auto point = pattern_data.pattern.GetPosition(plane, cast_dir, i);

				RE::Actor* target = nullptr;

				if (targets.size()) {
					target = targets[target_ind++];
					if (target_ind >= targets.size())
						target_ind = 0;
				}

				auto handle = multiCastGroupItem(point, data, SP_CD, needsound_every || needsound_single && i == 0, cast_dir,
					caster, target);

				if (auto proj = handle.get().get()) {
					if (data.call_triggers) {
						Triggers::Data ldata(proj);
						Triggers::eval(&ldata, Triggers::Event::ProjAppeared, proj, target);
					}

					data.functions.call(proj, target);
				}
			}
		}
	}
	
	void apply(Triggers::Data* ldata, uint32_t ind)
	{
		using namespace Casting;

		CastData current_CD;
		current_CD.start_pos = ldata->pos;
		current_CD.parallel_rot = ldata->rot;
		switch (ldata->type) {
		case Triggers::Data::Type::Arrow:
			{
				assert(ldata->weap);
				assert(ldata->ammo);
				current_CD.spellarrow_data = CastData::ArrowData{};
				auto& arrow_data = std::get<CastData::ArrowData>(current_CD.spellarrow_data);
				arrow_data.weap = ldata->weap;
				arrow_data.ammo = ldata->ammo;
				break;
			}
		case Triggers::Data::Type::Spell:
			{
				assert(ldata->spel);
				current_CD.spellarrow_data = CastData::SpellData{};
				auto& spell_data = std::get<CastData::SpellData>(current_CD.spellarrow_data);
				spell_data.spel = ldata->spel;
				break;
			}
		case Triggers::Data::Type::None:
		default:
			// "Current" disallowed
			current_CD.spellarrow_data = CastData::SpellData{};
			break;
		}

		auto& data = Storage::get_data(ind);
		for (const auto& spawn_data : data) {
			multiCastGroup(current_CD, spawn_data, ldata->shooter, ldata->shooter);
		}
	}

	void install() {}

	void clear_keys()
	{
		SpawnGroupStorage::clear_keys();
		Storage::clear_keys();
	}
	void clear()
	{
		SpawnGroupStorage::clear();
		Storage::clear();
	}

	void init(const std::string& filename, const Json::Value& json_root)
	{
		if (json_root.isMember("MulticastSpawnGroups")) {
			SpawnGroupStorage::init(filename, json_root["MulticastSpawnGroups"]);
		}
		if (json_root.isMember("MulticastData")) {
			Storage::init(filename, json_root["MulticastData"]);
		}
	}

	void init_keys(const std::string& filename, const Json::Value& json_root)
	{
		if (json_root.isMember("MulticastSpawnGroups")) {
			SpawnGroupStorage::init_keys(filename, json_root["MulticastSpawnGroups"]);
		}
		if (json_root.isMember("MulticastData")) {
			Storage::init_keys(filename, json_root["MulticastData"]);
		}
	}
}
