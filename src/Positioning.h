#pragma once

#include "JsonUtils.h"

namespace Positioning
{
	// TODO: half sphere, cylinder
	enum class Shape : uint32_t
	{
		Single,
		Line,
		Circle,
		HalfCircle,
		FillSquare,
		FillCircle,
		Sphere,

		Total
	};
	static constexpr Shape Shape__DEFAULT = Shape::Single;

	struct Plane
	{
		RE::NiPoint3 startPos, right_dir, up_dir;

		Plane(RE::NiPoint3 startPos, const RE::NiPoint3& cast_dir) : startPos(std::move(startPos))
		{
			right_dir = RE::NiPoint3(0, 0, -1).UnitCross(cast_dir);
			if (right_dir.SqrLength() == 0)
				right_dir = { 1, 0, 0 };
			up_dir = right_dir.Cross(cast_dir);
		}

		RE::NiPoint2 project(const RE::NiPoint3& P) const { return { P.Dot(right_dir), P.Dot(up_dir) }; }

		RE::NiPoint3 unproject(const RE::NiPoint2& P) const { return right_dir * P.x + up_dir * P.y + startPos; }
	};

	RE::NiPoint3 rotate(RE::NiPoint3 P, float alpha, const RE::NiPoint3& origin, const RE::NiPoint3& axis);

	struct Pattern
	{
		explicit Pattern(const Json::Value& item) :
			origin(item.isMember("origin") ? item["origin"].asString() : ""), normal(JsonUtils::readOrDefault3(item, "normal"sv)),
			rotate_alpha(item.isMember("planeRotate") ? (item["planeRotate"].asFloat() * 3.14159265358f / 180.0f) : 0),
			pos_offset(JsonUtils::readOrDefault3(item, "posOffset"sv)),
			normalDependsX(!item.isMember("normal") || parse_enum_ifIsMember<true>(item, "xDepends"sv)),
			shape(parse_enum_ifIsMember<Shape__DEFAULT>(item["Figure"], "shape")),
			count(parse_enum_ifIsMember<1u>(item["Figure"], "count")),
			size(shape != Shape::Single ? static_cast<float>(parse_enum_ifIsMember<0u>(item["Figure"], "size"sv)) : 0)
		{
			if (normal.SqrLength() < 0.0001f) {
				normal = { 0, 1, 0 };
			}
		}

	private:
		static RE::NiPoint3 rotate(const RE::NiPoint3& normal, const RE::Projectile::ProjectileRot& parallel_rot, bool dependsX)
		{
			RE::NiMatrix3 M;
			M.EulerAnglesToAxesZXY(dependsX ? parallel_rot.x : 0, 0, parallel_rot.z);
			return M * normal;
		}

	public:
		// By default center is in getposition.
		// Use bone position if possible, as wel as shift it to pos_offset
		void initCenter(RE::NiPoint3& center, const RE::Projectile::ProjectileRot& rot, RE::TESObjectREFR* origin_refr) const
		{
			if (!origin.empty()) {
				auto root = origin_refr->Get3D1(origin_refr->IsPlayerRef() && !origin_refr->Is3rdPersonVisible());
				if (auto bone = root->GetObjectByName(origin)) {
					center = bone->world.translate;
				}
			}
			center += rotate(pos_offset, rot, normalDependsX);
		}

		// Get actual pattern direction, uses normal to rotate initial cast direction
		RE::NiPoint3 getCastDir(const RE::Projectile::ProjectileRot& parallel_rot) const
		{
			return rotate(normal, parallel_rot, normalDependsX);
		}

	private:
		Shape shape: 4;
		uint32_t count: 27;
		uint32_t normalDependsX: 1;  // 2C used for armageddon
		float size;
		RE::BSFixedString origin;  // 08 node name of origin, getposition otherwise
		RE::NiPoint3 normal;       // 10 determines a pane of SP
		float rotate_alpha;        // 1C rotate everything along the plane normal
		RE::NiPoint3 pos_offset;   // 20 offset of SP center from actual cast pos

		RE::NiPoint3 GetPosition_Single(const Plane& plane, size_t) const;
		RE::NiPoint3 GetPosition_Line(const Plane& plane, size_t ind) const;
		RE::NiPoint3 GetPosition_Circle(const Plane& plane, size_t ind) const;
		RE::NiPoint3 GetPosition_HalfCircle(const Plane& plane, size_t ind) const;
		RE::NiPoint3 GetPosition_FillSquare(const Plane& plane, size_t ind) const;
		RE::NiPoint3 GetPosition_FillCircle(const Plane& plane, size_t ind) const;
		RE::NiPoint3 GetPosition_Sphere(const Plane& plane, size_t ind) const;

		// Rotate point of the figure
		RE::NiPoint3 rotateFigure(const RE::NiPoint3& P, const RE::NiPoint3& O, const RE::NiPoint3& axis) const
		{
			if (rotate_alpha != 0.0f) {
				return Positioning::rotate(P, rotate_alpha, O, axis);
			}
			return P;
		}

		RE::NiPoint3 GetPosition_(const Plane& plane, size_t ind) const
		{
			switch (shape) {
			case Positioning::Shape::Line:
				return GetPosition_Line(plane, ind);
			case Positioning::Shape::Circle:
				return GetPosition_Circle(plane, ind);
			case Positioning::Shape::HalfCircle:
				return GetPosition_HalfCircle(plane, ind);
			case Positioning::Shape::FillSquare:
				return GetPosition_FillSquare(plane, ind);
			case Positioning::Shape::FillCircle:
				return GetPosition_FillCircle(plane, ind);
			case Positioning::Shape::Sphere:
				return GetPosition_Sphere(plane, ind);
			case Positioning::Shape::Single:
			case Positioning::Shape::Total:
			default:
				return GetPosition_Single(plane, ind);
			}
		}

	public:
		RE::NiPoint3 GetPosition(const RE::NiPoint3& start_pos, const RE::NiPoint3& cast_dir, size_t ind) const
		{
			return rotateFigure(GetPosition_(Plane(start_pos, cast_dir), ind), start_pos, cast_dir);
		}

		RE::NiPoint3 GetPosition(const Plane& plane, const RE::NiPoint3& cast_dir, size_t ind) const
		{
			return rotateFigure(GetPosition_(plane, ind), plane.startPos, cast_dir);
		}

		std::vector<RE::NiPoint3> GetPositions(const RE::NiPoint3& start_pos, const RE::NiPoint3& cast_dir) const
		{
			std::vector<RE::NiPoint3> ans;
			Plane plane(start_pos, cast_dir);
			for (size_t i = 0; i < count; i++) {
				ans.push_back(GetPosition(plane, cast_dir, i));
			}
			return ans;
		}
		
		bool xDepends() const { return normalDependsX; }

		uint32_t getSize() const { return count; }

		bool isShapeless() const { return shape == Shape::Single; }
	};
	static_assert(sizeof(Pattern) == 0x30);
}
