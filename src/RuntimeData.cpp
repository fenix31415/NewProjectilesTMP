#include "RuntimeData.h"

struct FenixProjsRuntimeData
{
	void set_NormalType() { (uint32_t&)data = 0; }

	struct Indexes
	{
		uint32_t homing: 6;
		uint32_t emitter: 6;
		uint32_t emitter_rest: 5;
		uint32_t follower: 6;
		uint32_t follower_shape_ind: 8;
		uint32_t unused: 1;
	};
	static_assert(sizeof(Indexes) == 4);

	void set_homing_ind(uint32_t ind) { data.homing = ind; }
	uint32_t get_homing_ind() { return data.homing; }

	void set_emitter_ind(uint32_t ind) { data.emitter = ind; }
	uint32_t get_emitter_ind() { return data.emitter; }
	void set_emitter_rest(uint32_t count) { data.emitter_rest = count; }
	uint32_t get_emitter_rest() { return data.emitter_rest; }

	void set_follower_ind(uint32_t ind) { data.follower = ind; }
	uint32_t get_follower_ind() { return data.follower; }
	void set_follower_shape_ind(uint32_t ind) { data.follower_shape_ind = ind; }
	uint32_t get_follower_shape_ind() { return data.follower_shape_ind; }

	Indexes data;
};
static_assert(sizeof(FenixProjsRuntimeData) == 4);

FenixProjsRuntimeData& get_runtime_data(RE::Projectile* proj) { return (FenixProjsRuntimeData&)(uint32_t&)proj->pad164; }

void init_NormalType(RE::Projectile* proj) { get_runtime_data(proj).set_NormalType(); }

void set_homing_ind(RE::Projectile* proj, uint32_t ind) { get_runtime_data(proj).set_homing_ind(ind); }
uint32_t get_homing_ind(RE::Projectile* proj) { return get_runtime_data(proj).get_homing_ind(); }

void set_emitter_ind(RE::Projectile* proj, uint32_t ind) { get_runtime_data(proj).set_emitter_ind(ind); }
uint32_t get_emitter_ind(RE::Projectile* proj) { return get_runtime_data(proj).get_emitter_ind(); }
void set_emitter_rest(RE::Projectile* proj, uint32_t count) { get_runtime_data(proj).set_emitter_rest(count); }
uint32_t get_emitter_rest(RE::Projectile* proj) { return get_runtime_data(proj).get_emitter_rest(); }

void set_follower_ind(RE::Projectile* proj, uint32_t ind) { get_runtime_data(proj).set_follower_ind(ind); }
uint32_t get_follower_ind(RE::Projectile* proj) { return get_runtime_data(proj).get_follower_ind(); }
void set_follower_shape_ind(RE::Projectile* proj, uint32_t ind) { get_runtime_data(proj).set_follower_shape_ind(ind); }
uint32_t get_follower_shape_ind(RE::Projectile* proj) { return get_runtime_data(proj).get_follower_shape_ind(); }

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
