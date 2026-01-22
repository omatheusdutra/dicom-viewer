# Build (Release) and run with Qt/VCPKG env vars set
$cmake = "F:/Microsoft Visual Studio/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"

# Ensure Qt DLLs/plugins are found
$env:PATH = "F:/vcpkg/installed/x64-windows/bin;$env:PATH"
$env:QT_PLUGIN_PATH = "F:/vcpkg/installed/x64-windows/Qt6/plugins"
$env:QT_QPA_PLATFORM_PLUGIN_PATH = "F:/vcpkg/installed/x64-windows/Qt6/plugins/platforms"

# Sempre garante configure antes do build (evita falta de vcxproj)
& $cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_TOOLCHAIN_FILE=F:/vcpkg/scripts/buildsystems/vcpkg.cmake -Wno-dev

# Build Release
& $cmake --build build --config Release

# Run
$exePath = Join-Path -Path "build/Release" -ChildPath "dicom_viewer.exe"
if (Test-Path -Path $exePath) {
  & $exePath
} else {
  Write-Error "Executável não encontrado em $exePath"
}
