extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
//#ifndef DEBUG
//	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
//#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
//#endif

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
#include "Followers.h"

#ifdef VALIDATE

#	define PY_SSIZE_T_CLEAN
#	include <Python.h>

void before_all()
{
	Py_Initialize();
	PyRun_SimpleString("import sys");
	PyRun_SimpleString("sys.path.append(\"./Data/skse/plugins/\")");
}

void after_all()
{
	if (Py_FinalizeEx() < 0) {
		logger::error("Finalize error\n");
	}
}

bool validate_folder(const std::string& path)
{
	std::string schema = path + "/schema.json";

	std::string ans;

	PyObject *pName, *pModule, *pFunc;
	PyObject *pArgs, *pValue;

	const char* pythonfile = "multiply";
	const char* funcname = "validate_all";

	pName = PyUnicode_DecodeFSDefault(pythonfile);

	pModule = PyImport_Import(pName);
	Py_DECREF(pName);

	if (!pModule) {
		logger::error("Failed to load {}\n", pythonfile);
		return false;
	}

	pFunc = PyObject_GetAttrString(pModule, funcname);

	if (!pFunc || !PyCallable_Check(pFunc)) {
		logger::error("Cannot find function {}\n", funcname);
		Py_XDECREF(pFunc);
		Py_DECREF(pModule);
		return false;
	}

	pArgs = PyTuple_New(2);

	pValue = PyUnicode_FromString(path.c_str());
	if (!pValue) {
		Py_DECREF(pArgs);
		Py_DECREF(pModule);
		logger::error("Cannot convert argument\n");
		return false;
	}
	PyTuple_SetItem(pArgs, 0, pValue);

	pValue = PyUnicode_FromString(schema.c_str());
	if (!pValue) {
		Py_DECREF(pArgs);
		Py_DECREF(pModule);
		logger::error("Cannot convert argument\n");
		return false;
	}
	PyTuple_SetItem(pArgs, 1, pValue);

	pValue = PyObject_CallObject(pFunc, pArgs);
	Py_DECREF(pArgs);

	if (!pValue) {
		Py_DECREF(pFunc);
		Py_DECREF(pModule);
		PyErr_Print();
		logger::error("Call failed\n");
		return false;
	}

	ans = PyBytes_AsString(PyUnicode_AsUTF8String(pValue));
	if (!ans.empty())
		logger::error("{}", ans);

	Py_DECREF(pValue);
	Py_XDECREF(pFunc);
	Py_DECREF(pModule);

	return ans.empty();
}
#endif  // VALIDATE

void read_json()
{
#ifdef VALIDATE
	before_all();
	bool valid = validate_folder("Data/HomingProjectiles");
	after_all();

	if (!valid) {
		logger::error("Some jsons are invalid, skipping");
		return;
	}
#endif

	JsonUtils::FormIDsMap::clear();

	Homing::clear();
	Multicast::clear();
	Emitters::clear();
	Followers::clear();

	Triggers::clear();

	namespace fs = std::filesystem;

	for (const auto& entry : fs::directory_iterator("Data/HomingProjectiles")) {
		Json::Value json_root;
		std::ifstream ifs;
		const auto& path = entry.path();
		if (path.extension() == ".json" && path.filename() != "schema.json") {
			ifs.open(entry);
			ifs >> json_root;
			ifs.close();

			auto filename = path.filename().string();

			JsonUtils::FormIDsMap::init(filename, json_root);

			Homing::init_keys(filename, json_root);
			Multicast::init_keys(filename, json_root);
			Emitters::init_keys(filename, json_root);
			Followers::init_keys(filename, json_root);

			Homing::init(filename, json_root);
			Multicast::init(filename, json_root);
			Emitters::init(filename, json_root);
			Followers::init(filename, json_root);

			Triggers::init(filename, json_root);
		}
	}

	// Used only while reading json
	Homing::clear_keys();
	Multicast::clear_keys();
	Emitters::clear_keys();
	Followers::clear_keys();
}

