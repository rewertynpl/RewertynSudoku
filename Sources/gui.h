#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <random>
#include <sstream>
#include <thread>
#include <optional>
#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#endif

namespace sudoku_hpc {
#ifdef _WIN32
enum : int {
    IDC_PROGRESS_BAR = 1001,
    IDC_PROGRESS_TEXT,
    IDC_MONITOR_BOX,
    IDC_LOG_BOX,
    IDC_BTN_START,
    IDC_BTN_CANCEL,
    IDC_BTN_PAUSE
};

constexpr UINT WM_APP_PROGRESS = WM_APP + 101;
constexpr UINT WM_APP_LOG = WM_APP + 102;
constexpr UINT WM_APP_DONE = WM_APP + 103;
constexpr UINT WM_APP_INLINE_CLOSE = WM_APP + 104;
constexpr UINT WM_APP_FORM_NAV = WM_APP + 105;
constexpr UINT_PTR IDT_MONITOR_REFRESH = 1;
constexpr UINT_PTR IDSC_INLINE_EDITOR = 1;
constexpr int IDC_INLINE_EDIT = 4500;
constexpr int IDC_FORM_PANEL = 4501;

enum class FormFieldType {
    Int,
    Text,
    Combo,
    Checkbox,
    ActionButton
};

enum FormFieldKey {
    F_BOX_ROWS = 1,
    F_BOX_COLS,
    F_TARGET_PUZZLES,
    F_MIN_CLUES,
    F_MAX_CLUES,
    F_DIFFICULTY,
    F_CLUES_PRESET,
    F_THREADS,
    F_SEED,
    F_RESEED_INTERVAL,
    F_ATTEMPT_TIME_BUDGET,
    F_ATTEMPT_NODE_BUDGET,
    F_MAX_ATTEMPTS,
    F_MAX_TOTAL_TIME,  // Globalny limit czasu na całe uruchomienie
    F_REQUIRED_STRATEGY,
    F_OUTPUT_FOLDER,
    F_OUTPUT_FILE,
    F_SYMMETRY_CENTER,
    F_REQUIRE_UNIQUE,
    F_SHOW_MONITOR
};

struct FormField {
    int key = 0;
    FormFieldType type = FormFieldType::Text;
    std::wstring label;
    std::wstring text;
    std::vector<std::wstring> options;
    int option_index = 0;
    bool checked = false;
    bool enabled = true;
    bool has_browse = false;
    RECT label_rect{};
    RECT value_rect{};
    RECT browse_rect{};
};

struct FormLayoutItem {
    bool is_section = false;
    std::wstring section_title;
    int field_key = 0;
    RECT section_rect{};
};

struct GuiAppState {
    HWND hwnd = nullptr;
    HWND h_form_panel = nullptr;
    HWND h_inline_editor = nullptr;
    HWND h_progress = nullptr;
    HWND h_progress_text = nullptr;
    HWND h_monitor = nullptr;
    HWND h_log = nullptr;
    HWND h_start = nullptr;
    HWND h_cancel = nullptr;
    HWND h_pause = nullptr;
    HFONT h_mono_font = nullptr;
    int inline_field_key = 0;
    int focused_field_key = 0;
    int hover_field_key = 0;
    bool inline_is_combo = false;
    bool form_read_only = false;
    bool inline_close_posted = false;
    bool mouse_tracking = false;
    int form_scroll_y = 0;
    int form_content_height = 0;
    int form_view_height = 0;
    std::vector<FormField> form_fields;
    std::vector<FormLayoutItem> form_layout;

    std::jthread run_thread;
    std::atomic<bool> running{false};
    std::atomic<bool> cancel_requested{false};
    std::atomic<bool> paused{false};
    uint64_t target = 0;

    std::mutex monitor_mu;
    std::shared_ptr<ConsoleStatsMonitor> gui_monitor;
};

int form_first_focusable_key(const GuiAppState* state);
void form_set_focused_field(GuiAppState* state, int field_key, bool ensure_visible);

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overread"
#endif

std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) {
        return L"";
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) {
        return L"";
    }
    std::wstring out;
    out.resize(static_cast<size_t>(len));
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    out.resize(static_cast<size_t>(len - 1));
    return out;
}

std::string wide_to_utf8(const std::wstring& ws) {
    if (ws.empty()) {
        return "";
    }
    const int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        return "";
    }
    std::string out;
    out.resize(static_cast<size_t>(len));
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), len, nullptr, nullptr);
    out.resize(static_cast<size_t>(len - 1));
    return out;
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

