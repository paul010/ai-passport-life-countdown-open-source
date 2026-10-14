简体中文 · [English](flashing.md)

# 安全刷机与恢复

这些是设备拥有者自行操作的说明；本公开版本尚未做实机安装验收。先确认型号、8 MB Flash、稳定供电和当前分区表，其他板卡或不同布局停止操作。

## 保护范围

| 区域 | 偏移 | 大小/上限 | 操作 |
| --- | --- | --- | --- |
| bootloader | `0x0` | 构建产物长度 | 只写经验证的分段 |
| partition table | `0x8000` | `0x1000` 扇区 | 验证 MD5 和原布局兼容 |
| NVS | `0x9000` | `0x6000` | 保留用户资料 |
| PHY | `0xf000` | `0x1000` | 保留 |
| application | `0x10000` | 最大 `0x300000` | 只写经验证的应用 |
| cardid | `0x356000` | `0x4000` | 每台独有，禁止覆盖/公开 |

禁止 `erase-flash`、全片擦除、`--erase-all` 和写入整机备份。不要以 `0x0` 写入合并镜像：即使没到 cardid，合并填充也会覆盖 `0x9000` 的 NVS。发布包只允许源码构建产物，绝不能含设备导出的 cardid、NVS、整机备份或串口日志。

## 安装前准备

1. 在激活的 IDF 5.5.3 环境运行 `./tools/validate.sh --firmware`，必须通过。保存本次三个分段产物及 SHA-256；另行准备当前已知能工作的、相同布局的原固件分段作为回退材料。没有可靠回退材料时不要刷。
2. 用设备自己的串口替换下方变量。读取芯片和现有分区表确认兼容；身份内容仅可私人存储于仓库外，禁止发送到问题反馈或网站。示例为 esptool 5.x（IDF 5.5.3 环境）：

```sh
PORT='/dev/cu.YOUR_DEVICE'
esptool --chip esp32c3 --port "$PORT" flash-id
umask 077
PRIVATE_BACKUP_DIR="$HOME/ai-passport-private-backup"
mkdir -p "$PRIVATE_BACKUP_DIR"
esptool --chip esp32c3 --port "$PORT" read-flash 0x8000 0x1000 "$PRIVATE_BACKUP_DIR/partition-table.bin"
python "$IDF_PATH/components/partition_table/gen_esp32part.py" "$PRIVATE_BACKUP_DIR/partition-table.bin"
esptool --chip esp32c3 --port "$PORT" read-flash 0x9000 0x7000 "$PRIVATE_BACKUP_DIR/nvs-phy.bin"
esptool --chip esp32c3 --port "$PORT" read-flash 0x356000 0x4000 "$PRIVATE_BACKUP_DIR/cardid-private.bin"
chmod 600 "$PRIVATE_BACKUP_DIR"/*.bin
```

这些备份只能用于同一台设备的私下故障诊断。不要存入源码目录、云盘共享目录、公开发布包，也不要使用别人的身份数据。核对现有表中 NVS/PHY/application/cardid 与上表完全一致；若不一致，应先向厂商核对，不能覆盖分区表后再尝试。

## 分段写入

在本仓库根目录执行。再次运行布局验证，失败即停止。以下命令只写 bootloader、table、app，不写 NVS/PHY/cardid：

```sh
python3 tools/verify_firmware.py build
esptool --chip esp32c3 --port "$PORT" write-flash \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/FoloToy-AI-Passport.bin
esptool --chip esp32c3 --port "$PORT" verify-flash \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/FoloToy-AI-Passport.bin
```

核对写入哈希及 verify-flash 结果。之后只读回 NVS/PHY 和 cardid，与安装前私人文件逐字节比对；差异未解释前停止验收，不把文件公开。验证不需要执行 `LQ_PROFILE` 或上传任何身份数据。启动后校准日期，再检查实体三键、返回、页面、声音、五分钟休眠和唤醒。不能把编译通过或 USB 模拟按键当作这些实测通过。

## 故障恢复

写入中断时先检查供电、线缆、USB 端口和 esptool 报错；不要擦全片来“修复”。按厂商说明进入下载模式（本项目不猜测硬件 BOOT 手势），重新写入**同一套通过布局检查的三个分段**。仍不能启动时，用自己的已知可工作、相同分区布局的原固件三个分段回退，保留 NVS/PHY/cardid。

不要跨设备恢复 cardid，也不要把公开示例当身份数据。身份分区损坏、备份缺失、原布局不明或原固件与布局不兼容时停止，由设备拥有者联系 FoloToy 恢复；本仓库无法重新生成厂商身份。NVS 数据迁移兼容性不能由镜像哈希证明，回退前保留私人资料备份。
