#include "JsonUtils.h"

namespace JsonUtils
{
	uint32_t get_formid(const std::string& name)
	{
		if (name.starts_with("key_")) {
			return FormIDsMap::get(name);
		} else {
			return FenixUtils::Json::get_formid(name);
		}
	}

	void FormIDsMap::init(const Json::Value& json_root)
	{
		formIDs.clear();

		if (!json_root.isMember("FormIDs"))
			return;

		const auto& formids = json_root["FormIDs"];
		for (auto& key : formids.getMemberNames()) {
			formIDs.insert({ key, get_formid(formids[key].asString()) });
		}
	}
}