std::wstring get_text(HWND h) {
    const int len = GetWindowTextLengthW(h);
    std::wstring text(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(h, text.data(), len + 1);
    text.resize(static_cast<size_t>(len));
    return text;
}

void set_text(HWND h, const std::wstring& value) {
    SetWindowTextW(h, value.c_str());
}

int parse_int_text_or_default(const std::wstring& text, int fallback) {
    if (text.empty()) {
        return fallback;
    }
    try {
        return std::stoi(text);
    } catch (...) {
        return fallback;
    }
}

uint64_t parse_u64_text_or_default(const std::wstring& text, uint64_t fallback) {
    if (text.empty()) {
        return fallback;
    }
    try {
        return static_cast<uint64_t>(std::stoull(text));
    } catch (...) {
        return fallback;
    }
}

long long parse_i64_text_or_default(const std::wstring& text, long long fallback) {
    if (text.empty()) {
        return fallback;
    }
    try {
        return static_cast<long long>(std::stoll(text));
    } catch (...) {
        return fallback;
    }
}

FormField* find_form_field(GuiAppState* state, int key) {
    if (state == nullptr) {
        return nullptr;
    }
    for (auto& field : state->form_fields) {
        if (field.key == key) {
            return &field;
        }
    }
    return nullptr;
}

const FormField* find_form_field(const GuiAppState* state, int key) {
    if (state == nullptr) {
        return nullptr;
    }
    for (const auto& field : state->form_fields) {
        if (field.key == key) {
            return &field;
        }
    }
    return nullptr;
}

std::wstring form_field_text(const GuiAppState* state, int key, const std::wstring& fallback) {
    const FormField* field = find_form_field(state, key);
    if (field == nullptr || field->text.empty()) {
        return fallback;
    }
    return field->text;
}

int form_field_int(const GuiAppState* state, int key, int fallback) {
    const FormField* field = find_form_field(state, key);
    if (field == nullptr) {
        return fallback;
    }
    return parse_int_text_or_default(field->text, fallback);
}

uint64_t form_field_u64(const GuiAppState* state, int key, uint64_t fallback) {
    const FormField* field = find_form_field(state, key);
    if (field == nullptr) {
        return fallback;
    }
    return parse_u64_text_or_default(field->text, fallback);
}

long long form_field_i64(const GuiAppState* state, int key, long long fallback) {
    const FormField* field = find_form_field(state, key);
    if (field == nullptr) {
        return fallback;
    }
    return parse_i64_text_or_default(field->text, fallback);
}

int form_field_combo_index(const GuiAppState* state, int key, int fallback = 0) {
    const FormField* field = find_form_field(state, key);
    if (field == nullptr) {
        return fallback;
    }
    return field->option_index;
}

bool form_field_checked(const GuiAppState* state, int key, bool fallback = false) {
    const FormField* field = find_form_field(state, key);
    if (field == nullptr) {
        return fallback;
    }
    return field->checked;
}

void set_form_field_text(GuiAppState* state, int key, const std::wstring& value) {
    FormField* field = find_form_field(state, key);
    if (field != nullptr) {
        field->text = value;
    }
}

void set_form_field_combo_index(GuiAppState* state, int key, int index) {
    FormField* field = find_form_field(state, key);
    if (field == nullptr) {
        return;
    }
    if (field->options.empty()) {
        field->option_index = 0;
        return;
    }
    field->option_index = std::clamp(index, 0, static_cast<int>(field->options.size()) - 1);
}

void set_form_field_checked(GuiAppState* state, int key, bool checked) {
    FormField* field = find_form_field(state, key);
    if (field != nullptr) {
        field->checked = checked;
    }
}

void init_form_model(GuiAppState* state) {
    state->form_fields.clear();
    state->form_layout.clear();
    auto add_field = [&](FormField field) {
        state->form_fields.push_back(std::move(field));
    };
    auto add_layout_field = [&](int key) {
        FormLayoutItem it;
        it.field_key = key;
        state->form_layout.push_back(it);
    };
    auto add_section = [&](const wchar_t* title) {
        FormLayoutItem it;
        it.is_section = true;
        it.section_title = title;
        state->form_layout.push_back(std::move(it));
    };
    auto make_field = [](int key, FormFieldType type, const wchar_t* label, const wchar_t* text) {
        FormField f;
        f.key = key;
        f.type = type;
        f.label = label;
        f.text = text;
        return f;
    };

    add_field(make_field(F_BOX_ROWS, FormFieldType::Int, L"box_rows:", L"3"));
    add_field(make_field(F_BOX_COLS, FormFieldType::Int, L"box_cols:", L"3"));
    add_field(make_field(F_TARGET_PUZZLES, FormFieldType::Int, L"target_puzzles:", L"100"));
    add_field(make_field(F_MIN_CLUES, FormFieldType::Int, L"min_clues (0=auto, można nadpisać):", L"0"));
    add_field(make_field(F_MAX_CLUES, FormFieldType::Int, L"max_clues (0=auto, można nadpisać):", L"0"));

    FormField difficulty;
    difficulty.key = F_DIFFICULTY;
    difficulty.type = FormFieldType::Combo;
    difficulty.label = L"difficulty_level_required:";
    difficulty.options = {
        L"1 - Naked/Hidden Single",
        L"2 - Pointing / Box-Line",
        L"3 - Pairs/Triples",
        L"4 - Wings/Fishes basic",
        L"5 - Swordfish/UR/BUG+1",
        L"6 - Jellyfish/Chains",
        L"7 - Medusa/AIC/Sue de Coq",
        L"8 - MSLS/Exocet/Forcing Chains",
        L"9 - Backtracking/Brutalny"};
    difficulty.option_index = 0;
    add_field(std::move(difficulty));

    FormField preset;
    preset.key = F_CLUES_PRESET;
    preset.type = FormFieldType::ActionButton;
    preset.label = L"clues_preset:";
    preset.text = L"Auto (wg difficulty + rozmiaru)";
    add_field(std::move(preset));

    add_field(make_field(F_THREADS, FormFieldType::Int, L"threads (0=auto):", L"0"));
    add_field(make_field(F_SEED, FormFieldType::Int, L"seed (0=random):", L"0"));
    add_field(make_field(F_RESEED_INTERVAL, FormFieldType::Int, L"reseed_interval_s (0=off, full worker reset):", L"0"));
    add_field(make_field(F_ATTEMPT_TIME_BUDGET, FormFieldType::Int, L"attempt_time_budget_s (0=brak limitu):", L"0"));
    add_field(make_field(F_ATTEMPT_NODE_BUDGET, FormFieldType::Int, L"attempt_node_budget (0=auto):", L"0"));
    add_field(make_field(F_MAX_ATTEMPTS, FormFieldType::Int, L"max_attempts (0=bez limitu):", L"0"));
    add_field(make_field(F_MAX_TOTAL_TIME, FormFieldType::Int, L"max_total_time_s (0=bez limitu, CAŁE URUCHOMIENIE):", L"0"));

    FormField strategy;
    strategy.key = F_REQUIRED_STRATEGY;
    strategy.type = FormFieldType::Combo;
    strategy.label = L"required_strategy (opcjonalnie):";
    strategy.options = {L"(brak)", L"Naked Single", L"Hidden Single", L"Backtracking"};
    strategy.option_index = 0;
    add_field(std::move(strategy));

    FormField out_folder;
    out_folder.key = F_OUTPUT_FOLDER;
    out_folder.type = FormFieldType::Text;
    out_folder.label = L"output_folder:";
    out_folder.text = L"generated_sudoku_files";
    out_folder.has_browse = true;
    add_field(std::move(out_folder));

    FormField out_file;
    out_file.key = F_OUTPUT_FILE;
    out_file.type = FormFieldType::Text;
    out_file.label = L"output_file:";
    out_file.text = L"generated_sudoku.txt";
    out_file.has_browse = true;
    add_field(std::move(out_file));

    FormField symmetry;
    symmetry.key = F_SYMMETRY_CENTER;
    symmetry.type = FormFieldType::Checkbox;
    symmetry.label = L"symmetry_center";
    symmetry.checked = false;
    add_field(std::move(symmetry));

    FormField unique;
    unique.key = F_REQUIRE_UNIQUE;
    unique.type = FormFieldType::Checkbox;
    unique.label = L"require_unique (wymuszone)";
    unique.checked = true;
    unique.enabled = false;
    add_field(std::move(unique));

    FormField show_monitor;
    show_monitor.key = F_SHOW_MONITOR;
    show_monitor.type = FormFieldType::Checkbox;
    show_monitor.label = L"live monitor (GUI+console)";
    show_monitor.checked = true;
    add_field(std::move(show_monitor));

    add_section(L"Podstawowe parametry");
    add_layout_field(F_BOX_ROWS);
    add_layout_field(F_BOX_COLS);
    add_layout_field(F_TARGET_PUZZLES);
    add_layout_field(F_MIN_CLUES);
    add_layout_field(F_MAX_CLUES);
    add_layout_field(F_DIFFICULTY);
    add_layout_field(F_CLUES_PRESET);

    add_section(L"Ustawienia zaawansowane / silnika");
    add_layout_field(F_THREADS);
    add_layout_field(F_SEED);
    add_layout_field(F_RESEED_INTERVAL);
    add_layout_field(F_ATTEMPT_TIME_BUDGET);
    add_layout_field(F_ATTEMPT_NODE_BUDGET);
    add_layout_field(F_MAX_ATTEMPTS);
    add_layout_field(F_MAX_TOTAL_TIME);  // Globalny limit czasu
    add_layout_field(F_REQUIRED_STRATEGY);

    add_section(L"Zapis i eksport");
    add_layout_field(F_OUTPUT_FOLDER);
    add_layout_field(F_OUTPUT_FILE);
    add_layout_field(F_SYMMETRY_CENTER);
    add_layout_field(F_SHOW_MONITOR);
}

void append_log(GuiAppState* state, const std::wstring& line) {
    if (state == nullptr || state->h_log == nullptr) {
        return;
    }
    const int len = GetWindowTextLengthW(state->h_log);
    SendMessageW(state->h_log, EM_SETSEL, len, len);
    std::wstring text = line + L"\r\n";
    SendMessageW(state->h_log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
}

void set_monitor_text(GuiAppState* state, const std::wstring& text) {
    if (state == nullptr || state->h_monitor == nullptr) {
        return;
    }
    set_text(state->h_monitor, text);
}

void post_log(HWND hwnd, const std::wstring& line) {
    auto* payload = new std::wstring(line);
    PostMessageW(hwnd, WM_APP_LOG, 0, reinterpret_cast<LPARAM>(payload));
}

bool browse_for_folder(HWND owner, std::wstring& out_path) {
    BROWSEINFOW bi{};
    wchar_t display_name[MAX_PATH]{};
    bi.hwndOwner = owner;
    bi.pszDisplayName = display_name;
    bi.lpszTitle = L"Wybierz folder wyjsciowy";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl == nullptr) {
        return false;
    }
    wchar_t path[MAX_PATH]{};
    const BOOL ok = SHGetPathFromIDListW(pidl, path);
    CoTaskMemFree(pidl);
    if (!ok) {
        return false;
    }
    out_path = path;
    return true;
}

bool browse_for_save_file(HWND owner, std::wstring& out_path) {
    wchar_t file_buffer[MAX_PATH] = L"generated_sudoku.txt";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file_buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"txt";
    if (!GetSaveFileNameW(&ofn)) {
        return false;
    }
    out_path = file_buffer;
    return true;
}

RequiredStrategy required_strategy_from_index(int sel) {
    switch (sel) {
    case 1:
        return RequiredStrategy::NakedSingle;
    case 2:
        return RequiredStrategy::HiddenSingle;
    case 3:
        return RequiredStrategy::Backtracking;
    default:
        return RequiredStrategy::None;
    }
}

void apply_clues_preset(GuiAppState* state) {
    if (state == nullptr) {
        return;
    }
    const int box_rows = std::max(1, form_field_int(state, F_BOX_ROWS, 3));
    const int box_cols = std::max(1, form_field_int(state, F_BOX_COLS, 3));
    const int lvl = std::clamp(form_field_combo_index(state, F_DIFFICULTY, 8) + 1, 1, 9);
    const RequiredStrategy required = required_strategy_from_index(form_field_combo_index(state, F_REQUIRED_STRATEGY, 0));
    const int effective_budget_level = (required == RequiredStrategy::Backtracking) ? 9 : 1;
    const ClueRange range = resolve_auto_clue_range(box_rows, box_cols, lvl, required);
    const int min_clues = range.min_clues;
    const int max_clues = range.max_clues;
    const int suggested_reseed_s = suggest_reseed_interval_s(box_rows, box_cols, effective_budget_level);
    const int suggested_attempt_time_s = suggest_attempt_time_budget_seconds(box_rows, box_cols, effective_budget_level);
    const uint64_t suggested_attempt_nodes = suggest_attempt_node_budget(box_rows, box_cols, effective_budget_level);

    set_form_field_text(state, F_MIN_CLUES, std::to_wstring(min_clues));
    set_form_field_text(state, F_MAX_CLUES, std::to_wstring(max_clues));
    set_form_field_text(state, F_RESEED_INTERVAL, std::to_wstring(suggested_reseed_s));
    set_form_field_text(state, F_ATTEMPT_TIME_BUDGET, std::to_wstring(suggested_attempt_time_s));
    set_form_field_text(state, F_ATTEMPT_NODE_BUDGET, std::to_wstring(suggested_attempt_nodes));
    if (state->h_form_panel != nullptr) {
        InvalidateRect(state->h_form_panel, nullptr, TRUE);
    }
    append_log(
        state,
        L"Preset: clues=" + std::to_wstring(min_clues) + L"-" + std::to_wstring(max_clues) +
            L", reseed=" + std::to_wstring(suggested_reseed_s) + L"s, attempt_time=" +
            std::to_wstring(suggested_attempt_time_s) + L"s, attempt_nodes=" +
            std::to_wstring(suggested_attempt_nodes) + L", budget_lvl=" +
            std::to_wstring(effective_budget_level));
}

void set_running_state(GuiAppState* state, bool running) {
    state->running.store(running, std::memory_order_relaxed);
    state->form_read_only = running;
    EnableWindow(state->h_start, running ? FALSE : TRUE);
    EnableWindow(state->h_cancel, running ? TRUE : FALSE);
    EnableWindow(state->h_pause, running ? TRUE : FALSE);
    if (state->h_form_panel != nullptr) {
        form_set_focused_field(state, form_first_focusable_key(state), false);
        InvalidateRect(state->h_form_panel, nullptr, TRUE);
    }
}

bool is_live_monitor_enabled(const GuiAppState* state) {
    if (state == nullptr) {
        return true;
    }
    return form_field_checked(state, F_SHOW_MONITOR, true);
}

bool read_config_from_form(GuiAppState* state, GenerateRunConfig& cfg, std::map<std::string, std::string>& dict) {
    cfg.box_rows = form_field_int(state, F_BOX_ROWS, 3);
    cfg.box_cols = form_field_int(state, F_BOX_COLS, 3);
    cfg.target_puzzles = form_field_u64(state, F_TARGET_PUZZLES, 100);
    cfg.difficulty_level_required = std::clamp(form_field_combo_index(state, F_DIFFICULTY, 8) + 1, 1, 9);
    cfg.required_strategy = required_strategy_from_index(form_field_combo_index(state, F_REQUIRED_STRATEGY, 0));
    const int min_clues_input = form_field_int(state, F_MIN_CLUES, 0);
    const int max_clues_input = form_field_int(state, F_MAX_CLUES, 0);
    const ClueRange auto_clues = resolve_auto_clue_range(
        cfg.box_rows,
        cfg.box_cols,
        cfg.difficulty_level_required,
        cfg.required_strategy);
    cfg.min_clues = (min_clues_input > 0) ? min_clues_input : auto_clues.min_clues;
    cfg.max_clues = (max_clues_input > 0) ? max_clues_input : auto_clues.max_clues;
    cfg.threads = form_field_int(state, F_THREADS, 0);
    cfg.seed = form_field_i64(state, F_SEED, 0);
    cfg.reseed_interval_s = form_field_int(state, F_RESEED_INTERVAL, 0);
    cfg.attempt_time_budget_s = static_cast<double>(form_field_u64(state, F_ATTEMPT_TIME_BUDGET, 0));
    cfg.attempt_node_budget = form_field_u64(state, F_ATTEMPT_NODE_BUDGET, 0);
    cfg.max_attempts = form_field_u64(state, F_MAX_ATTEMPTS, 0);
    cfg.max_total_time_s = form_field_u64(state, F_MAX_TOTAL_TIME, 0);  // Globalny limit czasu
    cfg.symmetry_center = form_field_checked(state, F_SYMMETRY_CENTER, false);
    cfg.require_unique = true;
    cfg.write_individual_files = true;
    cfg.output_folder = wide_to_utf8(form_field_text(state, F_OUTPUT_FOLDER, L"generated_sudoku_files"));
    cfg.output_file = wide_to_utf8(form_field_text(state, F_OUTPUT_FILE, L"generated_sudoku.txt"));
    cfg.pause_on_exit_windows = false;

    if (cfg.box_rows <= 0 || cfg.box_cols <= 0) {
        MessageBoxW(state->hwnd, L"box_rows i box_cols musza byc > 0", L"Walidacja", MB_ICONWARNING);
        log_warn("gui.validation", "box_rows/box_cols must be > 0");
        return false;
    }
    if (cfg.max_clues < cfg.min_clues) {
        MessageBoxW(state->hwnd, L"max_clues musi byc >= min_clues", L"Walidacja", MB_ICONWARNING);
        log_warn("gui.validation", "max_clues must be >= min_clues");
        return false;
    }
    if (cfg.target_puzzles == 0) {
        MessageBoxW(state->hwnd, L"target_puzzles musi byc > 0", L"Walidacja", MB_ICONWARNING);
        log_warn("gui.validation", "target_puzzles must be > 0");
        return false;
    }
    const int n = cfg.box_rows * cfg.box_cols;
    const int nn = n * n;
    cfg.min_clues = std::clamp(cfg.min_clues, 0, nn);
    cfg.max_clues = std::clamp(cfg.max_clues, cfg.min_clues, nn);
    if (cfg.output_folder.empty() || cfg.output_file.empty()) {
        MessageBoxW(state->hwnd, L"output_folder i output_file nie moga byc puste", L"Walidacja", MB_ICONWARNING);
        log_warn("gui.validation", "output_folder/output_file cannot be empty");
        return false;
    }

    dict["box_rows"] = std::to_string(cfg.box_rows);
    dict["box_cols"] = std::to_string(cfg.box_cols);
    dict["target_puzzles"] = std::to_string(cfg.target_puzzles);
    dict["min_clues"] = std::to_string(cfg.min_clues);
    dict["max_clues"] = std::to_string(cfg.max_clues);
    dict["difficulty_level_required"] = std::to_string(cfg.difficulty_level_required);
    dict["required_strategy"] = to_string(cfg.required_strategy);
    dict["threads"] = std::to_string(cfg.threads);
    dict["seed"] = std::to_string(cfg.seed);
    dict["reseed_interval_s"] = std::to_string(cfg.reseed_interval_s);
    dict["attempt_time_budget_s"] = std::to_string(static_cast<int>(cfg.attempt_time_budget_s));
    dict["attempt_node_budget_s"] = std::to_string(cfg.attempt_node_budget);
    dict["max_attempts"] = std::to_string(cfg.max_attempts);
    dict["max_total_time_s"] = std::to_string(cfg.max_total_time_s);  // Globalny limit czasu
    dict["symmetry_center"] = cfg.symmetry_center ? "true" : "false";
    dict["require_unique"] = "true";
    dict["output_folder"] = cfg.output_folder;
    dict["output_file"] = cfg.output_file;
    return true;
}

void toggle_pause(GuiAppState* state);
void start_generation(GuiAppState* state);
void cancel_generation(GuiAppState* state);

void start_generation(GuiAppState* state) {
    if (state->running.load(std::memory_order_relaxed)) {
        log_warn("gui.start_generation", "ignored start while already running");
        return;
    }
    GenerateRunConfig cfg;
    std::map<std::string, std::string> dict;
    if (!read_config_from_form(state, cfg, dict)) {
        log_warn("gui.start_generation", "read_config_from_form failed");
        return;
    }

    std::cout << "start_generation() form values:\n";
    for (const auto& [k, v] : dict) {
        std::cout << "  " << k << " = " << v << "\n";
    }
    
    // Show suggestions for profile and currently configured attempt budgets.
    const double suggested_time = suggest_time_budget_s(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required);
    const ClueRange suggested_clues = resolve_auto_clue_range(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required, cfg.required_strategy);
    
    std::wcout << L"\n[SUGESTIA] Dla rozmiaru " << (cfg.box_rows * cfg.box_cols) << L"x" << (cfg.box_rows * cfg.box_cols) << L":\n";
    std::wcout << L"  attempt_time_budget_s: sugerowane=" << static_cast<int>(std::ceil(suggested_time))
               << L"s, ustawione=" << static_cast<int>(cfg.attempt_time_budget_s) << L"s\n";
    std::wcout << L"  min_clues: sugerowane=" << suggested_clues.min_clues << L", ustawione=" << cfg.min_clues << L"\n";
    std::wcout << L"  max_clues: sugerowane=" << suggested_clues.max_clues << L", ustawione=" << cfg.max_clues << L"\n\n";
    
    append_log(state, L"Start generation...");
    log_info(
        "gui.start_generation",
        "start target=" + std::to_string(cfg.target_puzzles) +
            " level=" + std::to_string(cfg.difficulty_level_required) +
            " required=" + to_string(cfg.required_strategy) +
            " threads=" + std::to_string(cfg.threads) +
            " suggested_time_budget=" + std::to_string(suggested_time) +
            " suggested_clues=" + std::to_string(suggested_clues.min_clues) + "-" + std::to_string(suggested_clues.max_clues));
    for (const auto& [k, v] : dict) {
        append_log(state, utf8_to_wide(k + "=" + v));
    }

    state->target = cfg.target_puzzles;
    SendMessageW(state->h_progress, PBM_SETRANGE32, 0, static_cast<LPARAM>(std::min<uint64_t>(cfg.target_puzzles, 0x7fffffffULL)));
    SendMessageW(state->h_progress, PBM_SETPOS, 0, 0);
    set_text(state->h_progress_text, L"Wygenerowano 0/" + std::to_wstring(cfg.target_puzzles));
    state->cancel_requested.store(false, std::memory_order_relaxed);
    state->paused.store(false, std::memory_order_relaxed);
    SetWindowTextW(state->h_pause, L"Pauza");
    set_running_state(state, true);
    SetTimer(state->hwnd, IDT_MONITOR_REFRESH, 500, nullptr);
    set_monitor_text(state, L"Monitor start...");
    const bool live_monitor_on = is_live_monitor_enabled(state);
    EnableWindow(state->h_monitor, live_monitor_on ? TRUE : FALSE);
    if (!live_monitor_on) {
        set_monitor_text(state, L"Live monitor: OFF");
    }

    state->run_thread = std::jthread([state, cfg, live_monitor_on]() {
        auto monitor = std::make_shared<ConsoleStatsMonitor>();
        monitor->start_ui_thread(5000);
        {
            std::lock_guard<std::mutex> lock(state->monitor_mu);
            state->gui_monitor = monitor;
        }
        GenerateRunResult result = run_generic_sudoku(
            cfg,
            monitor.get(),
            &state->cancel_requested,
            &state->paused,
            [hwnd = state->hwnd](uint64_t accepted, uint64_t target) {
                PostMessageW(hwnd, WM_APP_PROGRESS, static_cast<WPARAM>(accepted), static_cast<LPARAM>(target));
            },
            [hwnd = state->hwnd](const std::string& msg) {
                post_log(hwnd, utf8_to_wide(msg));
            });
        log_info(
            "gui.run_thread",
            "run done accepted=" + std::to_string(result.accepted) +
                " written=" + std::to_string(result.written) +
                " attempts=" + std::to_string(result.attempts));
        monitor->stop_ui_thread();
        {
            std::lock_guard<std::mutex> lock(state->monitor_mu);
            state->gui_monitor.reset();
        }
        auto* payload = new GenerateRunResult(result);
        PostMessageW(state->hwnd, WM_APP_DONE, reinterpret_cast<WPARAM>(payload), 0);
    });
}

void publish_cli_status(GuiAppState* state, const std::string& status) {
    if (state == nullptr) {
        return;
    }
    std::shared_ptr<ConsoleStatsMonitor> monitor;
    {
        std::lock_guard<std::mutex> lock(state->monitor_mu);
        monitor = state->gui_monitor;
    }
    if (monitor != nullptr) {
        monitor->set_background_status(status);
    }
}

void cancel_generation(GuiAppState* state) {
    if (!state->running.load(std::memory_order_relaxed)) {
        log_warn("gui.cancel_generation", "cancel ignored - not running");
        return;
    }
    state->cancel_requested.store(true, std::memory_order_relaxed);
    state->paused.store(false, std::memory_order_relaxed);
    SetWindowTextW(state->h_pause, L"Pauza");
    publish_cli_status(state, "Generowanie: cancel requested");
    log_info("gui.cancel_generation", "cancel requested - waiting for workers to finish");
    append_log(state, L"Anulowanie... czekam na zakonczenie workerow.");
}

void toggle_pause(GuiAppState* state) {
    if (!state->running.load(std::memory_order_relaxed)) {
        log_warn("gui.toggle_pause", "pause ignored - not running");
        return;
    }
    const bool new_paused = !state->paused.load(std::memory_order_relaxed);
    state->paused.store(new_paused, std::memory_order_relaxed);
    if (new_paused) {
        append_log(state, L"Pauza - generowanie wstrzymane");
        SetWindowTextW(state->h_pause, L"Wznow");
        publish_cli_status(state, "Generowanie: paused");
        log_info("gui.toggle_pause", "PAUSED - all workers will sleep");
    } else {
        append_log(state, L"Wznowienie - kontynuacja generowania");
        SetWindowTextW(state->h_pause, L"Pauza");
        publish_cli_status(state, "Generowanie: resumed");
        log_info("gui.toggle_pause", "RESUMED - workers continue processing");
    }
}

// 
void sync_live_monitor_visual_state(GuiAppState* state) {
    if (state == nullptr || state->h_monitor == nullptr) {
        return;
    }
    const bool on = is_live_monitor_enabled(state);
    EnableWindow(state->h_monitor, on ? TRUE : FALSE);
    if (!on) {
        set_monitor_text(state, L"Live monitor: OFF");
    } else if (!state->running.load(std::memory_order_relaxed)) {
        append_log(state, L"Live monitor: ON");
    }
}

void form_update_scrollbar(GuiAppState* state) {
    if (state == nullptr || state->h_form_panel == nullptr) {
        return;
    }
    RECT rc{};
    GetClientRect(state->h_form_panel, &rc);
    const int view_h = std::max(1, static_cast<int>(rc.bottom - rc.top));
    state->form_view_height = view_h;
    const int max_scroll = std::max(0, state->form_content_height - view_h);
    state->form_scroll_y = std::clamp(state->form_scroll_y, 0, max_scroll);
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = std::max(0, state->form_content_height - 1);
    si.nPage = static_cast<UINT>(view_h);
    si.nPos = state->form_scroll_y;
    SetScrollInfo(state->h_form_panel, SB_VERT, &si, TRUE);
}

void form_layout_fields(GuiAppState* state) {
    if (state == nullptr || state->h_form_panel == nullptr) {
        return;
    }
    RECT client{};
    GetClientRect(state->h_form_panel, &client);
    const int client_w = std::max(1, static_cast<int>(client.right - client.left));
    const int margin = 14;
    const int row_h = 26;
    const int row_gap = 8;
    const int section_h = 28;
    const int label_w = std::clamp(client_w / 2 - 20, 210, 430);
    const int value_x = margin + label_w + 10;
    const int browse_w = 28;
    const int value_w = std::max(160, client_w - value_x - margin);

    for (auto& field : state->form_fields) {
        field.label_rect = RECT{};
        field.value_rect = RECT{};
        field.browse_rect = RECT{};
    }

    int y = margin;
    FormLayoutItem* current_section = nullptr;
    for (auto& item : state->form_layout) {
        if (item.is_section) {
            item.section_rect = RECT{margin - 4, y + 10, client_w - margin + 4, y + 34};
            current_section = &item;
            y += section_h;
            continue;
        }
        FormField* field = find_form_field(state, item.field_key);
        if (field == nullptr) {
            continue;
        }
        if (item.field_key == F_SYMMETRY_CENTER) {
            FormField* req_unique = find_form_field(state, F_REQUIRE_UNIQUE);
            const int half_w = std::max(220, (client_w - margin * 2 - 24) / 2);
            field->value_rect = RECT{margin + 2, y, margin + 2 + half_w, y + row_h};
            if (req_unique != nullptr) {
                req_unique->value_rect = RECT{margin + 14 + half_w, y, margin + 14 + half_w + half_w, y + row_h};
            }
            if (current_section != nullptr) {
                current_section->section_rect.bottom =
                    std::max(current_section->section_rect.bottom, static_cast<LONG>(y + row_h + 12));
            }
            y += row_h + row_gap;
            continue;
        }

        field->label_rect = RECT{margin, y, margin + label_w, y + row_h};
        if (field->type == FormFieldType::Checkbox) {
            field->value_rect = RECT{margin + 2, y, margin + 2 + value_w, y + row_h};
        } else if (field->has_browse) {
            field->value_rect = RECT{value_x, y, value_x + value_w - (browse_w + 4), y + row_h};
            field->browse_rect = RECT{value_x + value_w - browse_w, y, value_x + value_w, y + row_h};
        } else {
            field->value_rect = RECT{value_x, y, value_x + value_w, y + row_h};
        }
        if (current_section != nullptr) {
            current_section->section_rect.bottom =
                std::max(current_section->section_rect.bottom, static_cast<LONG>(y + row_h + 12));
        }
        y += row_h + row_gap;
    }
    for (auto& item : state->form_layout) {
        if (item.is_section) {
            item.section_rect.bottom = std::max(item.section_rect.bottom, item.section_rect.top + 48);
        }
    }

    state->form_content_height = y + margin;
    form_update_scrollbar(state);
}

bool form_affects_clues_preset(int field_key);
void update_suggestions_for_grid_size(GuiAppState* state);

void form_destroy_inline_editor(GuiAppState* state, bool commit) {
    if (state == nullptr || state->h_inline_editor == nullptr) {
        return;
    }
    FormField* field = find_form_field(state, state->inline_field_key);
    int committed_field_key = 0;
    if (commit && field != nullptr) {
        if (state->inline_is_combo) {
            const int sel = static_cast<int>(SendMessageW(state->h_inline_editor, CB_GETCURSEL, 0, 0));
            if (sel >= 0 && sel < static_cast<int>(field->options.size())) {
                field->option_index = sel;
                committed_field_key = field->key;
            }
        } else {
            field->text = get_text(state->h_inline_editor);
            committed_field_key = field->key;
        }
    }
    DestroyWindow(state->h_inline_editor);
    state->h_inline_editor = nullptr;
    state->inline_field_key = 0;
    state->inline_is_combo = false;
    state->inline_close_posted = false;
    if (commit && !state->form_read_only && form_affects_clues_preset(committed_field_key)) {
        apply_clues_preset(state);
    }
    if (state->h_form_panel != nullptr) {
        InvalidateRect(state->h_form_panel, nullptr, TRUE);
    }
}

void form_request_inline_close(GuiAppState* state, bool commit) {
    if (state == nullptr || state->h_form_panel == nullptr || state->h_inline_editor == nullptr) {
        return;
    }
    if (state->inline_close_posted) {
        return;
    }
    state->inline_close_posted = true;
    PostMessageW(state->h_form_panel, WM_APP_INLINE_CLOSE, commit ? 1 : 0, 0);
}

LRESULT CALLBACK form_inline_editor_subclass_proc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR /*id*/, DWORD_PTR ref_data) {
    auto* state = reinterpret_cast<GuiAppState*>(ref_data);
    if (state != nullptr && msg == WM_KEYDOWN) {
        if (wParam == VK_TAB) {
            const bool back = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            form_request_inline_close(state, true);
            PostMessageW(state->h_form_panel, WM_APP_FORM_NAV, back ? static_cast<WPARAM>(0) : static_cast<WPARAM>(1), 0);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            form_request_inline_close(state, false);
            PostMessageW(state->h_form_panel, WM_APP_FORM_NAV, 0, 1);
            return 0;
        }
        if (!state->inline_is_combo && wParam == VK_RETURN) {
            form_request_inline_close(state, true);
            PostMessageW(state->h_form_panel, WM_APP_FORM_NAV, 0, 1);
            return 0;
        }
    }
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, form_inline_editor_subclass_proc, IDSC_INLINE_EDITOR);
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void form_attach_inline_subclass(GuiAppState* state) {
    if (state == nullptr || state->h_inline_editor == nullptr) {
        return;
    }
    SetWindowSubclass(state->h_inline_editor, form_inline_editor_subclass_proc, IDSC_INLINE_EDITOR, reinterpret_cast<DWORD_PTR>(state));
}

