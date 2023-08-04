#pragma once

#include "JsonUtils.h"
#include "TriggerFunctions.h"

struct Ldata
{
	RE::BGSProjectile* projectileBase;
	RE::MagicItem* spell;
	RE::TESAmmo* ammoSource;
	RE::TESObjectWEAP* weaponSource;
	RE::TESObjectREFR* shooter;
	RE::Projectile::ProjectileRot rot;
	RE::NiPoint3 pos;
};

namespace Triggers
{
	enum class TriggerConditions : uint32_t
	{
		BaseIsFormID,
		EffectHasKwd,
		SpellHasKwd,
		CasterIsFormID,
		CasterBaseIsFormID,
		CasterHasKwd,
		WeaponBaseIsFormID,
		WeaponHasKwd,

		Total
	};

	static const std::array<std::function<bool(RE::Projectile*, uint32_t)>, static_cast<size_t>(TriggerConditions::Total)>
		ConditionsFunctors = {
			// BaseIsFormID
			[](RE::Projectile* proj, uint32_t value) -> bool { return proj->GetProjectileBase()->formID == value; },
			// EffectHasKwd
			[](RE::Projectile* proj, uint32_t value) -> bool {
				if (auto spel = proj->spell) {
					for (const auto& eff : spel->effects) {
						if (eff->baseEffect->HasKeywordID(value))
							return true;
					}
				}
				return false;
			},
			// SpellHasKwd
			[](RE::Projectile* proj, uint32_t value) -> bool {
				if (auto spel = proj->spell) {
					return spel->HasKeywordID(value);
				}
				return false;
			},
			// CasterIsFormID
			[](RE::Projectile* proj, uint32_t value) -> bool {
				auto caster = proj->shooter.get().get();
				return caster && caster->formID == value;
			},
			// CasterBaseIsFormID
			[](RE::Projectile* proj, uint32_t value) -> bool {
				auto caster = proj->shooter.get().get();
				return caster && caster->GetBaseObject() && caster->GetBaseObject()->formID == value;
			},
			// CasterHasKwd
			[](RE::Projectile* proj, uint32_t value) -> bool {
				if (auto caster_ = proj->shooter.get().get()) {
					if (auto caster = caster_->As<RE::Actor>()) {
						if (auto kwd = RE::TESForm::LookupByID<RE::BGSKeyword>(value)) {
							return caster->HasKeyword(kwd) ||
					               (caster->GetActorBase() && caster->GetActorBase()->HasKeyword(kwd)) ||
					               FenixUtils::TESObjectREFR__HasEffectKeyword(caster, kwd);
						}
					}
				}
				return false;
			},
			// WeaponBaseIsFormID
			[](RE::Projectile* proj, uint32_t value) -> bool {
				return proj->weaponSource && proj->weaponSource->formID == value;
			},
			// WeaponHasKwd
			[](RE::Projectile* proj, uint32_t value) -> bool {
				return proj->weaponSource && proj->weaponSource->HasKeywordID(value);
			}
		};

	static const std::array<std::function<bool(Ldata*, uint32_t)>, static_cast<size_t>(TriggerConditions::Total)>
		ConditionsFunctorsLdata = {
			// BaseIsFormID
			[](Ldata* ldata, uint32_t value) -> bool {
				auto base = ldata->projectileBase;
				return base->formID == value;
			},
			// EffectHasKwd
			[](Ldata* ldata, uint32_t value) -> bool {
				if (auto spel = ldata->spell) {
					for (const auto& eff : spel->effects) {
						if (eff->baseEffect->HasKeywordID(value))
							return true;
					}
				}
				return false;
			},
			// SpellHasKwd
			[](Ldata* ldata, uint32_t value) -> bool {
				if (auto spel = ldata->spell) {
					return spel->HasKeywordID(value);
				}
				return false;
			},
			// CasterIsFormID
			[](Ldata* ldata, uint32_t value) -> bool {
				auto caster = ldata->shooter;
				return caster && caster->formID == value;
			},
			// CasterBaseIsFormID
			[](Ldata* ldata, uint32_t value) -> bool {
				auto caster = ldata->shooter;
				return caster && caster->GetBaseObject() && caster->GetBaseObject()->formID == value;
			},
			// CasterHasKwd
			[](Ldata* ldata, uint32_t value) -> bool {
				if (auto caster_ = ldata->shooter) {
					if (auto caster = caster_->As<RE::Actor>()) {
						if (auto kwd = RE::TESForm::LookupByID<RE::BGSKeyword>(value)) {
							return caster->HasKeyword(kwd) ||
					               (caster->GetActorBase() && caster->GetActorBase()->HasKeyword(kwd)) ||
					               FenixUtils::TESObjectREFR__HasEffectKeyword(caster, kwd);
						}
					}
				}
				return false;
			}
		};

