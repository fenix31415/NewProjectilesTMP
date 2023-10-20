#include "Positioning.h"

namespace Positioning
{
	RE::NiPoint3 rotate(RE::NiPoint3 P, float alpha, const RE::NiPoint3& origin, const RE::NiPoint3& axis)
	{
		P -= origin;
		float cos_phi = cos(alpha);
		float sin_phi = sin(alpha);
		float one_cos_phi = 1 - cos_phi;

		RE::NiMatrix3 R = { { cos_phi + one_cos_phi * axis.x * axis.x, axis.x * axis.y * one_cos_phi - axis.z * sin_phi,
								axis.x * axis.z * one_cos_phi + axis.y * sin_phi },
			{ axis.y * axis.x * one_cos_phi + axis.z * sin_phi, cos_phi + axis.y * axis.y * one_cos_phi,
				axis.y * axis.z * one_cos_phi - axis.x * sin_phi },
			{ axis.z * axis.x * one_cos_phi - axis.y * sin_phi, axis.z * axis.y * one_cos_phi + axis.x * sin_phi,
				cos_phi + axis.z * axis.z * one_cos_phi } };

		P = R * P;
		P += origin;
		return P;
	}

	RE::NiPoint3 Pattern::GetPosition_Single(const Plane& plane, size_t) const { return plane.startPos; }
	RE::NiPoint3 Pattern::GetPosition_Line(const Plane& plane, size_t ind) const
	{
		if (count == 1) {
			return plane.startPos;
		}

		auto from = plane.startPos - plane.right_dir * (size * 0.5f);
		float d = size / (count - 1);
		return from + (plane.right_dir * (d * ind));
	}
	RE::NiPoint3 Pattern::GetPosition_Circle(const Plane& plane, size_t ind) const
	{
		float alpha = 2 * 3.1415926f / count * ind;
		return plane.startPos + (plane.right_dir * cos(alpha) + plane.up_dir * sin(alpha)) * size;
	}
	RE::NiPoint3 Pattern::GetPosition_HalfCircle(const Plane& plane, size_t ind) const
	{
		if (count == 1) {
			return plane.startPos;
		}

		float alpha = 3.1415926f / (count - 1) * ind;
		return plane.startPos + (plane.right_dir * cos(alpha) + plane.up_dir * sin(alpha)) * size;
	}
	RE::NiPoint3 Pattern::GetPosition_FillSquare(const Plane& plane, size_t ind) const
	{
		uint32_t h = (uint32_t)ceil(sqrt(count));

		if (h == 1) {
			return GetPosition_Line(plane, ind);
		}

		// TODO
		return plane.startPos;

		/*auto from = plane.startPos + plane.up_dir * (shape_params.size * 0.5f);
				float d = shape_params.size / (h - 1);

				ShapeParams params = { shape_params.size, h };
				Pane cur_plane = plane;
				while (shape_params.count >= h) {
					cur_plane.startPos = from;
					MultiCasters[(uint32_t)Shape::Line](params, cur_plane, cast_item);
					from -= plane.up_dir * d;
					shape_params.count -= h;
				}

				if (shape_params.count > 0) {
					cur_plane.startPos = from;
					params.count = shape_params.count;
					MultiCasters[(uint32_t)Shape::Line](params, cur_plane, cast_item);
				}*/
	}
	RE::NiPoint3 Pattern::GetPosition_FillCircle(const Plane& plane, size_t ind) const
	{
		float c = size / sqrtf(static_cast<float>(count));
		auto alpha = 2.3999632297286533222f * ind;
		float r = c * sqrtf(static_cast<float>(ind));

		return plane.startPos + (plane.right_dir * cos(alpha) + plane.up_dir * sin(alpha)) * r;
	}
	RE::NiPoint3 Pattern::GetPosition_Sphere(const Plane& plane, size_t ind) const
	{
		float c = size;
		float phi = 3.883222077450933f;
		ind += 1;
		float y = 1 - (ind / (count - 1.0f)) * 2;
		float radius = sqrt(1 - y * y);
		float theta = phi * ind;
		float x = cos(theta) * radius;
		float z = sin(theta) * radius;

		auto forward_dir = plane.up_dir.UnitCross(plane.right_dir);

		return plane.startPos + (plane.right_dir * x + plane.up_dir * z + forward_dir * y) * c;
	}
}
