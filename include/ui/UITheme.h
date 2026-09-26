#pragma once

#include <cstdint>

#if defined(_WIN32)
#include <d2d1.h>
#endif

namespace Recorder {
namespace Ui {

    struct ColorRGB {
        float r, g, b, a;
    };

    namespace Theme {
        // Modern Fluent Dark Color Palette (Matched to Screenshots)
        constexpr ColorRGB BackgroundDark      = { 0.070f, 0.070f, 0.075f, 1.0f }; // #121214 deep black/dark charcoal
        constexpr ColorRGB SidebarBg           = { 0.070f, 0.070f, 0.075f, 1.0f }; // Seamless sidebar
        constexpr ColorRGB SurfaceCard         = { 0.106f, 0.106f, 0.118f, 1.0f }; // #1b1b1e card background
        constexpr ColorRGB SurfaceCardHover    = { 0.145f, 0.145f, 0.160f, 1.0f }; // #252529 card hover
        constexpr ColorRGB SurfaceCardSelected = { 0.055f, 0.157f, 0.282f, 1.0f }; // #0e2848 selected card subtle blue tint
        constexpr ColorRGB BorderSubtle        = { 0.180f, 0.180f, 0.196f, 1.0f }; // #2e2e32 subtle border
        constexpr ColorRGB BorderSelected      = { 0.102f, 0.451f, 0.910f, 1.0f }; // #1a73e8 vibrant blue border

        constexpr ColorRGB AccentBlue          = { 0.102f, 0.451f, 0.910f, 1.0f }; // #1a73e8 primary blue
        constexpr ColorRGB AccentBlueHover     = { 0.165f, 0.510f, 0.960f, 1.0f }; // #2a82f5
        constexpr ColorRGB AccentBluePill      = { 0.063f, 0.231f, 0.420f, 1.0f }; // #103b6b active sidebar pill
        constexpr ColorRGB RecordRed           = { 0.920f, 0.180f, 0.180f, 1.0f }; // #ea2e2e
        constexpr ColorRGB RecordRedHover      = { 1.000f, 0.260f, 0.260f, 1.0f }; // #ff4242
        constexpr ColorRGB WarningYellow       = { 0.960f, 0.710f, 0.100f, 1.0f }; // #f5b51a
        constexpr ColorRGB SuccessGreen        = { 0.204f, 0.780f, 0.349f, 1.0f }; // #34c759

        constexpr ColorRGB TextPrimary         = { 0.960f, 0.960f, 0.970f, 1.0f }; // #f5f5f7 crisp white
        constexpr ColorRGB TextSecondary       = { 0.650f, 0.650f, 0.690f, 1.0f }; // #a6a6b0 muted light gray
        constexpr ColorRGB TextMuted           = { 0.450f, 0.450f, 0.480f, 1.0f }; // #73737a dark gray / labels
    }

    enum class NavigationTab {
        Source = 0,
        Video,
        Audio,
        Hotkeys,
        Output,
        Library,
        Advanced
    };

    enum class SourceMode {
        Monitor = 0,
        Window,
        App,
        Region
    };

    enum class IconType {
        Source,
        Video,
        Audio,
        Hotkeys,
        Output,
        Library,
        Advanced,
        Monitor,
        Window,
        App,
        Region,
        ChevronDown,
        Chip,
        Warning,
        Record,
        Camera,
        DotsMenu,
        Search,
        Folder,
        Share,
        Trash
    };

} // namespace Ui
} // namespace Recorder