	enum class CasterTypes : uint32_t
	{
		Both,    // Allow to cast custom projectile for both
		Player,  // Allow to cast custom projectile only for player
		NPC      // Allow to cast custom projectile only for NPC
	};
	static constexpr CasterTypes CasterTypes__DEFAULT = CasterTypes::Both;

	class Trigger
	{
		CasterTypes caster_type: 2;
		TriggerConditions condition: 4;
		uint32_t disable_origin: 1;
		uint32_t value;
		TriggerFunctions::Functions functions;

		bool eval_condition_caster(RE::TESObjectREFR* caster) const {
			if (!caster)
				return false;

			bool isPlayer = caster->IsPlayerRef();
			bool caster_ok = caster_type == CasterTypes::NPC && !isPlayer || caster_type == CasterTypes::Player && isPlayer ||
			                 caster_type == CasterTypes::Both;
			return caster_ok;
		}

		bool eval_condition(RE::Projectile* proj) const
		{
			return eval_condition_caster(proj->shooter.get().get()) &&
			       ConditionsFunctors[static_cast<size_t>(condition)](proj, value);
		}

		bool eval_condition(Ldata* ldata) const
		{
			return eval_condition_caster(ldata->shooter) && ConditionsFunctorsLdata[static_cast<size_t>(condition)](ldata, value);
		}

		void eval_function(Ldata* ldata) const { functions.eval(ldata); }

		void eval_function(RE::Projectile* proj) const { functions.eval(proj); }

		void eval_function3dLoaded(RE::Projectile* proj) const { functions.eval3dLoaded(proj); }

	public:
		Trigger(CasterTypes caster_type, TriggerConditions condition, bool disable_origin, uint32_t value,
			TriggerFunctions::Functions functions) :
			caster_type(caster_type),
			condition(condition), disable_origin(disable_origin), value(value), functions(std::move(functions))
		{}

		void eval(RE::Projectile* proj) const
		{
			if (eval_condition(proj))
				eval_function(proj);
		}

		void eval3dLoaded(RE::Projectile* proj) const
		{
			if (eval_condition(proj))
				eval_function3dLoaded(proj);
		}

		void eval(Ldata* ldata) const
		{
			if (eval_condition(ldata))
				eval_function(ldata);
		}

		bool should_disable_origin(Ldata* ldata) const { return disable_origin && eval_condition(ldata); }

#ifdef DEBUG
		uint32_t get_homing_ind(RE::BGSProjectile* bproj) const
		{
			if (condition != TriggerConditions::BaseIsFormID)
				return 0;

			if (bproj->formID == value)
				return functions.get_homing_ind();

			return 0;
		}
#endif  // DEBUG

	};
	static_assert(sizeof(Trigger) == 0x24);

	class Triggers
	{
		static inline std::vector<Trigger> triggers;

	public:
		static void init(const Json::Value& json_root)
		{
			triggers.clear();

			auto& json_triggers = json_root["Triggers"];

			for (size_t i = 0; i < json_triggers.size(); i++) {
				auto& trigger = json_triggers[(int)i];

				CasterTypes caster = parse_enum_ifIsMember<CasterTypes__DEFAULT>(trigger, "caster"sv);
				TriggerConditions condition = parse_enum<TriggerConditions::BaseIsFormID>(trigger["condition"].asString());
				uint32_t value = JsonUtils::get_formid(trigger["value"].asString());
				bool disable_origin = parse_enum_ifIsMember<false>(trigger, "disableOrigin"sv);

				TriggerFunctions::Functions functions(trigger["TriggerFunctions"]);
				triggers.emplace_back(caster, condition, disable_origin, value, functions);
			}
		}

		static void onCreated(RE::Projectile* proj)
		{
			for (const auto& trigger : triggers) {
				trigger.eval(proj);
			}
		}

		static void on3dLoaded(RE::Projectile* proj)
		{
			for (const auto& trigger : triggers) {
				trigger.eval3dLoaded(proj);
			}
		}

		static void onCreated(Ldata* ldata)
		{
			for (const auto& trigger : triggers) {
				trigger.eval(ldata);
			}
		}

		static bool should_disable_origin(Ldata* ldata)
		{
			for (const auto& trigger : triggers) {
				if (trigger.should_disable_origin(ldata))
					return true;
			}

			return false;
		}

#ifdef DEBUG
		static uint32_t get_homing_ind(RE::BGSProjectile* bproj) {
			for (const auto& trigger : triggers) {
				if (uint32_t id = trigger.get_homing_ind(bproj))
					return id;
			}

			return 0;
		}
#endif
	};

