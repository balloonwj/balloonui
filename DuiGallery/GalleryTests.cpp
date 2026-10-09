/**
 *  画廊自身单元测试的实现。
 *  balloonwj@qq.com   2026-08-17
 */

#include "stdafx.h"
#include "GalleryTests.h"
#include "PageKit.h"
#include "PageRegistry.h"
#include "GalleryText.h"
#include "GalleryNav.h"
#include "GalleryCmdLine.h"

#include <set>
#include <vector>

using namespace balloonwjui;

namespace Gallery {

namespace GalleryTests {

namespace {

// 一条用例的执行结果。
struct Result
{
    // 用例名。
    CString name;
    // 是否通过。
    bool ok;
    // 失败时的说明，通过时为空。
    CString detail;
};

Result OK(const CString& name)
{
    Result r;
    r.name = name;
    r.ok = true;
    return r;
}

Result Fail(const CString& name, const CString& detail)
{
    Result r;
    r.name = name;
    r.ok = false;
    r.detail = detail;
    return r;
}

#define EXPECT_TRUE(cond, name) \
    do { if (!(cond)) { return Fail(name, _T("condition false")); } } while (0)
#define EXPECT_INT(actual, expected, name) \
    do { int _a = (actual); int _e = (expected); \
         if (_a != _e) { CString _d; _d.Format(_T("expected=%d got=%d"), _e, _a); return Fail(name, _d); } \
    } while (0)

// 语言开关是进程级的，用例改了它必须还原，否则会影响后面的用例以及界面本身。
// 构造时记下当前语言，析构时还原。
class LanguageSnapshot
{
public:
    LanguageSnapshot()
        : m_saved(CurrentLanguage())
    {
    }
    ~LanguageSnapshot()
    {
        SetCurrentLanguage(m_saved);
    }
private:
    // 进入用例时的语言。
    Language m_saved;
};

// 判断一个页面标识是否只由小写字母、数字和连字符组成。
// 标识会成为文档配图的文件名，也会被搜索匹配，所以必须限定字符集。
bool IsValidPageId(LPCTSTR idName)
{
    if (idName == NULL || idName[0] == _T('\0'))
    {
        return false;
    }
    for (const TCHAR* p = idName; *p != _T('\0'); ++p)
    {
        bool isLower = (*p >= _T('a') && *p <= _T('z'));
        bool isDigit = (*p >= _T('0') && *p <= _T('9'));
        bool isHyphen = (*p == _T('-'));
        if (!isLower && !isDigit && !isHyphen)
        {
            return false;
        }
    }
    return true;
}

// ===== 一、页面注册表 =================================================

// 分组表本身不能为空，每个分组的名称字段都要齐备。
Result Test_GroupsWellFormed()
{
    int groupCount = 0;
    const PageGroup* groups = GetPageGroups(groupCount);
    EXPECT_TRUE(groups != NULL, _T("GroupsWellFormed"));
    EXPECT_TRUE(groupCount > 0, _T("GroupsWellFormed"));

    for (int g = 0; g < groupCount; ++g)
    {
        if (groups[g].idName == NULL || groups[g].idName[0] == _T('\0'))
        {
            CString d;
            d.Format(_T("group %d has an empty idName"), g);
            return Fail(_T("GroupsWellFormed"), d);
        }
        if (groups[g].titleZh == NULL || groups[g].titleZh[0] == _T('\0')
            || groups[g].titleEn == NULL || groups[g].titleEn[0] == _T('\0'))
        {
            CString d;
            d.Format(_T("group '%s' is missing a title"), groups[g].idName);
            return Fail(_T("GroupsWellFormed"), d);
        }
        // 分组的页面数组为空说明对应的 Pages_*.cpp 没有链进来，
        // 这种情况编译期不会报错，只表现为导航里少了一整块。
        if (groups[g].pageCount <= 0 || groups[g].pages == NULL)
        {
            CString d;
            d.Format(_T("group '%s' has no pages"), groups[g].idName);
            return Fail(_T("GroupsWellFormed"), d);
        }
    }
    return OK(_T("GroupsWellFormed"));
}

// 分组标识不能重复。
Result Test_GroupIdsUnique()
{
    int groupCount = 0;
    const PageGroup* groups = GetPageGroups(groupCount);
    std::set<CString> seen;
    for (int g = 0; g < groupCount; ++g)
    {
        CString id = groups[g].idName;
        if (seen.find(id) != seen.end())
        {
            CString d;
            d.Format(_T("duplicate group id '%s'"), (LPCTSTR)id);
            return Fail(_T("GroupIdsUnique"), d);
        }
        seen.insert(id);
    }
    return OK(_T("GroupIdsUnique"));
}

// 每个页面的四个字段都要齐备，标识的字符集也要合规。
Result Test_PagesWellFormed()
{
    int groupCount = 0;
    const PageGroup* groups = GetPageGroups(groupCount);
    for (int g = 0; g < groupCount; ++g)
    {
        for (int p = 0; p < groups[g].pageCount; ++p)
        {
            const PageEntry& page = groups[g].pages[p];
            if (!IsValidPageId(page.idName))
            {
                CString d;
                d.Format(_T("group '%s' page %d has an invalid idName"),
                         groups[g].idName, p);
                return Fail(_T("PagesWellFormed"), d);
            }
            if (page.titleZh == NULL || page.titleZh[0] == _T('\0')
                || page.titleEn == NULL || page.titleEn[0] == _T('\0'))
            {
                CString d;
                d.Format(_T("page '%s' is missing a title"), page.idName);
                return Fail(_T("PagesWellFormed"), d);
            }
            if (page.build == NULL)
            {
                CString d;
                d.Format(_T("page '%s' has no build function"), page.idName);
                return Fail(_T("PagesWellFormed"), d);
            }
        }
    }
    return OK(_T("PagesWellFormed"));
}

// 页面标识全局唯一。重号不会报错，只会让 FindPageById 永远返回排在前面的
// 那一个，表现为点了导航里的某一项却显示了另一页。
Result Test_PageIdsUnique()
{
    int groupCount = 0;
    const PageGroup* groups = GetPageGroups(groupCount);
    std::set<CString> seen;
    for (int g = 0; g < groupCount; ++g)
    {
        for (int p = 0; p < groups[g].pageCount; ++p)
        {
            CString id = groups[g].pages[p].idName;
            if (seen.find(id) != seen.end())
            {
                CString d;
                d.Format(_T("duplicate page id '%s'"), (LPCTSTR)id);
                return Fail(_T("PageIdsUnique"), d);
            }
            seen.insert(id);
        }
    }
    return OK(_T("PageIdsUnique"));
}

// 页面标题也不应当重复：两页同名时使用者在导航里分不出哪个是哪个。
Result Test_PageTitlesUnique()
{
    int groupCount = 0;
    const PageGroup* groups = GetPageGroups(groupCount);
    std::set<CString> seenZh;
    std::set<CString> seenEn;
    for (int g = 0; g < groupCount; ++g)
    {
        for (int p = 0; p < groups[g].pageCount; ++p)
        {
            const PageEntry& page = groups[g].pages[p];
            if (!page.showInNav)
            {
                continue;
            }
            CString zh = page.titleZh;
            CString en = page.titleEn;
            if (seenZh.find(zh) != seenZh.end())
            {
                CString d;
                d.Format(_T("duplicate Chinese title on page '%s'"), page.idName);
                return Fail(_T("PageTitlesUnique"), d);
            }
            if (seenEn.find(en) != seenEn.end())
            {
                CString d;
                d.Format(_T("duplicate English title on page '%s'"), page.idName);
                return Fail(_T("PageTitlesUnique"), d);
            }
            seenZh.insert(zh);
            seenEn.insert(en);
        }
    }
    return OK(_T("PageTitlesUnique"));
}

// 默认页面必须真的存在，否则画廊启动之后右侧是一片空白。
Result Test_DefaultPageExists()
{
    LPCTSTR defaultId = GetDefaultPageId();
    EXPECT_TRUE(defaultId != NULL, _T("DefaultPageExists"));
    const PageEntry* entry = FindPageById(defaultId);
    EXPECT_TRUE(entry != NULL, _T("DefaultPageExists"));
    EXPECT_TRUE(entry->showInNav, _T("DefaultPageExists"));
    return OK(_T("DefaultPageExists"));
}

// 按标识查找：查得到的确实是那一页，查不到的返回空指针，传空指针也不崩溃。
Result Test_FindPageById()
{
    int groupCount = 0;
    const PageGroup* groups = GetPageGroups(groupCount);
    EXPECT_TRUE(groupCount > 0 && groups[0].pageCount > 0, _T("FindPageById"));

    LPCTSTR firstId = groups[0].pages[0].idName;
    const PageEntry* found = FindPageById(firstId);
    EXPECT_TRUE(found != NULL, _T("FindPageById"));
    EXPECT_TRUE(_tcscmp(found->idName, firstId) == 0, _T("FindPageById"));

    EXPECT_TRUE(FindPageById(_T("no-such-page-id")) == NULL, _T("FindPageById"));
    EXPECT_TRUE(FindPageById(NULL) == NULL, _T("FindPageById"));
    return OK(_T("FindPageById"));
}

// 文档配图夹具必须是隐藏的。它的内容是别的页面的复制品，出现在导航里只会
// 让人以为画廊有两份一样的东西。
Result Test_DocCaptureHiddenFromNav()
{
    const PageEntry* entry = FindPageById(_T("doc-captures"));
    EXPECT_TRUE(entry != NULL, _T("DocCaptureHiddenFromNav"));
    EXPECT_TRUE(!entry->showInNav, _T("DocCaptureHiddenFromNav"));
    return OK(_T("DocCaptureHiddenFromNav"));
}

// 每个页面都要能构建出一棵非空的控件树。这一条覆盖面最广：任何一个
// Build_* 忘了往页面里加东西、或者构建过程中提前返回，都会在这里暴露。
Result Test_EveryPageBuilds()
{
    // 一个什么都没加的空页面只报告自身内边距那点高度。拿它当基线，凡是
    // 报告高度不超过基线的页面就是没往里放任何东西。这里不数子控件个数，
    // 因为 DuiControl 没有公开子控件个数，而构建函数返回的是基类指针。
    std::unique_ptr<GalleryPageBox> emptyPage = NewPage();
    const int emptyHeight = (int)emptyPage->GetDesiredSize().cy;

    int groupCount = 0;
    const PageGroup* groups = GetPageGroups(groupCount);
    for (int g = 0; g < groupCount; ++g)
    {
        for (int p = 0; p < groups[g].pageCount; ++p)
        {
            const PageEntry& page = groups[g].pages[p];
            if (page.build == NULL)
            {
                continue;
            }
            std::unique_ptr<DuiControl> root = page.build();
            if (!root)
            {
                CString d;
                d.Format(_T("page '%s' returned a null root"), page.idName);
                return Fail(_T("EveryPageBuilds"), d);
            }
            if ((int)root->GetDesiredSize().cy <= emptyHeight)
            {
                CString d;
                d.Format(_T("page '%s' built an empty tree"), page.idName);
                return Fail(_T("EveryPageBuilds"), d);
            }
        }
    }
    return OK(_T("EveryPageBuilds"));
}

// 两种语言下每个页面都要能构建。切换语言之后页面是重新构建的，如果某一页
// 只在一种语言下写对了，另一种语言下就会出问题。
Result Test_EveryPageBuildsInBothLanguages()
{
    LanguageSnapshot snapshot;

    std::unique_ptr<GalleryPageBox> emptyPage = NewPage();
    const int emptyHeight = (int)emptyPage->GetDesiredSize().cy;

    Language languages[] = { LangChinese, LangEnglish };
    for (int i = 0; i < 2; ++i)
    {
        SetCurrentLanguage(languages[i]);
        int groupCount = 0;
        const PageGroup* groups = GetPageGroups(groupCount);
        for (int g = 0; g < groupCount; ++g)
        {
            for (int p = 0; p < groups[g].pageCount; ++p)
            {
                const PageEntry& page = groups[g].pages[p];
                if (page.build == NULL)
                {
                    continue;
                }
                std::unique_ptr<DuiControl> root = page.build();
                if (!root || (int)root->GetDesiredSize().cy <= emptyHeight)
                {
                    CString d;
                    d.Format(_T("page '%s' failed to build in language %d"),
                             page.idName, (int)languages[i]);
                    return Fail(_T("EveryPageBuildsInBothLanguages"), d);
                }
            }
        }
    }
    return OK(_T("EveryPageBuildsInBothLanguages"));
}

// ===== 二、中英文切换 =================================================

// 按当前语言取到对应的那一份。
Result Test_TxtPicksByLanguage()
{
    LanguageSnapshot snapshot;

    SetCurrentLanguage(LangChinese);
    EXPECT_TRUE(CurrentLanguage() == LangChinese, _T("TxtPicksByLanguage"));
    EXPECT_TRUE(_tcscmp(Txt(_T("中文"), _T("English")), _T("中文")) == 0,
                _T("TxtPicksByLanguage"));

    SetCurrentLanguage(LangEnglish);
    EXPECT_TRUE(CurrentLanguage() == LangEnglish, _T("TxtPicksByLanguage"));
    EXPECT_TRUE(_tcscmp(Txt(_T("中文"), _T("English")), _T("English")) == 0,
                _T("TxtPicksByLanguage"));
    return OK(_T("TxtPicksByLanguage"));
}

// 只给了一份文案时，两种语言下都返回那一份；两份都没给时返回空串而不是
// 空指针 —— 调用点会把返回值直接交给 SetText，返回空指针会当场崩溃。
Result Test_TxtHandlesMissingSide()
{
    LanguageSnapshot snapshot;

    SetCurrentLanguage(LangEnglish);
    EXPECT_TRUE(_tcscmp(Txt(_T("只有中文"), NULL), _T("只有中文")) == 0,
                _T("TxtHandlesMissingSide"));
    SetCurrentLanguage(LangChinese);
    EXPECT_TRUE(_tcscmp(Txt(NULL, _T("only english")), _T("only english")) == 0,
                _T("TxtHandlesMissingSide"));

    LPCTSTR both = Txt(NULL, NULL);
    EXPECT_TRUE(both != NULL, _T("TxtHandlesMissingSide"));
    EXPECT_INT((int)_tcslen(both), 0, _T("TxtHandlesMissingSide"));
    return OK(_T("TxtHandlesMissingSide"));
}

// 每个页面的中英文标题都得是两份不同的文字。只写一份、两个参数填一样的
// 内容，界面上看不出来，但切换语言之后会发现那一项没有跟着变。
// 纯控件名的标题除外，那类标题本来就与语言无关。
Result Test_PageTitlesAreTranslated()
{
    int groupCount = 0;
    const PageGroup* groups = GetPageGroups(groupCount);
    int translated = 0;
    int total = 0;
    for (int g = 0; g < groupCount; ++g)
    {
        for (int p = 0; p < groups[g].pageCount; ++p)
        {
            const PageEntry& page = groups[g].pages[p];
            if (!page.showInNav)
            {
                continue;
            }
            ++total;
            if (_tcscmp(page.titleZh, page.titleEn) != 0)
            {
                ++translated;
            }
        }
    }
    // 中文标题的写法是"控件名 + 全角空格 + 中文名"，英文标题只有控件名，
    // 所以正常情况下应当每一页都不同。这里留一条整体断言而不是逐页断言，
    // 是因为将来可能出现确实无须翻译的页面标题。
    EXPECT_INT(translated, total, _T("PageTitlesAreTranslated"));
    return OK(_T("PageTitlesAreTranslated"));
}

// ===== 三、导航搜索的匹配规则 =========================================

// 关键词为空时一律保留。
Result Test_NavEmptyKeywordKeepsAll()
{
    EXPECT_TRUE(NavMatchesKeyword(_T("tree-view"), _T("树形列表"), _T("DuiTreeView"), _T("")),
                _T("NavEmptyKeywordKeepsAll"));
    EXPECT_TRUE(NavMatchesKeyword(_T("tree-view"), _T("树形列表"), _T("DuiTreeView"), NULL),
                _T("NavEmptyKeywordKeepsAll"));
    return OK(_T("NavEmptyKeywordKeepsAll"));
}

// 三个字段都要参与匹配：标识、中文标题、英文标题。
Result Test_NavMatchesAllThreeFields()
{
    EXPECT_TRUE(NavMatchesKeyword(_T("tree-view"), _T("树形列表"), _T("DuiTreeView"), _T("tree-view")),
                _T("NavMatchesAllThreeFields"));
    EXPECT_TRUE(NavMatchesKeyword(_T("tree-view"), _T("树形列表"), _T("DuiTreeView"), _T("树形")),
                _T("NavMatchesAllThreeFields"));
    EXPECT_TRUE(NavMatchesKeyword(_T("tree-view"), _T("树形列表"), _T("DuiTreeView"), _T("duitree")),
                _T("NavMatchesAllThreeFields"));
    return OK(_T("NavMatchesAllThreeFields"));
}

// 英文匹配不分大小写。关键词由调用方转成小写传进来，被搜的内容在函数内部
// 转小写，所以 DuiTreeView 用 tree 能搜到。
Result Test_NavMatchIsCaseInsensitive()
{
    EXPECT_TRUE(NavMatchesKeyword(_T("tree-view"), _T("树形列表"), _T("DuiTreeView"), _T("treeview")),
                _T("NavMatchIsCaseInsensitive"));
    return OK(_T("NavMatchIsCaseInsensitive"));
}

// 不匹配的确实要被滤掉，否则搜索等于没搜。
Result Test_NavRejectsNonMatching()
{
    EXPECT_TRUE(!NavMatchesKeyword(_T("tree-view"), _T("树形列表"), _T("DuiTreeView"), _T("slider")),
                _T("NavRejectsNonMatching"));
    return OK(_T("NavRejectsNonMatching"));
}

// 任一字段为空指针时不崩溃，仍按其余字段匹配。
Result Test_NavHandlesNullFields()
{
    EXPECT_TRUE(NavMatchesKeyword(NULL, NULL, _T("DuiSlider"), _T("slider")),
                _T("NavHandlesNullFields"));
    EXPECT_TRUE(!NavMatchesKeyword(NULL, NULL, NULL, _T("slider")),
                _T("NavHandlesNullFields"));
    return OK(_T("NavHandlesNullFields"));
}

// 用真实的注册表验证：随便挑一个关键词，命中的页面数必须大于零且小于全部。
// 这一条防的是"匹配逻辑写反了"这类错误 —— 全命中和全不命中都不对。
Result Test_NavFilterNarrowsRealRegistry()
{
    int groupCount = 0;
    const PageGroup* groups = GetPageGroups(groupCount);
    int total = 0;
    int matched = 0;
    for (int g = 0; g < groupCount; ++g)
    {
        for (int p = 0; p < groups[g].pageCount; ++p)
        {
            const PageEntry& page = groups[g].pages[p];
            if (!page.showInNav)
            {
                continue;
            }
            ++total;
            if (NavMatchesKeyword(page.idName, page.titleZh, page.titleEn, _T("edit")))
            {
                ++matched;
            }
        }
    }
    EXPECT_TRUE(total > 0, _T("NavFilterNarrowsRealRegistry"));
    EXPECT_TRUE(matched > 0, _T("NavFilterNarrowsRealRegistry"));
    EXPECT_TRUE(matched < total, _T("NavFilterNarrowsRealRegistry"));
    return OK(_T("NavFilterNarrowsRealRegistry"));
}

// ===== 四、段落说明按宽度自动换行 =====================================

// 同一段说明，页面越窄需要的高度越大。这一条钉的是这次重构的核心机制 ——
// 重构之前说明是固定 20 像素高的单行标签，长句子会被窗口右边界直接裁掉，
// 无论页面多宽多窄高度都不变。
Result Test_DescriptionHeightGrowsWhenNarrow()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();
    AddSection(page.get(),
               _T("Measurement"),
               _T("This description is intentionally long so that it must wrap onto several ")
               _T("lines when the page is narrow, and onto fewer lines when the page is wide. ")
               _T("The page box measures it at layout time and writes the result back into ")
               _T("the label's layout hint."));

