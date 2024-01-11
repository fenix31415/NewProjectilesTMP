#pragma once

struct Ldata;

namespace Multicast
{
	void apply(Triggers::Data* ldata, uint32_t ind);
	void install();
	void init(const std::string& filename, const Json::Value& json_root);
	void clear();
	void clear_keys();
	void init_keys(const std::string& filename, const Json::Value& json_root);
	uint32_t get_key_ind(const std::string& filename, const std::string& key);
}
