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
		type(JsonUtils::read_enum<NumberFunctions>(data, "type")), value(JsonUtils::getFloat(data, "value"))
	{}

	void Function::eval_SetRotationHoming(RE::Projectile* proj, RE::Actor* targetOverride) const
	{
		Homing::applyRotate(proj, ind, targetOverride);
	}
	void Function::eval_SetRotationToSight(RE::Projectile* proj) const
	{
		if (auto caster = proj->shooter.get().get(); caster && caster->As<RE::Actor>()) {
			FenixUtils::Geom::Projectile::aimToPoint(proj, FenixUtils::Geom::Actor::raycast(caster->As<RE::Actor>()));
		}
	}
	void Function::eval_SetHoming(RE::Projectile* proj, RE::Actor* targetOverride) const
	{
		Homing::apply(proj, ind, targetOverride);
	}
	void Function::eval_DisableHoming(RE::Projectile* proj) const { Homing::disable(proj); }
	void Function::eval_SetEmitter(RE::Projectile* proj) const { Emitters::apply(proj, ind); }
	void Function::eval_DisableEmitter(RE::Projectile* proj) const { Emitters::disable(proj); }
	void Function::eval_SetFollower(RE::Projectile* proj) const { Followers::apply(proj, ind); }
	void Function::eval_DisableFollower(RE::Projectile* proj) const { Followers::disable(proj, restore_speed); }
	void Function::eval_ChangeSpeed(RE::Projectile* proj) const
	{
		float cur_speed = proj->linearVelocity.Length();
		float old_speed = numb.apply(cur_speed);
		proj->linearVelocity *= cur_speed / old_speed;
	}
	void Function::eval_ChangeRange(RE::Projectile* proj) const { numb.apply(proj->range); }
	void Function::eval_ApplyMultiCast(Triggers::Data* data) const { Multicast::apply(data, ind); }

	void Function::eval_impl(Triggers::Data* data, RE::Projectile* proj, RE::Actor* targetOverride) const
	{
		switch (type) {
		case Type::SetRotationToSight:
			if (proj)
				eval_SetRotationToSight(proj);
			break;
		case Type::SetRotationHoming:
			if (proj)
				eval_SetRotationHoming(proj, targetOverride);
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
		case Type::DisableFollower:
			eval_DisableFollower(proj);
			break;
		case Type::DisableEmitter:
			eval_DisableEmitter(proj);
			break;
		case Type::DisableHoming:
			eval_DisableHoming(proj);
			break;
		default:
			return;
		}
	}

	void Function::eval(Triggers::Data* data, RE::Projectile* proj, RE::Actor* targetOverride) const
	{
		if (on_follower) {
			Followers::forEachFollower(data->shooter, [this, data, targetOverride](RE::Projectile* proj_follower) {
				data->pos = proj_follower->GetPosition();
				data->rot = { proj_follower->GetAngleX(), proj_follower->GetAngleZ() };
				eval_impl(data, proj_follower, targetOverride);
				return Followers::forEachRes::kContinue;
			});
		} else {
			eval_impl(data, proj, targetOverride);
		}
	}

	uint32_t Function::get_homing_ind(bool rotation) const
	{
		if (!rotation && type == Type::SetHoming || rotation && type == Type::SetRotationHoming)
			return ind;

		return 0;
	}

	Function::Function(const Json::Value& function) :
		type(JsonUtils::read_enum<Type>(function, "type")), on_follower(JsonUtils::mb_read_field<false>(function, "on_followers"))
	{
		switch (type) {
		case Type::SetRotationToSight:
			break;
		case Type::SetRotationHoming:
			ind = Homing::get_key_ind(JsonUtils::getString(function, "id"));
			break;
		case Type::SetHoming:
			ind = Homing::get_key_ind(JsonUtils::getString(function, "id"));
			break;
		case Type::SetEmitter:
			ind = Emitters::get_key_ind(JsonUtils::getString(function, "id"));
			break;
		case Type::SetFollower:
			ind = Followers::get_key_ind(JsonUtils::getString(function, "id"));
			break;
		case Type::ApplyMultiCast:
			ind = Multicast::get_key_ind(JsonUtils::getString(function, "id"));
			break;
		case Type::ChangeSpeed:
		case Type::ChangeRange:
			numb = NumberFunctionData(function["data"]);
			break;
		case Type::DisableFollower:
			restore_speed = JsonUtils::mb_read_field<true>(function, "restore_speed");
			break;
		case Type::DisableEmitter:
		case Type::DisableHoming:
			break;
		default:
			assert(false);
			break;
		}
	}

	Functions::Functions(const Json::Value& json_TriggerFunctions) :
		disable_origin(JsonUtils::mb_read_field<false>(json_TriggerFunctions, "disableOrigin")), changeSpeedPresent(false),
		changeSpeed()
	{
		const auto& json_functions = json_TriggerFunctions["functions"];
		for (size_t i = 0; i < json_functions.size(); i++) {
			const auto& function = json_functions[(int)i];
			auto type = JsonUtils::read_enum<Function::Type>(function, "type");
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
