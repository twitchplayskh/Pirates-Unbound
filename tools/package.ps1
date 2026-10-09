param([switch]$Force,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
$version=(Get-Content -LiteralPath (Join-Path $project 'VERSION') -Raw).Trim()
if($version -notmatch '^\d+\.\d+\.\d+(?:-beta\.\d+)?$'){throw 'Invalid release version'}
$build=Join-Path $project 'build'
$info=Get-Content -LiteralPath (Join-Path $build 'build-info.json') -Raw | ConvertFrom-Json
if(!$info.layoutOnly -or $info.mapWidthTrial -or $info.version -ne $version){throw 'Package requires a matching layout-only release build, without the map trial.'}
if(!(Test-Path -LiteralPath (Join-Path $build 'layout-only.build'))){throw 'Missing layout-only marker'}
foreach($entry in $info.files.PSObject.Properties){
 if((Get-FileHash -LiteralPath (Join-Path $build $entry.Name) -Algorithm SHA256).Hash -ne $entry.Value){throw "Build artifact changed: $($entry.Name)"}
}
$dist=if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{[IO.Path]::GetFullPath((Join-Path $project 'dist'))}
$source=Join-Path $dist 'github\Pirates-Unbound'
$binary=Join-Path $dist "windows\Pirates-Unbound-v$version"
$windowsZip=Join-Path $dist "Pirates-Unbound-v$version-Windows.zip"
$sourceZip=Join-Path $dist "Pirates-Unbound-v$version-Source.zip"
# Only replace this script's generated outputs after checking resolved paths.
foreach($path in @($source,$binary,$windowsZip,$sourceZip)){
 $absolute=[IO.Path]::GetFullPath($path)
 if(!$absolute.StartsWith($dist+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output escaped dist'}
 if(Test-Path -LiteralPath $absolute){if(!$Force){throw "Output already exists; use -Force to regenerate: $absolute"};Remove-Item -LiteralPath $absolute -Recurse -Force}
}
New-Item -ItemType Directory -Path $source,$binary -Force | Out-Null
$publicFiles=@('README.md','LICENSE','THIRD_PARTY_NOTICES.md','CHANGELOG.md','CONTRIBUTING.md','RELEASE_NOTES.md','VERSION','.gitignore')
foreach($file in $publicFiles){Copy-Item -LiteralPath (Join-Path $project $file) -Destination $source}
foreach($folder in @('src','launcher','tests','vendor','docs','.github')){Copy-Item -LiteralPath (Join-Path $project $folder) -Destination $source -Recurse}
New-Item -ItemType Directory -Path (Join-Path $source 'tools') | Out-Null
foreach($file in @('build.ps1','package.ps1')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-Path $source 'tools')}
$runtime=Join-Path $binary 'PiratesWide\runtime'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'PiratesWideLauncher.exe') -Destination (Split-Path $runtime -Parent)
foreach($file in @('PiratesWide.dll','inject.exe','check-mode.exe','MinHook-LICENSE.txt','layout-only.build','build-info.json')){Copy-Item -LiteralPath (Join-Path $build $file) -Destination $runtime}
foreach($file in @('README.md','LICENSE','THIRD_PARTY_NOTICES.md','RELEASE_NOTES.md')){Copy-Item -LiteralPath (Join-Path $project $file) -Destination $binary}
# Reject accidental binaries, user data and private research in the source.
$allowedExtensions=@('.h','.cpp','.c','.cs','.ps1','.md','.txt','.yml','.yaml')
foreach($file in Get-ChildItem -LiteralPath $source -Recurse -Force -File){
 if($file.Name -notin @('LICENSE','VERSION','.gitignore') -and $file.Extension -notin $allowedExtensions){throw "Unexpected source file: $($file.FullName)"}
 if($file.FullName -match '\\(?:evidence|backups|toolchain|\.git)\\'){throw 'Private directory in source export'}
}
$manifest=@()
foreach($file in Get-ChildItem -LiteralPath $binary -Recurse -File | Sort-Object FullName){$manifest+=((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLower()+'  '+$file.FullName.Substring($binary.Length+1).Replace('\','/'))}
[IO.File]::WriteAllLines((Join-Path $binary 'FILES.sha256'),$manifest)
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($binary,$windowsZip)
[IO.Compression.ZipFile]::CreateFromDirectory($source,$sourceZip)
$sums=@();foreach($path in @($windowsZip,$sourceZip)){$sums+=((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLower()+'  '+[IO.Path]::GetFileName($path))}
[IO.File]::WriteAllLines((Join-Path $dist 'SHA256SUMS.txt'),$sums)
Copy-Item -LiteralPath (Join-Path $project 'RELEASE_NOTES.md') -Destination $dist -Force
Write-Output "GitHub source: $source"
Write-Output "Windows release: $windowsZip"
Write-Output "Source archive: $sourceZip"
Write-Output "Checksums: $(Join-Path $dist 'SHA256SUMS.txt')"
