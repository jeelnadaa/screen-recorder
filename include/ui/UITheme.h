#pragma once

#include <cstdint>

#if defined(_WIN32)
#include <d2d1.h>
#endif

namespace Recorder::Ui {

    struct ColorRGB {
        float r, g, b, a;
    };

    namespace Theme {
        // Modern Fluent Dark Color Palette
        constexpr ColorRGB BackgroundDark      = { 0.08f, 0.08f, 0.09f, 1.0f }; // #141417
        constexpr ColorRGB SurfaceCard         = { 0.13f, 0.13f, 0.15f, 1.0f }; // #212126
        constexpr ColorRGB SurfaceCardHover    = { 0.17f, 0.17f, 0.20f, 1.0f }; // #2B2B33
        constexpr ColorRGB BorderSubtle        = { 0.22f, 0.22f, 0.26f, 1.0f }; // #383842
        
        constexpr ColorRGB AccentBlue          = { 0.00f, 0.47f, 0.83f, 1.0f }; // #0078D4
        constexpr ColorRGB AccentBlueHover     = { 0.10f, 0.55f, 0.90f, 1.0f }; // #1A8CE6
        constexpr ColorRGB RecordRed           = { 0.92f, 0.16f, 0.16f, 1.0f }; // #EB2929
        constexpr ColorRGB WarningYellow       = { 1.00f, 0.78f, 0.00f, 1.0f }; // #FFC700
        constexpr ColorRGB SuccessGreen        = { 0.10f, 0.75f, 0.35f, 1.0f }; // #1ABF59

        constexpr ColorRGB TextPrimary         = { 0.96f, 0.96f, 0.97f, 1.0f }; // #F5F5F7
        constexpr ColorRGB TextSecondary       = { 0.65f, 0.65f, 0.70f, 1.0f }; // #A6A6B3
        constexpr ColorRGB TextMuted           = { 0.45f, 0.45f, 0.50f, 1.0f }; // #737380
    }

    enum class NavigationTab {
        Sources,
        Video,
        Audio,
        Hotkeys,
        Output,
        Library,
        Diagnostics
    };

} // namespace Recorder::Ui