RECT form_view_rect(const RECT& content_rect, int scroll_y) {
    RECT rc = content_rect;
    rc.top -= scroll_y;
    rc.bottom -= scroll_y;
    return rc;
}

void form_begin_inline_edit(GuiAppState* state, FormField* field) {
    if (state == nullptr || field == nullptr || state->h_form_panel == nullptr || state->form_read_only || !field->enabled) {
        return;
    }
    form_destroy_inline_editor(state, true);

    RECT rc = form_view_rect(field->value_rect, state->form_scroll_y);
    const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL |
                        ((field->type == FormFieldType::Int) ? ES_NUMBER : 0);
    state->h_inline_editor = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        field->text.c_str(),
        style,
        rc.left,
        rc.top,
        std::max(60, static_cast<int>(rc.right - rc.left)),
        std::max(22, static_cast<int>(rc.bottom - rc.top)),
        state->h_form_panel,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INLINE_EDIT)),
        GetModuleHandleW(nullptr),
        nullptr);
    if (state->h_inline_editor != nullptr) {
        form_attach_inline_subclass(state);
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(state->h_inline_editor, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(state->h_inline_editor, EM_SETSEL, 0, -1);
        SetFocus(state->h_inline_editor);
        state->inline_field_key = field->key;
        state->inline_is_combo = false;
        state->inline_close_posted = false;
    }
}

