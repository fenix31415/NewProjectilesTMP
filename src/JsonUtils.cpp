#include "JsonUtils.h"

namespace JsonUtils
{
	uint32_t get_formid(const std::string& filename, const std::string& name)
	{
		if (name.starts_with("key_")) {
			return FormIDsMap::get(filename, name);
		} else {
			return FenixUtils::Json::get_formid(name);
		}
	}

	void FormIDsMap::init(const std::string& filename, const Json::Value& json_root)
	{
		if (!json_root.isMember("FormIDs"))
			return;

		const auto& formids = json_root["FormIDs"];
		for (auto& key : formids.getMemberNames()) {
			formIDs.insert({ filename + key, FenixUtils::Json::get_formid(formids[key].asString()) });
		}
	}

	void FormIDsMap::clear() { formIDs.clear(); }
}
