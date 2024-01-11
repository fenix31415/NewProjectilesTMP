#include "Triggers.h"
#include "TriggerFunctions.h"
#include "JsonUtils.h"

namespace Triggers
{
	struct Condition
	{
		enum class Hand : uint32_t
		{
			Both,
			Left,
			Right
		};

		enum class Type
		{
			Hand,
			ProjBaseIsFormID,
			EffectIsFormID,
			EffectHasKwd,
			EffectsIsFormID,
			EffectsHasKwd,
			SpellHasKwd,
			SpellIsFormID,
			CasterIsFormID,
			CasterBaseIsFormID,
			CasterHasKwd,
			WeaponBaseIsFormID,
			WeaponHasKwd
		} type;

		union
		{
			Hand hand;
			RE::FormID formid;
		};

	private:
		bool eval_Hand(RE::MagicSystem::CastingSource source) const
		{
			using Src = RE::MagicSystem::CastingSource;
			return hand == Hand::Both || hand == Hand::Left && source == Src::kLeftHand ||
			       hand == Hand::Right && (source == Src::kRightHand || source == Src::kInstant || source == Src::kOther);
		}
		bool eval_BaseIsFormID(RE::BGSProjectile* bproj) const { return bproj && bproj->formID == formid; }
		bool eval_EffectsHasKwd(RE::MagicItem* spel) const
		{
			if (spel) {
				for (auto eff : spel->effects) {
					if (eff->baseEffect->HasKeywordID(formid))
						return true;
				}
			}
			return false;
		}
		bool eval_EffectsIsFormID(RE::MagicItem* spel) const
		{
			if (spel) {
				for (auto eff : spel->effects) {
					if (eff->baseEffect->formID == formid)
						return true;
				}
			}
			return false;
		}
		bool eval_EffectHasKwd(RE::EffectSetting* mgef) const { return mgef ? mgef->HasKeywordID(formid) : false; }
		bool eval_EffectIsFormID(RE::EffectSetting* mgef) const { return mgef ? mgef->formID == formid : false; }
		bool eval_SpellHasKwd(RE::MagicItem* spel) const { return spel ? spel->HasKeywordID(formid) : false; }
		bool eval_SpellIsFormID(RE::MagicItem* spel) const { return spel ? spel->formID == formid : false; }
		bool eval_CasterIsFormID(RE::TESObjectREFR* caster) const { return caster && caster->formID == formid; }
		bool eval_CasterBaseIsFormID(RE::TESObjectREFR* caster) const
		{
			return caster && caster->GetBaseObject() && caster->GetBaseObject()->formID == formid;
		}
		bool eval_CasterHasKwd(RE::TESObjectREFR* caster_) const
		{
			if (caster_) {
				if (auto caster = caster_->As<RE::Actor>()) {
					if (auto kwd = RE::TESForm::LookupByID<RE::BGSKeyword>(formid)) {
						return caster->HasKeyword(kwd) || (caster->GetActorBase() && caster->GetActorBase()->HasKeyword(kwd)) ||
						       FenixUtils::TESObjectREFR__HasEffectKeyword(caster, kwd);
					}
				}
			}
			return false;
		}
		bool eval_WeaponBaseIsFormID(RE::TESObjectWEAP* weap) const { return weap && weap->formID == formid; }
		bool eval_WeaponHasKwd(RE::TESObjectWEAP* weap) const { return weap && weap->HasKeywordID(formid); }

	public:
		Condition(const std::string& filename, const std::string& type_name, const Json::Value& val) :
			type(JsonUtils::string2enum<Type>(type_name))
		{
			switch (type) {
				break;
			case Type::ProjBaseIsFormID:
			case Type::EffectIsFormID:
			case Type::EffectHasKwd:
			case Type::EffectsIsFormID:
			case Type::EffectsHasKwd:
			case Type::SpellHasKwd:
			case Type::SpellIsFormID:
			case Type::CasterIsFormID:
			case Type::CasterBaseIsFormID:
			case Type::CasterHasKwd:
			case Type::WeaponBaseIsFormID:
			case Type::WeaponHasKwd:
				formid = JsonUtils::get_formid(filename, val.asString());
				break;
			case Type::Hand:
				hand = JsonUtils::read_enum<Hand>(val.asString());
				break;
			default:
				assert(false);
				break;
			}
		}

