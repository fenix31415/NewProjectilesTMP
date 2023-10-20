#include "TriggerFunctions.h"

#include "JsonUtils.h"

#include "Homing.h"
#include "Emitters.h"
#include "Followers.h"
#include "Multicast.h"
#include "Triggers.h"

namespace TriggerFunctions
{
	Function::NumberFunctionData::NumberFunctionData(const Json::Value& data) :
		type(parse_enum<NumberFunctions::Add>(data["type"].asString())), value(data["value"].asFloat())
	{}

	void Function::eval_SetRotation(RE::Projectile* proj, RE::Actor* targetOverride) const
	{
		Homing::applyRotate(proj, ind, targetOverride);
	}
	void Function::eval_SetHoming(RE::Projectile* proj, RE::Actor* targetOverride) const
	{
		Homing::apply(proj, ind, targetOverride);
	}
	void Function::eval_SetEmitter(RE::Projectile* proj) const { Emitters::onCreated(proj, ind); }
	void Function::eval_SetFollower(RE::Projectile* proj) const { Followers::onCreated(proj, ind); }
	void Function::eval_ChangeSpeed(RE::Projectile* proj) const
	{
		float cur_speed = proj->linearVelocity.Length();
		float old_speed = numb.apply(cur_speed);
		proj->linearVelocity *= cur_speed / old_speed;
	}
	void Function::eval_ChangeRange(RE::Projectile* proj) const { numb.apply(proj->range); }
	void Function::eval_ApplyMultiCast(Triggers::Data* data) const { Multicast::apply(data, ind); }

	void Function::eval(Triggers::Data* data, RE::Projectile* proj, RE::Actor* targetOverride) const
	{
		switch (type) {
		case Type::SetRotation:
			if (proj)
				eval_SetRotation(proj, targetOverride);
			break;
		case Type::SetHoming:
			if (proj)
				eval_SetHoming(proj, targetOverride);
			break;
		case Type::SetEmitter:
			if (proj)
				eval_SetEmitter(proj);
			break;
		case Type::SetFollower:
			if (proj)
				eval_SetFollower(proj);
			break;
		case Type::ChangeSpeed:
			if (proj)
				eval_ChangeSpeed(proj);
			break;
		case Type::ChangeRange:
			if (proj)
				eval_ChangeRange(proj);
			break;
		case Type::ApplyMultiCast:
			eval_ApplyMultiCast(data);
			break;
		default:
			return;
		}
	}

	uint32_t Function::get_homing_ind(bool rotation) const
	{
		if (!rotation && type == Type::SetHoming || rotation && type == Type::SetRotation)
			return ind;

		return 0;
	}

	Function::Function(const Json::Value& function) :
		type(parse_enum<Type::SetRotation>(function["type"].asString())),
		on_follower(parse_enum_ifIsMember<false>(function, "on_followers"sv))
	{
		switch (type) {
		case Type::SetRotation:
			ind = get_key_ind_disabled(function["id"].asString(), Homing::get_key_ind);
			break;
		case Type::SetHoming:
			ind = get_key_ind_disabled(function["id"].asString(), Homing::get_key_ind);
			break;
		case Type::SetEmitter:
			ind = get_key_ind_disabled(function["id"].asString(), Emitters::get_key_ind);
			break;
		case Type::SetFollower:
			ind = get_key_ind_disabled(function["id"].asString(), Followers::get_key_ind);
			break;
		case Type::ApplyMultiCast:
			ind = get_key_ind_disabled(function["id"].asString(), Multicast::get_key_ind);
			break;
		case Type::ChangeSpeed:
		case Type::ChangeRange:
			numb = NumberFunctionData(function["data"]);
			break;
		default:
			assert(false);
			break;
		}
	}

	Functions::Functions(const Json::Value& json_TriggerFunctions) :
		disable_origin(parse_enum_ifIsMember<false>(json_TriggerFunctions, "disableOrigin"sv)), changeSpeedPresent(false),
		changeSpeed()
	{
		const auto& json_functions = json_TriggerFunctions["functions"];
		for (size_t i = 0; i < json_functions.size(); i++) {
			const auto& function = json_functions[(int)i];
			auto type = parse_enum<Function::Type::SetRotation>(function["type"].asString());
			if (type == Function::Type::ChangeSpeed) {
				changeSpeedPresent = true;
				changeSpeed = Function(function);
			} else {
				functions.emplace_back(function);
			}
		}
	}

	void Functions::call(Triggers::Data* data, RE::Projectile* proj, RE::Actor* targetOverride, bool change_speedOnly) const
	{
		if (change_speedOnly) {
			if (changeSpeedPresent)
				changeSpeed.eval(data, proj);
		} else {
			for (auto& func : functions) {
				func.eval(data, proj, targetOverride);
			}
		}
	}

	uint32_t Functions::get_homing_ind(bool rotation) const
	{
		for (const auto& function : functions) {
			if (auto ind = function.get_homing_ind(rotation)) {
				return ind;
			}
		}
		return 0;
	}

	void Functions::call(RE::Projectile* proj, RE::Actor* targetOverride) const
	{
		Triggers::Data trigger_data(proj);
		call(&trigger_data, proj, targetOverride);
	}
}
