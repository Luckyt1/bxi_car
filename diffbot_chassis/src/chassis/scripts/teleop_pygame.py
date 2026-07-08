#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import pygame
import sys

class PygameTeleop(Node):
    def __init__(self):
        super().__init__('pygame_teleop')
        self.publisher_ = self.create_publisher(Twist, '/cmd_vel', 10)
        self.timer = self.create_timer(0.1, self.timer_callback)
        
        # 参数设置
        self.linear_speed = 0.5
        self.angular_speed = 1.0
        self.lift_speed = 1.0
        
        # 初始化 pygame
        pygame.init()
        # 创建一个小窗口来捕获焦点
        self.screen = pygame.display.set_mode((300, 200))
        pygame.display.set_caption('ROS 2 Keyboard Teleop')
        
        self.get_logger().info("""
        Pygame Keyboard Teleop Started
        ---------------------------
        控制说明:
        W / ⬆️ : 前进
        S / ⬇️ : 后退
        A / ⬅️ : 左转
        D / ➡️ : 右转
        
        Q      : 升降机上升
        E      : 升降机下降
        
        SPACE  : 立即停止
        ---------------------------
        请确保此时焦点在 Pygame 的小窗口上
        """)
        
    def timer_callback(self):
        # 处理 Pygame 事件
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                sys.exit()

        #以此构建 Twist 消息
        msg = Twist()
        
        # 获取键盘按键状态
        keys = pygame.key.get_pressed()
        
        # 线速度 x
        if keys[pygame.K_w] or keys[pygame.K_UP]:
            msg.linear.x = self.linear_speed
        elif keys[pygame.K_s] or keys[pygame.K_DOWN]:
            msg.linear.x = -self.linear_speed
        else:
            msg.linear.x = 0.0
            
        # 角速度 z
        if keys[pygame.K_a] or keys[pygame.K_LEFT]:
            msg.angular.z = self.angular_speed
        elif keys[pygame.K_d] or keys[pygame.K_RIGHT]:
            msg.angular.z = -self.angular_speed
        else:
            msg.angular.z = 0.0

        # 升降机控制 (mapped to linear.z based on your chassis code)
        if keys[pygame.K_q]:
            msg.linear.z = self.lift_speed
        elif keys[pygame.K_e]:
            msg.linear.z = -self.lift_speed
        else:
            msg.linear.z = 0.0
            
        # 空格键急停
        if keys[pygame.K_SPACE]:
            msg.linear.x = 0.0
            msg.angular.z = 0.0
            msg.linear.z = 0.0

        # 发布消息
        # 只有在有按键按下或者刚刚松开变为0的时候才通过持续发布来保持心跳，
        # 或者就像现在这样以 10Hz 持续发布当前状态（通常也是安全的）
        self.publisher_.publish(msg)
        
        # 更新 pygame 窗口保持响应
        self.screen.fill((0, 0, 0))
        # 可以在这里渲染一些文字显示当前速度，这里暂时省略
        pygame.display.flip()

def main(args=None):
    rclpy.init(args=args)
    node = PygameTeleop()
    
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
        pygame.quit()

if __name__ == '__main__':
    main()
