@echo off
setlocal enabledelayedexpansion

rem ============================================================
rem clear.bat -- one-click cleanup of build outputs / IDE temp
rem files inside third_party/ and its subdirectories.
rem
rem Mechanism: calls "git clean -fdX" so it only removes files
rem matched by the repo-root .gitignore (Debug/, Release/, Bin/,
rem .vs/, *.vcxproj.user, *.tlog, *.pdb, *.ilk, *.ipdb, *.iobj,
rem *.aps, *.bak ...).
rem
rem Safety:
rem   - Tracked source / docs / resources are never touched.
rem   - Untracked-but-not-ignored files (e.g. a brand-new .cpp
rem     not yet "git add"-ed) are NOT deleted, because we use
rem     -X (capital), not -x (lowercase).
rem   - Scope is fixed to this script's directory (third_party/)
rem     and below; the repo root, docs/, sibling dirs untouched.
rem
rem Usage: double-click, or run "third_party\clear.bat" in cmd.
rem ============================================================

rem Pin scope to this script's directory.
cd /d "%~dp0"

rem Verify git is available.
where git >nul 2>nul
if errorlevel 1 (
    echo [error] git not found on PATH; this script needs git clean.
    pause
    exit /b 1
)

rem Verify we are inside a git working tree.
git rev-parse --is-inside-work-tree >nul 2>nul
if errorlevel 1 (
    echo [error] current directory is not inside a git repo.
    pause
    exit /b 1
)

echo ============================================================
echo Files / directories that WILL be removed (dry-run preview):
echo ------------------------------------------------------------
git clean -ndX .
echo ============================================================
echo.

set /p answer="Proceed with deletion? (Y/N): "
if /i not "%answer%"=="Y" (
    echo [cancelled] no changes made.
    pause
    exit /b 0
)

echo.
echo [running] cleaning...
git clean -fdX .

echo.
echo [done] cleanup complete.
pause
endlocal
