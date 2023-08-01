#include "Triggers.h"

namespace Triggers
{
	TriggerFunctions parse_functions(const Json::Value& functions)
	{
		TriggerFunctions ans = {};
		if (functions.isMember("Homing")) {
			ans.homingInd = Homing::get_key_ind(functions["Homing"].asString());
		}
		return ans;
	}
}