		bool eval(Data* data) const
		{
			switch (type) {
			case Type::WeaponBaseIsFormID:
				return eval_WeaponBaseIsFormID(data->weap);
			case Type::WeaponHasKwd:
				return eval_WeaponHasKwd(data->weap);
			case Type::CasterIsFormID:
				return eval_CasterIsFormID(data->shooter);
			case Type::CasterBaseIsFormID:
				return eval_CasterBaseIsFormID(data->shooter);
			case Type::CasterHasKwd:
				return eval_CasterHasKwd(data->shooter);
			case Type::ProjBaseIsFormID:
				return eval_BaseIsFormID(data->bproj);
			case Type::SpellIsFormID:
				return eval_SpellIsFormID(data->spel);
			case Type::SpellHasKwd:
				return eval_SpellHasKwd(data->spel);
			case Type::EffectIsFormID:
				return eval_EffectIsFormID(data->mgef);
			case Type::EffectHasKwd:
				return eval_EffectHasKwd(data->mgef);
			case Type::EffectsIsFormID:
				return eval_EffectsIsFormID(data->spel);
			case Type::EffectsHasKwd:
				return eval_EffectsHasKwd(data->spel);
			case Type::Hand:
				return eval_Hand(data->hand);
			default:
				assert(false);
				return false;
			}
		}
	};
	static_assert(sizeof(Condition) == 0x8);

	struct Trigger
	{
	private:
		std::vector<Condition> conditions;
		TriggerFunctions::Functions functions;

		void call_functions(Data* data, RE::Projectile* proj, RE::Actor* targetOverride) const
		{
			functions.call(data, proj, targetOverride);
		}

		bool call_conditions(Data* data) const
		{
			for (const auto& cond : conditions) {
				if (!cond.eval(data))
					return false;
			}
			return true;
		}

	public:
		Trigger(const std::string& filename, const Json::Value& json_trigger) :
			functions(filename, json_trigger["TriggerFunctions"])
		{
			if (json_trigger.isMember("conditions")) {
				auto& json_conditions = json_trigger["conditions"];
				for (const auto& condition : json_conditions.getMemberNames()) {
					conditions.emplace_back(filename, condition, json_conditions[condition]);
				}
			}
		}

		void eval(Data* data, RE::Projectile* proj, RE::Actor* targetOverride) const
		{
			if (call_conditions(data))
				call_functions(data, proj, targetOverride);
		}

		bool should_disable_origin(Data* data) const { return call_conditions(data) && functions.should_disable_origin(); }
	};

	class Triggers
	{
		static inline std::array<std::vector<Trigger>, (uint32_t)Event::Total> triggers;

	public:
		static void clear()
		{
			for (auto& cur_triggers : triggers) {
				cur_triggers.clear();
			}
		}

		static void init(const std::string& filename, const Json::Value& json_triggers)
		{
			for (size_t i = 0; i < json_triggers.size(); i++) {
				auto& trigger = json_triggers[(int)i];

				auto type = JsonUtils::read_enum<Event>(trigger, "event");
				triggers[(uint32_t)type].emplace_back(filename, trigger);
			}
		}

		static void eval(Data* data, Event e, RE::Projectile* proj, RE::Actor* targetOverride)
		{
			for (const auto& trigger : triggers[(uint32_t)e]) {
				trigger.eval(data, proj, targetOverride);
			}
		}

		// Called on ProjAppeared
		static bool should_disable_origin(Data* data)
		{
			std::pair<bool, bool> ans{ true, false };
			for (const auto& trigger : triggers[(uint32_t)Event::ProjAppeared]) {
				if (trigger.should_disable_origin(data))
					return true;
			}
			return false;
		}
	};

	void clear() { Triggers::clear(); }

	void init(const std::string& filename, const Json::Value& json_root) { return Triggers::init(filename, json_root["Triggers"]); }

	void eval(Data* data, Event e, RE::Projectile* proj, RE::Actor* targetOverride)
	{
		Triggers::eval(data, e, proj, targetOverride);
	}

