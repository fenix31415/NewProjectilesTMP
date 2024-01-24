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

				// 1407211ea HitFrameHandler::Handle
				_DoMeleeAttack = trmp.write_call<5>(REL::ID(41747).address() + 0x3a, DoMeleeAttack);
				
				_EffectAddedC = REL::Relocation<uintptr_t>(RE::VTABLE_Character[4]).write_vfunc(0x8, EffectAddedC);
				_EffectAddedP = REL::Relocation<uintptr_t>(RE::VTABLE_PlayerCharacter[4]).write_vfunc(0x8, EffectAddedP);
				_EffectRemovedC = REL::Relocation<uintptr_t>(RE::VTABLE_Character[4]).write_vfunc(0x9, EffectRemovedC);
				_EffectRemovedP = REL::Relocation<uintptr_t>(RE::VTABLE_PlayerCharacter[4]).write_vfunc(0x9, EffectRemovedP);

				// 140550a37 MagicCaster::FireProjectile
				_Launch1 = trmp.write_call<5>(REL::ID(33672).address() + 0x377, Launch1);

				// 140550a37 TESObjectWEAP::Fire
				_Launch2 = trmp.write_call<5>(REL::ID(17693).address() + 0xe82, Launch2);

				// SkyrimSE.exe+754bd8
				_CalcVelocityVector = trmp.write_call<5>(REL::ID(43030).address() + 0x3b8, CalcVelocityVector);

				// SkyrimSE.exe+74bc21 Proj::Kill
				_ClearFollowedObject = trmp.write_call<5>(REL::ID(42930).address() + 0x21, ClearFollowedObject);

				// SkyrimSE.exe+7478cc MissileProj::AddImpact
				_AddImpact1 = trmp.write_call<5>(REL::ID(42866).address() + 0xbc, AddImpact1);
				// SkyrimSE.exe+735b06 ConeProj::AddImpact
				_AddImpact2 = trmp.write_call<5>(REL::ID(42633).address() + 0x66, AddImpact2);
				// SkyrimSE.exe+734232 BeamProj::AddImpact
				_AddImpact3 = trmp.write_call<5>(REL::ID(42594).address() + 0xc2, AddImpact3);
				// SkyrimSE.exe+73e757 FlameProj::AddImpact
				_AddImpact4 = trmp.write_call<5>(REL::ID(42736).address() + 0x167, AddImpact4);
				// SkyrimSE.exe+73fc9a GrenadeProj::AddImpact
				_AddImpact5 = trmp.write_call<5>(REL::ID(42768).address() + 0x8a, AddImpact5);
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

			static void EffectAdded(RE::MagicTarget* _this, RE::ActiveEffect* a_effect)
			{
				auto a = (RE::Actor*)((char*)_this - 0x98);

				auto effsetting = a_effect->GetBaseObject();
				

				Data data(nullptr, a, effsetting ? effsetting->data.projectileBase : nullptr, a_effect->spell, effsetting,
					nullptr, a_effect->castingSource, Data::Type::None, { a->GetAimAngle(), a->GetAimHeading() },
					a->GetPosition());

				eval(&data, Event::EffectStart, nullptr);
			}

			static void EffectAddedP(RE::MagicTarget* _this, RE::ActiveEffect* a_effect)
			{
				_EffectAddedP(_this, a_effect);
				if (a_effect) {
					EffectAdded(_this, a_effect);
				}
			}

			static void EffectAddedC(RE::MagicTarget* _this, RE::ActiveEffect* a_effect)
			{
				_EffectAddedC(_this, a_effect);
				if (a_effect) {
					EffectAdded(_this, a_effect);
				}
			}

			static void EffectRemoved(RE::MagicTarget* _this, RE::ActiveEffect* a_effect)
			{
				auto a = (RE::Actor*)((char*)_this - 0x98);

				auto effsetting = a_effect->GetBaseObject();

				Data data(nullptr, a, effsetting ? effsetting->data.projectileBase : nullptr, a_effect->spell, effsetting,
					nullptr, a_effect->castingSource, Data::Type::None, { a->GetAimAngle(), a->GetAimHeading() },
					a->GetPosition());

				eval(&data, Event::EffectEnd, nullptr);
			}

			static void EffectRemovedP(RE::MagicTarget* _this, RE::ActiveEffect* a_effect)
			{
				if (a_effect) {
					EffectRemoved(_this, a_effect);
				}
				_EffectRemovedP(_this, a_effect);
			}

			static void EffectRemovedC(RE::MagicTarget* _this, RE::ActiveEffect* a_effect)
			{
				if (a_effect) {
					EffectRemoved(_this, a_effect);
				}
				_EffectRemovedC(_this, a_effect);
			}

			static void CalcVelocityVector(RE::Projectile* proj)
			{
				if (proj->linearVelocity.x != 0 || proj->linearVelocity.y != 0 || proj->linearVelocity.z != 0) {
					if (proj->linearVelocity.x != 0) {
						RE::COL_LAYER layer = RE::COL_LAYER::kSpell;
						memcpy(&layer, &proj->linearVelocity.x, 4);
						FenixUtils::Projectile__set_collision_layer(proj, layer);
					}

					// if both 0, then it is Add 0, so fine if it is skipped
					if (proj->linearVelocity.y != 0 || proj->linearVelocity.z != 0) {
						TriggerFunctions::Function changeVel(proj->linearVelocity);
						_CalcVelocityVector(proj);
						Data data(proj);
						changeVel.eval(&data, proj);
					} else {
						_CalcVelocityVector(proj);
					}

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

			static RE::Projectile::ImpactData* OnAddImpact(RE::Projectile* proj, RE::Projectile::ImpactData* ans)
			{
				if (ans) {
					Data data(proj);
					data.pos = ans->desiredTargetLoc;
					data.rot = FenixUtils::Geom::rot_at(-ans->negativeVelocity);
					eval(&data, Event::ProjImpact, nullptr);
					if (auto target = ans->collidee.get().get(); target && target->As<RE::Actor>()) {
						eval(&data, Event::HitProjectile, nullptr);
						data.shooter = target;
						eval(&data, Event::HitByProjectile, nullptr);
					}
				}
				return ans;
			}
			static RE::Projectile::ImpactData* AddImpact1(RE::Projectile* proj, RE::TESObjectREFR* refr, RE::NiPoint3& targetLoc,
				RE::NiPoint3* velocity_or_normal, RE::hkpCollidable* collidable, uint32_t shape_key, bool hit_happend)
			{
				return OnAddImpact(proj,
					_AddImpact1(proj, refr, targetLoc, velocity_or_normal, collidable, shape_key, hit_happend));
			}
			static RE::Projectile::ImpactData* AddImpact2(RE::Projectile* proj, RE::TESObjectREFR* refr, RE::NiPoint3& targetLoc,
				RE::NiPoint3* velocity_or_normal, RE::hkpCollidable* collidable, uint32_t shape_key, bool hit_happend)
			{
				return OnAddImpact(proj,
					_AddImpact2(proj, refr, targetLoc, velocity_or_normal, collidable, shape_key, hit_happend));
			}
			static RE::Projectile::ImpactData* AddImpact3(RE::Projectile* proj, RE::TESObjectREFR* refr, RE::NiPoint3& targetLoc,
				RE::NiPoint3* velocity_or_normal, RE::hkpCollidable* collidable, uint32_t shape_key, bool hit_happend)
			{
				return OnAddImpact(proj,
					_AddImpact3(proj, refr, targetLoc, velocity_or_normal, collidable, shape_key, hit_happend));
			}
			static RE::Projectile::ImpactData* AddImpact4(RE::Projectile* proj, RE::TESObjectREFR* refr, RE::NiPoint3& targetLoc,
				RE::NiPoint3* velocity_or_normal, RE::hkpCollidable* collidable, uint32_t shape_key, bool hit_happend)
			{
				return OnAddImpact(proj,
					_AddImpact4(proj, refr, targetLoc, velocity_or_normal, collidable, shape_key, hit_happend));
			}
			static RE::Projectile::ImpactData* AddImpact5(RE::Projectile* proj, RE::TESObjectREFR* refr, RE::NiPoint3& targetLoc,
				RE::NiPoint3* velocity_or_normal, RE::hkpCollidable* collidable, uint32_t shape_key, bool hit_happend)
			{
				return OnAddImpact(proj,
					_AddImpact5(proj, refr, targetLoc, velocity_or_normal, collidable, shape_key, hit_happend));
			}

			static inline REL::Relocation<decltype(AddImpact1)> _AddImpact1;
			static inline REL::Relocation<decltype(AddImpact2)> _AddImpact2;
			static inline REL::Relocation<decltype(AddImpact3)> _AddImpact3;
			static inline REL::Relocation<decltype(AddImpact4)> _AddImpact4;
			static inline REL::Relocation<decltype(AddImpact5)> _AddImpact5;
			static inline REL::Relocation<decltype(ClearFollowedObject)> _ClearFollowedObject;
			static inline REL::Relocation<decltype(CalcVelocityVector)> _CalcVelocityVector;
			static inline REL::Relocation<decltype(InitializeHitData)> _InitializeHitData;
			static inline REL::Relocation<decltype(DoMeleeAttack)> _DoMeleeAttack;
			static inline REL::Relocation<decltype(EffectAddedC)> _EffectAddedC;
			static inline REL::Relocation<decltype(EffectAddedP)> _EffectAddedP;
			static inline REL::Relocation<decltype(EffectRemovedC)> _EffectRemovedC;
			static inline REL::Relocation<decltype(EffectRemovedP)> _EffectRemovedP;
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
