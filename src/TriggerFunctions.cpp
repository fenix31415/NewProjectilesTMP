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

	Function::NumberFunctionData::NumberFunctionData(const RE::NiPoint3& linVel)
	{
		memcpy(&type, &linVel.y, 4);
		value = linVel.z;
	}

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
		if (!proj->flags.any(RE::Projectile::Flags::kInited)) {
			// x for col_layer
			assert(proj->linearVelocity.y == 0 && proj->linearVelocity.z == 0);

			memcpy(&proj->linearVelocity.y, &numb.type, 4);
			proj->linearVelocity.z = numb.value;
		} else {
			float cur_speed = proj->linearVelocity.Length();
			float old_speed = numb.apply(cur_speed);
			proj->linearVelocity *= cur_speed / old_speed;
		}
	}
	void Function::eval_ChangeRange(RE::Projectile* proj) const { numb.apply(proj->range); }
	void Function::eval_ApplyMultiCast(Triggers::Data* data) const { Multicast::apply(data, ind); }
	void Function::eval_Placeatme(Triggers::Data* data) const
	{
		RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(form->As<RE::TESBoundObject>(), data->pos,
			RE::NiPoint3(data->rot.x, 0, data->rot.z), data->shooter->GetParentCell(), data->shooter->GetWorldspace(), nullptr,
			nullptr, RE::ObjectRefHandle(), false, true);
	}
	void Function::eval_SendAnimEvent(Triggers::Data* data) const { data->shooter->NotifyAnimationGraph(event); }
	void Function::eval_Explode(Triggers::Data* data) const
	{
		RE::NiMatrix3 M;
		M.EulerAnglesToAxesZXY(data->rot.x, 0, data->rot.z);
		RE::Explosion::SpawnExplosionData expldata{ form->As<RE::BGSExplosion>(), data->shooter->GetParentCell(), data->shooter,
			nullptr, nullptr, nullptr, nullptr, 0, data->pos, M, 1, 0 };
		RE::Explosion::SpawnExplosion(expldata);
	}
	void Function::eval_SetColLayer(RE::Projectile* proj) const
	{
		if (!proj->flags.any(RE::Projectile::Flags::kInited)) {
			// y, z for speed
			assert(proj->linearVelocity.x == 0);

			memcpy(&proj->linearVelocity.x, &layer, 4);
		} else {
			FenixUtils::Projectile__set_collision_layer(proj, layer);
		}
	}

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
			if (proj)
				eval_DisableFollower(proj);
			break;
		case Type::DisableEmitter:
			if (proj)
				eval_DisableEmitter(proj);
			break;
		case Type::DisableHoming:
			if (proj)
				eval_DisableHoming(proj);
			break;
		case Type::Placeatme:
			eval_Placeatme(data);
			break;
		case Type::SendAnimEvent:
			eval_SendAnimEvent(data);
			break;
		case Type::Explode:
			eval_Explode(data);
			break;
		case Type::SetColLayer:
			if (proj)
				eval_SetColLayer(proj);
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

	Function::Function(const std::string& filename, const Json::Value& function) :
		type(JsonUtils::read_enum<Type>(function, "type")), on_follower(JsonUtils::mb_read_field<false>(function, "on_followers"))
	{
		switch (type) {
		case Type::SetRotationToSight:
			break;
		case Type::SetRotationHoming:
			ind = Homing::get_key_ind(filename, JsonUtils::getString(function, "id"));
			break;
		case Type::SetHoming:
			ind = Homing::get_key_ind(filename, JsonUtils::getString(function, "id"));
			break;
		case Type::SetEmitter:
			ind = Emitters::get_key_ind(filename, JsonUtils::getString(function, "id"));
			break;
		case Type::SetFollower:
			ind = Followers::get_key_ind(filename, JsonUtils::getString(function, "id"));
			break;
		case Type::ApplyMultiCast:
			ind = Multicast::get_key_ind(filename, JsonUtils::getString(function, "id"));
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
		case Type::Placeatme:
			form = RE::TESForm::LookupByID(JsonUtils::get_formid(filename, JsonUtils::getString(function, "form")));
			break;
		case Type::SendAnimEvent:
			memset(&event, 0, 8);
			event = JsonUtils::getString(function, "event");
			break;
		case Type::Explode:
			form = RE::TESForm::LookupByID(JsonUtils::get_formid(filename, JsonUtils::getString(function, "explosion")));
			break;
		case Type::SetColLayer:
			layer = Followers::layer2layer(JsonUtils::read_enum<Followers::Collision>(function, "layer"));
			break;
		default:
			assert(false);
			break;
		}
	}

	Function::Function(const RE::NiPoint3& linVel) : type(Type::ChangeSpeed), on_follower(false)
	{
		numb = NumberFunctionData(linVel);
	}

	Function::Function(const Function& other) : type(other.type), on_follower(other.on_follower)
	{
		if (type == Type::SendAnimEvent) {
			memset(&event, 0, 8);
			event = other.event;
			return;
		}

		memcpy(this, &other, sizeof(Function));
	}

	Functions::Functions(const std::string& filename, const Json::Value& json_TriggerFunctions) :
		disable_origin(JsonUtils::mb_read_field<false>(json_TriggerFunctions, "disableOrigin"))
	{
		const auto& json_functions = json_TriggerFunctions["functions"];
		for (size_t i = 0; i < json_functions.size(); i++) {
			functions.emplace_back(filename, json_functions[(int)i]);
		}
	}

	void Functions::call(Triggers::Data* data, RE::Projectile* proj, RE::Actor* targetOverride) const
	{
		for (auto& func : functions) {
			func.eval(data, proj, targetOverride);
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