	namespace Hooks
	{
		class ApplyTriggersHook
		{
		public:
			static void Hook()
			{
				auto& trmp = SKSE::GetTrampoline();

				// arrow->unk140 = 0i64; with arrow=nullptr
				FenixUtils::writebytes<17693, 0xefa>("\x0F\x1F\x80\x00\x00\x00\x00"sv);
				// SkyrimSE.exe+2360C2 -- TESObjectWEAP::Fire_140235240
				_LaunchArrow = trmp.write_call<5>(REL::ID(17693).address() + 0xe82, LaunchArrow);

				// 1405504F5 -- MagicCaster::FireProjectileFromSource
				_FireProjectile1 = trmp.write_call<5>(REL::ID(33670).address() + 0x575, FireProjectile1);
				// SkyrimSE.exe+5504F5 -- MagicCaster::FireProjectile_0
				_FireProjectile2 = trmp.write_call<5>(REL::ID(33671).address() + 0x125, FireProjectile2);

				// 140628dd7 Actor::CombatHit
				_InitializeHitData = trmp.write_call<5>(REL::ID(37673).address() + 0x1b7, InitializeHitData);

				// 140628dc8 Actor::CombatHit
				_InitializeHitDataProj = trmp.write_call<5>(REL::ID(37673).address() + 0x1a8, InitializeHitDataProj);

				// 1407211ea HitFrameHandler::Handle
				_DoMeleeAttack = trmp.write_call<5>(REL::ID(41747).address() + 0x3a, DoMeleeAttack);

				// 14063311e Actor::MagicTarget::AddTarget
				_AddTarget = trmp.write_call<5>(REL::ID(37832).address() + 0x8e, AddTarget);

				// 140550a37 MagicCaster::FireProjectile
				_Launch1 = trmp.write_call<5>(REL::ID(33672).address() + 0x377, Launch1);

				// 140550a37 TESObjectWEAP::Fire
				_Launch2 = trmp.write_call<5>(REL::ID(17693).address() + 0xe82, Launch2);

				// SkyrimSE.exe+754bd8
				_CalcVelocityVector = trmp.write_call<5>(REL::ID(43030).address() + 0x3b8, CalcVelocityVector);

				// SkyrimSE.exe+74bc21 Proj::Kill
				_ClearFollowedObject = trmp.write_call<5>(REL::ID(42930).address() + 0x21, ClearFollowedObject);

				_HandleHits1 = REL::Relocation<uintptr_t>(RE::VTABLE_MissileProjectile[0]).write_vfunc(0xBE, HandleHits1);
				_HandleHits2 = REL::Relocation<uintptr_t>(RE::VTABLE_FlameProjectile[0]).write_vfunc(0xBE, HandleHits2);
				_HandleHits3 = REL::Relocation<uintptr_t>(RE::VTABLE_ConeProjectile[0]).write_vfunc(0xBE, HandleHits3);
				_HandleHits4 = REL::Relocation<uintptr_t>(RE::VTABLE_BeamProjectile[0]).write_vfunc(0xBE, HandleHits4);
				_HandleHits5 = REL::Relocation<uintptr_t>(RE::VTABLE_ArrowProjectile[0]).write_vfunc(0xBE, HandleHits5);

				// SkyrimSE.exe+7478cc MissileProj::AddImpact
				_AddImpact1 = trmp.write_call<5>(REL::ID(42866).address() + 0xbc, AddImpact1);
				// SkyrimSE.exe+735b06 ConeProj::AddImpact
				_AddImpact2 = trmp.write_call<5>(REL::ID(42633).address() + 0x66, AddImpact2);
			}

		private:
			static bool FireProjectile1(RE::MagicCaster* a, RE::BGSProjectile* bproj, RE::TESObjectREFR* a_char,
				RE::CombatController* a4, RE::NiPoint3* startPos, float rotationZ, float rotationX, uint32_t area, void* a9)
			{
				Data data(nullptr, a->GetCasterAsActor(), bproj, a->currentSpell,
					a->currentSpell ? a->currentSpell->GetAVEffect() : nullptr, nullptr, a->GetCastingSource(), Data::Type::Spell,
					{ rotationX, rotationZ }, *startPos);

				if (Triggers::should_disable_origin(&data)) {
					eval(&data, Event::ProjAppeared, nullptr);
					return false;
				} else {
					return _FireProjectile1(a, bproj, a_char, a4, startPos, rotationZ, rotationX, area, a9);
				}
			}
			static bool FireProjectile2(RE::MagicCaster* a, RE::BGSProjectile* bproj, RE::TESObjectREFR* a_char,
				RE::CombatController* a4, RE::NiPoint3* startPos, float rotationZ, float rotationX, uint32_t area, void* a9)
			{
				Data data(nullptr, a->GetCasterAsActor(), bproj, a->currentSpell,
					a->currentSpell ? a->currentSpell->GetAVEffect() : nullptr, nullptr, a->GetCastingSource(), Data::Type::Spell,
					{ rotationX, rotationZ }, *startPos);

				if (Triggers::should_disable_origin(&data)) {
					eval(&data, Event::ProjAppeared, nullptr);
					return false;
				} else {
					return _FireProjectile2(a, bproj, a_char, a4, startPos, rotationZ, rotationX, area, a9);
				}
			}