void form_begin_inline_combo(GuiAppState* state, FormField* field) {
    if (state == nullptr || field == nullptr || state->h_form_panel == nullptr || state->form_read_only || !field->enabled) {
        return;
    }
    form_destroy_inline_editor(state, true);

    RECT rc = form_view_rect(field->value_rect, state->form_scroll_y);
    state->h_inline_editor = CreateWindowExW(
        0,
        L"COMBOBOX",
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        rc.left,
        rc.top,
        std::max(120, static_cast<int>(rc.right - rc.left)),
        260,
        state->h_form_panel,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INLINE_EDIT)),
        GetModuleHandleW(nullptr),
        nullptr);
    if (state->h_inline_editor != nullptr) {
        form_attach_inline_subclass(state);
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(state->h_inline_editor, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        for (const auto& opt : field->options) {
            SendMessageW(state->h_inline_editor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(opt.c_str()));
        }
        SendMessageW(state->h_inline_editor, CB_SETCURSEL, field->option_index, 0);
        SetFocus(state->h_inline_editor);
        SendMessageW(state->h_inline_editor, CB_SHOWDROPDOWN, TRUE, 0);
        state->inline_field_key = field->key;
        state->inline_is_combo = true;
        state->inline_close_posted = false;
    }
}

void form_set_scroll(GuiAppState* state, int new_scroll) {
    if (state == nullptr || state->h_form_panel == nullptr) {
        return;
    }
    const int max_scroll = std::max(0, state->form_content_height - state->form_view_height);
    new_scroll = std::clamp(new_scroll, 0, max_scroll);
    if (new_scroll == state->form_scroll_y) {
        return;
    }
    form_destroy_inline_editor(state, true);
    state->form_scroll_y = new_scroll;
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS;
    si.nPos = state->form_scroll_y;
    SetScrollInfo(state->h_form_panel, SB_VERT, &si, TRUE);
    InvalidateRect(state->h_form_panel, nullptr, TRUE);
}

