# Vendoring (platform layer)

Platform-specific third-party deps live here rather than under
`engine/third_party/`. The engine tree is the shared substrate; the
platform tree is per-OS. A Win32 input lib belongs with the Win32
platform impl, not next to Vulkan deps that every backend would share.

Same pinning policy as `engine/third_party/VENDORING.md`: tag-pinned
where upstream tags exist; exact version and commit/package SHA
recorded so a re-download produces byte-identical files.

## Vendored deps

| Dep       | Version   | Source |
| ---       | ---       | --- |
| GameInput | `3.3.221` | https://www.nuget.org/packages/Microsoft.GameInput |

GameInput ships as a NuGet package (`Microsoft.GameInput`). We don't
consume it through NuGet — vendoring the unpacked headers + `.lib`
keeps the build graph CMake-native and avoids configure-time package
manager ceremony. The package also contains an MSI redistributable
(`GameInputRedist.msi`); that's a shipping concern handled at
packaging time, not vendored here.

x64-only. ARM64 build of the lib is in the package but skipped since
the engine targets Windows on x64.

## Re-download command

```pwsh
# GameInput
$version = '3.3.221'
$tmp = New-TemporaryFile | %{ Remove-Item $_; New-Item -ItemType Directory -Path $_ }
$nupkg = Join-Path $tmp 'pkg.nupkg'
Invoke-WebRequest -Uri "https://api.nuget.org/v3-flatcontainer/microsoft.gameinput/$version/microsoft.gameinput.$version.nupkg" -OutFile $nupkg
Expand-Archive -LiteralPath $nupkg -DestinationPath $tmp
$dest = 'platform/third_party/gameinput'
Copy-Item -Force "$tmp/native/include/GameInput.h"    "$dest/include/"
Copy-Item -Force "$tmp/native/include/v0/GameInput.h" "$dest/include/v0/"
Copy-Item -Force "$tmp/native/include/v1/GameInput.h" "$dest/include/v1/"
Copy-Item -Force "$tmp/native/include/v2/GameInput.h" "$dest/include/v2/"
Copy-Item -Force "$tmp/native/lib/x64/GameInput.lib"  "$dest/lib/x64/"
Copy-Item -Force "$tmp/LICENSE.txt","$tmp/NOTICE.txt","$tmp/README.md" $dest
Remove-Item -Recurse $tmp
```
