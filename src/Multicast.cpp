#include "JsonUtils.h"
#include "TriggerFunctions.h"
#include "Triggers.h"
#include "Homing.h"

namespace Multicast
{
	using ProjectileRot = RE::Projectile::ProjectileRot;

	// Given a pane and center position. Determine SP initial start position.
	enum class Shape : uint32_t
	{
		Single,
		HorizontalLine,
		VerticalLine,
		Circle,
		HalfCircle,
		FillSquare,
		FillCircle,
		Sphere,

		Total
	};
	static constexpr Shape Shape__DEFAULT = Shape::Single;

	// Projectiles already placed to cast. Determine SP initial direction.
	enum class LaunchDir : uint32_t
	{
		Parallel,
		ToSight,
		ToCenter,
		FromCenter
	};
	static constexpr LaunchDir LaunchDir__DEFAULT = LaunchDir::Parallel;

	enum class SoundType : uint32_t
	{
		Every,
		Single,
		None
	};
	static constexpr SoundType SoundType__DEFAULT = SoundType::Single;

	// A blueprint for setting projectiles
	struct SpawnGroupData
	{
		uint32_t count: 8;
		Shape shape: 4;
		uint32_t size: 14;  // size of a shape, if shape is not Single
		LaunchDir rot: 3;
		SoundType sound: 2;
		uint32_t normalDependsX: 1;  // used for armageddon

		RE::NiPoint3 normal;       // 04  determines a pane of SP
		RE::NiPoint3 pos_offset;   // 10  offset of SP center from actual cast pos
		RE::NiPoint3 pos_rnd;      // 1C  rnd offset for every individual proj
		ProjectileRot rot_offset;  // 28  offset of SP rotation from actual cast rotation
		ProjectileRot rot_rnd;     // 30  rnd rotation offset for every individual proj
	};
	static_assert(offsetof(SpawnGroupData, normal) == 0x04);
	static_assert(sizeof(SpawnGroupData) == 0x38);

	struct SpawnGroupStorage
	{
		static void init(const Json::Value& SpawnGroups)
		{
			for (auto& key : SpawnGroups.getMemberNames()) {
				read_json_entry(key, SpawnGroups[key]);
			}
		}

		static const auto& get_data(uint32_t ind) { return data[ind - 1]; }

		static void forget()
		{
			keys.init();
			data.clear();
		}

		static uint32_t get_key_ind(const std::string& key) { return keys.get(key); }

	private:
		static void read_json_entry(const std::string& key, const Json::Value& item)
		{
			uint32_t ind = keys.add(key);
			assert(ind == data.size() + 1);

			SpawnGroupData new_data;

			new_data.count = parse_enum_ifIsMember<1u>(item, "count");
			new_data.shape = parse_enum_ifIsMember<Shape__DEFAULT>(item, "shape");
			new_data.size = new_data.shape != Shape::Single ? parse_enum_ifIsMember<0u>(item, "size"sv) : 0u;
			new_data.rot = parse_enum_ifIsMember<LaunchDir__DEFAULT>(item, "rotation");
			new_data.sound = parse_enum_ifIsMember<SoundType__DEFAULT>(item, "sound");
			new_data.normalDependsX = !item.isMember("normal") || parse_enum_ifIsMember<true>(item, "xDepends"sv);

			new_data.normal = JsonUtils::readOrDefault3(item, "normal"sv);
			if (new_data.normal.SqrLength() < 0.0001f) {
				new_data.normal = { 0, 1, 0 };
			}
			new_data.pos_offset = JsonUtils::readOrDefault3(item, "posOffset"sv);
			new_data.pos_rnd = JsonUtils::readOrDefault3(item, "posRnd"sv);
			new_data.rot_offset = JsonUtils::readOrDefault2(item, "rotOffset"sv);
			new_data.rot_rnd = JsonUtils::readOrDefault2(item, "rotRnd"sv);

			data.push_back(new_data);
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

		static uint32_t currentOrID(const Json::Value& item, std::string_view field)
		{
			auto spellID = item[field.data()].asString();
			if (spellID == "Current")
				return CURRENT;
			else
				return JsonUtils::get_formid(spellID);
		}

		static constexpr uint32_t CURRENT = static_cast<uint32_t>(-1);
	};

	enum class HomingDetectionType : uint32_t
	{
		Individual,
		Evenly
	};
	static constexpr HomingDetectionType HomingDetectionType__DEFAULT = HomingDetectionType::Individual;

