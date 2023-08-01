#pragma once

#include "Homing.h"
#include "JsonUtils.h"

namespace Triggers
{
	struct TriggerFunctions
	{
		uint32_t homingInd;  // in HomingData
	};
	static_assert(sizeof(TriggerFunctions) == 4);

	TriggerFunctions parse_functions(const Json::Value& functions);

	enum class TriggerConditions : uint32_t
	{
		BaseIsFormID,
		EffectHasKwd,
		SpellHasKwd,
		CasterIsFormID,
		CasterBaseIsFormID,
		CasterHasKwd,

		Total
	};

	static const std::array<std::function<bool(RE::Projectile*, uint32_t)>, static_cast<size_t>(TriggerConditions::Total)>
		ConditionsFunctors = {
			// BaseIsFormID
			[](RE::Projectile* proj, uint32_t value) -> bool {
				auto base = proj->GetProjectileBase();
				return base->formID == value;
			},
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
		uint32_t value;
		TriggerFunctions functions;

		bool eval_condition(RE::Projectile* proj) const
		{
			auto caster = proj->shooter.get().get();
			if (!caster)
				return false;

			bool isPlayer = caster->IsPlayerRef();
			bool caster_ok = caster_type == CasterTypes::NPC && !isPlayer || caster_type == CasterTypes::Player && isPlayer ||
			                 caster_type == CasterTypes::Both;
			if (!caster_ok)
				return false;

			return ConditionsFunctors[static_cast<size_t>(condition)](proj, value);
		}

		void eval_function(RE::Projectile* proj) const
		{
			if (functions.homingInd) {
				Homing::onCreated(proj, functions.homingInd);
			}
		}

	public:
		Trigger(CasterTypes caster_type, TriggerConditions condition, uint32_t value, TriggerFunctions functions) :
			caster_type(caster_type), condition(condition), value(value), functions(std::move(functions))
		{}

		void eval(RE::Projectile* proj) const
		{
			if (eval_condition(proj))
				eval_function(proj);
		}

#ifdef DEBUG
		uint32_t get_homing_ind(RE::BGSProjectile* bproj) const
		{
			if (condition != TriggerConditions::BaseIsFormID)
				return 0;

			if (bproj->formID == value)
				if (functions.homingInd)
					return functions.homingInd;

			return 0;
		}
#endif  // DEBUG

	};
	static_assert(sizeof(Trigger) == 12);

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

				triggers.emplace_back(caster, condition, value, parse_functions(trigger["TriggerFunctions"]));
			}
		}

		static void onCreated(RE::Projectile* proj)
		{
			for (const auto& trigger : triggers) {
				trigger.eval(proj);
			}
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
}