			static RE::ProjectileHandle* LaunchArrow(RE::ProjectileHandle* handle, RE::Projectile::LaunchData* a_ldata)
			{
				Data data(Data::Type::Arrow, a_ldata);
				if (Triggers::should_disable_origin(&data)) {
					eval(&data, Event::ProjAppeared, nullptr);
					handle->reset();
					return handle;
				} else {
					return _LaunchArrow(handle, a_ldata);
				}
			}

			static RE::ProjectileHandle* Launch1(RE::ProjectileHandle* handle, RE::Projectile::LaunchData* ldata)
			{
				auto ans = _Launch1(handle, ldata);

				Data data(Data::Type::Spell, ldata);
				if (auto proj = handle->get().get()) {
					eval(&data, Event::ProjAppeared, proj);
				}

				return ans;
			}
			static RE::ProjectileHandle* Launch2(RE::ProjectileHandle* handle, RE::Projectile::LaunchData* ldata)
			{
				auto ans = _Launch2(handle, ldata);

				Data data(Data::Type::Arrow, ldata);
				if (auto proj = handle->get().get()) {
					eval(&data, Event::ProjAppeared, proj);
				}

				return ans;
			}

			static void InitializeHitData(RE::HitData* hitdata, RE::Actor* attacker, RE::Actor* victim,
				RE::InventoryEntryData* weapitem, bool left)
			{
				_InitializeHitData(hitdata, attacker, victim, weapitem, left);

				Data data(weapitem ? weapitem->object->As<RE::TESObjectWEAP>() : nullptr, attacker, nullptr, nullptr, nullptr,
					nullptr, left ? RE::MagicSystem::CastingSource::kLeftHand : RE::MagicSystem::CastingSource::kRightHand,
					Data::Type::None, FenixUtils::Geom::rot_at(hitdata->hitDirection), hitdata->hitPosition);

				eval(&data, Event::HitMelee, nullptr);
				data.shooter = victim;
				eval(&data, Event::HitByMelee, nullptr);
			}

			static void InitializeHitDataProj(RE::HitData* hitdata, RE::Actor* attacker, RE::Actor* victim, RE::Projectile* proj)
			{
				_InitializeHitDataProj(hitdata, attacker, victim, proj);

				Data data(hitdata->weapon, attacker, proj->GetProjectileBase(), proj->spell, nullptr, proj->ammoSource,
					proj->castingSource, proj->ammoSource ? Data::Type::Arrow : Data::Type::Spell,
					FenixUtils::Geom::rot_at(hitdata->hitDirection), hitdata->hitPosition);

				eval(&data, Event::HitProjectile, nullptr);
				data.shooter = victim;
				eval(&data, Event::HitByProjectile, nullptr);
			}

			static void DoMeleeAttack(RE::Actor* a, bool left, char a3)
			{
				_DoMeleeAttack(a, left, a3);

				RE::MagicSystem::CastingSource hand =
					left ? RE::MagicSystem::CastingSource::kLeftHand : RE::MagicSystem::CastingSource::kRightHand;

				auto caster = a->GetMagicCaster(hand);
				if (!caster) {
					return;
				}

				auto invweap = a->GetAttackingWeapon();
				auto weap = invweap ? invweap->object->As<RE::TESObjectWEAP>() : nullptr;

				RE::NiPoint3 pos;
				if (auto node = caster->GetMagicNode()) {
					pos = node->world.translate;
				} else {
					pos = a->GetPosition();
					pos.z += (a->GetBoundMax().z - a->GetBoundMin().z) * 0.7f;
				}

				Data data(weap, a, nullptr, nullptr, nullptr, nullptr,
					left ? RE::MagicSystem::CastingSource::kLeftHand : RE::MagicSystem::CastingSource::kRightHand,
					Data::Type::None, { a->GetAimAngle(), a->GetAimHeading() }, std::move(pos));

				eval(&data, Event::Swing, nullptr);
			}

			static bool AddTarget(RE::MagicTarget* mtarget, RE::MagicTarget::AddTargetData* addData)
			{
				if (_AddTarget(mtarget, addData)) {
					auto a = (RE::Actor*)((char*)mtarget - 0x98);

					Data data(nullptr, a, nullptr, addData->magicItem, addData->effect ? addData->effect->baseEffect : nullptr,
						nullptr, addData->castingSource, Data::Type::None, { a->GetAimAngle(), a->GetAimHeading() },
						a->GetPosition());

					eval(&data, Event::EffectStart, nullptr);

					return true;
				} else {
					return false;
				}
			}

