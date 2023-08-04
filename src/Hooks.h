#pragma once

#include "RuntimeData.h"

namespace Hooks
{
	// Create proj & load from save with zero padding value
	// TODO: replace load part to serialization
	class PaddingsProjectileHook
	{
	public:
		static void Hook()
		{
			auto& trmpl = SKSE::GetTrampoline();

			_TESForm__SetInitedFormFlag_140194B90 =
				trmpl.write_call<5>(REL::ID(42920).address() + 0x392, Ctor);  // SkyrimSE.exe+74ACE2
			_TESObjectREFR__ReadFromSaveGame_140286FD0 =
				trmpl.write_call<5>(REL::ID(42953).address() + 0x4b, LoadGame);  // SkyrimSE.exe+74D28B
		}

	private:
		static void Ctor(RE::Projectile* proj, char a2)
		{
			_TESForm__SetInitedFormFlag_140194B90(proj, a2);
			init_NormalType(proj);
		}

		static void __fastcall LoadGame(RE::Projectile* proj, RE::BGSLoadGameBuffer* buf)
		{
			_TESObjectREFR__ReadFromSaveGame_140286FD0(proj, buf);
			init_NormalType(proj);
		}

		static inline REL::Relocation<decltype(Ctor)> _TESForm__SetInitedFormFlag_140194B90;
		static inline REL::Relocation<decltype(LoadGame)> _TESObjectREFR__ReadFromSaveGame_140286FD0;
	};

	// Allows to create many beams.
	// NewBeam returns `found`, if false, old proj not removes
	class MultipleBeamsHook
	{
	public:
		static void Hook()
		{
			auto& trmpl = SKSE::GetTrampoline();

			_RefHandle__get = trmpl.write_call<5>(REL::ID(42928).address() + 0x117, NewBeam);  // SkyrimSE.exe+74B287

			{
				// SkyrimSE.exe+733F93
				uintptr_t ret_addr = REL::ID(42586).address() + 0x2d3;

				struct Code : Xbyak::CodeGenerator
				{
					Code(uintptr_t func_addr, uintptr_t ret_addr)
					{
						Xbyak::Label nocancel;

						// rsi  = proj
						// xmm0 -- xmm2 = node pos
						mov(r9, rsi);
						mov(rax, func_addr);
						call(rax);
						mov(rax, ret_addr);
						jmp(rax);
					}
				} xbyakCode{ uintptr_t(update_node_pos), ret_addr };

				FenixUtils::add_trampoline<5, 42586, 0x2c1>(&xbyakCode);  // SkyrimSE.exe+733F81
			}

			_TESObjectREFR__SetPosition_140296910 =
				trmpl.write_call<5>(REL::ID(42586).address() + 0x2db, UpdatePos);                         // SkyrimSE.exe+733F9B
			_Projectile__SetRotation = trmpl.write_call<5>(REL::ID(42586).address() + 0x249, UpdateRot);  // SkyrimSE.exe+733F09
			_matrix_mul = trmpl.write_call<5>(REL::ID(42586).address() + 0x212, matrix_mul);
		}

	private:
		static RE::NiMatrix3* matrix_mul(RE::NiMatrix3* A, RE::NiMatrix3* ans, RE::NiMatrix3* B)
		{
			auto proj = (RE::Projectile*)((char*)A - 0xA8);
			if (allows_multiple_beams(proj)) {
				auto node = proj->Get3D2();
				return &node->local.rotate;
			} else {
				return _matrix_mul(A, ans, B);
			}
		}

		static bool NewBeam(uint32_t* handle, RE::Projectile** proj)
		{
			auto found = _RefHandle__get(handle, proj);
			if (!found || !*proj)
				return found;

			return !allows_multiple_beams(*proj);
		}

		static void update_node_pos(float x, float y, float z, RE::Projectile* proj)
		{
			if (auto node = proj->Get3D()) {
				if (!allows_multiple_beams(proj)) {
					node->local.translate.x = x;
					node->local.translate.y = y;
					node->local.translate.z = z;
				}
			}
		}

		static void UpdatePos(RE::Projectile* proj, RE::NiPoint3* pos)
		{
			if (!allows_multiple_beams(proj)) {
				_TESObjectREFR__SetPosition_140296910(proj, pos);
			}
		}

		static void UpdateRot(RE::Projectile* proj, float rot_X)
		{
			if (!allows_multiple_beams(proj)) {
				_Projectile__SetRotation(proj, rot_X);
			}
		}

		static inline REL::Relocation<decltype(NewBeam)> _RefHandle__get;
		static inline REL::Relocation<decltype(UpdatePos)> _TESObjectREFR__SetPosition_140296910;
		static inline REL::Relocation<decltype(UpdateRot)> _Projectile__SetRotation;
		static inline REL::Relocation<decltype(matrix_mul)> _matrix_mul;
	};

	// Detach instant beams from magic node
	class NormLightingsHook
	{
	public:
		static void Hook()
		{
			_BeamProjectile__ctor =
				SKSE::GetTrampoline().write_call<5>(REL::ID(42928).address() + 0x185, Ctor);  // SkyrimSE.exe+74B2F5
		}

	private:
		static RE::BeamProjectile* Ctor(RE::BeamProjectile* proj, RE::Projectile::LaunchData* ldata)
		{
			if (allows_detach_beam(ldata->spell)) {
				ldata->useOrigin = true;
				ldata->autoAim = false;
			}

			return _BeamProjectile__ctor(proj, ldata);
		}

		static inline REL::Relocation<decltype(Ctor)> _BeamProjectile__ctor;
	};
}
