// samp_grass_restore - GrassRestore.asi
// Author: yessirks
//
// Brings the grass and plants back in SA-MP.
//
// Why there is no grass in SA-MP: samp.dll overwrites the call to CPlantMgr::Update (inside CGame::Process, 0x53C159)
// with five NOPs. That function creates the grass/plant patches around the camera every frame, so without it the plant
// lists stay empty and CPlantMgr::Render (which SA-MP leaves alone) has nothing to draw.
//
// This plugin puts that one call back as soon as it finds the NOPs, and keeps watching in case SA-MP patches it again
// (e.g. on every connect). It only ever replaces the exact "5 x NOP" pattern, so it never overrides another plugin's own
// hook there, and it does nothing unless it recognises the stock GTA:SA 1.0 US executable.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

namespace {

constexpr uintptr_t kSite = 0x53C159;        // CALL CPlantMgr::Update in CGame::Process
constexpr uintptr_t kUpdate = 0x5DCFA0;      // CPlantMgr::Update
constexpr uintptr_t kRender = 0x5DBAE0;      // CPlantMgr::Render
constexpr uintptr_t kRenderSite = 0x53E103;  // CALL CPlantMgr::Render in Idle (used to recognise the exe)
constexpr uintptr_t kScanCode = 0xC03980;    // CPlantMgr::m_scanCode (int16), incremented by every Update

FILE* g_log = nullptr;
DWORD g_t0 = 0;

void Log(const char* fmt, ...) {
    if (!g_log) return;
    fprintf(g_log, "[%7.1fs] ", (GetTickCount() - g_t0) / 1000.0);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

bool IsNops(const uint8_t* p) {
    for (int i = 0; i < 5; i++) if (p[i] != 0x90) return false;
    return true;
}

bool CallsTo(uintptr_t site, uintptr_t target) {
    const uint8_t* p = (const uint8_t*)site;
    return p[0] == 0xE8 && site + 5 + *(const int32_t*)(p + 1) == target;
}

// Is this really the stock 1.0 US exe at the places we look at? (Other game versions have other code there.)
bool LooksLikeStockExe() {
    const uint8_t* update = (const uint8_t*)kUpdate;   // 51 33 C0 66 A1 <m_scanCode>
    const uint8_t* render = (const uint8_t*)kRender;   // 83 EC 78 B9 <g_fx>
    bool updOk = update[0] == 0x51 && update[1] == 0x33 && update[2] == 0xC0 && update[3] == 0x66 && update[4] == 0xA1 &&
                 *(const uint32_t*)(update + 5) == kScanCode;
    bool renOk = render[0] == 0x83 && render[1] == 0xEC && render[2] == 0x78 && render[3] == 0xB9;
    return updOk && renOk && CallsTo(kRenderSite, kRender);
}

// Writes the original `call CPlantMgr::Update` over the five NOPs. The game thread may run this code at any moment, so it
// is done with ONE locked 8-byte compare-exchange (the 3 bytes after the call are written back unchanged), not byte by byte.
bool RestoreCall() {
    DWORD old;
    if (!VirtualProtect((void*)kSite, 8, PAGE_EXECUTE_READWRITE, &old)) return false;
    volatile LONG64* p = (volatile LONG64*)kSite;
    bool done = false;
    for (int tries = 0; tries < 16 && !done; tries++) {
        LONG64 cur = *p;
        uint8_t b[8];
        memcpy(b, &cur, 8);
        if (!IsNops(b)) break;                       // changed under us: leave it alone
        b[0] = 0xE8;
        int32_t rel = (int32_t)(kUpdate - (kSite + 5));
        memcpy(b + 1, &rel, 4);
        LONG64 want;
        memcpy(&want, b, 8);
        done = InterlockedCompareExchange64(p, want, cur) == cur;
    }
    VirtualProtect((void*)kSite, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)kSite, 8);
    return done;
}

DWORD WINAPI Watcher(LPVOID) {
    bool stock = false, warnedForeign = false, verified = true;
    int restores = 0, waits = 0;
    int16_t scanAtRestore = 0;
    DWORD restoredAt = 0;
    for (;; Sleep(250)) {
        if (!stock) {                                // wait until the game's code is readable and recognised
            if (IsBadReadPtr((void*)kRenderSite, 8) || IsBadReadPtr((void*)kUpdate, 16)) continue;
            if (!LooksLikeStockExe()) {
                if (++waits == 1) Log("waiting: the code at the expected places does not look like the stock 1.0 US exe (yet?)");
                if (waits > 240) { Log("giving up: not a 1.0 US exe, nothing is changed"); return 0; }
                continue;
            }
            stock = true;
            Log("stock 1.0 US exe recognised, watching the CPlantMgr::Update call at %X", (unsigned)kSite);
        }
        const uint8_t* p = (const uint8_t*)kSite;
        if (IsNops(p)) {
            if (RestoreCall()) {
                restores++;
                scanAtRestore = *(volatile int16_t*)kScanCode;
                restoredAt = GetTickCount();
                verified = false;
                Log("the call was NOPed out (by SA-MP): restored it (#%d)", restores);
            } else {
                Log("could not restore the call");
            }
        } else if (!CallsTo(kSite, kUpdate) && !warnedForeign) {
            warnedForeign = true;
            Log("another plugin has its own code at %X (%02X %02X %02X %02X %02X): leaving it alone", (unsigned)kSite, p[0], p[1], p[2], p[3], p[4]);
        }
        // self check: the plant manager's scan code moves with every Update
        if (!verified && GetTickCount() - restoredAt > 3000) {
            int16_t now = *(volatile int16_t*)kScanCode;
            Log("self-check: CPlantMgr scanCode %d -> %d: %s", scanAtRestore, now,
                now != scanAtRestore ? "Update is running, the grass is back" : "Update is NOT running yet (still loading or not in game?)");
            if (now != scanAtRestore) verified = true; else restoredAt = GetTickCount();
        }
    }
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE h, DWORD why, LPVOID) {
    if (why == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        char path[MAX_PATH];
        GetModuleFileNameA(h, path, MAX_PATH);
        char* slash = strrchr(path, '\\');
        if (slash) *slash = 0;
        char ini[MAX_PATH], logp[MAX_PATH];
        snprintf(ini, sizeof ini, "%s\\GrassRestore.ini", path);
        snprintf(logp, sizeof logp, "%s\\GrassRestore.log", path);
        int enabled = GetPrivateProfileIntA("GrassRestore", "Enabled", 1, ini);
        if (GetPrivateProfileIntA("GrassRestore", "Log", 0, ini)) g_log = fopen(logp, "w");
        g_t0 = GetTickCount();
        Log("GrassRestore v1 loaded (Enabled=%d)", enabled);
        if (enabled) CreateThread(nullptr, 0, Watcher, nullptr, 0, nullptr);
    }
    return TRUE;
}
