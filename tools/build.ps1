param([switch]$LayoutOnly,[string]$Toolchain)
$ErrorActionPreference='Stop'
$runtimeDefines=@()
if($LayoutOnly){$runtimeDefines+='-DPIRATES_LAYOUT_ONLY'}
$project=Split-Path $PSScriptRoot -Parent
if(!$Toolchain){$Toolchain=Join-Path $PSScriptRoot 'toolchain\mingw32'}
$compiler=Join-Path $Toolchain 'bin\g++.exe'
$cc=Join-Path $Toolchain 'bin\gcc.exe'
if(!(Test-Path -LiteralPath $compiler)){throw "Install an i686 MinGW-w64 toolchain or pass -Toolchain. Missing: $compiler"}
$target=(& $compiler -dumpmachine) -join ''
if($LASTEXITCODE -or $target -notmatch '^i[3-6]86-'){throw 'A 32-bit i686 compiler is required.'}
$specsPath=Join-Path $PSScriptRoot 'compiler.specs'
$specs=(& $compiler -dumpspecs) -join "`n"
# GCC's optional manifest spec fails to quote a toolchain path containing spaces.
[IO.File]::WriteAllText($specsPath,$specs.Replace('%{!shared:%:if-exists(default-manifest.o%s)}',''))
$minhook=Join-Path $project 'vendor\minhook'
$out=Join-Path $project 'build'
New-Item -ItemType Directory -Path $out -Force | Out-Null
foreach($name in @('buffer','hook','trampoline')){
 & $cc -c -O2 '-D_WIN32_WINNT=0x0601' "-I$minhook\include" "-I$minhook\src" "$minhook\src\$name.c" -o "$out\$name.o"
 if($LASTEXITCODE){throw "Compile failed: $name"}
}
& $cc -c -O2 "$minhook\src\hde\hde32.c" -o "$out\hde32.o"
if($LASTEXITCODE){throw 'hde32 compile failed'}
foreach($source in @('widescreen')){
 & $compiler "-specs=$specsPath" -std=c++17 -O2 -Wall -shared -static-libgcc -static-libstdc++ '-D_WIN32_WINNT=0x0601' @runtimeDefines "-I$minhook\include" "$project\src\$source.cpp" "$out\buffer.o" "$out\hook.o" "$out\trampoline.o" "$out\hde32.o" -o "$out\PiratesWide.dll" -ld3d9 -luser32 -static -lwinpthread
 if($LASTEXITCODE){throw "DLL compile failed: $source"}
}
& $compiler "-specs=$specsPath" -std=c++17 -O2 -Wall -static '-D_WIN32_WINNT=0x0601' "$project\src\inject.cpp" -o "$out\inject.exe"
if($LASTEXITCODE){throw 'Injector compile failed'}
& $compiler "-specs=$specsPath" -std=c++17 -O2 -Wall -static "$project\src\check-mode.cpp" -ld3d9 -o "$out\check-mode.exe"
if($LASTEXITCODE){throw 'Mode checker compile failed'}
& $compiler "-specs=$specsPath" -std=c++17 -O2 -Wall -static "-I$minhook\include" "$project\tests\coordinates.cpp" "$out\buffer.o" "$out\hook.o" "$out\trampoline.o" "$out\hde32.o" -ld3d9 -luser32 -lwinpthread -o "$out\test-coordinates.exe"
if($LASTEXITCODE){throw 'Coordinate tests compile failed'}
& "$out\test-coordinates.exe"
if($LASTEXITCODE){throw 'Coordinate tests failed'}
& $compiler "-specs=$specsPath" -std=c++17 -O2 -Wall -static "-I$minhook\include" "$project\tests\map-expansion.cpp" "$out\buffer.o" "$out\hook.o" "$out\trampoline.o" "$out\hde32.o" -ld3d9 -luser32 -lwinpthread -o "$out\test-map-expansion.exe"
if($LASTEXITCODE){throw 'Map tests compile failed'}
& "$out\test-map-expansion.exe"
if($LASTEXITCODE){throw 'Map tests failed'}
& $compiler "-specs=$specsPath" -std=c++17 -O2 -Wall -static "$project\tests\sailing-transient.cpp" -o "$out\test-sailing-transient.exe"
if($LASTEXITCODE){throw 'Transient redraw test compile failed'}
& "$out\test-sailing-transient.exe"
if($LASTEXITCODE){throw 'Transient redraw tests failed'}
 & $compiler "-specs=$specsPath" -std=c++17 -O2 -Wall -Wno-unused-function -static "-I$minhook\include" "$project\tests\sailing-ui.cpp" "$out\buffer.o" "$out\hook.o" "$out\trampoline.o" "$out\hde32.o" -o "$out\test-sailing-ui.exe" -ld3d9 -luser32
if($LASTEXITCODE){throw 'Overlay replay test compile failed'}
& "$out\test-sailing-ui.exe"
if($LASTEXITCODE){throw 'Overlay replay tests failed'}
Copy-Item -LiteralPath "$minhook\LICENSE.txt" -Destination "$out\MinHook-LICENSE.txt"
& $compiler "-specs=$specsPath" -std=c++17 -O2 -Wall -static "-I$minhook\include" "$project\tests\antialiasing.cpp" "$out\buffer.o" "$out\hook.o" "$out\trampoline.o" "$out\hde32.o" -o "$out\test-antialiasing.exe" -ld3d9 -luser32
if($LASTEXITCODE){throw 'MSAA test compile failed'}
& "$out\test-antialiasing.exe"
if($LASTEXITCODE){throw 'MSAA tests failed'}
$csharp=Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'
& $compiler "-specs=$specsPath" -std=c++17 -O2 -Wall -static "$project\tests\texture-filtering.cpp" -o "$out\test-texture-filtering.exe" -ld3d9 -luser32
if($LASTEXITCODE){throw 'Texture filtering test compile failed'}
& "$out\test-texture-filtering.exe"
if($LASTEXITCODE){throw 'Texture filtering tests failed'}
& $csharp /nologo /target:winexe /platform:x86 "/out:$out\PiratesWideLauncher.exe" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll "$project\launcher\Launcher.cs" "$project\launcher\Controls.cs" "$project\launcher\Filtering.cs"
if($LASTEXITCODE){throw 'Launcher compile failed'}
$launcherTest=Start-Process -FilePath "$out\PiratesWideLauncher.exe" -ArgumentList '--self-test' -WindowStyle Hidden -Wait -PassThru
if($launcherTest.ExitCode){throw 'Launcher tests failed'}
Get-Content -LiteralPath "$out\launcher-tests.log"
Write-Output "Built x86 widescreen runtime and Windows launcher in $out"
if($LayoutOnly){[IO.File]::WriteAllText((Join-Path $out 'layout-only.build'),'Public layout build; unfinished sailing FPS hooks disabled.')}elseif(Test-Path -LiteralPath (Join-Path $out 'layout-only.build')){Remove-Item -LiteralPath (Join-Path $out 'layout-only.build')}
$buildInfo=@{version=(Get-Content -LiteralPath (Join-Path $project 'VERSION') -Raw).Trim();layoutOnly=[bool]$LayoutOnly;mapWidthTrial=$false;target=$target;files=@{}}
foreach($name in @('PiratesWide.dll','PiratesWideLauncher.exe','inject.exe','check-mode.exe','MinHook-LICENSE.txt')){$buildInfo.files[$name]=(Get-FileHash -LiteralPath (Join-Path $out $name) -Algorithm SHA256).Hash}
$buildInfo | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $out 'build-info.json') -Encoding UTF8
