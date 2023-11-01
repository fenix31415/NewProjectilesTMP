#include "Positioning.h"

namespace Positioning
{
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
	RE::NiPoint3 Pattern::GetPosition_FillSquare(const Plane& plane, size_t _ind) const
	{
		if (count == 1) {
			return plane.startPos;
		}

		uint32_t m = static_cast<uint32_t>(sqrt(count));
		uint32_t rest = count - m * m;
		bool has_right = rest >= m;
		bool has_up = rest != 0 && rest != m;

		uint32_t w = has_right ? m + 1 : m;
		uint32_t h = has_up ? m + 1 : m;

		float dx = size / (w - 1);
		float dy = h == 1 ? 0 : size / (h - 1);

		uint32_t ind = _ind % count;

		if (ind < w * m) {
			uint32_t x = ind % w;
			uint32_t y = ind / w;

			auto from = plane.startPos - (plane.right_dir + plane.up_dir) * (size * 0.5f);
			return from + plane.right_dir * (dx * x) + plane.up_dir * (dy * y);
		} else {
			ind -= w * m;
			uint32_t up_size = rest >= m ? rest - m : rest;

			uint32_t x = ind;
			auto from = plane.startPos - plane.right_dir * ((up_size - 1) * 0.5f * dx) - plane.up_dir * (size * 0.5f - dy * m);
			return from + plane.right_dir * (dx * x);
		}
	}
	RE::NiPoint3 Pattern::GetPosition_FillCircle(const Plane& plane, size_t ind) const
	{
		float c = size / sqrtf(static_cast<float>(count));
		auto alpha = 2.3999632297286533222f * ind;
		float r = c * sqrtf(static_cast<float>(ind));

		return plane.startPos + (plane.right_dir * cos(alpha) + plane.up_dir * sin(alpha)) * r;
	}
	RE::NiPoint3 Pattern::GetPosition_FillHalfCircle(const Plane& plane, size_t ind) const
	{
		float c = size / sqrtf(static_cast<float>(count));
		float alpha = 0.5f * 2.3999632297286533222f * ind;
		const float pi = 3.141592653589793f;
		while (alpha >= 2 * pi)
			alpha -= 2 * pi;
		if (alpha >= pi)
			alpha = alpha - pi;
		float r = c * sqrtf(static_cast<float>(ind));
		return plane.startPos + (plane.right_dir * cos(alpha) + plane.up_dir * sin(alpha)) * r;
	}
	RE::NiPoint3 Pattern::GetPosition_Sphere(const Plane& plane, size_t ind) const
	{
		if (count == 1) {
			return plane.startPos;
		}

		float c = size;
		float phi = 3.883222077450933f;
		float y = 1 - (ind / (count - 1.0f)) * 2;
		float radius = sqrt(1 - y * y);
		float theta = phi * ind;
		float x = cos(theta) * radius;
		float z = sin(theta) * radius;

		auto forward_dir = plane.up_dir.UnitCross(plane.right_dir);

		return plane.startPos + (plane.right_dir * x + plane.up_dir * z + forward_dir * y) * c;
	}
	RE::NiPoint3 Pattern::GetPosition_HalfSphere(const Plane& plane, size_t ind) const
	{
		if (count == 1) {
			return plane.startPos;
		}

		float c = size;
		float phi = 3.883222077450933f;
		float z = 1 - (ind / (count - 1.0f));
		float radius = sqrt(1 - z * z);
		float theta = phi * ind;
		float x = cos(theta) * radius;
		float y = sin(theta) * radius;

		auto forward_dir = plane.up_dir.UnitCross(plane.right_dir);

		return plane.startPos + (plane.right_dir * x + plane.up_dir * z + forward_dir * y) * c;
	}
	RE::NiPoint3 Pattern::GetPosition_Cylinder(const Plane& plane, size_t ind) const
	{
		if (count == 1) {
			return plane.startPos;
		}

		float c = size;
		float phi = 3.883222077450933f;
		float y = 1 - (ind / (count - 1.0f)) * 2;
		float theta = phi * ind;
		float x = cos(theta);
		float z = sin(theta);

		auto forward_dir = plane.up_dir.UnitCross(plane.right_dir);

		return plane.startPos + (plane.right_dir * x + plane.up_dir * z + forward_dir * y) * c;
	}
}
