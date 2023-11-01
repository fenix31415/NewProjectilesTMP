#pragma once

#include "JsonUtils.h"

namespace Positioning
{
	enum class Shape : uint32_t
	{
		Single,
		Line,
		Circle,
		HalfCircle,
		FillSquare,
		FillCircle,
		FillHalfCircle,
		Sphere,
		HalfSphere,
		Cylinder,

		Total
	};

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

	struct Pattern
	{
		explicit Pattern(const Json::Value& item) :
			origin(JsonUtils::mb_getString(item, "origin")),
			normal(JsonUtils::mb_getPoint3<RE::NiPoint3(0, 1, 0)>(item, "normal")),
			rotate_alpha(JsonUtils::mb_getFloat(item, "planeRotate") * 3.14159265358f / 180.0f),
			pos_offset(JsonUtils::mb_getPoint3(item, "posOffset")),
			normalDependsX(JsonUtils::mb_read_field<true>(item, "xDepends")),
			shape(JsonUtils::read_enum<Shape>(item["Figure"], "shape")),
			count(JsonUtils::mb_read_field<1u>(item["Figure"], "count")),
			size(shape != Shape::Single ? static_cast<float>(JsonUtils::mb_read_field<0u>(item["Figure"], "size")) : 0)
		{}

		static RE::NiPoint3 rotateDependsX(const RE::NiPoint3& A, RE::Projectile::ProjectileRot parallel_rot, bool dependsX)
		{
			return FenixUtils::Geom::rotate(A, RE::NiPoint3(dependsX ? parallel_rot.x : 0, 0, parallel_rot.z));
		}

		RE::NiPoint3 rotateDependsX(const RE::NiPoint3& A, RE::Projectile::ProjectileRot parallel_rot) const
		{
			return rotateDependsX(A, parallel_rot, normalDependsX);
		}

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
			center += rotateDependsX(pos_offset, rot);
		}

		// Get actual pattern direction, uses normal to rotate initial cast direction
		RE::NiPoint3 getCastDir(const RE::Projectile::ProjectileRot& parallel_rot) const
		{
			return rotateDependsX(normal, parallel_rot);
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
		RE::NiPoint3 GetPosition_FillHalfCircle(const Plane& plane, size_t ind) const;
		RE::NiPoint3 GetPosition_Sphere(const Plane& plane, size_t ind) const;
		RE::NiPoint3 GetPosition_HalfSphere(const Plane& plane, size_t ind) const;
		RE::NiPoint3 GetPosition_Cylinder(const Plane& plane, size_t) const;

		// Rotate point of the figure
		RE::NiPoint3 rotateFigure(const RE::NiPoint3& P, const RE::NiPoint3& O, const RE::NiPoint3& axis) const
		{
			if (rotate_alpha != 0.0f) {
				return FenixUtils::Geom::rotate(P, rotate_alpha, O, axis);
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
			case Positioning::Shape::FillHalfCircle:
				return GetPosition_FillHalfCircle(plane, ind);
			case Positioning::Shape::Sphere:
				return GetPosition_Sphere(plane, ind);
			case Positioning::Shape::HalfSphere:
				return GetPosition_HalfSphere(plane, ind);
			case Positioning::Shape::Cylinder:
				return GetPosition_Cylinder(plane, ind);
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
