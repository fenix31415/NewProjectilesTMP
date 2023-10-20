#pragma once

void init_NormalType(RE::Projectile* proj);

bool allows_multiple_beams(RE::Projectile* proj);
bool allows_detach_beam(RE::MagicItem* proj);
void set_homing_ind(RE::Projectile* proj, uint32_t ind);
uint32_t get_homing_ind(RE::Projectile* proj);

void set_emitter_ind(RE::Projectile* proj, uint32_t ind);
uint32_t get_emitter_ind(RE::Projectile* proj);
void set_emitter_rest(RE::Projectile* proj, uint32_t count);
uint32_t get_emitter_rest(RE::Projectile* proj);

void set_follower_ind(RE::Projectile* proj, uint32_t ind);
uint32_t get_follower_ind(RE::Projectile* proj);
void set_follower_shape_ind(RE::Projectile* proj, uint32_t ind);
uint32_t get_follower_shape_ind(RE::Projectile* proj);
