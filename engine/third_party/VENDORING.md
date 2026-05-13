# Vendoring

Single-file / single-header dependencies are vendored directly here.
Pinning policy: tag-pinned where upstream tags exist; record exact tag
and commit SHA so a re-download produces byte-identical files. Update
by re-running the download with a newer tag and recording the new
pin below.

Reach for vcpkg (or another package manager) the first time we want a
dep that's actually annoying to vendor — heavy compiled libraries with
transitive deps. Single-header stuff stays here.

## Vendored deps

| Dep | Version | Source |
| --- | --- | --- |
| Volk | `vulkan-sdk-1.4.350.0` (commit `3ca312a4f38baa63d8006b6905abbeeb89c8087d`) | https://github.com/zeux/volk |
| VMA | `v3.3.0` (commit `1d8f600fd424278486eade7ed3e877c99f0846b1`) | https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator |
| doctest | `2.x` (vendored from `cpp/third_party/doctest/`) | https://github.com/doctest/doctest |

Volk's vulkan-sdk-X.Y.Z tags follow the Vulkan SDK version. The tag
above is newer than our `apiVersion = VK_API_VERSION_1_3` baseline,
which is fine — Volk loads whatever entry points the runtime ICD
exposes, regardless of which API version the loader code knows about.

## Re-download commands

```sh
# Volk
TAG=vulkan-sdk-1.4.350.0
cd engine/third_party/volk
for f in volk.h volk.c LICENSE.md README.md; do
    curl -sSL -o "$f" "https://raw.githubusercontent.com/zeux/volk/${TAG}/${f}"
done

# VMA
TAG=v3.3.0
cd engine/third_party/vma
curl -sSL -o vk_mem_alloc.h "https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/${TAG}/include/vk_mem_alloc.h"
curl -sSL -o LICENSE.txt    "https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/${TAG}/LICENSE.txt"
curl -sSL -o README.md      "https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/${TAG}/README.md"
```
