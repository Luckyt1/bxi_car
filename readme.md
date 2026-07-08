# car2

`car2` 是一个 ROS 2 差速轮底盘控制示例工程，包含底盘电机控制、升降柱 Modbus 控制、手柄遥控节点，以及 BXI PCI/CAN-FD 驱动测试代码。

## 目录结构

```text
.
├── diffbot_chassis/          # ROS 2 工作区
│   └── src/chassis/          # chassis 功能包
│       ├── src/chassis.cpp   # 底盘主控制节点
│       ├── src/control.cpp   # Linux joystick 手柄遥控节点
│       ├── src/modbus4lift.cpp
│       └── scripts/teleop_pygame.py
├── bxi_pci_drv/              # BXI PCI 驱动测试工程和静态库
└── libcanfd/                 # CAN-FD 示例代码和静态库
```

## 功能

- 订阅 `geometry_msgs/msg/Twist` 控制指令，控制差速轮底盘。
- 通过 BXI PCI/CAN-FD 静态库向左右轮电机发送控制帧。
- 通过 Modbus RTU 控制升降柱上升、下降和停止。
- 提供 `key_control` 手柄遥控节点，从 `/dev/input/js0` 读取 Linux joystick 事件并发布速度指令。
- 提供 BXI PCI 驱动测试程序 `drv_test`、`motor_test`。

## 环境依赖

建议环境：

- Ubuntu 22.04
- ROS 2 Humble
- CMake / colcon
- BXI PCI/CAN-FD 硬件和对应静态库
- Modbus RTU 升降柱设备

安装系统依赖：

```bash
sudo apt update
sudo apt install -y ros-humble-rclcpp ros-humble-geometry-msgs libmodbus-dev joystick
```

如果使用 `teleop_pygame.py`，还需要 Python 依赖：

```bash
pip3 install pygame
```

## 构建

### 构建 ROS 2 底盘工作区

```bash
cd diffbot_chassis
source /opt/ros/humble/setup.bash
colcon build --symlink-install
source install/setup.bash
```

`chassis` 包构建后会生成两个可执行文件：

- `chassis`：底盘和升降柱主控制节点
- `key_control`：手柄遥控节点

### 构建 BXI PCI 驱动测试程序

```bash
cd bxi_pci_drv
make
```

运行测试程序通常需要 root 权限：

```bash
sudo ./build/motor_test
sudo ./build/drv_test
```

## 运行

### 1. 启动底盘主控制节点

```bash
cd diffbot_chassis
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 run chassis chassis
```

主节点启动后会：

- 初始化 BXI PCI/CAN-FD 硬件
- 打开电机电源
- 向 CAN bus `2` 上的电机 ID `1`、`2` 发送使能帧
- 连接升降柱 Modbus RTU 设备 `/dev/ttyUSB0`
- 订阅 `/cmd_vel_car`

### 2. 启动手柄遥控节点

```bash
cd diffbot_chassis
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 run chassis key_control
```

默认参数：

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `device_path` | `/dev/input/js0` | 手柄设备路径 |
| `cmd_vel_topic` | `/cmd_vel_car` | 发布控制指令的话题 |
| `axis_linear` | `1` | 前后线速度轴 |
| `axis_angular` | `0` | 角速度轴 |
| `axis_linear_z` | `4` | 升降柱控制轴 |
| `scale_linear` | `10.0` | 线速度比例 |
| `scale_angular` | `20.0` | 角速度比例 |
| `scale_linear_z` | `2.0` | 升降控制比例 |
| `deadzone` | `0.05` | 摇杆死区 |
| `timeout_sec` | `0.5` | 手柄超时后自动发布零速度 |
| `stop_button` | `0` | 急停按钮 |
| `start_button` | `10` | 启动底盘节点按钮 |
| `exit_button` | `7` | 退出底盘节点按钮 |

示例：指定手柄设备和速度比例。

```bash
ros2 run chassis key_control --ros-args \
  -p device_path:=/dev/input/js0 \
  -p scale_linear:=2.0 \
  -p scale_angular:=4.0
```

### 3. 手动发布控制指令

底盘主节点订阅 `/cmd_vel_car`。可以用下面的命令测试前进：

```bash
ros2 topic pub /cmd_vel_car geometry_msgs/msg/Twist \
  "{linear: {x: 0.2, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.0}}" \
  --rate 10
```

控制含义：

- `linear.x`：底盘前后线速度
- `angular.z`：底盘旋转角速度
- `linear.z > 0`：升降柱上升
- `linear.z < 0`：升降柱下降
- `linear.z = 0`：升降柱停止

### 4. 使用 Pygame 键盘遥控脚本

脚本 `diffbot_chassis/src/chassis/scripts/teleop_pygame.py` 默认发布 `/cmd_vel`，而底盘主节点订阅 `/cmd_vel_car`。运行时需要 remap：

```bash
cd diffbot_chassis/src/chassis/scripts
source /opt/ros/humble/setup.bash
python3 teleop_pygame.py --ros-args -r /cmd_vel:=/cmd_vel_car
```

键盘控制：

- `W` / 上方向键：前进
- `S` / 下方向键：后退
- `A` / 左方向键：左转
- `D` / 右方向键：右转
- `Q`：升降柱上升
- `E`：升降柱下降
- `Space`：停止

## 硬件连接约定

当前代码中的硬件配置是写死的：

- CAN bus：`2`
- 左轮电机 CAN ID：`1`
- 右轮电机 CAN ID：`2`
- 轮距：`0.46 m`
- 升降柱串口：`/dev/ttyUSB0`
- Modbus 参数：`9600` baud，`N` parity，`8` data bits，`1` stop bit，slave id `1`

如果实际硬件连接不同，需要修改 `diffbot_chassis/src/chassis/src/chassis.cpp` 中的对应配置。

## 常见问题

### 手柄节点能启动，但底盘不动

检查底盘节点是否已经启动，并确认手柄节点发布到 `/cmd_vel_car`：

```bash
ros2 topic echo /cmd_vel_car
```

### `teleop_pygame.py` 控制无效

脚本默认发布 `/cmd_vel`，需要 remap 到 `/cmd_vel_car`：

```bash
python3 teleop_pygame.py --ros-args -r /cmd_vel:=/cmd_vel_car
```

### 无法打开 `/dev/input/js0`

检查手柄设备是否存在：

```bash
ls /dev/input/js*
jstest /dev/input/js0
```

如果权限不足，可以临时使用 `sudo`，或为当前用户配置 input 设备权限。

### 无法连接 `/dev/ttyUSB0`

检查升降柱串口是否存在，并确认当前用户有串口权限：

```bash
ls /dev/ttyUSB*
sudo usermod -aG dialout $USER
```

修改用户组后需要重新登录终端。

## 注意事项

- 启动底盘节点前请确认车辆已架空或处于安全测试环境。
- `chassis` 节点析构时会发送电机退出帧并关闭电机电源，但异常断电或进程被强制杀死时仍需人工确认硬件状态。
- `key_control` 中的 `start_button` 会通过硬编码路径启动底盘节点，当前路径是 `/home/bxi/car2/diffbot_chassis/install/setup.bash`。如果部署路径不同，建议直接用终端启动 `ros2 run chassis chassis`，或修改源码中的路径。
- 当前项目包含已经构建过的 `build/`、`install/`、`log/` 目录；重新部署时可按需清理后重新 `colcon build`。

## License

`diffbot_chassis/src/chassis` 声明为 Apache-2.0。第三方或硬件厂商静态库请以各自授权为准。
