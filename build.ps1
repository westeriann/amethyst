# Builds Amethyst with MSYS2 (UCRT64) and assembles a self-contained dist\ folder.
# Requires MSYS2 at C:\msys64 with: gcc cmake ninja SDL2 glew miniaudio (ucrt64 packages).
param([switch]$NoDist)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$bash = 'C:\msys64\usr\bin\bash.exe'
if (-not (Test-Path $bash)) { throw 'MSYS2 not found at C:\msys64' }

# Build in a short path: Windows' 260-character limit trips up deep source folders.
$build = 'C:/msys64/tmp/amethyst-build'
$src = ($root -replace '\\', '/')
$dist = Join-Path $root 'dist'

# If the project folder moved, the old CMake cache points elsewhere: start fresh.
$cache = 'C:\msys64\tmp\amethyst-build\CMakeCache.txt'
if (Test-Path $cache) {
  $cachedSrc = (Select-String -Path $cache -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$').Matches.Groups[1].Value
  if ($cachedSrc -and ($cachedSrc -ne $src)) { Remove-Item 'C:\msys64\tmp\amethyst-build' -Recurse -Force }
}

& $bash -lc "export PATH=/ucrt64/bin:`$PATH; cmake -S '$src' -B '$build' -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build '$build'"
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
if ($NoDist) { Write-Host "Built (no dist): C:\msys64\tmp\amethyst-build\Amethyst.exe" -ForegroundColor Green; return }

# Assemble dist: exe + every DLL it needs from MSYS2 + projectM + presets.
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item "C:\msys64\tmp\amethyst-build\Amethyst.exe" $dist
Copy-Item (Join-Path $root 'third_party\projectm\bin\libprojectM-4.dll') $dist
$dlls = & $bash -lc "export PATH=/ucrt64/bin:`$PATH; cd '$build' && cp '$src/third_party/projectm/bin/libprojectM-4.dll' . && ldd Amethyst.exe libprojectM-4.dll | grep -i ucrt64 | awk '{print `$3}' | sort -u"
foreach ($d in $dlls) {
  $win = & $bash -lc "cygpath -w '$d'"
  Copy-Item $win $dist -Force
}
Copy-Item (Join-Path $root 'presets') (Join-Path $dist 'presets') -Recurse
Copy-Item (Join-Path $root 'shaders') (Join-Path $dist 'shaders') -Recurse
New-Item -ItemType Directory -Force (Join-Path $dist 'textures') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $dist 'licenses') | Out-Null
Copy-Item (Join-Path $root 'third_party\projectm\LICENSE.txt') (Join-Path $dist 'licenses\projectM-LGPL-2.1.txt')
Copy-Item (Join-Path $root 'third_party\imgui\LICENSE.txt') (Join-Path $dist 'licenses\imgui-MIT.txt')
Copy-Item (Join-Path $root 'third_party\spout\LICENSE.txt') (Join-Path $dist 'licenses\Spout2-BSD-2.txt')
Copy-Item (Join-Path $root 'third_party\link\LICENSE.md') (Join-Path $dist 'licenses\AbletonLink-LICENSE.md')
Copy-Item (Join-Path $root 'third_party\link\GNU-GPL-v2.0.md') (Join-Path $dist 'licenses\AbletonLink-GPL-2.0.md')
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $dist 'LICENSE.txt')
Copy-Item (Join-Path $root 'README.md') (Join-Path $dist 'README.md')
Write-Host "Built: $dist\Amethyst.exe" -ForegroundColor Green
