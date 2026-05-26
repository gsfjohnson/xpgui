#include "xpgui/platform.h"

#if defined(XPGUI_PLATFORM_WINDOWS)

#include "xpgui/menu.h"
#include "xpgui/log.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>

namespace xpgui {
namespace win32 {

namespace {

// ---------------------------------------------------------------------------
// Per-item bookkeeping.
//
// Setting MFT_OWNERDRAW on a menu item replaces its string entirely — the
// system no longer stores the label. We stash the original label (split on
// the first \t into label + accelerator) plus a few flags here so draw/measure
// have everything they need without re-querying the menu each time.
// ---------------------------------------------------------------------------

struct Item {
    UINT id = 0;
    std::wstring label;       // text before the first \t
    std::wstring accel;       // text after the first \t (may be empty)
    bool hasSubmenu = false;
    bool isSeparator = false;
};

// Owning storage for per-menu items. Keyed by HMENU so applyOwnerDrawMenu is
// idempotent (a second call updates the same entry).
std::mutex g_mu;
std::unordered_map<HMENU, std::vector<std::unique_ptr<Item>>> g_items;

DarkMenuPalette g_palette = {
    theme::Color::fromCOLORREF(RGB( 32,  32,  32)),  // background
    theme::Color::fromCOLORREF(RGB(230, 230, 230)),  // text
    theme::Color::fromCOLORREF(RGB(120, 120, 120)),  // textDisabled
    theme::Color::fromCOLORREF(RGB( 60,  60,  60)),  // hotBackground
    theme::Color::fromCOLORREF(RGB( 80,  80,  80)),  // separator
};

// Brush used as the popup's MENUINFO::hbrBack so the system paints the
// popup's interior gutter/padding with our background color instead of the
// default light gray. Rebuilt whenever setDarkMenuPalette is called. We do
// NOT delete the old brush — any HMENU previously set to it could still be
// painted on its next refresh. Pre-1.0 leak is small and bounded by palette
// switches.
HBRUSH g_bgBrush = nullptr;

void rebuildBgBrush() {
    g_bgBrush = CreateSolidBrush(g_palette.background.toCOLORREF());
}

// Lazily-cached menu font (matches what Windows uses for system-drawn menus).
HFONT g_menuFont = nullptr;
int   g_menuFontHeight = 0;

void ensureMenuFont() {
    if (g_menuFont) return;
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(ncm);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
        g_menuFont = CreateFontIndirectW(&ncm.lfMenuFont);
    }
    if (!g_menuFont) {
        // Fallback to default GUI font if the system call fails.
        g_menuFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    HDC hdc = GetDC(nullptr);
    HFONT old = (HFONT)SelectObject(hdc, g_menuFont);
    TEXTMETRICW tm = {};
    GetTextMetricsW(hdc, &tm);
    g_menuFontHeight = tm.tmHeight;
    SelectObject(hdc, old);
    ReleaseDC(nullptr, hdc);
}

COLORREF toCOLORREF(theme::Color c) { return c.toCOLORREF(); }

// Split a label like "Cu&t\tCtrl+X" into ("Cu&t", "Ctrl+X"). The "&" prefix
// markers are preserved — DrawTextW with/without DT_HIDEPREFIX interprets
// them at paint time.
void splitLabel(const std::wstring& raw, std::wstring* label, std::wstring* accel) {
    auto tab = raw.find(L'\t');
    if (tab == std::wstring::npos) {
        *label = raw;
        accel->clear();
    } else {
        *label = raw.substr(0, tab);
        *accel = raw.substr(tab + 1);
    }
}

// Build (or update) the per-item context for a single menu position. Returns
// the Item* to stash in dwItemData.
Item* stashItem(HMENU hMenu, UINT position,
                std::vector<std::unique_ptr<Item>>& bucket) {
    // Read everything we need with one query.
    wchar_t labelBuf[256] = {};
    MENUITEMINFOW mii = {};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU
              | MIIM_STRING;
    mii.dwTypeData = labelBuf;
    mii.cch = (UINT)(sizeof(labelBuf) / sizeof(labelBuf[0]) - 1);
    if (!GetMenuItemInfoW(hMenu, position, TRUE, &mii)) {
        return nullptr;
    }

    auto it = std::make_unique<Item>();
    it->id = mii.wID;
    it->hasSubmenu = (mii.hSubMenu != nullptr);
    it->isSeparator = (mii.fType & MFT_SEPARATOR) != 0;
    if (!it->isSeparator) {
        std::wstring raw(labelBuf, mii.cch);
        splitLabel(raw, &it->label, &it->accel);
    }

    Item* raw = it.get();
    bucket.push_back(std::move(it));
    return raw;
}

void applyOne(HMENU hMenu, UINT position, Item* item) {
    MENUITEMINFOW mii = {};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_FTYPE | MIIM_DATA;
    if (!GetMenuItemInfoW(hMenu, position, TRUE, &mii)) return;

    mii.fType = (mii.fType & MFT_SEPARATOR) | MFT_OWNERDRAW;
    mii.dwItemData = (ULONG_PTR)item;
    SetMenuItemInfoW(hMenu, position, TRUE, &mii);
}

void applyRecursive(HMENU hMenu, bool recursive) {
    if (!hMenu) return;

    std::unique_lock<std::mutex> lock(g_mu);
    auto& bucket = g_items[hMenu];
    bucket.clear();  // idempotent: rebuild from scratch.

    int count = GetMenuItemCount(hMenu);
    for (int i = 0; i < count; ++i) {
        Item* it = stashItem(hMenu, (UINT)i, bucket);
        if (!it) continue;
        applyOne(hMenu, (UINT)i, it);
    }

    // Override the popup's background brush so the system-painted gutter
    // and padding around our owner-drawn items use our background color
    // instead of the default light gray. Without this, the popup shows a
    // light frame around dark items.
    if (g_bgBrush) {
        MENUINFO mi = {};
        mi.cbSize = sizeof(mi);
        mi.fMask = MIM_BACKGROUND;
        mi.hbrBack = g_bgBrush;
        SetMenuInfo(hMenu, &mi);
    }
    lock.unlock();

    if (recursive) {
        int count2 = GetMenuItemCount(hMenu);
        for (int i = 0; i < count2; ++i) {
            HMENU sub = GetSubMenu(hMenu, i);
            if (sub) applyRecursive(sub, true);
        }
    }
}

void restoreRecursive(HMENU hMenu, bool recursive) {
    if (!hMenu) return;

    if (recursive) {
        int count = GetMenuItemCount(hMenu);
        for (int i = 0; i < count; ++i) {
            HMENU sub = GetSubMenu(hMenu, i);
            if (sub) restoreRecursive(sub, true);
        }
    }

    std::unique_lock<std::mutex> lock(g_mu);
    auto it = g_items.find(hMenu);
    if (it == g_items.end()) return;

    // Clear the custom background brush so the system reverts to the default
    // menu background. NULL hbrBack with MIM_BACKGROUND restores the default.
    {
        MENUINFO mi = {};
        mi.cbSize = sizeof(mi);
        mi.fMask = MIM_BACKGROUND;
        mi.hbrBack = nullptr;
        SetMenuInfo(hMenu, &mi);
    }

    // Restore each item to a standard MF_STRING (or MF_SEPARATOR).
    int count = GetMenuItemCount(hMenu);
    for (int i = 0; i < count; ++i) {
        MENUITEMINFOW mii = {};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_FTYPE | MIIM_DATA;
        if (!GetMenuItemInfoW(hMenu, i, TRUE, &mii)) continue;

        Item* ctx = (Item*)mii.dwItemData;
        // Strip MFT_OWNERDRAW; keep MFT_SEPARATOR if present.
        mii.fType &= ~MFT_OWNERDRAW;
        mii.dwItemData = 0;

        if (ctx && !ctx->isSeparator) {
            // Rebuild the original "label\taccel" string.
            std::wstring full = ctx->label;
            if (!ctx->accel.empty()) {
                full += L'\t';
                full += ctx->accel;
            }
            mii.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_STRING;
            mii.dwTypeData = full.data();
            mii.cch = (UINT)full.size();
            SetMenuItemInfoW(hMenu, i, TRUE, &mii);
        } else {
            mii.fMask = MIIM_FTYPE | MIIM_DATA;
            SetMenuItemInfoW(hMenu, i, TRUE, &mii);
        }
    }

    g_items.erase(it);
}

// ---------------------------------------------------------------------------
// Measure / draw helpers
// ---------------------------------------------------------------------------

constexpr int kCheckGutter  = 24;  // left gutter for checkmark
constexpr int kArrowGutter  = 16;  // right gutter for submenu arrow
constexpr int kAccelGap     = 24;  // gap between label and accel
constexpr int kSidePadding  = 12;  // base horizontal padding (extra slack)
constexpr int kVertPadding  = 6;   // top/bottom item padding
constexpr int kSeparatorH   = 7;   // separator total height

int measureTextWidth(const std::wstring& s) {
    if (s.empty()) return 0;
    ensureMenuFont();
    HDC hdc = GetDC(nullptr);
    HFONT old = (HFONT)SelectObject(hdc, g_menuFont);
    RECT r = { 0, 0, 0, 0 };
    DrawTextW(hdc, s.c_str(), (int)s.size(), &r,
              DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, old);
    ReleaseDC(nullptr, hdc);
    return r.right - r.left;
}

void paintSubmenuArrow(HDC hdc, const RECT& itemRect, COLORREF color) {
    // Simple right-pointing triangle in the right gutter.
    int cy = (itemRect.top + itemRect.bottom) / 2;
    int cx = itemRect.right - kArrowGutter / 2 - 2;
    int s  = 4;  // half size
    POINT pts[3] = {
        { cx - s/2, cy - s },
        { cx + s/2, cy     },
        { cx - s/2, cy + s },
    };
    HBRUSH br = CreateSolidBrush(color);
    HPEN   pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH oldBr  = (HBRUSH)SelectObject(hdc, br);
    HPEN   oldPen = (HPEN)SelectObject(hdc, pen);
    Polygon(hdc, pts, 3);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);
}

void paintCheckmark(HDC hdc, const RECT& itemRect, COLORREF color) {
    // Render the standard Unicode check (U+2713) in the menu font, vertically
    // centered in the left gutter. Hinting/anti-aliasing make this look
    // crisper than a hand-drawn Polyline, particularly at hi-DPI.
    RECT r = itemRect;
    r.right = r.left + kCheckGutter;
    COLORREF oldColor = GetTextColor(hdc);
    SetTextColor(hdc, color);
    DrawTextW(hdc, L"✓", 1, &r,
              DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    SetTextColor(hdc, oldColor);
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void setDarkMenuPalette(const DarkMenuPalette& palette) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_palette = palette;
    rebuildBgBrush();
}

void applyOwnerDrawMenu(void* hMenu, bool recursive) {
    applyRecursive(reinterpret_cast<HMENU>(hMenu), recursive);
}

void restoreStandardMenu(void* hMenu, bool recursive) {
    restoreRecursive(reinterpret_cast<HMENU>(hMenu), recursive);
}

bool handleDarkMenuMeasure(void* measureItemStruct) {
    auto* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(measureItemStruct);
    if (!mis || mis->CtlType != ODT_MENU) return false;
    Item* item = (Item*)mis->itemData;

    ensureMenuFont();
    if (item && item->isSeparator) {
        mis->itemWidth  = 0;
        mis->itemHeight = kSeparatorH;
        return true;
    }

    int labelW = item ? measureTextWidth(item->label) : 0;
    int accelW = item ? measureTextWidth(item->accel) : 0;

    int w = kCheckGutter + labelW + kSidePadding;
    if (accelW > 0) w += kAccelGap + accelW;
    w += kArrowGutter;  // reserve for submenu arrow (whether or not present)

    mis->itemWidth  = (UINT)w;
    mis->itemHeight = (UINT)(g_menuFontHeight + 2 * kVertPadding);
    return true;
}

bool handleDarkMenuDraw(void* drawItemStruct) {
    auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(drawItemStruct);
    if (!dis || dis->CtlType != ODT_MENU) return false;
    Item* item = (Item*)dis->itemData;

    DarkMenuPalette pal;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        pal = g_palette;
    }

    HDC hdc = dis->hDC;
    RECT r = dis->rcItem;
    bool selected = (dis->itemState & ODS_SELECTED) != 0;
    bool disabled = (dis->itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
    bool checked  = (dis->itemState & ODS_CHECKED) != 0;
    bool noAccel  = (dis->itemState & ODS_NOACCEL) != 0;

    // Background.
    {
        COLORREF bg = selected ? toCOLORREF(pal.hotBackground)
                               : toCOLORREF(pal.background);
        HBRUSH br = CreateSolidBrush(bg);
        FillRect(hdc, &r, br);
        DeleteObject(br);
    }

    // Separator: single horizontal line.
    if (item && item->isSeparator) {
        int y = (r.top + r.bottom) / 2;
        HPEN pen = CreatePen(PS_SOLID, 1, toCOLORREF(pal.separator));
        HPEN old = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, r.left + 6, y, nullptr);
        LineTo(hdc, r.right - 6, y);
        SelectObject(hdc, old);
        DeleteObject(pen);
        return true;
    }

    COLORREF fg = disabled ? toCOLORREF(pal.textDisabled)
                           : toCOLORREF(pal.text);

    ensureMenuFont();
    HFONT oldFont = (HFONT)SelectObject(hdc, g_menuFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fg);

    // Checkmark.
    if (checked) {
        paintCheckmark(hdc, r, fg);
    }

    // Label (left, after gutter).
    if (item && !item->label.empty()) {
        RECT lr = r;
        lr.left  += kCheckGutter;
        lr.right -= kArrowGutter;
        UINT flags = DT_SINGLELINE | DT_VCENTER | DT_LEFT;
        if (noAccel) flags |= DT_HIDEPREFIX;
        DrawTextW(hdc, item->label.c_str(), (int)item->label.size(), &lr, flags);
    }

    // Accelerator (right, before submenu-arrow gutter).
    if (item && !item->accel.empty()) {
        RECT ar = r;
        ar.left  += kCheckGutter;
        ar.right -= kArrowGutter;
        SetTextColor(hdc, disabled ? toCOLORREF(pal.textDisabled)
                                   : toCOLORREF(pal.textDisabled));
        DrawTextW(hdc, item->accel.c_str(), (int)item->accel.size(), &ar,
                  DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
        SetTextColor(hdc, fg);
    }

    // Submenu arrow.
    if (item && item->hasSubmenu) {
        paintSubmenuArrow(hdc, r, fg);
    }

    SelectObject(hdc, oldFont);
    return true;
}

// ---------------------------------------------------------------------------
// PopupFrameScope — thread-local WH_CBT hook that applies
// DWMWA_USE_IMMERSIVE_DARK_MODE to popup-menu windows (class #32768) at
// creation so their non-client frame matches the owner window's DWM frame.
// ---------------------------------------------------------------------------

namespace {

thread_local int  tl_frameDepth = 0;
thread_local bool tl_frameDark  = false;
thread_local HHOOK tl_frameHook = nullptr;

LRESULT CALLBACK popupFrameCbtProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HCBT_CREATEWND) {
        HWND hwnd = reinterpret_cast<HWND>(wParam);
        if (hwnd) {
            wchar_t cls[16] = {};
            int n = GetClassNameW(hwnd, cls, 16);
            // Popup-menu window class is "#32768".
            if (n > 0 && lstrcmpW(cls, L"#32768") == 0) {
                BOOL useDark = tl_frameDark ? TRUE : FALSE;
                // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 on Win10 20H1+
                DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));
                SetWindowTheme(hwnd, tl_frameDark ? L"DarkMode_Explorer" : nullptr, nullptr);

                // Pin the popup's outer border to the host-supplied color
                // (palette.separator, which the app maps from Theme.border).
                // DWMWA_BORDER_COLOR = 34, Win11 22H2+; older Windows: no-op.
                COLORREF border;
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    border = g_palette.separator.toCOLORREF();
                }
                DwmSetWindowAttribute(hwnd, 34, &border, sizeof(border));
            }
        }
    }
    return CallNextHookEx(tl_frameHook, code, wParam, lParam);
}

}  // namespace

PopupFrameScope::PopupFrameScope(theme::Mode mode) {
    tl_frameDark = (mode == theme::Mode::Dark);
    if (tl_frameDepth++ == 0) {
        tl_frameHook = SetWindowsHookExW(WH_CBT, popupFrameCbtProc, nullptr,
                                          GetCurrentThreadId());
    }
}

PopupFrameScope::~PopupFrameScope() {
    if (--tl_frameDepth == 0 && tl_frameHook) {
        UnhookWindowsHookEx(tl_frameHook);
        tl_frameHook = nullptr;
    }
}

}  // namespace win32
}  // namespace xpgui

#endif  // XPGUI_PLATFORM_WINDOWS
