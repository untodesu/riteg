// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/pch.hh"
#include "riteg/style.hh"

void style::apply(void)
{
    ImGuiStyle &style = ImGui::GetStyle();

    style.DockingSeparatorSize = 2.0f;

    style.ChildRounding         = 0.0f;
    style.FrameRounding         = 0.0f;
    style.GrabRounding          = 0.0f;
    style.PopupRounding         = 0.0f;
    style.ScrollbarRounding     = 0.0f;
    style.TabRounding           = 0.0f;
    style.WindowRounding        = 0.0f;

    style.WindowBorderSize      = 1.0f;
    style.TabBorderSize         = 0.0f;
    style.TabBarOverlineSize    = 0.0f;
    style.TabBarBorderSize      = 1.0f;
    style.PopupBorderSize       = 1.0f;
    style.FrameBorderSize       = 0.0f;
    style.ChildBorderSize       = 1.0f;

    style.Colors[ImGuiCol_Text]                         = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]                 = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]                     = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    style.Colors[ImGuiCol_ChildBg]                      = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg]                      = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    style.Colors[ImGuiCol_Border]                       = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    style.Colors[ImGuiCol_BorderShadow]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]                      = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]               = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive]                = ImVec4(0.62f, 0.62f, 0.62f, 1.00f);
    style.Colors[ImGuiCol_TitleBg]                      = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]                = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]             = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
    style.Colors[ImGuiCol_MenuBarBg]                    = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]                  = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab]                = ImVec4(0.23f, 0.23f, 0.23f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]         = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]          = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]                    = ImVec4(0.77f, 0.77f, 0.77f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]                   = ImVec4(0.77f, 0.77f, 0.77f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive]             = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_Button]                       = ImVec4(0.23f, 0.23f, 0.23f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered]                = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]                 = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_Header]                       = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered]                = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive]                 = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_Separator]                    = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_SeparatorHovered]             = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_SeparatorActive]              = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]                   = ImVec4(0.98f, 0.98f, 0.98f, 0.20f);
    style.Colors[ImGuiCol_ResizeGripHovered]            = ImVec4(0.98f, 0.98f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive]             = ImVec4(0.98f, 0.98f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_TabHovered]                   = ImVec4(0.44f, 0.44f, 0.44f, 0.80f);
    style.Colors[ImGuiCol_Tab]                          = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_TabSelected]                  = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_TabSelectedOverline]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_TabDimmed]                    = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_TabDimmedSelected]            = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_TabDimmedSelectedOverline]    = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_DockingPreview]               = ImVec4(0.50f, 0.50f, 0.50f, 0.70f);
    style.Colors[ImGuiCol_DockingEmptyBg]               = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_PlotLines]                    = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]             = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]                = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]         = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TableHeaderBg]                = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_TableBorderStrong]            = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_TableBorderLight]             = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_TableRowBg]                   = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_TableRowBgAlt]                = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    style.Colors[ImGuiCol_TextLink]                     = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]               = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    style.Colors[ImGuiCol_DragDropTarget]               = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    style.Colors[ImGuiCol_NavHighlight]                 = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_NavWindowingHighlight]        = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    style.Colors[ImGuiCol_NavWindowingDimBg]            = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    style.Colors[ImGuiCol_ModalWindowDimBg]             = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}
