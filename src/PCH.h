#pragma once

//#define DEBUG

#pragma warning(push)
#pragma warning(disable: 4702)
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#pragma warning(pop)

#pragma warning(push)
//#ifdef DEBUG
#	include <spdlog/sinks/basic_file_sink.h>
//#else
//#	include <spdlog/sinks/msvc_sink.h>
//#endif
#pragma warning(pop)

using namespace std::literals;

namespace logger = SKSE::log;

#define DLLEXPORT __declspec(dllexport)

#include "Version.h"
#ifdef USELESS_UTILS
#	include "UselessFenixUtils.h"
#endif
