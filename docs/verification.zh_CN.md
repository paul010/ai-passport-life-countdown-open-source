简体中文 · [English](verification.md)

# 1.3.1-life-quest-public 验证记录

整理日期 2026-10-14。最终公开源码检查结果在完成命令后追加；源码能编译与设备验收是两种不同证据。

## 检查与复现

- `./tools/validate.sh --static`：源码/文件/链接扫描；六项 C 主机测试（像素数学、日历、任务状态、导航、待机计时、校验后的 ADC 采样）；固件布局解析及失败分支；中文字体覆盖/许可；串口分帧与带请求标识的设备协议单元测试。使用模拟对象，不打开设备。
- `./tools/validate.sh --firmware`：ESP-IDF 5.5.3 全新目录构建、合并镜像和分区验证，包括分区表 MD5、分段偏移、应用不超过 3 MB、cardid 没有身份数据。没有刷机或串口访问。
- 托管组件下载后，原生渲染检查命令：

```sh
cmake -S tests/native -B /tmp/life-quest-render -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/life-quest-render -j 6
mkdir -p /tmp/life-quest-frames
/tmp/life-quest-render/life_quest_render /tmp/life-quest-frames
```

检查共用首页渲染器的可见文字/图片边界、全部九个焦点、任务完成态、另一虚构闰日资料和 LVGL 内存下限。主机为 64 位指针允许 48 KiB，固件为 32 KiB。原生图为界面示意，不是设备截屏；可单独安装 Pillow 将 PPM 转为 PNG。首页检查不等于每个真机页面均已验收。

## 历史证据边界

原私有项目 1.3.0 记录分段安装和 USB 诊断回归，但实体按键手感、听感、定时休眠仍需用户验收。原 1.3.1 记录构建和社区投稿，并明确没有刷设备。这些不能证明修改后的 `1.3.1-life-quest-public` 实机通过。原始日志、私人备份、投稿回执均不导出。

实机测试：**NOT RUN**。本次没有刷写、复位或读取工牌。仍需验证本版本实体按键与唤醒输入、听感、电量、五分钟休眠/唤醒、NVS 升级/回退、真机界面内存和导航。不承诺跨机器二进制逐字节相同：固定 SDK/工具版本、源码与锁文件支持复现构建，但时间戳、路径可能影响镜像哈希。

## 本次最终结果

2026-10-14 实际执行：源码/秘密/文件/链接扫描 PASS；六项 C 主机测试和 Python 测试 PASS；原生渲染 11 帧及首页边界/焦点检查 PASS；全新 ESP-IDF 5.5.3 构建与身份分区验证 PASS。实机 NOT RUN。

Application: 834096 bytes; merged: 899632 bytes. Merged SHA-256: `f8c78c23fa7ec6bc441e7937e9a79e7d0cb6a1164d38fe563881feef12397251`.

[机器可读证据](build-evidence.json)记录各分段偏移、大小和 SHA-256。二进制只保留于被忽略的本机构建目录，不加入公开仓库。

初次测试因旧生日断言失败、导出缺少协议工具模块失败，以及首次构建因 SDK 下载不完整失败；这些问题已修复，以上为最终重跑结果。SDK 临时副本已补全固定提交，没有修改用户原 SDK。历史浏览器测试未运行，因为本包不包含网页原型。
