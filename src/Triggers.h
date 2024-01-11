#pragma once

#include "json/json.h"

namespace Triggers
{
	enum class Event : uint32_t
	{
		ProjAppeared,
		Swing,
		HitMelee,
		HitByMelee,
		HitProjectile,
		HitByProjectile,
		Cast,
		EffectStart,
		EffectEnd,
		ProjDestroyed,
		ProjHits,
		ProjImpact,

		Total  // for std::array
	};

	struct Data
	{
		RE::TESObjectWEAP* weap;
		RE::TESObjectREFR* shooter;
		RE::BGSProjectile* bproj;
		RE::MagicItem* spel;
		RE::EffectSetting* mgef;
		RE::TESAmmo* ammo;
		RE::MagicSystem::CastingSource hand;

		enum class Type : uint32_t
		{
			Spell,
			Arrow,
			None
		} type;

		RE::Projectile::ProjectileRot rot;
		RE::NiPoint3 pos;

		Data(Type type, RE::Projectile::LaunchData* ldata) :
			weap(ldata->weaponSource), shooter(ldata->shooter), bproj(ldata->projectileBase), spel(ldata->spell),
			mgef((ldata->spell && ldata->spell->As<RE::SpellItem>()) ? ldata->spell->GetAVEffect() : nullptr),
			ammo(ldata->ammoSource), hand(ldata->castingSource), type(type), rot({ ldata->angleX, ldata->angleZ }),
			pos(ldata->origin)
		{}

		explicit Data(RE::Projectile* proj) :
			weap(proj->weaponSource), shooter(proj->shooter.get().get()), bproj(proj->GetProjectileBase()), spel(proj->spell),
			mgef(proj->spell ? proj->spell->GetAVEffect() : nullptr), ammo(proj->ammoSource), hand(proj->castingSource),
			type(proj->weaponSource ? Type::Arrow : (proj->spell ? Type::Spell : Type::None)),
			rot({ proj->GetAngleX(), proj->GetAngleZ() }), pos(proj->GetPosition())
		{}

		Data(RE::TESObjectWEAP* weap, RE::TESObjectREFR* shooter, RE::BGSProjectile* bproj, RE::MagicItem* spel,
			RE::EffectSetting* mgef, RE::TESAmmo* ammo, RE::MagicSystem::CastingSource hand, Type type,
			RE::Projectile::ProjectileRot rot, RE::NiPoint3 pos) :
			weap(weap),
			shooter(shooter), bproj(bproj), spel(spel), mgef(mgef), ammo(ammo), hand(hand), type(type), rot(std::move(rot)),
			pos(std::move(pos))
		{}
	};

	void init(const std::string& filename, const Json::Value& json_root);
	void clear();
	
	// targetOverride used only for Multicast::Evenly support
	void eval(Data* data, Event e, RE::Projectile* proj, RE::Actor* targetOverride = nullptr);

	void install();
}
