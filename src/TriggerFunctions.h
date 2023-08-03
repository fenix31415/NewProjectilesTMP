#pragma once

#include "JsonUtils.h"

struct Ldata;

namespace TriggerFunctions
{
	struct Functions
	{
		uint32_t homingInd;  // in HomingData
		uint32_t multicastInd;  // in MulticastData

		void eval(Ldata* ldata) const;
		void eval(RE::Projectile* proj) const;
	};
	static_assert(sizeof(Functions) == 8);

	Functions parse_functions(const Json::Value& functions);
}
