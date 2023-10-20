#pragma once

#include "json/json.h"

namespace Emitters
{
	uint32_t get_key_ind(const std::string& key);
	void install();
	void init(const Json::Value& json_root);
	void init_keys(const Json::Value& json_root);
	void onCreated(RE::Projectile* proj, uint32_t ind);
}
