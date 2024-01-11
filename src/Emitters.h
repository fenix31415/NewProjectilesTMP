#pragma once

#include "json/json.h"

namespace Emitters
{
	uint32_t get_key_ind(const std::string& filename, const std::string& key);
	void install();
	void init(const std::string& filename, const Json::Value& json_root);
	void init_keys(const std::string& filename, const Json::Value& json_root);
	void clear();
	void clear_keys();
	void apply(RE::Projectile* proj, uint32_t ind);
	void disable(RE::Projectile* proj);
}