bool form_field_is_focusable(const GuiAppState* state, const FormField* field) {
    if (state == nullptr || field == nullptr) {
        return false;
    }
    if (field->key == F_REQUIRE_UNIQUE) {
        return false;
    }
    if (state->form_read_only) {
        return field->key == F_SHOW_MONITOR;
    }
    return field->enabled;
}

int form_first_focusable_key(const GuiAppState* state) {
    if (state == nullptr) {
        return 0;
    }
    for (const auto& item : state->form_layout) {
        if (item.is_section) {
            continue;
        }
        const FormField* field = find_form_field(state, item.field_key);
        if (form_field_is_focusable(state, field)) {
            return item.field_key;
        }
    }
    return 0;
}

int form_next_focusable_key(const GuiAppState* state, int current_key, int direction) {
    if (state == nullptr) {
        return 0;
    }
    std::vector<int> keys;
    keys.reserve(state->form_layout.size());
    for (const auto& item : state->form_layout) {
        if (item.is_section) {
            continue;
        }
        const FormField* field = find_form_field(state, item.field_key);
        if (form_field_is_focusable(state, field)) {
            keys.push_back(item.field_key);
        }
    }
    if (keys.empty()) {
        return 0;
    }
    int idx = -1;
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        if (keys[static_cast<size_t>(i)] == current_key) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        return direction < 0 ? keys.back() : keys.front();
    }
    idx = (idx + (direction < 0 ? -1 : 1) + static_cast<int>(keys.size())) % static_cast<int>(keys.size());
    return keys[static_cast<size_t>(idx)];
}

void form_ensure_field_visible(GuiAppState* state, int field_key) {
    if (state == nullptr || state->h_form_panel == nullptr || field_key == 0) {
        return;
    }
    const FormField* field = find_form_field(state, field_key);
    if (field == nullptr) {
        return;
    }
    RECT rc = field->value_rect;
    if (rc.bottom <= rc.top) {
        return;
    }
    const int top = static_cast<int>(rc.top);
    const int bottom = static_cast<int>(rc.bottom);
    const int view_top = state->form_scroll_y;
    const int view_bottom = state->form_scroll_y + std::max(1, state->form_view_height);
    if (top < view_top + 8) {
        form_set_scroll(state, std::max(0, top - 8));
    } else if (bottom > view_bottom - 8) {
        form_set_scroll(state, bottom - std::max(1, state->form_view_height) + 8);
    }
}

void form_set_focused_field(GuiAppState* state, int field_key, bool ensure_visible) {
    if (state == nullptr) {
        return;
    }
    if (field_key != 0) {
        const FormField* field = find_form_field(state, field_key);
        if (!form_field_is_focusable(state, field)) {
            field_key = 0;
        }
    }
    if (field_key == 0) {
        field_key = form_first_focusable_key(state);
    }
    if (state->focused_field_key == field_key) {
        if (ensure_visible) {
            form_ensure_field_visible(state, field_key);
        }
        return;
    }
    state->focused_field_key = field_key;
    if (ensure_visible) {
        form_ensure_field_visible(state, field_key);
    }
    if (state->h_form_panel != nullptr) {
        InvalidateRect(state->h_form_panel, nullptr, TRUE);
    }
}

void form_navigate_focus(GuiAppState* state, int direction) {
    if (state == nullptr) {
        return;
    }
    const int next_key = form_next_focusable_key(state, state->focused_field_key, direction);
    form_set_focused_field(state, next_key, true);
}

int form_hit_test_field_key(const GuiAppState* state, POINT pt_client, bool* hit_browse) {
    if (state == nullptr) {
        return 0;
    }
    POINT pt = pt_client;
    pt.y += state->form_scroll_y;
    if (hit_browse != nullptr) {
        *hit_browse = false;
    }

    const FormField* sym = find_form_field(state, F_SYMMETRY_CENTER);
    const FormField* req = find_form_field(state, F_REQUIRE_UNIQUE);
    if (sym != nullptr && PtInRect(&sym->value_rect, pt)) {
        return F_SYMMETRY_CENTER;
    }
    if (req != nullptr && PtInRect(&req->value_rect, pt)) {
        return F_REQUIRE_UNIQUE;
    }

    for (const auto& field : state->form_fields) {
        if (field.key == F_REQUIRE_UNIQUE || field.key == F_SYMMETRY_CENTER) {
            continue;
        }
        if (PtInRect(&field.value_rect, pt) || PtInRect(&field.label_rect, pt)) {
            return field.key;
        }
        if (field.has_browse && PtInRect(&field.browse_rect, pt)) {
            if (hit_browse != nullptr) {
                *hit_browse = true;
            }
            return field.key;
        }
    }
    return 0;
}

void form_set_hover_field(GuiAppState* state, int field_key) {
    if (state == nullptr) {
        return;
    }
    if (state->hover_field_key == field_key) {
        return;
    }
    state->hover_field_key = field_key;
    if (state->h_form_panel != nullptr) {
        InvalidateRect(state->h_form_panel, nullptr, TRUE);
    }
}

bool form_affects_clues_preset(int field_key) {
    return field_key == F_CLUES_PRESET ||
           field_key == F_BOX_ROWS ||
           field_key == F_BOX_COLS ||
           field_key == F_DIFFICULTY ||
           field_key == F_REQUIRED_STRATEGY;
}

// Update clues suggestions based on grid size/level/strategy.
void update_suggestions_for_grid_size(GuiAppState* state) {
    if (state == nullptr) {
        return;
    }
    const int box_rows = form_field_int(state, F_BOX_ROWS, 3);
    const int box_cols = form_field_int(state, F_BOX_COLS, 3);
    const int difficulty = std::clamp(form_field_combo_index(state, F_DIFFICULTY, 8) + 1, 1, 9);
    const RequiredStrategy required = required_strategy_from_index(form_field_combo_index(state, F_REQUIRED_STRATEGY, 0));
    const ClueRange suggested_clues = resolve_auto_clue_range(box_rows, box_cols, difficulty, required);
    
    // Update min/max clues fields (only if user hasn't modified them)
    const int current_min = form_field_int(state, F_MIN_CLUES, 0);
    const int current_max = form_field_int(state, F_MAX_CLUES, 0);
    if (current_min == 0) {
        set_form_field_text(state, F_MIN_CLUES, std::to_wstring(suggested_clues.min_clues));
    }
    if (current_max == 0) {
        set_form_field_text(state, F_MAX_CLUES, std::to_wstring(suggested_clues.max_clues));
    }
    
    if (state->h_form_panel != nullptr) {
        InvalidateRect(state->h_form_panel, nullptr, TRUE);
    }
}

