简体中文 · [English](attribution.md)

# 署名、依赖和公开导出边界

## 源码与素材

- 板级支持和模板上游：[FoloToy/ai-passport](https://github.com/FoloToy/ai-passport)，MIT，© 2026 FoloToy。2026-10-14 只读核对 GitHub 官方上游 license 接口，确认为 MIT。
- 人生倒计时 / Life Quest 应用、原创 TIPO/REED/怪物像素素材、几何示例头像：© 2026 Lei Pan，MIT。根目录 LICENSE 保留 FoloToy 和 Lei Pan 两项署名。
- 本包为源码快照 `1d2e42ba08b8136790afc8508949bc838a495c25` 的脱敏导出，不复制私有 Git 历史。公开版使用虚构名字/角色/生日、可重建几何头像，并加宽名字标签；仅保留相关固件测试和工具，不包含浏览器原型或考试内容。
- `assets/photos/real-quests-1.3.jpg` 为 © Lei Pan 的作者实物照片，作者授权用于项目发布。它展示历史 1.3 界面，不是本版本安装验收。源码 MIT 不授予另行的照片通用许可；再次使用照片需作者许可。本次仅删除元数据标记，保留 JPEG 图像数据；已打开清理后的确切文件复核。
- `assets/screenshots/public-example-home.png` 是本公开源码、虚构资料的原生渲染，不是照片或设备截屏。

## 字体

Noto Sans CJK SC Regular：© 2014–2021 Adobe，[官方来源](https://github.com/notofonts/noto-cjk/blob/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf)，SIL OFL 1.1。已从 OpenType name 表核对版权和许可。原字体及 `life_quest_font_{11,14,18}.c` 子集都保留 OFL，**不能改称 MIT**；子集名称与原名不同。完整许可在 `assets/fonts/OFL-NotoSansCJK.txt`。`quest-glyphs.json` 记录原字体 SHA-256、字符范围、固定转换器 `lv_font_conv@1.5.3`（MIT 构建工具，不嵌入固件）。npm 缓存转换器后，运行 `tools/generate_quest_fonts.mjs` 重建子集。

## 锁定构建依赖

已查看实际下载组件包中的许可文件。`managed_components/` 和 SDK 不提交，锁文件保留版本及组件摘要，对应许可副本保留在 `licenses/`。

| 依赖 | 版本 | 许可 |
| --- | --- | --- |
| ESP-IDF | 5.5.3，SDK commit `2c211b236707889e8400c4dc5644dd5c4ee071e0` | 主要为 Apache-2.0，组件另有各自说明 |
| espressif/button | 4.2.0 | Apache-2.0 |
| espressif/cmake_utilities | 1.1.1 | Apache-2.0 |
| espressif/esp_codec_dev | 1.6.2 | Apache-2.0 |
| espressif/esp_lvgl_port | 2.9.0 | Apache-2.0 |
| lvgl/lvgl | 9.5.0 | MIT，© 2025 LVGL Kft |

ESP-IDF 包含其他第三方组件，SDK 的版权和各组件文件仍为准。本次只公开本仓库源码，不打包 SDK、工具链、托管依赖源码或预编译固件。自行发布衍生二进制时，必须保留实际链接 SDK/组件的适用说明。可选本机工具 pyserial 3.5（BSD-3-Clause）、Pillow（HPND）、lv_font_conv（MIT）需单独安装，不是固件依赖；原生渲染与固件共用锁定的 LVGL 和字体。

## 人口数据和计算含义

`main/life_quest_model.c` 内中国五岁年龄组及世界摘要数值，沿用原项目固定的 2026 年 7 月参照（UN WPP 2024 中方案预测）。原项目从 [World Population Pyramid](https://worldpopulationpyramid.com/countries/china/) 转录，该站方法说明归属 UN WPP；底层一手统计为 [UN WPP](https://population.un.org/wpp/)。本包仅保留少量事实数值，没有网站正文、图形、商标或复制的数据集文件。它们不是实时人口或个人官方排名；中国组内按均匀分布估算，世界不编造全球百分位。本次没有重新刷新人口数据。

1 LT 为日历换算的一分钟。已使用 LT = 出生以来完整日历天×1440；剩余 LT = max(0, 至计划日期的天数)×1440。它不是模型词元、计费单位、货币或健康判断。

## 隐私审计范围

已排除私有 Git 历史、个人肖像、真实生日、考试图片、投稿回执、备份、串口日志、设备身份、凭据、云端会话存储、浏览器原型和旧截图。`cardid` 只作为受保护分区约定出现，不包含设备内容。默认 `2000-01-01` 是虚构日期，用户已有真实资料只保存在本机设备。

三张已遮生日的历史照片因仍有剩余时间/已用 LT，可反推生日，被排除。只保留没有生日推导数字、身份或 QR 的任务页照片，已去除 EXIF、Photoshop 和注释元数据。文本秘密/文件扫描补充人工源码和素材审查，不能保证未来任意修改都安全。发布修改前运行 `python3 tools/audit_public_source.py`，并逐张审查图片。

字体生成默认使用随包字符清单；修改界面字符后加 `--update-glyphs`，并审查重建的字体和清单。
