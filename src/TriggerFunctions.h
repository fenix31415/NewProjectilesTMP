#pragma once

#include "json/json.h"

namespace Triggers
{
	struct Data;
}

namespace TriggerFunctions
{
	struct Function
	{
		enum class Type : uint32_t
		{
			SetRotation,
			SetHoming,
			SetEmitter,
			SetFollower,
			ChangeSpeed,
			ChangeRange,
			ApplyMultiCast
		};

	private:
		Type type: 8;
		uint32_t on_follower: 1;

		struct NumberFunctionData
		{
			enum class NumberFunctions : uint32_t
			{
				Add,
				Mul,
				Set
			} type;
			float value;

			NumberFunctionData() : type(NumberFunctions::Add), value(0) {}
			NumberFunctionData(const Json::Value& data);

			float apply(float& val) const
			{
				float ans = val;
				switch (type) {
				case NumberFunctions::Set:
					val = value;
					break;
				case NumberFunctions::Add:
					val += value;
					break;
				case NumberFunctions::Mul:
					val *= value;
					break;
				default:
					break;
				}
				return ans;
			}
		};
		static_assert(sizeof(NumberFunctionData) == 0x8);

		union
		{
			uint32_t ind;
			NumberFunctionData numb;
		};

		void eval_SetRotation(RE::Projectile* proj, RE::Actor* targetOverride) const;
		void eval_SetHoming(RE::Projectile* proj, RE::Actor* targetOverride) const;
		void eval_SetEmitter(RE::Projectile* proj) const;
		void eval_SetFollower(RE::Projectile* proj) const;
		void eval_ChangeSpeed(RE::Projectile* proj) const;
		void eval_ChangeRange(RE::Projectile* proj) const;
		void eval_ApplyMultiCast(Triggers::Data* data) const;

		uint32_t get_key_ind_disabled(const std::string& val, const std::function<uint32_t(const std::string&)>& get_key_ind)
		{
			if (val == "Disable") {
				return static_cast<uint32_t>(-1);
			} else {
				return get_key_ind(val);
			}
		}

	public:
		void eval(Triggers::Data* data, RE::Projectile* proj, RE::Actor* targetOverride = nullptr) const;
		uint32_t get_homing_ind(bool rotation) const;

		Function() : type(Type::ChangeSpeed), numb() {}
		Function(const Json::Value& function);
	};

	struct Functions
	{
	private:
		std::vector<Function> functions;
		Function changeSpeed;
		uint32_t disable_origin: 1;
		uint32_t changeSpeedPresent: 1;

	public:
		Functions() = default;
		explicit Functions(const Json::Value& json_TriggerFunctions);

		void call(RE::Projectile* proj, RE::Actor* targetOverride = nullptr) const;
		void call(Triggers::Data* data, RE::Projectile* proj, RE::Actor* targetOverride, bool change_speedOnly = false) const;

		uint32_t get_homing_ind(bool rotation) const;

		bool should_disable_origin() const { return disable_origin; }
	};
}
