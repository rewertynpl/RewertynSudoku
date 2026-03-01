#pragma once

#include "utils/logging.h"
#include "config/run_config.h"
#include "core/geometry.h"
#include "monitor.h"
#include "generator/runtime_runner.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <commdlg.h>
#endif

#include "gui-old.h"
