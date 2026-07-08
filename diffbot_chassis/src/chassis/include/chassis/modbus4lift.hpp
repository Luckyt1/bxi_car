#pragma once
#include <modbus/modbus.h>
#include <string>
#include <vector>
#include <stdexcept>

class Modbus4lift {
public:
    // 构造
    Modbus4lift(const std::string& port,
                int baud,
                char parity,
                int data_bit,
                int stop_bit,
                int slave_id);

    // 稀构
    ~Modbus4lift();

    // 禁止拷贝
    Modbus4lift(const Modbus4lift&) = delete;
    Modbus4lift& operator=(const Modbus4lift&) = delete;

    // 写单个寄存器
    void writeRegister(uint16_t reg_addr, uint16_t value);

    // 写多个寄存器
    void writeMultiRegisters(uint16_t start_addr, const std::vector<uint16_t>& values);

    // 读保持寄存器
    std::vector<uint16_t> readRegisters(uint16_t start_addr, uint16_t count);

    // 平台动作快捷封装
    enum class PlatformCmd : uint16_t {
        STOP   = 0x0001,
        UP     = 0x0002,
        DOWN   = 0x0004,
        RESET  = 0x0008
    };
    void platformControl(PlatformCmd cmd) {
        writeRegister(0x0001, static_cast<uint16_t>(cmd));
    }

    // 改设备地址（广播指令，00 地址）
    void changeDeviceAddress(uint16_t new_addr);

private:
    modbus_t* ctx_;
    void checkError(int rc, const char* op);
};