	namespace Hooks
	{
		// If you use Projectile::Launch itself, set types manually
		class ApplyTriggersHook
		{
		public:
			static void Hook()
			{
				// SkyrimSE.exe+16CDD0 -- placeatme
				_Usage1 = SKSE::GetTrampoline().write_call<5>(REL::ID(13629).address() + 0x130, Launch_1);
				// SkyrimSE.exe+2360C2 -- TESObjectWEAP::Fire_140235240
				_Usage2 = SKSE::GetTrampoline().write_call<5>(REL::ID(17693).address() + 0xe82, Launch_2);
				// SkyrimSE.exe+550A37 -- ActorMagicCaster::castProjectile
				_Usage3 = SKSE::GetTrampoline().write_call<5>(REL::ID(33672).address() + 0x377, Launch_3);
				// SkyrimSE.exe+5A897E -- ChainExplosion
				_Usage4 = SKSE::GetTrampoline().write_call<5>(REL::ID(35450).address() + 0x20e, Launch_4);
			}

		private:
			static void set_custom_type(uint32_t handle)
			{
				RE::TESObjectREFRPtr _refr;
				RE::LookupReferenceByHandle(handle, _refr);
				if (auto proj = _refr.get() ? _refr.get()->As<RE::Projectile>() : nullptr) {
					Triggers::onCreated(proj);
				}
			}

			static uint32_t* Launch_1(uint32_t* handle, RE::Projectile::LaunchData* ldata)
			{
				handle = _Usage1(handle, ldata);
				set_custom_type(*handle);
				return handle;
			}
			static uint32_t* Launch_2(uint32_t* handle, RE::Projectile::LaunchData* ldata)
			{
				handle = _Usage2(handle, ldata);
				set_custom_type(*handle);
				return handle;
			}
			static uint32_t* Launch_3(uint32_t* handle, RE::Projectile::LaunchData* ldata)
			{
				handle = _Usage3(handle, ldata);
				set_custom_type(*handle);
				return handle;
			}
			static uint32_t* Launch_4(uint32_t* handle, RE::Projectile::LaunchData* ldata)
			{
				handle = _Usage4(handle, ldata);
				set_custom_type(*handle);
				return handle;
			}

			using func_type = decltype(Launch_1);

			static inline REL::Relocation<func_type> _Usage1;
			static inline REL::Relocation<func_type> _Usage2;
			static inline REL::Relocation<func_type> _Usage3;
			static inline REL::Relocation<func_type> _Usage4;
		};
		
		class CancelSpellHook
		{
		public:
			static void Hook()
			{
				_castProjectile = SKSE::GetTrampoline().write_call<5>(REL::ID(33670).address() + 0x575,
					castProjectile);  // SkyrimSE.exe+5504F5
			}

		private:
			static bool castProjectile(RE::MagicCaster* a, RE::BGSProjectile* bproj, RE::TESObjectREFR* a_char,
				RE::CombatController* a4, RE::NiPoint3* startPos, float rotationZ, float rotationX, uint32_t area, void* a9)
			{
				Ldata ldata{ bproj, a->currentSpell, nullptr, nullptr, a_char,
					RE::Projectile::ProjectileRot{ rotationX, rotationZ }, *startPos };

				bool ans;
				if (Triggers::should_disable_origin(&ldata)) {
					ans = true;
					Triggers::onCreated(&ldata);
				} else {
					ans = _castProjectile(a, bproj, a_char, a4, startPos, rotationZ, rotationX, area, a9);
				}

				return ans;
			}

			static inline REL::Relocation<decltype(castProjectile)> _castProjectile;
		};

		// Cancel launch and apply triggers
		class CancelArrowHook
		{
		public:
			static void Hook()
			{
				// arrow->unk140 = 0i64; with arrow=nullptr
				FenixUtils::writebytes<17693, 0xefa>("\x0F\x1F\x80\x00\x00\x00\x00"sv);

				_Launch = SKSE::GetTrampoline().write_call<5>(REL::ID(17693).address() + 0xe82,
					Launch);  // SkyrimSE.exe+2360C2 -- TESObjectWEAP::Fire_140235240
			}

		private:
			static RE::ProjectileHandle* Launch(RE::ProjectileHandle* handle, RE::Projectile::LaunchData* a_ldata)
			{
				Ldata ldata{ a_ldata->projectileBase, a_ldata->spell, a_ldata->ammoSource, a_ldata->weaponSource,
					a_ldata->shooter, {a_ldata->angleX, a_ldata->angleZ}, a_ldata->origin };

				if (Triggers::should_disable_origin(&ldata)) {
					handle->reset();
					Triggers::onCreated(&ldata);
				} else {
					_Launch(handle, a_ldata);
				}

				return handle;
			}

			static inline REL::Relocation<decltype(Launch)> _Launch;
		};

		class ChangeSpeedHook
		{
		public:
			static void Hook()
			{
				_CalcVelocityVector = SKSE::GetTrampoline().write_call<5>(REL::ID(43030).address() + 0x3b8,
					CalcVelocityVector);  // SkyrimSE.exe+754bd8
			}

		private:
			static void CalcVelocityVector(RE::Projectile* proj)
			{
				_CalcVelocityVector(proj);
				Triggers::on3dLoaded(proj);
			}

			static inline REL::Relocation<decltype(CalcVelocityVector)> _CalcVelocityVector;
		};
	}

	void install();
}