bool form_step_combo_value(GuiAppState* state, int field_key, int delta) {
    if (state == nullptr || delta == 0) {
        return false;
    }
    FormField* field = find_form_field(state, field_key);
    if (field == nullptr || field->type != FormFieldType::Combo || field->options.empty()) {
        return false;
    }
    const int count = static_cast<int>(field->options.size());
    int idx = field->option_index;
    idx = (idx + delta + count) % count;
    if (idx == field->option_index) {
        return false;
    }
    field->option_index = idx;
    if (!state->form_read_only && form_affects_clues_preset(field_key)) {
        apply_clues_preset(state);
    }
    if (state->h_form_panel != nullptr) {
        InvalidateRect(state->h_form_panel, nullptr, TRUE);
    }
    return true;
}

void form_activate_focused_field(GuiAppState* state) {
    if (state == nullptr) {
        return;
    }
    FormField* field = find_form_field(state, state->focused_field_key);
    if (field == nullptr || !form_field_is_focusable(state, field)) {
        return;
    }
    switch (field->type) {
    case FormFieldType::Int:
    case FormFieldType::Text:
        form_begin_inline_edit(state, field);
        break;
    case FormFieldType::Combo:
        form_begin_inline_combo(state, field);
        break;
    case FormFieldType::Checkbox:
        if (field->enabled) {
            field->checked = !field->checked;
            if (field->key == F_SHOW_MONITOR) {
                sync_live_monitor_visual_state(state);
            }
            InvalidateRect(state->h_form_panel, nullptr, TRUE);
        }
        break;
    case FormFieldType::ActionButton:
        if (!state->form_read_only) {
            apply_clues_preset(state);
            InvalidateRect(state->h_form_panel, nullptr, TRUE);
        }
        break;
    default:
        break;
    }
}