	struct Data
	{
		Data(SpellArrowData origin_formIDs, TriggerFunctions::Functions newtypes, uint32_t pattern_ind,
			HomingDetectionType homing_setting, bool call_triggers) :
			origin_formIDs(std::move(origin_formIDs)),
			newtypes(std::move(newtypes)), pattern_ind(pattern_ind), homing_setting(homing_setting), call_triggers(call_triggers)
		{}

		SpellArrowData origin_formIDs;
		TriggerFunctions::Functions newtypes;
		uint32_t pattern_ind;                   // for SpawnGroupData
		HomingDetectionType homing_setting: 3;  // if new type is homing, how to chose targets
		uint32_t call_triggers: 1;
	};

	struct Storage
	{
		static void init(const Json::Value& MulticastData)
		{
			for (auto& key : MulticastData.getMemberNames()) {
				read_json_entry(key, MulticastData[key]);
			}
		}

		static const auto& get_data(uint32_t ind) { return data[ind - 1]; }

		static void forget()
		{
			keys.init();
			data.clear();
		}

		static uint32_t get_key_ind(const std::string& key) { return keys.get(key); }

	private:
		static void read_json_entry(const std::string& key, const Json::Value& item)
		{
			uint32_t ind = keys.add(key);
			assert(ind == data.size() + 1);

			data.push_back(std::vector<Data>());
			auto& new_data = data.back();

			for (int i = 0; i < (int)item.size(); i++) {
				read_json_entry_item(new_data, item[i]);
			}
		}

		static void read_json_entry_item(std::vector<Data>& new_data, const Json::Value& item)
		{
			SpellArrowData origin_formIDs;
			if (item.isMember("spellID")) {
				origin_formIDs.data = SpellData{};
				auto& spelldata = std::get<SpellData>(origin_formIDs.data);

				spelldata.spellID = SpellArrowData::currentOrID(item, "spellID"sv);
			} else if (item.isMember("weapID")) {
				origin_formIDs.data = ArrowData{};
				auto& arrowdata = std::get<ArrowData>(origin_formIDs.data);

				arrowdata.weapID = SpellArrowData::currentOrID(item, "weapID"sv);

				if (item.isMember("arrowID")) {
					arrowdata.arrowID = SpellArrowData::currentOrID(item, "arrowID"sv);
				} else {
					arrowdata.arrowID = SpellArrowData::CURRENT;
				}
			} else {
				assert(false);
			}

			TriggerFunctions::Functions newtypes = { 0 };
			HomingDetectionType homing_detection;
			if (item.isMember("NewProjsType")) {
				newtypes = TriggerFunctions::parse_functions(item["NewProjsType"]);
				if (newtypes.homingInd)
					homing_detection =
						parse_enum_ifIsMember<HomingDetectionType__DEFAULT>(item["NewProjsType"], "homing_detection"sv);
			}

			auto pattern_ind = SpawnGroupStorage::get_key_ind(item["spawn_group"].asString());
			auto call_triggers = parse_enum_ifIsMember<false>(item, "callTriggers"sv);

			new_data.emplace_back(origin_formIDs, newtypes, pattern_ind, homing_detection, call_triggers);
		}

		static inline JsonUtils::KeysMap keys;
		static inline std::vector<std::vector<Data>> data;
	};

	void forget()
	{
		Storage::forget();
		SpawnGroupStorage::forget();
	}
	uint32_t get_key_ind(const std::string& key) { return Storage::get_key_ind(key); }

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

