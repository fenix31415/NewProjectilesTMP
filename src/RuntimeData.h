#pragma once

void init_NormalType(RE::Projectile* proj);

bool allows_multiple_beams(RE::Projectile* proj);
bool allows_detach_beam(RE::MagicItem* proj);
void set_homing_ind(RE::Projectile* proj, uint32_t ind);
uint32_t get_homing_ind(RE::Projectile* proj);
