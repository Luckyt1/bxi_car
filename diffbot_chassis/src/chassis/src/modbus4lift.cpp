#include "chassis/modbus4lift.hpp"
#include <cstring>

Modbus4lift::Modbus4lift(const std::string& port,
                        int baud,
                        char parity,
                        int data_bit, 
                        int stop_bit,
                        int slave_id){
    ctx_ = modbus_new_rtu(port.c_str(), baud, parity, data_bit, stop_bit);
    if (!ctx_) throw std::runtime_error("modbus_new_rtu failed");

    modbus_set_slave(ctx_, slave_id);
    if (modbus_connect(ctx_) == -1) {
        modbus_free(ctx_);
        throw std::runtime_error("modbus_connect failed: " + std::string(modbus_strerror(errno)));
    }
    modbus_set_response_timeout(ctx_, 0, 500000);
    modbus_flush(ctx_);
}

Modbus4lift::~Modbus4lift(){
    if (ctx_) {
        modbus_close(ctx_);
        modbus_free(ctx_);
    }
}

void Modbus4lift::checkError(int rc, const char* op){
    if (rc == -1)
        throw std::runtime_error(std::string(op) + " failed: " + modbus_strerror(errno));
}

void Modbus4lift::writeRegister(uint16_t reg_addr, uint16_t value){
    const int retry_count=3;
    for (int retry=0 ; retry<retry_count; ++retry){
        int rc = modbus_write_register(ctx_, reg_addr, value);
        if (rc != -1) return;
        if (retry < retry_count-1) {
            usleep(100000); // 100ms
        } else {
            checkError(rc, "writeRegister");
        }
    }
}

void Modbus4lift::writeMultiRegisters(uint16_t start_addr, const std::vector<uint16_t>& values){
    int rc = modbus_write_registers(ctx_, start_addr, static_cast<int>(values.size()), values.data());
    checkError(rc, "writeMultiRegisters");
}

std::vector<uint16_t>
Modbus4lift::readRegisters(uint16_t start_addr, uint16_t count){
    std::vector<uint16_t> regs(count);
    int rc = modbus_read_registers(ctx_, start_addr, count, regs.data());
    checkError(rc, "readRegisters");
    return regs;
}

void Modbus4lift::changeDeviceAddress(uint16_t new_addr){
    // 临时切到广播地址 00
    modbus_set_slave(ctx_, 0);
    int rc = modbus_write_register(ctx_, 0x000A, new_addr);
    checkError(rc, "changeDeviceAddress");
    // 改完切回新地址，后续通信都要用新地址
    modbus_set_slave(ctx_, new_addr);
}


