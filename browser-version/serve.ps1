# Amethyst launcher: serves this folder on http://localhost and opens it as an app window.
# No installs needed - uses the HTTP listener built into Windows/.NET.
param([int]$Port = 8765, [switch]$NoBrowser)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$mime = @{
  '.html' = 'text/html; charset=utf-8'; '.js' = 'text/javascript; charset=utf-8'; '.css' = 'text/css; charset=utf-8'
  '.json' = 'application/json'; '.wasm' = 'application/wasm'; '.png' = 'image/png'; '.jpg' = 'image/jpeg'
  '.jpeg' = 'image/jpeg'; '.svg' = 'image/svg+xml'; '.ico' = 'image/x-icon'; '.milk' = 'text/plain; charset=utf-8'
  '.txt' = 'text/plain; charset=utf-8'
}

# Find a free port
$listener = $null
for ($p = $Port; $p -lt $Port + 20; $p++) {
  try {
    $l = New-Object System.Net.HttpListener
    $l.Prefixes.Add("http://localhost:$p/")
    $l.Start()
    $listener = $l; $Port = $p; break
  } catch { }
}
if (-not $listener) { Write-Host "Could not open a local port." -ForegroundColor Red; Read-Host "Press Enter to exit"; exit 1 }

$url = "http://localhost:$Port/"
Write-Host ""
Write-Host "  Amethyst is running at $url" -ForegroundColor Magenta
Write-Host "  Keep this window open while you play. Close it (or press Ctrl+C) to stop."
Write-Host ""

if (-not $NoBrowser) {
  # Prefer a chromeless app window in Chrome or Edge; fall back to the default browser.
  $candidates = @(
    "$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
    "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
    "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe",
    "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
    "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe"
  )
  $exe = $candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
  if ($exe) { Start-Process -FilePath $exe -ArgumentList "--app=$url", "--autoplay-policy=no-user-gesture-required" }
  else { Start-Process $url }
}

$rootFull = [System.IO.Path]::GetFullPath($root)
try {
  while ($listener.IsListening) {
    $ctx = $listener.GetContext()
    $req = $ctx.Request; $res = $ctx.Response
    try {
      $rel = [Uri]::UnescapeDataString($req.Url.AbsolutePath.TrimStart('/'))
      if ([string]::IsNullOrEmpty($rel)) { $rel = 'index.html' }
      $path = [System.IO.Path]::GetFullPath((Join-Path $rootFull $rel))
      if (-not $path.StartsWith($rootFull, [StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $res.StatusCode = 404
        $bytes = [Text.Encoding]::UTF8.GetBytes('Not found')
      } else {
        $ext = [System.IO.Path]::GetExtension($path).ToLowerInvariant()
        $res.ContentType = if ($mime.ContainsKey($ext)) { $mime[$ext] } else { 'application/octet-stream' }
        $res.Headers.Add('Cache-Control', 'no-cache')
        $bytes = [System.IO.File]::ReadAllBytes($path)
      }
      $res.ContentLength64 = $bytes.Length
      $res.OutputStream.Write($bytes, 0, $bytes.Length)
    } catch {
      try { $res.StatusCode = 500 } catch { }
    } finally {
      try { $res.OutputStream.Close() } catch { }
    }
  }
} finally {
  $listener.Stop()
}
