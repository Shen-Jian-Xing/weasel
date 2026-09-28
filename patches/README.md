# patches 说明

本目录存放**不影响 Weasel 主仓库源码**、但用于本地构建环境的补丁存档。

## opencc-marisa.patch

librime 的嵌套依赖 OpenCC（`librime/deps/opencc`）在 Windows 下配合预编译
marisa 库构建时，需要把 `find_library(LIBMARISA NAMES marisa)` 改为
`find_package(marisa)`。

应用方法（在 `librime/deps/opencc` 目录中执行）：

```
git apply opencc-marisa.patch
```

> 说明：该补丁仅对应本仓库作者的本地构建环境，属于构建适配，不属于 Weasel
> 源码改动；使用官方 `get-rime.ps1` 流程构建时通常不需要。
