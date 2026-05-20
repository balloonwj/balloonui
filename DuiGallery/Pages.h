#pragma once

#include <memory>
#include <vector>
#include <atlstr.h>
#include "DuiControl.h"

// Each Build_* function returns a fully-built page (a vertical box that
// stacks header + description labels + variant rows). Pages are rebuilt
// fresh whenever the user switches tabs - cheap because controls are POD.
namespace Pages {

// Doc-screenshot infrastructure (used by --capture-all CLI mode).
//
// Each Build_*() can register a "capture mark" pointing at a
// freshly-added DuiControl (typically the row container returned by
// AddVariantRow). When the harness runs in --capture-all mode it walks
// the page list, calls each Build_*(), forces layout / paint, then
// captures each marked control's host-client rect to an individual PNG
// named ctl-<mark-name>.png in the output directory.
//
// The mark anchor is a non-owning raw pointer; it stays valid until the
// next page is built (the previous page's controls are torn down).
// Marks must therefore be consumed before SwitchToPage moves on.
struct CaptureMark
{
    CString             name;
    balloonwjui::DuiControl* anchor; // non-owning
};

std::vector<CaptureMark>& GetCaptureMarks();
void RegisterCapture(LPCTSTR name, balloonwjui::DuiControl* anchor);

std::unique_ptr<balloonwjui::DuiControl> Build_Button     ();
std::unique_ptr<balloonwjui::DuiControl> Build_Avatar     ();
std::unique_ptr<balloonwjui::DuiControl> Build_Label      ();
std::unique_ptr<balloonwjui::DuiControl> Build_Edit       ();
std::unique_ptr<balloonwjui::DuiControl> Build_RichEdit   ();
std::unique_ptr<balloonwjui::DuiControl> Build_ComboBox   ();
std::unique_ptr<balloonwjui::DuiControl> Build_Slider     ();
std::unique_ptr<balloonwjui::DuiControl> Build_Switch     ();
std::unique_ptr<balloonwjui::DuiControl> Build_ProgressBar();
std::unique_ptr<balloonwjui::DuiControl> Build_Tab        ();
std::unique_ptr<balloonwjui::DuiControl> Build_ListBox    ();
std::unique_ptr<balloonwjui::DuiControl> Build_VirtualList();
std::unique_ptr<balloonwjui::DuiControl> Build_ScrollView ();
std::unique_ptr<balloonwjui::DuiControl> Build_Menu       ();
std::unique_ptr<balloonwjui::DuiControl> Build_MenuBar    ();
std::unique_ptr<balloonwjui::DuiControl> Build_ToolTip    ();
std::unique_ptr<balloonwjui::DuiControl> Build_Layout     ();
std::unique_ptr<balloonwjui::DuiControl> Build_Splitter   ();
std::unique_ptr<balloonwjui::DuiControl> Build_TabPage    ();
std::unique_ptr<balloonwjui::DuiControl> Build_TreeView   ();
std::unique_ptr<balloonwjui::DuiControl> Build_PopupHost  ();
std::unique_ptr<balloonwjui::DuiControl> Build_Emoji      ();
std::unique_ptr<balloonwjui::DuiControl> Build_FrameWindow();
std::unique_ptr<balloonwjui::DuiControl> Build_Dock       ();
std::unique_ptr<balloonwjui::DuiControl> Build_LoginXml   ();
std::unique_ptr<balloonwjui::DuiControl> Build_Tier3      ();
std::unique_ptr<balloonwjui::DuiControl> Build_Theme      ();
std::unique_ptr<balloonwjui::DuiControl> Build_DocCaptures();
std::unique_ptr<balloonwjui::DuiControl> Build_Layouts();

// Page metadata for the tab bar.
struct PageInfo { LPCTSTR title; std::unique_ptr<balloonwjui::DuiControl> (*build)(); };
const PageInfo* GetPages(int& outCount);

} // namespace Pages