		void play_cast_sound(RE::TESObjectREFR* caster, RE::MagicItem* spel, const RE::NiPoint3& start_pos)
		{
			if (auto root = caster->Get3D2()) {
				RE::BSSoundHandle shandle;
				RE::BGSSoundDescriptorForm* sndr = nullptr;
				auto sid = RE::MagicSystem::SoundID::kRelease;
				if (auto eff = FenixUtils::getAVEffectSetting(spel)) {
					sndr = EffectSetting__get_sndr(eff, sid);
				}
				RE::BSAudioManager::GetSingleton()->BuildSoundDataFromDescriptor(shandle, sndr, 0);
				PlaySound_func3_140BEDB10(&shandle, root);

				set_sound_position(&shandle, start_pos.x, start_pos.y, start_pos.z);
				shandle.Play();
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

		RE::NiPoint3 rotate(RE::NiPoint3 normal, ProjectileRot parallel_rot, bool dependsX)
		{
			RE::NiMatrix3 M;
			M.EulerAnglesToAxesZXY(dependsX ? parallel_rot.x : 0, 0, parallel_rot.z);
			return M * normal;
		}

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
				if (rnd.x == 0 && rnd.z == 0)
					return rot;

				rot.x = add_rot_x(rot.x, rnd.x * FenixUtils::random_range(-1.0f, 1.0f));
				rot.z = add_rot_z(rot.z, rnd.z * FenixUtils::random_range(-1.0f, 1.0f));
				return rot;
			}

			auto add_point_rnd(const RE::NiPoint3& rnd)
			{
				using FenixUtils::random_range;

				if (rnd.x == 0 && rnd.y == 0 && rnd.z == 0)
					return RE::NiPoint3{ 0, 0, 0 };

				return RE::NiPoint3{ rnd.x * random_range(-1.0f, 1.0f), rnd.y * random_range(-1.0f, 1.0f),
					rnd.z * random_range(-1.0f, 1.0f) };
			}

			// get point `caster` is looking at
			auto raycast(RE::Actor* caster)
			{
				auto havokWorldScale = RE::bhkWorld::GetWorldScale();
				RE::bhkPickData pick_data;
				RE::NiPoint3 ray_start, ray_end;

				FenixUtils::Actor__get_eye_pos(caster, ray_start, 2);
				ray_end = ray_start + FenixUtils::rotate(20000, caster->data.angle);
				pick_data.rayInput.from = ray_start * havokWorldScale;
				pick_data.rayInput.to = ray_end * havokWorldScale;

				uint32_t collisionFilterInfo = 0;
				caster->GetCollisionFilterInfo(collisionFilterInfo);
				pick_data.rayInput.filterInfo = (static_cast<uint32_t>(collisionFilterInfo >> 16) << 16) |
				                                static_cast<uint32_t>(RE::COL_LAYER::kCharController);

				caster->GetParentCell()->GetbhkWorld()->PickObject(pick_data);
				RE::NiPoint3 hitpos;
				if (pick_data.rayOutput.HasHit()) {
					hitpos = ray_start + (ray_end - ray_start) * pick_data.rayOutput.hitFraction;
				} else {
					hitpos = ray_end;
				}
				return hitpos;
			}
		}

		struct Pane
		{
			RE::NiPoint3 startPos, right_dir, up_dir;
		};

		struct ShapeParams
		{
			float size;
			uint32_t count;
		};

		auto get_SPItem_rot(LaunchDir rot, const RE::NiPoint3& item_pos, const RE::NiPoint3& SP_center,
			const RE::NiPoint3& cast_dir, RE::TESObjectREFR* caster)
		{
			using FenixUtils::rot_at;

			switch (rot) {
			case LaunchDir::FromCenter:
				return rot_at(SP_center, item_pos);
			case LaunchDir::ToCenter:
				return rot_at(item_pos, SP_center);
			case LaunchDir::ToSight:
				if (caster->As<RE::Actor>())
					return rot_at(item_pos, Rotation::raycast(caster->As<RE::Actor>()));
				[[fallthrough]];
			case LaunchDir::Parallel:
			default:
				return rot_at(cast_dir);
			}
		}

		// Given cur_proj pos, SP's CD
		// 1. Determine rotation
		//    1. Initial rotation determined right after blueprint constructed
		//    2. Added rot_offset
		//    3. Added rot_rnd
		// 2. Add rnd_offset to pos
		// 3. Launch the proj either as spell or as arrow
		auto multiCastGroupItem(RE::NiPoint3 pos, const Data& data, const CastData& SP_CD, bool withSound,
			const RE::NiPoint3& cast_dir, RE::TESObjectREFR* caster)
		{
			auto& pattern_data = SpawnGroupStorage::get_data(data.pattern_ind);
			ProjectileRot item_rot = get_SPItem_rot(pattern_data.rot, pos, SP_CD.start_pos, cast_dir, caster);

			item_rot = Rotation::add_rot(item_rot, pattern_data.rot_offset);
			item_rot = Rotation::add_rot_rnd(item_rot, pattern_data.rot_rnd);

			RE::NiPoint3 rnd_offset = Rotation::add_point_rnd(pattern_data.pos_rnd);
			rnd_offset = rotate(rnd_offset, { SP_CD.parallel_rot.x, SP_CD.parallel_rot.z }, pattern_data.normalDependsX);
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
				}

			}

			if (auto proj = handle.get().get()) {
				data.newtypes.eval(proj);
			}

