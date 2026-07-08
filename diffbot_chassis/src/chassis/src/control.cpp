#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <linux/joystick.h>
#include <memory>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"

class RemoteCtrl : public rclcpp::Node
{
public:
    RemoteCtrl() : Node("remote_ctrl")
    {
        device_path_ = declare_parameter<std::string>("device_path", "/dev/input/js0");
        const std::string cmd_vel_topic = declare_parameter<std::string>("cmd_vel_topic", "/cmd_vel_car");

        axis_angular_ = declare_parameter<int>("axis_angular", 0);
        axis_linear_ = declare_parameter<int>("axis_linear", 1);
        axis_linear_z_ = declare_parameter<int>("axis_linear_z", 4); 
       
        scale_linear_ = declare_parameter<double>("scale_linear", 10.0);
        scale_angular_ = declare_parameter<double>("scale_angular", 20.0);
        scale_linear_z_ = declare_parameter<double>("scale_linear_z", 2.0);

        deadzone_ = declare_parameter<double>("deadzone", 0.05);
        timeout_sec_ = declare_parameter<double>("timeout_sec", 0.5);
        enable_button_ = declare_parameter<int>("enable_button", -1);
        stop_button_ = declare_parameter<int>("stop_button", 0);

        max_accel_ = declare_parameter<double>("max_accel", 25.0);
        max_decel_ = declare_parameter<double>("max_decel", 30.0);
        max_angular_accel_ = declare_parameter<double>("max_angular_accel", 20.0);
        start_button_ = declare_parameter<int>("start_button", 10);
        exit_button_ = declare_parameter<int>("exit_button", 7);
        publisher_ = create_publisher<geometry_msgs::msg::Twist>(cmd_vel_topic, 10);
        open_joystick();
        timer_ = create_wall_timer(std::chrono::milliseconds(50), std::bind(&RemoteCtrl::timer_callback, this));

        RCLCPP_INFO(get_logger(), "Remote controller started");
        RCLCPP_INFO(get_logger(), "  device: %s", device_path_.c_str());
        RCLCPP_INFO(get_logger(), "  cmd topic: %s", cmd_vel_topic.c_str());
        RCLCPP_INFO(get_logger(), "  axes: linear=%d angular=%d", axis_linear_, axis_angular_);
        RCLCPP_INFO(get_logger(), "  scale: linear=%.2f angular=%.2f", scale_linear_, scale_angular_);
        RCLCPP_INFO(get_logger(), "  timeout: %.2fs", timeout_sec_);
        RCLCPP_INFO(get_logger(), "  max accel: linear=%.2f angular=%.2f", max_accel_, max_angular_accel_);
    }

    ~RemoteCtrl() override
    {
        if (joy_fd_ >= 0) {
            close(joy_fd_);
        }
    }

private:
    static double limit_rate(double current,double target, double max_accel,double max_decel)
    {
        double diff = target - current;

        if (diff > 0.0)
        {
            // 加速
            diff = std::min(diff, max_accel);
        }
        else
        {
            // 减速
            diff = std::max(diff, -max_decel);
        }

        return current + diff;
    }
    static double apply_deadzone(double value, double deadzone)
    {
        return std::abs(value) < deadzone ? 0.0 : value;
    }

    static double normalize_axis(int16_t value)
    {
        const double normalized = static_cast<double>(value) / 32767.0;
        return std::clamp(normalized, -1.0, 1.0);
    }

    bool button_pressed(int index) const
    {
        return index >= 0 && static_cast<size_t>(index) < buttons_.size() && buttons_[index] != 0;
    }

    double axis_value(int index) const
    {
        if (index < 0 || static_cast<size_t>(index) >= axes_.size()) {
            return 0.0;
        }
        return axes_[index];
    }
    static double smooth(double x)
    {
        constexpr double expo = 0.7;   // 0~1，越大越柔和
        return (1.0 - expo) * x + expo * x * x * x;
    }
    void open_joystick()
    {
        joy_fd_ = open(device_path_.c_str(), O_RDONLY | O_NONBLOCK);
        if (joy_fd_ < 0) {
            RCLCPP_ERROR(get_logger(), "Failed to open %s: %s", device_path_.c_str(), std::strerror(errno));
            return;
        }

        unsigned char axis_count = 0;
        unsigned char button_count = 0;
        if (ioctl(joy_fd_, JSIOCGAXES, &axis_count) < 0) {
            RCLCPP_WARN(get_logger(), "Failed to query joystick axes: %s", std::strerror(errno));
            axis_count = 8;
        }
        if (ioctl(joy_fd_, JSIOCGBUTTONS, &button_count) < 0) {
            RCLCPP_WARN(get_logger(), "Failed to query joystick buttons: %s", std::strerror(errno));
            button_count = 16;
        }

        axes_.assign(axis_count, 0.0);
        buttons_.assign(button_count, 0);
        RCLCPP_INFO(get_logger(), "Opened joystick with %u axes and %u buttons", axis_count, button_count);
    }

