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
			SetRotationHoming,
			SetRotationToSight,
			SetHoming,
			SetEmitter,
			SetFollower,
			ChangeSpeed,
			ChangeRange,
			ApplyMultiCast,
			DisableHoming,
			DisableFollower,
			DisableEmitter
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
			explicit NumberFunctionData(const Json::Value& data);
			explicit NumberFunctionData(const RE::NiPoint3& linVel);  // For ChangeSpeed (triggers only on 3dLoaded)

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
			bool restore_speed;
		};

		void eval_SetRotationToSight(RE::Projectile* proj) const;
		void eval_SetRotationHoming(RE::Projectile* proj, RE::Actor* targetOverride) const;
		void eval_SetHoming(RE::Projectile* proj, RE::Actor* targetOverride) const;
		void eval_DisableHoming(RE::Projectile* proj) const;
		void eval_SetEmitter(RE::Projectile* proj) const;
		void eval_DisableEmitter(RE::Projectile* proj) const;
		void eval_SetFollower(RE::Projectile* proj) const;
		void eval_DisableFollower(RE::Projectile* proj) const;
		void eval_ChangeSpeed(RE::Projectile* proj) const;
		void eval_ChangeRange(RE::Projectile* proj) const;
		void eval_ApplyMultiCast(Triggers::Data* data) const;

		void eval_impl(Triggers::Data* data, RE::Projectile* proj, RE::Actor* targetOverride = nullptr) const;

	public:
		void eval(Triggers::Data* data, RE::Projectile* proj, RE::Actor* targetOverride = nullptr) const;
		uint32_t get_homing_ind(bool rotation) const;

		Function() : type(Type::ChangeSpeed), numb() {}
		explicit Function(const std::string& filename, const Json::Value& function);
		explicit Function(const RE::NiPoint3& linVel);  // For ChangeSpeed (triggers only on 3dLoaded)
	};

	struct Functions
	{
	private:
		std::vector<Function> functions;
		uint32_t disable_origin: 1;

	public:
		Functions() = default;
		explicit Functions(const std::string& filename, const Json::Value& json_TriggerFunctions);

		void call(RE::Projectile* proj, RE::Actor* targetOverride = nullptr) const;
		void call(Triggers::Data* data, RE::Projectile* proj, RE::Actor* targetOverride) const;

		uint32_t get_homing_ind(bool rotation) const;

		bool should_disable_origin() const { return disable_origin; }
	};
}