			return handle;
		}

		const std::array<std::function<void(const ShapeParams& shape_params, const Pane& plane,
							 const std::function<void(const RE::NiPoint3&)>& cast_item)>,
			(size_t)Shape::Total>
			MultiCasters = {  // Single
				[](ShapeParams shape_params, const Pane& plane, const std::function<void(RE::NiPoint3)>& cast_item) {
					for (size_t i = 0; i < shape_params.count; i++) {
						cast_item(plane.startPos);
					}
				},
				// HorizontalLine. Used in others.
				[](ShapeParams shape_params, const Pane& plane, const std::function<void(RE::NiPoint3)>& cast_item) {
					if (shape_params.count == 1) {
						cast_item(plane.startPos);
						return;
					}

					auto from = plane.startPos - plane.right_dir * (shape_params.size * 0.5f);
					float d = shape_params.size / (shape_params.count - 1);
					for (uint32_t i = 0; i < shape_params.count; i++) {
						cast_item(from);
						from += plane.right_dir * d;
					}
				},
				// VerticalLine. Uses HorizontalLine.
				[](ShapeParams shape_params, const Pane& plane, const std::function<void(RE::NiPoint3)>& cast_item) {
					Pane new_plane = plane;
					std::swap(new_plane.right_dir, new_plane.up_dir);
					MultiCasters[(size_t)Shape::HorizontalLine](shape_params, new_plane, cast_item);
				},
				// Circle
				[](ShapeParams shape_params, const Pane& plane, const std::function<void(RE::NiPoint3)>& cast_item) {
					for (uint32_t i = 0; i < shape_params.count; i++) {
						float alpha = 2 * 3.1415926f / shape_params.count * i;

						auto cur_p = plane.startPos + plane.right_dir * cos(alpha) * shape_params.size +
				                     plane.up_dir * sin(alpha) * shape_params.size;

						cast_item(cur_p);
					}
				},
				// HalfCircle
				[](ShapeParams shape_params, const Pane& plane, const std::function<void(RE::NiPoint3)>& cast_item) {
					if (shape_params.count == 1) {
						cast_item(plane.startPos);
						return;
					}

					for (uint32_t i = 0; i < shape_params.count; i++) {
						float alpha = 3.1415926f / (shape_params.count - 1) * i;

						auto cur_p = plane.startPos + plane.right_dir * cos(alpha) * shape_params.size +
				                     plane.up_dir * sin(alpha) * shape_params.size;

						cast_item(cur_p);
					}
				},
				// FillSquare
				[](ShapeParams shape_params, const Pane& plane, const std::function<void(RE::NiPoint3)>& cast_item) {
					uint32_t h = (uint32_t)ceil(sqrt(shape_params.count));

					if (h == 1) {
						MultiCasters[(size_t)Shape::HorizontalLine](shape_params, plane, cast_item);
					}

					auto from = plane.startPos + plane.up_dir * (shape_params.size * 0.5f);
					float d = shape_params.size / (h - 1);

					ShapeParams params = { shape_params.size, h };
					Pane cur_plane = plane;
					while (shape_params.count >= h) {
						cur_plane.startPos = from;
						MultiCasters[(uint32_t)Shape::HorizontalLine](params, cur_plane, cast_item);
						from -= plane.up_dir * d;
						shape_params.count -= h;
					}

					if (shape_params.count > 0) {
						cur_plane.startPos = from;
						params.count = shape_params.count;
						MultiCasters[(uint32_t)Shape::HorizontalLine](params, cur_plane, cast_item);
					}
				},
				// FillCircle
				[](ShapeParams shape_params, const Pane& plane, const std::function<void(RE::NiPoint3)>& cast_item) {
					float c = shape_params.size / sqrtf(static_cast<float>(shape_params.count));
					for (uint32_t i = 1; i <= shape_params.count; i++) {
						auto alpha = 2.3999632297286533222f * i;
						float r = c * sqrtf(static_cast<float>(i));

						auto cur_p = plane.startPos + (plane.right_dir * cos(alpha) + plane.up_dir * sin(alpha)) * r;

						cast_item(cur_p);
					}
				},
				// Sphere
				[](ShapeParams shape_params, const Pane& plane, const std::function<void(RE::NiPoint3)>& cast_item) {
					float c = shape_params.size;
					float phi = 3.883222077450933f;
					for (uint32_t i = 1; i <= shape_params.count; i++) {
						float y = 1 - (i / (shape_params.count - 1.0f)) * 2;
						float radius = sqrt(1 - y * y);
						float theta = phi * i;
						float x = cos(theta) * radius;
						float z = sin(theta) * radius;

						auto forward_dir = plane.up_dir.UnitCross(plane.right_dir);

						auto cur_p = plane.startPos + (plane.right_dir * x + plane.up_dir * z + forward_dir * y) * c;

						cast_item(cur_p);
					}
				}
			};

		// SP_CD has info about cast. Copied, because every SP has info itself.
		void multiCastGroup(CastData SP_CD, const Data& data, RE::TESObjectREFR* caster)
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

			SP_CD.start_pos += rotate(pattern_data.pos_offset, SP_CD.parallel_rot, pattern_data.normalDependsX);

			// SpellData
			if (type == 0 && pattern_data.sound == SoundType::Single)
				Sounds::play_cast_sound(caster, std::get<CastData::SpellData>(SP_CD.spellarrow_data).spel, SP_CD.start_pos);
			// TODO: ArrowData

			bool needsound = pattern_data.sound == SoundType::Every;

			float size = pattern_data.shape == Shape::Single ? 0 : static_cast<float>(pattern_data.size);

			RE::NiPoint3 cast_dir = rotate(pattern_data.normal, SP_CD.parallel_rot, pattern_data.normalDependsX);
			cast_dir.Unitize();
			RE::NiPoint3 right_dir = RE::NiPoint3(0, 0, -1).UnitCross(cast_dir);
			if (right_dir.SqrLength() == 0)
				right_dir = { 1, 0, 0 };
			RE::NiPoint3 up_dir = right_dir.Cross(cast_dir);

			draw_line(caster->GetPosition(), caster->GetPosition() + cast_dir * 100);

			// Homing::Evenly support
			std::vector<RE::TESObjectREFR*> targets;
			if (data.newtypes.homingInd && data.homing_setting == HomingDetectionType::Evenly) {
				auto& homing_data = Homing::get_data(data.newtypes.homingInd);
				targets = homing_data.target == Homing::TargetTypes::Cursor ?
				              Homing::Targeting::Cursor::get_cursor_targets(caster, homing_data) :
				              Homing::Targeting::get_nearest_targets(caster, SP_CD.start_pos, homing_data);
			}

			MultiCasters[(uint32_t)pattern_data.shape]({ size, pattern_data.count }, { SP_CD.start_pos, right_dir, up_dir },
				[&data, &SP_CD, needsound, &cast_dir, caster, &targets](const RE::NiPoint3& item_pos) {
					auto handle = multiCastGroupItem(item_pos, data, SP_CD, needsound, cast_dir, caster);
					
					if (targets.size()) {
						if (auto proj = handle.get().get()) {
							proj->desiredTarget = targets[FenixUtils::random_range(0, (int32_t)targets.size() - 1)]->GetHandle();
						}
					}
				});

			// TODO: initial direction to target
		}
	}

	void onCreated(RE::Projectile* proj, uint32_t ind)
	{
		using namespace Casting;

		CastData current_CD;
		current_CD.start_pos = proj->GetPosition();
		current_CD.parallel_rot = { proj->GetAngleX(), proj->GetAngleZ() };
		if (auto weap = proj->weaponSource) {
			current_CD.spellarrow_data = CastData::ArrowData{};
			auto& arrow_data = std::get<CastData::ArrowData>(current_CD.spellarrow_data);
			arrow_data.weap = weap;
			arrow_data.ammo = proj->ammoSource;
		} else {
			current_CD.spellarrow_data = CastData::SpellData{};
			auto& spell_data = std::get<CastData::SpellData>(current_CD.spellarrow_data);
			spell_data.spel = proj->spell;
		}

		auto& data = Storage::get_data(ind);
		for (const auto& spawn_data : data) {
			multiCastGroup(current_CD, spawn_data, proj->shooter.get().get());
		}
	}

	void onCreated(Ldata* ldata, uint32_t ind)
	{
		using namespace Casting;

		CastData current_CD;
		current_CD.start_pos = ldata->pos;
		current_CD.parallel_rot = ldata->rot;
		if (auto weap = ldata->weaponSource) {
			current_CD.spellarrow_data = CastData::ArrowData{};
			auto& arrow_data = std::get<CastData::ArrowData>(current_CD.spellarrow_data);
			arrow_data.weap = weap;
			arrow_data.ammo = ldata->ammoSource;
		} else {
			current_CD.spellarrow_data = CastData::SpellData{};
			auto& spell_data = std::get<CastData::SpellData>(current_CD.spellarrow_data);
			spell_data.spel = ldata->spell;
		}

		auto& data = Storage::get_data(ind);
		for (const auto& spawn_data : data) {
			multiCastGroup(current_CD, spawn_data, ldata->shooter);
		}
	}

	void install() {}

	void init(const Json::Value& json_root)
	{
		if (json_root.isMember("MulticastSpawnGroups")) {
			SpawnGroupStorage::init(json_root["MulticastSpawnGroups"]);
		}
		if (json_root.isMember("MulticastData")) {
			Storage::init(json_root["MulticastData"]);
		}
	}

	// TODO: cancel initial proj
}