void form_draw_checkbox(HDC hdc, const RECT& row_rc, const std::wstring& text, bool checked, bool enabled) {
    RECT box = row_rc;
    box.left += 2;
    box.top += 4;
    box.right = box.left + 16;
    box.bottom = box.top + 16;
    UINT state = DFCS_BUTTONCHECK;
    if (checked) {
        state |= DFCS_CHECKED;
    }
    if (!enabled) {
        state |= DFCS_INACTIVE;
    }
    DrawFrameControl(hdc, &box, DFC_BUTTON, state);

    RECT text_rc = row_rc;
    text_rc.left = box.right + 8;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, enabled ? RGB(20, 20, 20) : RGB(130, 130, 130));
    DrawTextW(hdc, text.c_str(), -1, &text_rc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

void form_draw_outline(HDC hdc, const RECT& rc, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

void form_handle_click(GuiAppState* state, POINT pt_client) {
    if (state == nullptr) {
        return;
    }
    bool hit_browse = false;
    const int hit_key = form_hit_test_field_key(state, pt_client, &hit_browse);
    form_destroy_inline_editor(state, true);
    if (hit_key == 0) {
        form_set_focused_field(state, 0, false);
        return;
    }
    form_set_focused_field(state, hit_key, true);

    FormField* field = find_form_field(state, hit_key);
    if (field == nullptr) {
        return;
    }
    if (hit_key == F_REQUIRE_UNIQUE) {
        return;
    }
    if (state->form_read_only && hit_key != F_SHOW_MONITOR) {
        return;
    }
    if (hit_browse && field->has_browse) {
        if (state->form_read_only) {
            return;
        }
        std::wstring path;
        if (field->key == F_OUTPUT_FOLDER) {
            if (browse_for_folder(state->hwnd, path)) {
                field->text = path;
                InvalidateRect(state->h_form_panel, nullptr, TRUE);
            }
        } else if (field->key == F_OUTPUT_FILE) {
            if (browse_for_save_file(state->hwnd, path)) {
                std::filesystem::path p(path);
                field->text = p.filename().wstring();
                set_form_field_text(state, F_OUTPUT_FOLDER, p.parent_path().wstring());
                InvalidateRect(state->h_form_panel, nullptr, TRUE);
            }
        }
        return;
    }

    POINT pt = pt_client;
    pt.y += state->form_scroll_y;
    const bool clicked_value = PtInRect(&field->value_rect, pt);
    if (!clicked_value && field->type != FormFieldType::Checkbox) {
        return;
    }

    switch (field->type) {
    case FormFieldType::Int:
    case FormFieldType::Text:
        form_begin_inline_edit(state, field);
        return;
    case FormFieldType::Combo:
        form_begin_inline_combo(state, field);
        return;
    case FormFieldType::Checkbox:
        if (field->enabled) {
            field->checked = !field->checked;
            if (field->key == F_SHOW_MONITOR) {
                sync_live_monitor_visual_state(state);
            }
            InvalidateRect(state->h_form_panel, nullptr, TRUE);
        }
        return;
    case FormFieldType::ActionButton:
        apply_clues_preset(state);
        InvalidateRect(state->h_form_panel, nullptr, TRUE);
        return;
    default:
        return;
    }
}

void form_paint(HWND hwnd, GuiAppState* state) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = std::max(1, static_cast<int>(client.right - client.left));
    const int height = std::max(1, static_cast<int>(client.bottom - client.top));

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ old_bmp = SelectObject(mem, bmp);

    HBRUSH bg = CreateSolidBrush(RGB(242, 246, 250));
    FillRect(mem, &client, bg);
    DeleteObject(bg);

    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ old_font = SelectObject(mem, font);

    const int scroll_y = state->form_scroll_y;

    for (const auto& item : state->form_layout) {
        if (!item.is_section) {
            continue;
        }
        RECT sec = form_view_rect(item.section_rect, scroll_y);
        if (sec.bottom < 0 || sec.top > height) {
            continue;
        }
        FrameRect(mem, &sec, reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
        RECT title_bg{sec.left + 10, sec.top - 9, sec.left + 320, sec.top + 10};
        HBRUSH bg_br = CreateSolidBrush(RGB(242, 246, 250));
        FillRect(mem, &title_bg, bg_br);
        DeleteObject(bg_br);
        RECT title_txt = title_bg;
        title_txt.left += 4;
        SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, RGB(25, 36, 48));
        DrawTextW(mem, item.section_title.c_str(), -1, &title_txt, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }

    for (const auto& item : state->form_layout) {
        if (item.is_section) {
            continue;
        }

        FormField* field = find_form_field(state, item.field_key);
        if (field == nullptr || item.field_key == F_REQUIRE_UNIQUE) {
            continue;
        }
        RECT label_rc = form_view_rect(field->label_rect, scroll_y);
        RECT value_rc = form_view_rect(field->value_rect, scroll_y);
        RECT browse_rc = form_view_rect(field->browse_rect, scroll_y);
        const bool is_hover = (state->hover_field_key == field->key);
        const bool is_focus = (state->focused_field_key == field->key);
        if (value_rc.bottom < 0 || value_rc.top > height) {
            continue;
        }

        if (item.field_key == F_SYMMETRY_CENTER) {
            FormField* req_unique = find_form_field(state, F_REQUIRE_UNIQUE);
            form_draw_checkbox(mem, value_rc, field->label, field->checked, !state->form_read_only && field->enabled);
            if (req_unique != nullptr) {
                RECT req_rc = form_view_rect(req_unique->value_rect, scroll_y);
                form_draw_checkbox(mem, req_rc, req_unique->label, req_unique->checked, false);
            }
            if (is_focus || is_hover) {
                form_draw_outline(mem, value_rc, is_focus ? RGB(52, 109, 194) : RGB(129, 165, 214));
            }
            continue;
        }

        if (field->type == FormFieldType::Checkbox) {
            form_draw_checkbox(mem, value_rc, field->label, field->checked, !state->form_read_only && field->enabled);
            if (is_focus || is_hover) {
                form_draw_outline(mem, value_rc, is_focus ? RGB(52, 109, 194) : RGB(129, 165, 214));
            }
            continue;
        }

        SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, RGB(26, 26, 26));
        DrawTextW(mem, field->label.c_str(), -1, &label_rc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        RECT frame = value_rc;
        if (field->type == FormFieldType::ActionButton) {
            HBRUSH btn_br = CreateSolidBrush(
                state->form_read_only ? RGB(214, 220, 226) : (is_hover ? RGB(196, 216, 236) : RGB(206, 223, 238)));
            FillRect(mem, &frame, btn_br);
            DeleteObject(btn_br);
            DrawEdge(mem, &frame, EDGE_RAISED, BF_RECT);
            RECT txt = frame;
            txt.left += 8;
            SetTextColor(mem, RGB(20, 34, 50));
            DrawTextW(mem, field->text.c_str(), -1, &txt, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        } else {
            COLORREF field_bg = field->enabled ? (is_hover ? RGB(248, 252, 255) : RGB(255, 255, 255)) : RGB(236, 236, 236);
            HBRUSH white_br = CreateSolidBrush(field_bg);
            FillRect(mem, &frame, white_br);
            DeleteObject(white_br);
            form_draw_outline(mem, frame, RGB(140, 140, 140));
            RECT txt = frame;
            txt.left += 6;
            txt.right -= 6;
            std::wstring display;
            if (field->type == FormFieldType::Combo) {
                if (!field->options.empty() && field->option_index >= 0 && field->option_index < static_cast<int>(field->options.size())) {
                    display = field->options[static_cast<size_t>(field->option_index)];
                }
            } else {
                display = field->text;
            }
            SetTextColor(mem, field->enabled ? RGB(22, 22, 22) : RGB(130, 130, 130));
            DrawTextW(mem, display.c_str(), -1, &txt, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            if (field->type == FormFieldType::Combo) {
                RECT arrow = frame;
                arrow.left = arrow.right - 20;
                DrawFrameControl(mem, &arrow, DFC_SCROLL, DFCS_SCROLLCOMBOBOX);
            }
        }
        if (is_focus || is_hover) {
            form_draw_outline(mem, frame, is_focus ? RGB(52, 109, 194) : RGB(129, 165, 214));
        }

        if (field->has_browse) {
            const bool browse_hover = is_hover;
            HBRUSH b = CreateSolidBrush(state->form_read_only ? RGB(214, 220, 226) : (browse_hover ? RGB(216, 226, 236) : RGB(226, 232, 238)));
            FillRect(mem, &browse_rc, b);
            DeleteObject(b);
            DrawEdge(mem, &browse_rc, EDGE_RAISED, BF_RECT);
            RECT dots = browse_rc;
            DrawTextW(mem, L"...", -1, &dots, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        }
    }

    BitBlt(hdc, 0, 0, width, height, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old_font);
    SelectObject(mem, old_bmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK form_panel_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<GuiAppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* s = reinterpret_cast<GuiAppState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_GETDLGCODE:
        return static_cast<LRESULT>(DLGC_WANTTAB | DLGC_WANTARROWS | DLGC_WANTCHARS);
    case WM_SETFOCUS:
        if (state != nullptr) {
            if (!form_field_is_focusable(state, find_form_field(state, state->focused_field_key))) {
                form_set_focused_field(state, form_first_focusable_key(state), false);
            } else {
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        }
        return 0;
    case WM_KILLFOCUS:
        if (state != nullptr) {
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    case WM_SIZE:
        if (state != nullptr) {
            form_layout_fields(state);
            if (!form_field_is_focusable(state, find_form_field(state, state->focused_field_key))) {
                form_set_focused_field(state, form_first_focusable_key(state), false);
            }
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    case WM_VSCROLL:
        if (state != nullptr) {
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            int pos = state->form_scroll_y;
            switch (LOWORD(wParam)) {
            case SB_TOP:
                pos = 0;
                break;
            case SB_BOTTOM:
                pos = si.nMax;
                break;
            case SB_LINEUP:
                pos -= 26;
                break;
            case SB_LINEDOWN:
                pos += 26;
                break;
            case SB_PAGEUP:
                pos -= static_cast<int>(si.nPage);
                break;
            case SB_PAGEDOWN:
                pos += static_cast<int>(si.nPage);
                break;
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION:
                pos = si.nTrackPos;
                break;
            default:
                break;
            }
            form_set_scroll(state, pos);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (state != nullptr) {
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            const int lines = delta / WHEEL_DELTA;
            form_set_scroll(state, state->form_scroll_y - lines * 48);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state != nullptr) {
            if (!state->mouse_tracking) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                state->mouse_tracking = true;
            }
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            form_set_hover_field(state, form_hit_test_field_key(state, pt, nullptr));
        }
        return 0;
    case WM_MOUSELEAVE:
        if (state != nullptr) {
            state->mouse_tracking = false;
            form_set_hover_field(state, 0);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (state != nullptr) {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            form_handle_click(state, pt);
            if (state->h_inline_editor == nullptr) {
                SetFocus(hwnd);
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (state != nullptr) {
            if (wParam == VK_TAB) {
                const bool back = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                form_navigate_focus(state, back ? -1 : 1);
                return 0;
            }
            if (wParam == VK_UP || wParam == VK_DOWN) {
                const int delta = (wParam == VK_UP) ? -1 : 1;
                if (form_step_combo_value(state, state->focused_field_key, delta)) {
                    return 0;
                }
                form_set_scroll(state, state->form_scroll_y + (delta < 0 ? -26 : 26));
                return 0;
            }
            if (wParam == VK_RETURN || wParam == VK_SPACE) {
                form_activate_focused_field(state);
                return 0;
            }
        }
        break;
    case WM_APP_INLINE_CLOSE:
        if (state != nullptr) {
            const bool commit = (wParam != 0);
            form_destroy_inline_editor(state, commit);
        }
        return 0;
    case WM_APP_FORM_NAV:
        if (state != nullptr) {
            if (lParam != 0) {
                SetFocus(hwnd);
                return 0;
            }
            form_navigate_focus(state, (wParam == 0) ? -1 : 1);
            SetFocus(hwnd);
        }
        return 0;
    case WM_COMMAND:
        if (state != nullptr && LOWORD(wParam) == IDC_INLINE_EDIT) {
            const int code = HIWORD(wParam);
            if (!state->inline_is_combo && code == EN_KILLFOCUS) {
                form_request_inline_close(state, true);
                return 0;
            }
            if (state->inline_is_combo) {
                if (code == CBN_SELENDOK) {
                    form_request_inline_close(state, true);
                    return 0;
                }
                if (code == CBN_SELENDCANCEL) {
                    form_request_inline_close(state, false);
                    return 0;
                }
                if (code == CBN_KILLFOCUS && !state->inline_close_posted) {
                    const LRESULT dropped = SendMessageW(state->h_inline_editor, CB_GETDROPPEDSTATE, 0, 0);
                    if (dropped != 0) {
                        return 0;
                    }
                    form_request_inline_close(state, true);
                    return 0;
                }
            }
        }
        break;
    case WM_PAINT:
        if (state != nullptr) {
            form_paint(hwnd, state);
            return 0;
        }
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void layout_gui_controls(GuiAppState* s) {
    if (s == nullptr) {
        return;
    }
    RECT client{};
    GetClientRect(s->hwnd, &client);
    const int margin = 12;
    const int gap = 8;
    const int client_w = std::max(1, static_cast<int>(client.right - client.left));
    const int client_h = std::max(1, static_cast<int>(client.bottom - client.top));
    const int content_w = std::max(1, client_w - margin * 2);

    const int btn_h = 30;
    const int btn_w = 100;
    const int btn_y = std::max(margin, client_h - margin - btn_h);
    const int btn_total = btn_w * 3 + 20 * 2;
    const int btn_x0 = std::max(margin, client_w / 2 - btn_total / 2);
    SetWindowPos(s->h_start, nullptr, btn_x0, btn_y, btn_w, btn_h, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(s->h_cancel, nullptr, btn_x0 + btn_w + 20, btn_y, btn_w, btn_h, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(s->h_pause, nullptr, btn_x0 + btn_w * 2 + 40, btn_y, btn_w, btn_h, SWP_NOZORDER | SWP_NOACTIVATE);

    const int body_bottom = std::max(margin, btn_y - gap);
    int y = margin;
    int remain = body_bottom - y;
    const int progress_pack_h = 54;
    const int fixed_spacing = gap * 3;
    const int usable = std::max(40, remain - progress_pack_h - fixed_spacing);
    int form_h = std::max(40, (usable * 58) / 100);
    int monitor_h = std::max(0, (usable * 24) / 100);
    int log_h = std::max(0, usable - form_h - monitor_h);
    if (log_h < 36) {
        const int need = 36 - log_h;
        const int take_from_form = std::min(need, std::max(0, form_h - 60));
        form_h -= take_from_form;
        log_h += take_from_form;
    }
    if (log_h < 36) {
        const int need = 36 - log_h;
        const int take_from_monitor = std::min(need, std::max(0, monitor_h - 40));
        monitor_h -= take_from_monitor;
        log_h += take_from_monitor;
    }
    if (form_h < 40) {
        const int need = 40 - form_h;
        const int take = std::min(need, std::max(0, monitor_h - 20));
        monitor_h -= take;
        form_h += take;
    }
    log_h = std::max(0, usable - form_h - monitor_h);

    SetWindowPos(s->h_form_panel, nullptr, margin, y, content_w, form_h, SWP_NOZORDER | SWP_NOACTIVATE);
    y += form_h + gap;
    SetWindowPos(s->h_progress_text, nullptr, margin + 4, y, 380, 22, SWP_NOZORDER | SWP_NOACTIVATE);
    y += 24;
    SetWindowPos(s->h_progress, nullptr, margin, y, content_w, 20, SWP_NOZORDER | SWP_NOACTIVATE);
    y += 20 + gap;
    SetWindowPos(s->h_monitor, nullptr, margin, y, content_w, monitor_h, SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(s->h_monitor, monitor_h > 0 ? SW_SHOW : SW_HIDE);
    y += monitor_h + gap;
    const int final_log_h = std::max(0, body_bottom - y);
    SetWindowPos(s->h_log, nullptr, margin, y, content_w, final_log_h, SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(s->h_log, final_log_h > 0 ? SW_SHOW : SW_HIDE);

    form_layout_fields(s);
}

void create_gui_controls(GuiAppState* s) {
    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    init_form_model(s);

    s->h_form_panel = CreateWindowExW(
        0,
        L"SudokuHpcFormPanelClass",
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0,
        0,
        100,
        100,
        s->hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FORM_PANEL)),
        GetModuleHandleW(nullptr),
        s);

    s->h_progress_text = CreateWindowExW(
        0,
        L"STATIC",
        L"Wygenerowano 0/0",
        WS_CHILD | WS_VISIBLE,
        0,
        0,
        260,
        22,
        s->hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROGRESS_TEXT)),
        GetModuleHandleW(nullptr),
        nullptr);
    s->h_progress = CreateWindowExW(
        0,
        PROGRESS_CLASSW,
        nullptr,
        WS_CHILD | WS_VISIBLE,
        0,
        0,
        260,
        20,
        s->hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROGRESS_BAR)),
        GetModuleHandleW(nullptr),
        nullptr);
    s->h_monitor = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0,
        0,
        260,
        120,
        s->hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_MONITOR_BOX)),
        GetModuleHandleW(nullptr),
        nullptr);
    s->h_log = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0,
        0,
        260,
        90,
        s->hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LOG_BOX)),
        GetModuleHandleW(nullptr),
        nullptr);

    s->h_start = CreateWindowExW(
        0,
        L"BUTTON",
        L"Start",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        0,
        0,
        100,
        30,
        s->hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BTN_START)),
        GetModuleHandleW(nullptr),
        nullptr);
    s->h_cancel = CreateWindowExW(
        0,
        L"BUTTON",
        L"Anuluj",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0,
        0,
        100,
        30,
        s->hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BTN_CANCEL)),
        GetModuleHandleW(nullptr),
        nullptr);
    s->h_pause = CreateWindowExW(
        0,
        L"BUTTON",
        L"Pauza",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0,
        0,
        100,
        30,
        s->hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BTN_PAUSE)),
        GetModuleHandleW(nullptr),
        nullptr);

    const HWND controls[] = {s->h_progress_text, s->h_progress, s->h_monitor, s->h_log, s->h_start, s->h_cancel, s->h_pause};
    for (HWND h : controls) {
        if (h != nullptr) {
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
    }

    HFONT mono_font = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        FF_DONTCARE, L"Consolas");
    if (mono_font != nullptr) {
        s->h_mono_font = mono_font;
        SendMessageW(s->h_monitor, WM_SETFONT, reinterpret_cast<WPARAM>(mono_font), TRUE);
    }

    SendMessageW(s->h_progress, PBM_SETRANGE32, 0, 100);
    SendMessageW(s->h_progress, PBM_SETPOS, 0, 0);
    EnableWindow(s->h_cancel, FALSE);
    layout_gui_controls(s);
    form_set_focused_field(s, form_first_focusable_key(s), false);
    sync_live_monitor_visual_state(s);
    apply_clues_preset(s);
}

