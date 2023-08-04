extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef DEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef DEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

#include "Hooks.h"
#include "json/json.h"
#include <JsonUtils.h>
#include "Triggers.h"
#include "Multicast.h"
#include "Homing.h"
#include "Emitters.h"

void read_json()
{
	Json::Value json_root;
	std::ifstream ifs;
	ifs.open("Data/HomingProjectiles/HomieSpells.json");
	ifs >> json_root;
	ifs.close();

	JsonUtils::FormIDsMap::init(json_root);

	Homing::init_keys(json_root);
	Multicast::init_keys(json_root);
	Emitters::init_keys(json_root);

	Homing::init(json_root);
	Multicast::init(json_root);
	Emitters::init(json_root);
	Triggers::Triggers::init(json_root);
}

void reset_json()
{
	Homing::forget();
	Multicast::forget();
	Emitters::forget();

	read_json();
}

class InputHandler : public RE::BSTEventSink<RE::InputEvent*>
{
public:
	static InputHandler* GetSingleton()
	{
		static InputHandler singleton;
		return std::addressof(singleton);
	}

	RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* e, RE::BSTEventSource<RE::InputEvent*>*) override
	{
		if (!*e)
			return RE::BSEventNotifyControl::kContinue;

		if (auto buttonEvent = (*e)->AsButtonEvent();
			buttonEvent && buttonEvent->HasIDCode() && (buttonEvent->IsDown() || buttonEvent->IsPressed())) {
			if (int key = buttonEvent->GetIDCode(); key == 71) {
				reset_json();
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	void enable()
	{
		if (auto input = RE::BSInputDeviceManager::GetSingleton()) {
			input->AddEventSink(this);
		}
	}
};

class DebugAPIHook
{
public:
	static void Hook() { _Update = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_PlayerCharacter[0])).write_vfunc(0xad, Update); }

private:
	static void Update(RE::PlayerCharacter* a, float delta)
	{
		_Update(a, delta);

		SKSE::GetTaskInterface()->AddUITask([]() { DebugAPI_IMPL::DebugAPI::Update(); });
	}

	static inline REL::Relocation<decltype(Update)> _Update;
};

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		Hooks::PaddingsProjectileHook::Hook();
		Hooks::MultipleBeamsHook::Hook();
		Hooks::NormLightingsHook::Hook();
		Triggers::install();
		Homing::install();
		Multicast::install();
		Emitters::install();
		read_json();
		InputHandler::GetSingleton()->enable();

#ifdef DEBUG
		DebugAPIHook::Hook();
#endif  // DEBUG

		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
		return false;
	}

	logger::info("loaded");

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 10);

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
