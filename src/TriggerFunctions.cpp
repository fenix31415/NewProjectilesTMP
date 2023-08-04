#include "TriggerFunctions.h"
#include "JsonUtils.h"
#include "Homing.h"
#include "Multicast.h"
#include "Emitters.h"

namespace TriggerFunctions
{
	Functions::NumberFunctionData::NumberFunctionData(const Json::Value& data) :
		type(parse_enum<NumberFunctions__DEFAULT>(data["type"].asString())), value(data["value"].asFloat())
	{}

	uint32_t get_key_ind_disabled(const std::string& val, const std::function<uint32_t(const std::string&)>& get_key_ind)
	{
		if (val == "Disable") {
			return static_cast<uint32_t>(-1);
		} else {
			return get_key_ind(val);
		}
	}

	Functions::Functions(const Json::Value& functions) :
		homingInd(functions.isMember("Homing") ? get_key_ind_disabled(functions["Homing"].asString(), Homing::get_key_ind) : 0),
		multicastInd(functions.isMember("Multicast") ? Multicast::get_key_ind(functions["Multicast"].asString()) : 0),
		emitterInd(
			functions.isMember("Emitters") ? get_key_ind_disabled(functions["Emitters"].asString(), Emitters::get_key_ind) : 0),
		changeSpeed(functions.isMember("Speed") ? NumberFunctionData(functions["Speed"]) : NumberFunctionData()),
		changeRange(functions.isMember("Range") ? NumberFunctionData(functions["Range"]) : NumberFunctionData())
	{}

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
		if (emitterInd) {
			Emitters::onCreated(proj, emitterInd);
		}

		changeRange.apply(proj->range);

		if (changeSpeed.type != NumberFunctions::None) {
			float cur_speed = proj->linearVelocity.Length();
			float old_speed = changeSpeed.apply(cur_speed);
			proj->linearVelocity *= cur_speed / old_speed;
		}
	}

	void Functions::eval3dLoaded(RE::Projectile* proj) const
	{
		if (changeSpeed.type != NumberFunctions::None) {
			float cur_speed = proj->linearVelocity.Length();
			float old_speed = changeSpeed.apply(cur_speed);
			proj->linearVelocity *= cur_speed / old_speed;
		}
	}
}