void reset_json()
{
	read_json();
}

class Settings : public SettingsBase
{
	static constexpr auto path = "Data/HomingProjectiles/settings.ini";

public:

	class ReloadHotkey
	{
		enum class AddKeys : uint32_t
		{
			Shift,
			Ctrl,
			Alt,

			Total
		};
		static inline constexpr size_t AddKeysTotal = static_cast<size_t>(AddKeys::Total);

		static inline std::array<int, AddKeysTotal> additionals = { {} };
		static inline int key = 71;
		
		static void strip(std::string& str)
		{
			if (str.length() == 0) {
				return;
			}

			auto start_it = str.begin();
			auto end_it = str.rbegin();
			while (std::isspace(*start_it)) {
				++start_it;
				if (start_it == str.end())
					break;
			}
			while (std::isspace(*end_it)) {
				++end_it;
				if (end_it == str.rend())
					break;
			}
			auto start_pos = start_it - str.begin();
			auto end_pos = end_it.base() - str.begin();
			str = start_pos <= end_pos ? std::string(start_it, end_it.base()) : "";
		}

		static void load_key(std::string s)
		{
			using K = RE::BSKeyboardDevice::Key;

			strip(s);
			int keycode = std::stoi(s);

			AddKeys type = AddKeys::Total;
			switch (keycode) {
			case K::kRightAlt:
			case K::kLeftAlt:
				type = AddKeys::Alt;
				break;
			case K::kLeftControl:
			case K::kRightControl:
				type = AddKeys::Ctrl;
				break;
			case K::kLeftShift:
			case K::kRightShift:
				type = AddKeys::Shift;
				break;
			default:
				break;
			}

			if (type != AddKeys::Total) {
				additionals[static_cast<size_t>(type)] = keycode;
			} else {
				key = keycode;
			}
		}

		static bool isPressed_adds()
		{
			bool ans = true;
			for (int k : additionals) {
				ans = ans && (k == 0 || RE::BSInputDeviceManager::GetSingleton()->GetKeyboard()->IsPressed(k));
			}
			return ans;
		}

	public:
		static void load(const CSimpleIniA& ini) {
			std::string keys;
			if (ReadString(ini, "General", "reload_key", keys)) {
				size_t pos = 0;
				std::string token;
				while ((pos = keys.find('+')) != std::string::npos) {
					load_key(keys.substr(0, pos));
					keys.erase(0, pos + 1);
				}
				load_key(keys);
			}
		}

		static bool isPressed(int k) { return k == key && isPressed_adds(); }
	};

	static void load() {
		CSimpleIniA ini;
		ini.LoadFile(path);

		ReloadHotkey::load(ini);
	}
};

class InputHandler : public RE::BSTEventSink<RE::InputEvent*>
{
public:
	static InputHandler* GetSingleton()
	{
		static InputHandler singleton;
		return std::addressof(singleton);
	}

	RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* evns, RE::BSTEventSource<RE::InputEvent*>*) override
	{
		if (!*evns)
			return RE::BSEventNotifyControl::kContinue;

		for (RE::InputEvent* e = *evns; e; e = e->next) {
			if (auto buttonEvent = e->AsButtonEvent(); buttonEvent && buttonEvent->HasIDCode() && buttonEvent->IsDown()) {
				if (Settings::ReloadHotkey::isPressed(buttonEvent->GetIDCode())) {
					reset_json();
				}
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

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	//DebugRender::OnMessage(message);

	switch (message->type) {
	case SKSE::MessagingInterface::kPostLoad:
		Hooks::PaddingsProjectileHook::Hook();
		break;

	case SKSE::MessagingInterface::kDataLoaded:
		Hooks::MultipleBeamsHook::Hook();
		Hooks::NormLightingsHook::Hook();
		Triggers::install();
		Homing::install();
		Multicast::install();
		Emitters::install();
		Followers::install();
		read_json();
		InputHandler::GetSingleton()->enable();
		Settings::load();

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

	//DebugRender::UpdateHooks::Hook();

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
