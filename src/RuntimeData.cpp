#include "RuntimeData.h"

struct FenixProjsRuntimeData
{
	void set_NormalType() { (uint32_t&)data = 0; }

	struct Indexes
	{
		uint32_t homing: 6;
		uint32_t emitter: 6;
		uint32_t unused: 20;
	};
	static_assert(sizeof(Indexes) == 4);

	void set_homing_ind(uint32_t ind) { data.homing = ind; }
	uint32_t get_homing_ind() { return data.homing; }

	void set_emitter_ind(uint32_t ind) { data.emitter = ind; }
	uint32_t get_emitter_ind() { return data.emitter; }

	Indexes data;
};
static_assert(sizeof(FenixProjsRuntimeData) == 4);

FenixProjsRuntimeData& get_runtime_data(RE::Projectile* proj) { return (FenixProjsRuntimeData&)(uint32_t&)proj->pad164; }

void init_NormalType(RE::Projectile* proj) { get_runtime_data(proj).set_NormalType(); }

void set_homing_ind(RE::Projectile* proj, uint32_t ind) { get_runtime_data(proj).set_homing_ind(ind); }
uint32_t get_homing_ind(RE::Projectile* proj) { return get_runtime_data(proj).get_homing_ind(); }

void set_emitter_ind(RE::Projectile* proj, uint32_t ind) { get_runtime_data(proj).set_emitter_ind(ind); }
uint32_t get_emitter_ind(RE::Projectile* proj) { return get_runtime_data(proj).get_emitter_ind(); }

bool allows_multiple_beams(RE::Projectile* proj)
{
	auto spell = proj->spell;
	return spell && spell->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget && proj->IsBeamProjectile() &&
	       proj->flags.all(RE::Projectile::Flags::kUseOrigin) && !proj->flags.any(RE::Projectile::Flags::kAutoAim);
}

bool allows_detach_beam(RE::MagicItem* spel)
{
	return spel && spel->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget;
}
