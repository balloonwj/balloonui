# balloonui

**Language:** English | [中文](README.md)

A no-HWND DUI (DirectUI) control library for Windows desktop apps, shipped
together with several full-feature demos and an all-controls showcase window
(DuiGallery). Built with Visual Studio 2022 + WTL 9.0 + Win32 API — no
external dependencies, works out of the box.

![NewChatDemo main window](docs/images/NewChatDemo_final.png)

---

## What problem does this library solve

In traditional Win32/WTL apps every control is its own HWND, which causes:

- HWND counts explode on complex UIs, hurting repaint and event routing;
- Translucency, animation, and custom-drawn effects are awkward (child
  HWNDs occlude each other's DCs);
- High-DPI adaptation has to be done per-control.

balloonui takes the **"one HWND for the host, every child is pure DUI"**
approach:

- A single `DuiHost` owns the only HWND; the entire control tree lives
  inside its client area;
- Child controls derive from `DuiControl`, have no HWND of their own, and
  bubble events to the parent window through `WM_DUI_NOTIFY`;
- RichEdit / Edit and other controls that depend on the system IME are an
  explicit exception — they are embedded as "enclaves" through the
  `HwndHostControl` adapter.

![DUI control tree](docs/images/ctl-host-tree.png)

---

## Control overview

The library ships with 30+ controls, grouped into seven categories under
`balloonui/Controls/`:

| Category | Controls |
|---|---|
| Basic | `DuiLabel` `DuiButton` `DuiAvatar` `DuiBadge` `DuiSeparator` `DuiGroupBox` |
| Input | `DuiEditHost` `DuiRichEditHost` `DuiSearchBox` `DuiSpinBox` `DuiComboBox` `DuiSlider` `DuiSwitch` |
| List / container | `DuiListBox` `DuiTreeView` `DuiTab` `DuiTabPage` `DuiMenu` `DuiMenuBar` |
| Layout | `DuiLayout` (`DuiVBox` / `DuiHBox` / `DuiGrid`) `DuiDock` `DuiSplitter` |
| Feedback | `DuiProgressBar` `DuiToolTip` `DuiPopupHost` `DuiEmojiPanel` |
| Media | `DuiGif` `DuiImageOle` |
| Window / system | `DuiFrameWindow` `DuiScrollBar` |

### Buttons: brand color + multiple forms

`DuiButton` in PushButton mode defaults to brand blue `#2D6CDF` with an 8px
corner radius; hover / pressed / disabled states are derived automatically.
The same `DuiButton` class also acts as Checkbox / Radio / Icon, sharing
the same click, focus, and hit-test plumbing.

![Button styles overview](docs/images/ctl-button-styles-overview.png)
![PushButton states](docs/images/ctl-button-pushbutton-states.png)

### Input: native IME + composite controls

`DuiEditHost` and `DuiRichEditHost` wrap real Win32 EDIT / RICHEDIT
controls, guaranteeing that Chinese / Japanese / Korean IMEs work
correctly. `DuiSearchBox`, `DuiSpinBox`, and `DuiComboBox` are composite
controls built on top of `DuiEditHost`.

![Edit states](docs/images/ctl-edit-states.png)
![SearchBox states](docs/images/ctl-searchbox-states.png)
![Switch states](docs/images/ctl-switch-states.png)

### List / tree: four-quadrant rendering + frozen columns

`DuiTreeView` doubles as a single-level multi-column table and a
multi-level tree. Each column supports 6 cell types
(TEXT / ICON / IMAGE / CHECKBOX / PROGRESS / HYPERLINK), with frozen left
N columns / top N rows, click-to-sort headers, and inline cell editing —
enough to drive a Task-Manager-class data view.

![TreeView states](docs/images/ctl-treeview-states.png)
![TreeView multi-column](docs/images/ctl-treeview-multicol.png)
![ListBox multi-select](docs/images/ctl-listbox-multi.png)

### Layout: declarative + multiple shapes

VBox / HBox / Grid cover the usual linear / grid layouts; Dock provides
"top / bottom / left / right + center fill" IDE-style docking; Splitter
gives you a draggable separator. Layouts can be built imperatively with
chained `AddChild` calls, or declaratively in XML and handed to
`DuiXmlBuilder`.

![Login layout](docs/images/ctl-layout-login.png)
![Three-pane layout](docs/images/ctl-layout-three-pane.png)
![Dock five zones](docs/images/ctl-dock-five-zones.png)

### Tab: horizontal + vertical + auto page swap

`DuiTab` is a pure tab strip (it only tracks "selected"); `DuiTabPage`
sits on top of `DuiTab` and manages the show/hide of multiple content
pages for you.

![Horizontal tab](docs/images/ctl-tab-horizontal.png)
![Vertical tab](docs/images/ctl-tab-vertical.png)

### Feedback: progress bar / popup / emoji panel

![ProgressBar states](docs/images/ctl-progressbar-states.png)
![EmojiPanel default](docs/images/ctl-emojipanel-default.png)
![Avatar grid](docs/images/ctl-avatar-grid.png)
![Badge types](docs/images/ctl-badge-types.png)

---

## Top-level window: DuiFrameWindow

`DuiFrameWindow` provides a complete borderless top-level window: a
9-grid stretched background, a custom-drawn title bar, min / max / close
buttons, `WM_NCHITTEST`-driven corner resize, and a layout-hosted client
area. In practice a single
`DuiXmlBuilder::FromFrameXml(xml)` is enough to spin up a full main
window.

![Title bar full view](docs/images/demo_titlebar_full.png)
![9-grid background](docs/images/bg-9grid-medium.png)
![Multi-window sample](docs/images/frame-4-windows-2x2.png)

---

## GDI+ anti-aliasing

Non-axis-aligned shapes (triangles, diamonds, arrows, circles, diagonal
lines) MUST go through the helpers in `DuiPaintAA`
(`DuiAA::FillPolygon` / `DuiAA::FillEllipse` / `DuiAA::DrawLine`), which
internally use GDI+ with anti-aliasing enabled. Plain GDI `Polygon` /
`Ellipse` / `LineTo` produce visible jaggies on diagonals and are banned
inside the library.

![GDI+ anti-aliasing comparison](docs/images/ctl-paintaa-comparison.png)

---

## Theme and palette

`DuiTheme` / `DuiResMgr` centrally manage colors, fonts, and spacing.
The default UI font is **Microsoft YaHei 9pt** (CHINESE_GB2312); every
DUI control pulls its font from `DuiResMgr::GetDefaultFont()`, so the
whole library can be re-skinned in one place.

![Theme swatches](docs/images/ctl-theme-swatches.png)

---

## Declarative XML layout

Complex UIs can be described as a short XML snippet and handed to
`DuiXmlBuilder`, which parses it into a control tree — no more long
chains of `AddChild`:

```xml
<frame-window title="Settings" width="640" height="480">
  <vbox padding="12" spacing="8">
    <label text="Account settings"/>
    <hbox spacing="8">
      <label text="Username"/>
      <edit id="ed_user" width="240"/>
    </hbox>
    <button id="btn_ok" text="Save" style="pushbutton"/>
  </vbox>
</frame-window>
```

Every built-in tag maps to a built-in control class; business code can
extend the dispatch table through `CustomFactory`.

---

## Feature Strip (compile-time tree-shaking)

`BalloonUiFeatures.h` exposes fine-grained `BUI_DISABLE_XXX` macros so
that business code can compile only the controls it actually uses,
excluding the unused `.cpp` files from the build entirely — which
shrinks the final exe / DLL noticeably. Dependency consistency (for
example, disabling `SCROLLBAR` also forces `LISTBOX` / `TREEVIEW` off)
is enforced inside the header via `#if/#error`, so you can't end up
with a silently half-built library.

See the header comments in `balloonui/BalloonUiFeatures.h` and the
"Feature Strip" section in `docs/guides.html` for details.

---

## Bundled projects

| Project | Path | Description |
|---|---|---|
| balloonui | `balloonui/` | The library itself, can be built as static lib or DLL |
| DuiGallery | `DuiGallery/` | All-controls showcase window — every control has its own page |
| NewChatDemo | `NewChatDemo/` | Full chat-UI demo (XML layout + custom controls) |
| CloudMelodyDesktop | `CloudMelodyDesktop/` | Desktop music player demo (multi-page + media assets) |
| DemoTaskManager | `DemoTaskManager/` | Task-Manager-style demo (multi-column TreeView + multi Tab) |
| DemoChatBubble | `DemoChatBubble/` | Chat-bubble control demo |
| DemoCircularProgress | `DemoCircularProgress/` | Circular progress ring demo |
| DemoFileTypeIcon | `DemoFileTypeIcon/` | File-type icon demo |
| DemoNinePatchBg | `DemoNinePatchBg/` | 9-grid background stretching demo |
| DemoTextBadgeTile | `DemoTextBadgeTile/` | Text-badge tile demo |
| DemoTreeViewLargeData | `DemoTreeViewLargeData/` | TreeView large-data performance demo |

Open `Demos.sln` at the root to load all projects at once.

### Demo screenshots

NewChatDemo / CloudMelodyDesktop / Task Manager demonstrate full
application-level capability:

![NewChat UI](docs/images/NewChatDemo_final.png)
![CloudMelody now playing](docs/images/cloudmelody/now-playing.png)
![CloudMelody discover](docs/images/cloudmelody/discover.png)
![Task Manager processes](docs/images/demo-taskmgr-processes.png)
![Task Manager performance](docs/images/demo-taskmgr-performance.png)

Smaller demos illustrate single controls or single element types:

![Chat bubble](docs/images/demo-chatbubble-out-long.png)
![Circular progress](docs/images/demo-circprog-p66.png)
![File icon PDF](docs/images/demo-fileicon-pdf.png)
![Text badge](docs/images/demo-textbadge-success.png)

---

## Build requirements

- Visual Studio 2022 (VS 2019 also works, downgrade the toolset yourself)
- WTL 9.0 (a copy is bundled under `wtl9.0/`)
- Platform: **Win32 (x86/x64)**, Debug / Release both fine
- Target OS: Windows 7 and above

---

## Quick start

```cpp
#include "balloonui/DuiHost.h"
#include "balloonui/DuiXmlBuilder.h"
#include "balloonui/Controls/Window/DuiFrameWindow.h"

using namespace balloonwjui;

// Inside ATL CFrameWindowImpl::OnCreate:
const char* xml = R"(
  <frame-window title="Hello" width="320" height="200">
    <vbox padding="16" spacing="8">
      <label text="Hello, balloonui!"/>
      <button id="btn_ok" text="OK" style="pushbutton"/>
    </vbox>
  </frame-window>
)";

auto* host = DuiXmlBuilder::FromFrameXml(xml);
host->Create(/* parent hwnd */);
host->ShowWindow(SW_SHOW);

// Handle the button click in the parent window's OnDuiNotify:
// case DUIN_BUTTON_CLICKED:
//   if (id == "btn_ok") { /* ... */ }
//   break;
```

For richer examples, open the DuiGallery project — every control can be
triggered there in isolation, with all its states reachable from the UI.

---

## Directory layout

```
third_party/
├── balloonui/              Library source
│   ├── Controls/           Seven category subdirs
│   ├── Tests/              Unit / integration tests
│   ├── BalloonUiApi.h      DLL import / export macros
│   ├── BalloonUiFeatures.h Compile-time feature strip
│   ├── DuiHost / DuiControl / DuiLayout / DuiTheme ...   kernel
│   └── balloonui.vcxproj
├── DuiGallery/             All-controls showcase
├── NewChatDemo/            Chat UI demo
├── CloudMelodyDesktop/     Music player demo
├── Demo*/                  Small single-purpose demos
├── docs/
│   ├── guides.html         Companion guide
│   └── images/             Documentation assets
├── wtl9.0/                 Bundled WTL 9.0
├── Goldens/                Golden images (used for regression diffing)
├── Bin/                    Output directory
└── Demos.sln               Loads every project at once
```

---

## Full user guide

A more in-depth manual (16 chapters + an appendix, covering the control
reference, XML layout, event routing, custom-drawn controls, full layout
examples, the 9-grid background, the Feature Strip, three case studies,
and a glossary):

- Markdown (renders natively on GitHub and similar platforms)
  - English: [`docs/guides_en.md`](docs/guides_en.md)
  - 中文: [`docs/guides.md`](docs/guides.md)
- HTML (with a left sidebar + scrollspy; best opened in a local browser)
  - English: [`docs/guides_en.html`](docs/guides_en.html)
  - 中文: [`docs/guides.html`](docs/guides.html)

---

## License

Anyone may use this library for free, including commercial use. No
attribution is required. No fees of any kind.

Source code, demos, and documentation assets are provided **AS IS**.
The author accepts no liability for any consequences arising from use.
Bug reports and suggestions are welcome, but the author is under no
obligation to respond.