			static void CalcVelocityVector(RE::Projectile* proj)
			{
				if (proj->linearVelocity.Length() != 0) {
					TriggerFunctions::Function changeVel(proj->linearVelocity);
					_CalcVelocityVector(proj);
					Data data(proj);
					changeVel.eval(&data, proj);
				} else {
					_CalcVelocityVector(proj);
				}
			}

			static void ClearFollowedObject(RE::BSSoundHandle* shandle)
			{
				auto proj = (RE::Projectile*)((char*)shandle - 0x128);

				Data data(proj);
				eval(&data, Event::ProjDestroyed, nullptr);

				_ClearFollowedObject(shandle);
			}

			static bool OnHandleHits(RE::Projectile* proj, bool ans)
			{
				if (ans) {
					Data data(proj);
					eval(&data, Event::ProjHits, nullptr);
				}
				return ans;
			}

			static bool HandleHits1(RE::Projectile* proj, void* collector)
			{
				return OnHandleHits(proj, _HandleHits1(proj, collector));
			}
			static bool HandleHits2(RE::Projectile* proj, void* collector)
			{
				return OnHandleHits(proj, _HandleHits2(proj, collector));
			}
			static bool HandleHits3(RE::Projectile* proj, void* collector)
			{
				return OnHandleHits(proj, _HandleHits3(proj, collector));
			}
			static bool HandleHits4(RE::Projectile* proj, void* collector)
			{
				return OnHandleHits(proj, _HandleHits4(proj, collector));
			}
			static bool HandleHits5(RE::Projectile* proj, void* collector)
			{
				return OnHandleHits(proj, _HandleHits5(proj, collector));
			}

			static void* OnAddImpact(RE::Projectile* proj, void* ans, RE::NiPoint3& targetLoc)
			{
				if (ans) {
					Data data(proj);
					data.pos = targetLoc;
					eval(&data, Event::ProjImpact, nullptr);
				}
				return ans;
			}
			static void* AddImpact1(RE::Projectile* proj, RE::TESObjectREFR* refr, RE::NiPoint3& targetLoc,
				RE::NiPoint3* velocity_or_normal, RE::hkpCollidable* collidable, uint32_t shape_key, bool hit_happend)
			{
				return OnAddImpact(proj,
					_AddImpact1(proj, refr, targetLoc, velocity_or_normal, collidable, shape_key, hit_happend), targetLoc);
			}
			static void* AddImpact2(RE::Projectile* proj, RE::TESObjectREFR* refr, RE::NiPoint3& targetLoc,
				RE::NiPoint3* velocity_or_normal, RE::hkpCollidable* collidable, uint32_t shape_key, bool hit_happend)
			{
				return OnAddImpact(proj,
					_AddImpact2(proj, refr, targetLoc, velocity_or_normal, collidable, shape_key, hit_happend), targetLoc);
			}

			static inline REL::Relocation<decltype(AddImpact1)> _AddImpact1;
			static inline REL::Relocation<decltype(AddImpact2)> _AddImpact2;
			static inline REL::Relocation<decltype(HandleHits1)> _HandleHits1;
			static inline REL::Relocation<decltype(HandleHits2)> _HandleHits2;
			static inline REL::Relocation<decltype(HandleHits3)> _HandleHits3;
			static inline REL::Relocation<decltype(HandleHits4)> _HandleHits4;
			static inline REL::Relocation<decltype(HandleHits5)> _HandleHits5;
			static inline REL::Relocation<decltype(ClearFollowedObject)> _ClearFollowedObject;
			static inline REL::Relocation<decltype(CalcVelocityVector)> _CalcVelocityVector;
			static inline REL::Relocation<decltype(InitializeHitData)> _InitializeHitData;
			static inline REL::Relocation<decltype(InitializeHitDataProj)> _InitializeHitDataProj;
			static inline REL::Relocation<decltype(DoMeleeAttack)> _DoMeleeAttack;
			static inline REL::Relocation<decltype(AddTarget)> _AddTarget;
			static inline REL::Relocation<decltype(LaunchArrow)> _LaunchArrow;
			static inline REL::Relocation<decltype(FireProjectile1)> _FireProjectile1;
			static inline REL::Relocation<decltype(FireProjectile2)> _FireProjectile2;
			static inline REL::Relocation<decltype(Launch1)> _Launch1;
			static inline REL::Relocation<decltype(Launch2)> _Launch2;
		};
	}

	void install()
	{
		using namespace Hooks;

		ApplyTriggersHook::Hook();
	}
}
