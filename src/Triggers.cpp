#include "Triggers.h"

namespace Triggers
{
	void install()
	{
		using namespace Hooks;

		ApplyTriggersHook::Hook();

		CancelArrowHook::Hook();
		CancelSpellHook::Hook();
	}
}
