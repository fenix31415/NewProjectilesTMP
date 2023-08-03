#pragma once

struct Ldata;

namespace Multicast
{
	void onCreated(RE::Projectile* proj, uint32_t ind);
	void onCreated(Ldata* ldata, uint32_t ind);
	void install();
	void init(const Json::Value& HomingData);
	void forget();
	uint32_t get_key_ind(const std::string& key);
}
