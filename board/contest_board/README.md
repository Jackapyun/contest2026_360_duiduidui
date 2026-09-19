# board/contest_board — MUSE Pi Pro / K1 RCPU 板级适配

本目录是本队（`contest2026_360_duiduidui`）**板级适配代码的权威副本**。

> 目标核：**SpacemiT K1 RCPU = Nuclei N308（RV32, M-Mode, 无 MMU）**
> 目标板：**MUSE Pi Pro**

## 代码如何进入编译树（官方 <linkfile> 映射）

openvela 的编译入口 `build.sh` 需要一个 **board config 路径**，本适配的板级代码必须位于
**`vendor/SpaceMiT/`** 下（该 vendor 仓即上游 `vendor_SpaceMiT`，也是获奖后的 PR 目标）。

按大赛约定，**生产仓库零改动**——适配代码保留在本仓 `board/contest_board/k1-rcpu/`，
由 manifest `contest2026_360_duiduidui.xml` 通过 `<linkfile>` 软链到编译树：

```text
vendor/SpaceMiT/chips/k1-rcpu                   -> board/contest_board/k1-rcpu/chips/k1-rcpu
vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu -> board/contest_board/k1-rcpu/boards/k1-rcpu/muse_pi_pro_rcpu
```

> 注意：基础清单 `openvela.xml` 中**不含** `vendor/SpaceMiT`，因此本队 manifest 显式声明了
> `<project path="vendor/SpaceMiT" name="vendor_SpaceMiT"/>`，并补了上面两条 linkfile。
> 这样 `repo init + repo sync` 之后即可直接编译，**无需手工 copy**。

`apply.sh` 作为**备用手段**保留：用于非 repo 工作区（例如开发机上已存在的旧工作区），
它把权威副本 `cp -a` 到 `vendor/SpaceMiT/` 并刷新 mtime、清理构建缓存；`--patches` 可顺带
把 `nuttx-patches/` 打进 nuttx（正式流程应改为 fork + PR 至 `open-vela/nuttx`）。

## 目录

```
board/contest_board/
├── apply.sh                      # 同步到 openvela 工作区（可选 --patches 给 nuttx 打补丁）
└── k1-rcpu/
    ├── chips/k1-rcpu/            # 芯片层（→ vendor/SpaceMiT/chips/k1-rcpu）
    ├── boards/k1-rcpu/muse_pi_pro_rcpu/   # 板级（→ vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu）
    ├── nuttx-patches/            # 需同步到公共 nuttx 仓的补丁
    └── docs/adaptation-guide.md  # ★ 完整适配指南
```

## 用法

```bash
./apply.sh              # 同步源码
./apply.sh --patches    # 同时给 nuttx 打补丁（2 个）
./apply.sh --dry-run    # 只看将执行的动作
```

编译与烧录步骤见 **`k1-rcpu/docs/adaptation-guide.md`** 与仓库根 `README.md`。

## 三个可用配置

| 配置 | 用途 |
|---|---|
| `configs/nsh` | 主配置：NSH 交互 + `ostest` + PXA UART + 内核自配置时钟（**不含 rpmsg**） |
| `configs/knsh` | PROTECTED：内核/用户态隔离（PMP + 2-pass + ECALLU 系统调用门） |
| `configs/nsh-min` | 最小体积（≈103KB，SRAM 布局场景） |

> 本队**未使用**应用/快应用形态，模板自带的 `app/hello_app`、`quickapp/hello_quickapp`
> 以及 `board/contest_board/{Kconfig,CMakeLists.txt,src/,configs/}` 占位文件已按官方说明
> （"用不到的形态目录可以删掉"）移除，`vendor/openvela/boards/contest2026_000_board`
> 这条模板 linkfile 也一并去掉；本队实际适配代码全部在 `k1-rcpu/` 子目录下。