    void read_joystick_events()
    {
        if (joy_fd_ < 0) {
            open_joystick();
            return;
        }

        js_event event;
        while (true) {
            const ssize_t bytes = read(joy_fd_, &event, sizeof(event));
            if (bytes == static_cast<ssize_t>(sizeof(event))) {
                joy_seen_ = true;
                last_joy_time_ = now();
                handle_event(event);
                continue;
            }

           if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }

            if (bytes == 0 || (bytes < 0 && errno == ENODEV)) {
                RCLCPP_WARN(get_logger(), "Joystick disconnected: %s", device_path_.c_str());
            } else if (bytes < 0) {
                RCLCPP_WARN(get_logger(), "Joystick read failed: %s", std::strerror(errno));
            }

            close(joy_fd_);
            joy_fd_ = -1;
            joy_seen_ = false;
            target_twist_ = geometry_msgs::msg::Twist();
            break;
        }
    }

    void handle_event(const js_event & event)
    {
        const uint8_t type = event.type & ~JS_EVENT_INIT;
        if (type == JS_EVENT_AXIS && event.number < axes_.size()) {
            axes_[event.number] = normalize_axis(event.value);
            //  RCLCPP_INFO(
            // get_logger(),
            // "axes: %d %d",
            // event.number,
            // event.value);
        } else if (type == JS_EVENT_BUTTON && event.number < buttons_.size()) {
            buttons_[event.number] = event.value;
            if (event.number == start_button_ && event.value) {
                if (!program_running_) {
               std::system(
                    "bash -c '"
                    "source /opt/bxi/bxi_ros2_pkg/setup.bash && "
                    "source /home/bxi/car2/diffbot_chassis/install/setup.bash && "
                    "ros2 run chassis chassis"
                    "' &"
                );
                program_running_ = true;
                RCLCPP_INFO(get_logger(), "Program started");
                
                }
            } else if (event.number == exit_button_ && event.value) {
                std::system("pkill -f '/install/chassis/lib/chassis/chassis'");
                program_running_ = false;
                RCLCPP_INFO(get_logger(), "Program exited");
            }
            // RCLCPP_INFO(
            // get_logger(),
            // "Button %d %s",
            // event.number,
            // event.value ? "Pressed" : "Released");
        }

        update_target_twist();
    }
      
    void update_target_twist()
    {
        geometry_msgs::msg::Twist cmd;
        const bool enabled = enable_button_ < 0 || button_pressed(enable_button_);
        const bool stop = button_pressed(stop_button_);

        if (enabled && !stop) {

            double linear_x = apply_deadzone(axis_value(axis_linear_), deadzone_);
            double angular_z = apply_deadzone(axis_value(axis_angular_), deadzone_);

            double linear_z = apply_deadzone(axis_value(axis_linear_z_), deadzone_);

            cmd.linear.x = smooth(linear_x) * scale_linear_;
            cmd.angular.z = smooth(angular_z) * scale_angular_;

            cmd.linear.z=-linear_z*scale_linear_z_ ;
            // RCLCPP_INFO(get_logger(),"linear.z=%f ", cmd.linear.z);
        }

        target_twist_ = cmd;
    }

    void timer_callback()
    {
        read_joystick_events();

        geometry_msgs::msg::Twist cmd = target_twist_;
        if (!joy_seen_ || (now() - last_joy_time_).seconds() > timeout_sec_) {
            cmd = geometry_msgs::msg::Twist();
        }

        const double dt = 0.05;  // 你的定时器是 50ms

        smooth_twist_.linear.x = limit_rate(
            smooth_twist_.linear.x,
            cmd.linear.x,
            max_accel_ * dt,
            max_decel_ * dt
        );

        // smooth_twist_.angular.z = limit_rate(
        //     smooth_twist_.angular.z,
        //     cmd.angular.z,
        //     max_angular_accel_ * dt
        // );
        smooth_twist_.angular.z =cmd.angular.z;
        smooth_twist_.linear.z=cmd.linear.z;
        publisher_->publish(smooth_twist_);
    }

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;

    std::string device_path_;
    int joy_fd_{-1};
    std::vector<double> axes_;
    std::vector<int> buttons_;

    geometry_msgs::msg::Twist target_twist_;
    geometry_msgs::msg::Twist smooth_twist_;
    rclcpp::Time last_joy_time_;
    bool joy_seen_{false};
    int start_button_{10};
    int exit_button_{7};
    bool program_running_{false};
    double max_accel_{3.0};
    double max_decel_{3.0};
    int axis_linear_{1};
    int axis_angular_{0};
    int enable_button_{-1};
    int stop_button_{0};
    double scale_linear_{1.0};
    double scale_angular_{3.7};
    double max_angular_accel_{5.0};
    double deadzone_{0.05};
    double timeout_sec_{0.5};
    int axis_linear_z_{3};
    double scale_linear_z_{1.0};
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<RemoteCtrl>());
    rclcpp::shutdown();
    return 0;
}
