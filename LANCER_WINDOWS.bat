@echo off
setlocal
cd /d "%~dp0"
cmake -S . -B build-1.13.0 -DPRISME_OPENGL_VERSION=2.1
if errorlevel 1 goto failed
cmake --build build-1.13.0 --config Release --parallel
if errorlevel 1 goto failed
if exist "build-1.13.0\Release\python-studio.exe" (
  "build-1.13.0\Release\python-studio.exe" %*
) else (
  "build-1.13.0\python-studio.exe" %*
)
if errorlevel 1 goto failed
exit /b 0
:failed
echo.
echo Echec. Verifiez le diagnostic OpenGL ci-dessus et le README.md.
pause
exit /b 1
