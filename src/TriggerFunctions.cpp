#include "TriggerFunctions.h"
#include "JsonUtils.h"
#include "Homing.h"
#include "Multicast.h"

namespace TriggerFunctions
{
	Functions parse_functions(const Json::Value& functions)
	{
		Functions ans = {};
		if (functions.isMember("Homing")) {
			ans.homingInd = Homing::get_key_ind(functions["Homing"].asString());
		}
		if (functions.isMember("Multicast")) {
			ans.multicastInd = Multicast::get_key_ind(functions["Multicast"].asString());
		}
		return ans;
	}

	void Functions::eval(Ldata* ldata) const
	{
		if (multicastInd) {
			Multicast::onCreated(ldata, multicastInd);
		}
	}

	void Functions::eval(RE::Projectile* proj) const
	{
		if (homingInd) {
			Homing::onCreated(proj, homingInd);
		}
		if (multicastInd) {
			Multicast::onCreated(proj, multicastInd);
		}
	}
}