    const int kTallEnough = 4000;
    const int kNarrowWidth = 320;
    const int kWideWidth = 1200;

    // 用 ForceLayout 而不是 SetRect：后者在矩形没有变化时直接返回，而这里
    // 每次给的矩形都不同，两者本来等价；用 ForceLayout 是因为它是公开的，
    // Layout 本身是受保护的成员。
    RECT rcNarrow = { 0, 0, kNarrowWidth, kTallEnough };
    page->ForceLayout(rcNarrow);
    SIZE narrow = page->GetDesiredSize();

    RECT rcWide = { 0, 0, kWideWidth, kTallEnough };
    page->ForceLayout(rcWide);
    SIZE wide = page->GetDesiredSize();

    if (narrow.cy <= wide.cy)
    {
        CString d;
        d.Format(_T("narrow=%d wide=%d, expected narrow to be taller"),
                 (int)narrow.cy, (int)wide.cy);
        return Fail(_T("DescriptionHeightGrowsWhenNarrow"), d);
    }
    return OK(_T("DescriptionHeightGrowsWhenNarrow"));
}

// 段落调用 AddSection 之后，演示行要落在该段落的卡片里，而不是直接落在
// 页面上。页面的直接子控件个数应当等于段落数。
Result Test_SectionsBecomeCards()
{
    std::unique_ptr<GalleryPageBox> page = NewPage();

    AddSection(page.get(), _T("First"), _T("first description"));
    std::unique_ptr<DuiHBox> row1(new DuiHBox());
    AddVariantRow(page.get(), std::move(row1));

    AddSection(page.get(), _T("Second"), _T("second description"));
    std::unique_ptr<DuiHBox> row2(new DuiHBox());
    AddVariantRow(page.get(), std::move(row2));
    std::unique_ptr<DuiHBox> row3(new DuiHBox());
    AddVariantRow(page.get(), std::move(row3));

    // 两个段落 = 两张卡片 = 页面的两个直接子控件。
    EXPECT_INT(page->GetChildCountForTests(), 2, _T("SectionsBecomeCards"));
    return OK(_T("SectionsBecomeCards"));
}

// 无卡片模式下不再套一层容器，标题、说明与演示行直接排在页面上。文档配图
// 夹具依赖这个模式让截图几何与已经发布的那批配图保持一致。
Result Test_PlainPageHasNoCards()
{
    std::unique_ptr<GalleryPageBox> page = NewPlainPage();

    AddSection(page.get(), _T("First"), _T("first description"));
    std::unique_ptr<DuiHBox> row1(new DuiHBox());
    AddVariantRow(page.get(), std::move(row1));

    // 标题 + 说明 + 演示行 = 三个直接子控件，中间没有卡片这一层。
    EXPECT_INT(page->GetChildCountForTests(), 3, _T("PlainPageHasNoCards"));
    return OK(_T("PlainPageHasNoCards"));
}

// ---- 命令行解析（2026-10-09）：DuiGallery.exe [--lang en|zh] [--capture-all <目录>] ----

// C1：没有参数时正常启动、界面为中文。
Result Test_CmdLineEmpty()
{
    GalleryCmdLine cmd = ParseGalleryCmdLine(_T(""));
    EXPECT_TRUE(cmd.m_valid, _T("C1/valid"));
    EXPECT_TRUE(!cmd.m_captureAll, _T("C1/noCapture"));
    EXPECT_TRUE(!cmd.m_langGiven, _T("C1/noLang"));
    EXPECT_INT(cmd.m_lang, LangChinese, _T("C1/chinese"));
    GalleryCmdLine nullCmd = ParseGalleryCmdLine(NULL);
    EXPECT_TRUE(nullCmd.m_valid && !nullCmd.m_captureAll, _T("C1/null"));
    return OK(_T("CmdLineEmpty"));
}

// C2：只给 --capture-all 时进入截图模式，界面仍为中文。
Result Test_CmdLineCaptureOnly()
{
    GalleryCmdLine cmd = ParseGalleryCmdLine(_T("--capture-all docs\\images"));
    EXPECT_TRUE(cmd.m_valid, _T("C2/valid"));
    EXPECT_TRUE(cmd.m_captureAll, _T("C2/capture"));
    EXPECT_TRUE(cmd.m_captureDir == _T("docs\\images"), _T("C2/dir"));
    EXPECT_INT(cmd.m_lang, LangChinese, _T("C2/chinese"));
    return OK(_T("CmdLineCaptureOnly"));
}

// C3：--lang en 在前、--capture-all 在后。
Result Test_CmdLineLangThenCapture()
{
    GalleryCmdLine cmd = ParseGalleryCmdLine(_T("--lang en --capture-all docs\\images"));
    EXPECT_TRUE(cmd.m_valid, _T("C3/valid"));
    EXPECT_TRUE(cmd.m_langGiven, _T("C3/langGiven"));
    EXPECT_INT(cmd.m_lang, LangEnglish, _T("C3/english"));
    EXPECT_TRUE(cmd.m_captureAll && cmd.m_captureDir == _T("docs\\images"), _T("C3/capture"));
    return OK(_T("CmdLineLangThenCapture"));
}

// C4：带空格的目录要加引号；参数顺序颠倒、取值大写也能识别。
Result Test_CmdLineQuotedDirAnyOrder()
{
    GalleryCmdLine cmd = ParseGalleryCmdLine(_T("--capture-all \"C:\\a b\\img\" --lang EN"));
    EXPECT_TRUE(cmd.m_valid, _T("C4/valid"));
    EXPECT_TRUE(cmd.m_captureDir == _T("C:\\a b\\img"), _T("C4/dir"));
    EXPECT_INT(cmd.m_lang, LangEnglish, _T("C4/english"));
    return OK(_T("CmdLineQuotedDirAnyOrder"));
}

// C5：--lang 缺值时无效。
Result Test_CmdLineLangMissingValue()
{
    GalleryCmdLine cmd = ParseGalleryCmdLine(_T("--lang"));
    EXPECT_TRUE(!cmd.m_valid, _T("C5/invalid"));
    EXPECT_TRUE(!cmd.m_error.IsEmpty(), _T("C5/error"));
    EXPECT_TRUE(!cmd.m_langGiven, _T("C5/noLang"));
    return OK(_T("CmdLineLangMissingValue"));
}

// C6：--lang 取值不是 en / zh 时无效，语言保持中文。
Result Test_CmdLineLangUnknownValue()
{
    GalleryCmdLine cmd = ParseGalleryCmdLine(_T("--lang fr --capture-all x"));
    EXPECT_TRUE(!cmd.m_valid, _T("C6/invalid"));
    EXPECT_TRUE(!cmd.m_langGiven, _T("C6/noLang"));
    EXPECT_INT(cmd.m_lang, LangChinese, _T("C6/chinese"));
    EXPECT_TRUE(cmd.m_captureAll, _T("C6/captureSeen"));
    return OK(_T("CmdLineLangUnknownValue"));
}

// C7：--capture-all 缺目录时无效。
Result Test_CmdLineCaptureMissingDir()
{
    GalleryCmdLine cmd = ParseGalleryCmdLine(_T("--capture-all"));
    EXPECT_TRUE(!cmd.m_valid, _T("C7/invalid"));
    EXPECT_TRUE(cmd.m_captureAll, _T("C7/captureSeen"));
    EXPECT_TRUE(cmd.m_captureDir.IsEmpty(), _T("C7/noDir"));
    return OK(_T("CmdLineCaptureMissingDir"));
}

} // 匿名命名空间

CString RunAll()
{
    typedef Result (*TestFn)();
    struct Entry
    {
        // 用例名，出现在报告里。
        LPCTSTR name;
        // 用例函数。
        TestFn fn;
    };
    // 用例必须手工登记到这张表里，漏登记的用例不会有任何提示、只是永远不跑。
    Entry tests[] = {
        { _T("GroupsWellFormed"),                &Test_GroupsWellFormed                },
        { _T("GroupIdsUnique"),                  &Test_GroupIdsUnique                  },
        { _T("PagesWellFormed"),                 &Test_PagesWellFormed                 },
        { _T("PageIdsUnique"),                   &Test_PageIdsUnique                   },
        { _T("PageTitlesUnique"),                &Test_PageTitlesUnique                },
        { _T("DefaultPageExists"),               &Test_DefaultPageExists               },
        { _T("FindPageById"),                    &Test_FindPageById                    },
        { _T("DocCaptureHiddenFromNav"),         &Test_DocCaptureHiddenFromNav         },
        { _T("EveryPageBuilds"),                 &Test_EveryPageBuilds                 },
        { _T("EveryPageBuildsInBothLanguages"),  &Test_EveryPageBuildsInBothLanguages  },
        { _T("TxtPicksByLanguage"),              &Test_TxtPicksByLanguage              },
        { _T("TxtHandlesMissingSide"),           &Test_TxtHandlesMissingSide           },
        { _T("PageTitlesAreTranslated"),         &Test_PageTitlesAreTranslated         },
        { _T("NavEmptyKeywordKeepsAll"),         &Test_NavEmptyKeywordKeepsAll         },
        { _T("NavMatchesAllThreeFields"),        &Test_NavMatchesAllThreeFields        },
        { _T("NavMatchIsCaseInsensitive"),       &Test_NavMatchIsCaseInsensitive       },
        { _T("NavRejectsNonMatching"),           &Test_NavRejectsNonMatching           },
        { _T("NavHandlesNullFields"),            &Test_NavHandlesNullFields            },
        { _T("NavFilterNarrowsRealRegistry"),    &Test_NavFilterNarrowsRealRegistry    },
        { _T("DescriptionHeightGrowsWhenNarrow"),&Test_DescriptionHeightGrowsWhenNarrow},
        { _T("SectionsBecomeCards"),             &Test_SectionsBecomeCards             },
        { _T("PlainPageHasNoCards"),             &Test_PlainPageHasNoCards             },
        { _T("CmdLineEmpty"),                    &Test_CmdLineEmpty                    },
        { _T("CmdLineCaptureOnly"),              &Test_CmdLineCaptureOnly              },
        { _T("CmdLineLangThenCapture"),          &Test_CmdLineLangThenCapture          },
        { _T("CmdLineQuotedDirAnyOrder"),        &Test_CmdLineQuotedDirAnyOrder        },
        { _T("CmdLineLangMissingValue"),         &Test_CmdLineLangMissingValue         },
        { _T("CmdLineLangUnknownValue"),         &Test_CmdLineLangUnknownValue         },
        { _T("CmdLineCaptureMissingDir"),        &Test_CmdLineCaptureMissingDir        },
    };

    CString out;
    int passed = 0;
    int failed = 0;
    const int kTestCount = (int)(sizeof(tests) / sizeof(tests[0]));
    for (int i = 0; i < kTestCount; ++i)
    {
        Result r = tests[i].fn();
        CString line;
        if (r.ok)
        {
            ++passed;
            line.Format(_T("[ok]   %s"), tests[i].name);
        }
        else
        {
            ++failed;
            line.Format(_T("[FAIL] %s : %s"), tests[i].name, (LPCTSTR)r.detail);
        }
        if (!out.IsEmpty())
        {
            out += _T("\r\n");
        }
        out += line;
    }
    CString summary;
    summary.Format(_T("[summary] GalleryTests passed=%d failed=%d"), passed, failed);
    if (!out.IsEmpty())
    {
        out += _T("\r\n");
    }
    out += summary;
    return out;
}

} // namespace GalleryTests

} // namespace Gallery