LRESULT CALLBACK gui_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<GuiAppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* s = new GuiAppState();
        s->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        create_gui_controls(s);
        append_log(s, L"GUI gotowe.");
        return 0;
    }
    case WM_COMMAND:
        if (state == nullptr) {
            break;
        }
        if (LOWORD(wParam) == IDC_BTN_START) {
            start_generation(state);
            return 0;
        }
        if (LOWORD(wParam) == IDC_BTN_CANCEL) {
            cancel_generation(state);
            return 0;
        }
        if (LOWORD(wParam) == IDC_BTN_PAUSE) {
            toggle_pause(state);
            return 0;
        }
        break;
    case WM_APP_PROGRESS:
        if (state == nullptr) {
            return 0;
        }
        SendMessageW(state->h_progress, PBM_SETRANGE32, 0, static_cast<LPARAM>(std::min<uint64_t>(static_cast<uint64_t>(lParam), 0x7fffffffULL)));
        SendMessageW(state->h_progress, PBM_SETPOS, static_cast<WPARAM>(std::min<uint64_t>(static_cast<uint64_t>(wParam), 0x7fffffffULL)), 0);
        set_text(state->h_progress_text, L"Wygenerowano " + std::to_wstring(static_cast<uint64_t>(wParam)) + L"/" + std::to_wstring(static_cast<uint64_t>(lParam)));
        if (is_live_monitor_enabled(state)) {
            std::shared_ptr<ConsoleStatsMonitor> monitor;
            {
                std::lock_guard<std::mutex> lock(state->monitor_mu);
                monitor = state->gui_monitor;
            }
            if (monitor != nullptr) {
                set_monitor_text(state, utf8_to_wide(monitor->snapshot_text()));
            }
        }
        return 0;
    case WM_APP_LOG:
        if (state == nullptr) {
            return 0;
        } else {
            std::unique_ptr<std::wstring> payload(reinterpret_cast<std::wstring*>(lParam));
            if (payload) {
                append_log(state, *payload);
            }
        }
        return 0;
    case WM_APP_DONE:
        if (state == nullptr) {
            return 0;
        }
        KillTimer(hwnd, IDT_MONITOR_REFRESH);
        {
            std::unique_ptr<GenerateRunResult> result(reinterpret_cast<GenerateRunResult*>(wParam));
            set_running_state(state, false);
            if (result) {
                append_log(state, L"Zakonczono: accepted=" + std::to_wstring(result->accepted) + L", attempts=" + std::to_wstring(result->attempts));
                set_text(state->h_progress_text, L"Wygenerowano " + std::to_wstring(result->written) + L"/" + std::to_wstring(state->target));
                SendMessageW(state->h_progress, PBM_SETPOS, static_cast<WPARAM>(std::min<uint64_t>(result->written, 0x7fffffffULL)), 0);
            }
        }
        if (state->run_thread.joinable()) {
            state->run_thread.join();
        }
        return 0;
    case WM_TIMER:
        if (state != nullptr && wParam == IDT_MONITOR_REFRESH) {
            if (is_live_monitor_enabled(state)) {
                std::shared_ptr<ConsoleStatsMonitor> monitor;
                {
                    std::lock_guard<std::mutex> lock(state->monitor_mu);
                    monitor = state->gui_monitor;
                }
                if (monitor != nullptr) {
                    set_monitor_text(state, utf8_to_wide(monitor->snapshot_text()));
                }
            }
            return 0;
        }
        break;
    case WM_SIZE:
        if (state != nullptr) {
            layout_gui_controls(state);
        }
        return 0;
    case WM_CLOSE:
        if (state != nullptr && state->running.load(std::memory_order_relaxed)) {
            if (MessageBoxW(hwnd, L"Generowanie trwa. Zamknac i anulowac?", L"Potwierdzenie", MB_ICONQUESTION | MB_YESNO) == IDYES) {
                cancel_generation(state);
                DestroyWindow(hwnd);
            }
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (state != nullptr) {
            KillTimer(hwnd, IDT_MONITOR_REFRESH);
            state->cancel_requested.store(true, std::memory_order_relaxed);
            if (state->run_thread.joinable()) {
                state->run_thread.request_stop();
                state->run_thread.join();
            }
            if (state->h_mono_font != nullptr) {
                DeleteObject(state->h_mono_font);
                state->h_mono_font = nullptr;
            }
            delete state;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int run_gui_winapi(HINSTANCE hinst) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_PROGRESS_CLASS | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    const wchar_t* panel_cls = L"SudokuHpcFormPanelClass";
    WNDCLASSW wc_panel{};
    wc_panel.lpfnWndProc = form_panel_wnd_proc;
    wc_panel.hInstance = hinst;
    wc_panel.lpszClassName = panel_cls;
    wc_panel.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc_panel.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc_panel);

    const wchar_t* cls = L"SudokuHpcGuiWndClass";
    WNDCLASSW wc{};
    wc.lpfnWndProc = gui_wnd_proc;
    wc.hInstance = hinst;
    wc.lpszClassName = cls;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int work_w = std::max(640, static_cast<int>(work.right - work.left));
    const int work_h = std::max(480, static_cast<int>(work.bottom - work.top));
    const int wnd_w = std::min(1040, std::max(760, work_w - 32));
    const int wnd_h = std::min(860, std::max(560, work_h - 32));
    const int wnd_x = work.left + std::max(0, (work_w - wnd_w) / 2);
    const int wnd_y = work.top + std::max(0, (work_h - wnd_h) / 2);

    HWND hwnd = CreateWindowExW(
        0,
        cls,
        L"Sudoku HPC Generator - GUI",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        wnd_x,
        wnd_y,
        wnd_w,
        wnd_h,
        nullptr,
        nullptr,
        hinst,
        nullptr);
    if (hwnd == nullptr) {
        return 1;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}

void ensure_console_attached() {
    if (GetConsoleWindow() != nullptr) {
        return;
    }
    if (!AllocConsole()) {
        return;
    }
    std::freopen("CONIN$", "r", stdin);
    std::freopen("CONOUT$", "w", stdout);
    std::freopen("CONOUT$", "w", stderr);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::ios::sync_with_stdio(true);
}

bool is_parent_explorer() {
    const DWORD self_pid = GetCurrentProcessId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD parent_pid = 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snapshot, &pe)) {
        do {
            if (pe.th32ProcessID == self_pid) {
                parent_pid = pe.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &pe));
    }

    std::wstring parent_name;
    if (parent_pid != 0) {
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snapshot, &pe)) {
            do {
                if (pe.th32ProcessID == parent_pid) {
                    parent_name = pe.szExeFile;
                    break;
                }
            } while (Process32NextW(snapshot, &pe));
        }
    }
    CloseHandle(snapshot);

    if (parent_name.empty()) {
        return false;
    }
    std::transform(parent_name.begin(), parent_name.end(), parent_name.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return parent_name == L"explorer.exe";
}
#endif

bool has_arg(int argc, char** argv, const char* key) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == key) {
            return true;
        }
    }
    return false;
}

} // namespace sudoku_hpc
