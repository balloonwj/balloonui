// =================================================================
// BalloonUiApi.h —— balloonui kernel 的 DLL 导出 / 导入宏
// =================================================================
//
// 三态开关：
//   · 定义了 BUI_BUILD_DLL —— DLL 自身在编译，BUI_API 标的类<u>导出</u>。
//   · 定义了 BUI_USE_DLL   —— 消费方链接到 DLL，BUI_API 标的类<u>导入</u>。
//   · 都没定义            —— 源文件直接编进静态库 / exe（Flamingo.exe
//                            和 DuiGallery.exe 走的传统路径），BUI_API
//                            展开成空，无任何影响。
//
// 三种模式在本代码库共存：balloonui.dll 定义 BUI_BUILD_DLL；
// NewChatDemo.exe 定义 BUI_USE_DLL；Flamingo.exe / DuiGallery.exe 都不
// 定义（直接编 kernel 源）。所以给类加 BUI_API 是<u>无害</u>的 ——
// 没 opt-in 的消费方完全感知不到。
#pragma once

#if defined(BUI_BUILD_DLL)
    #define BUI_API __declspec(dllexport)
#elif defined(BUI_USE_DLL)
    #define BUI_API __declspec(dllimport)
#else
    #define BUI_API
#endif
