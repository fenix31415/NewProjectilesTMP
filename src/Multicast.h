#pragma once

struct Ldata;

namespace Multicast
{
	void apply(Triggers::Data* ldata, uint32_t ind);
	void install();
	void init(const Json::Value& json_root);
	void init_keys(const Json::Value& json_root);
	uint32_t get_key_ind(const std::string& key);
}
