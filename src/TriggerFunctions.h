#pragma once

#include "json/json.h"

struct Ldata;

namespace TriggerFunctions
{
	enum class NumberFunctions : uint32_t
	{
		None,
		Set,
		Add,
		Mul
	};
	static constexpr NumberFunctions NumberFunctions__DEFAULT = NumberFunctions::None;

	struct Functions
	{
		void eval(Ldata* ldata) const;
		void eval(RE::Projectile* proj) const;
		void eval3dLoaded(RE::Projectile* proj) const;

		Functions() = default;
		Functions(const Json::Value& functions);

		bool has_homing() const { return homingInd != 0; }

		uint32_t get_homing_ind() const { return homingInd; }

		struct NumberFunctionData
		{
			NumberFunctions type;
			float value;

			NumberFunctionData()
			{
				type = NumberFunctions::None;
				value = 0;
			}
			NumberFunctionData(const Json::Value& data);

			float apply(float& val) const
			{
				float ans = val;
				switch (type) {
				case TriggerFunctions::NumberFunctions::Set:
					val = value;
					break;
				case TriggerFunctions::NumberFunctions::Add:
					val += value;
					break;
				case TriggerFunctions::NumberFunctions::Mul:
					val *= value;
					break;
				case TriggerFunctions::NumberFunctions::None:
				default:
					break;
				}
				return ans;
			}
		};
		static_assert(sizeof(NumberFunctionData) == 0x8);

	private:
		uint32_t homingInd = 0;     // in HomingData
		uint32_t multicastInd = 0;  // in MulticastData
		uint32_t emitterInd = 0;    // in EmittersData
		NumberFunctionData changeSpeed;
		NumberFunctionData changeRange;
	};
	static_assert(sizeof(Functions) == 0x1C);
}
