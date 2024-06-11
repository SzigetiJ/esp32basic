/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdbool.h>

#include "dport.h"
#include "gpio.h"
#include "i2c.h"
#include "iomux.h"
#include "romfunctions.h"

#define I2C0_SCL_IDX 29U
#define I2C0_SDA_IDX 30U
#define I2C1_SCL_IDX 95U
#define I2C1_SDA_IDX 96U
#define DPORT_I2C0_BIT 7U
#define DPORT_I2C1_BIT 18U

static inline uint8_t _scl_idx(EI2CBus eBus) {
  return eBus == I2C0 ? I2C0_SCL_IDX : I2C1_SCL_IDX;
}

static inline uint8_t _sda_idx(EI2CBus eBus) {
  return eBus == I2C0 ? I2C0_SDA_IDX : I2C1_SDA_IDX;
}

/**
 * Get DPORT_PERIP_{CLK|RST}_EN_REG bit of the given I2C channel.
 * @param eBus I2C channel
 * @return Bit
 */
static inline uint8_t _dport_peri_bit(EI2CBus eBus) {
  return eBus == I2C0 ? DPORT_I2C0_BIT : DPORT_I2C1_BIT;
}

void i2c_write(EI2CBus eBus, uint8_t u8Addr, uint8_t u8Len, const uint8_t *pu8Dat) {
  I2C_Type *psI2C = i2c_regs(eBus);
  RegAddr prData = i2c_nonfifo(eBus);

  i2c_reset_fifo(psI2C);

  // copy data
  prData[0] = (u8Addr << 1) | 0; // slave addr
  for (int i = 0; i < 31 && i < u8Len; ++i) {
    prData[i + 1] = pu8Dat[i];
  }

  psI2C->COMD[0] = i2c_cmd_start();
  psI2C->COMD[1] = i2c_cmd_write(true, u8Len + 1);
  psI2C->COMD[2] = i2c_cmd_stop();

  //  CTR
  psI2C->INT_CLR = I2C_INT_MASK_ALL;
  i2c_trans_start(psI2C);
}

void i2c_read(EI2CBus eBus, uint8_t u8Addr, uint8_t u8RxLen) {
  I2C_Type *psI2C = i2c_regs(eBus);
  RegAddr prData = i2c_nonfifo(eBus);
  uint8_t u8MoreBytes = (1 < u8RxLen ? 1 : 0);

  i2c_reset_fifo(psI2C);

  //   WRITE slave addr to buffer
  prData[0] = (u8Addr << 1) | 1; // slave addr

  psI2C->COMD[0] = i2c_cmd_start();
  psI2C->COMD[1] = i2c_cmd_write(true, 1);
  if (u8MoreBytes) {
    psI2C->COMD[2] = i2c_cmd_read(false, u8RxLen - 1);
  }
  psI2C->COMD[2 + u8MoreBytes] = i2c_cmd_read(true, 1);
  psI2C->COMD[3 + u8MoreBytes] = i2c_cmd_stop();

  //  CTR
  psI2C->INT_CLR = I2C_INT_MASK_ALL;
  i2c_trans_start(psI2C);
}

void i2c_read_mem(EI2CBus eBus, uint8_t u8Addr, uint8_t u8MemAddr, uint8_t u8RxLen) {
  I2C_Type *psI2C = i2c_regs(eBus);
  RegAddr prData = i2c_nonfifo(eBus);
  uint8_t u8MoreBytes = (1 < u8RxLen ? 1 : 0);

  i2c_reset_fifo(psI2C);

  //   WRITE slave addr to buffer
  prData[0] = (u8Addr << 1) | 0; // slave addr (WR)
  prData[1] = u8MemAddr;
  prData[2] = (u8Addr << 1) | 1; // slave addr (RD)

  psI2C->COMD[0] = i2c_cmd_start();
  psI2C->COMD[1] = i2c_cmd_write(true, 2);
  psI2C->COMD[2] = i2c_cmd_start();
  psI2C->COMD[3] = i2c_cmd_write(true, 1);
  if (u8MoreBytes) {
    psI2C->COMD[4] = i2c_cmd_read(false, u8RxLen - 1);
  }
  psI2C->COMD[4 + u8MoreBytes] = i2c_cmd_read(true, 1);
  psI2C->COMD[5 + u8MoreBytes] = i2c_cmd_stop();

  //  CTR
  psI2C->INT_CLR = I2C_INT_MASK_ALL;
  i2c_trans_start(psI2C);
}

void i2c_init_controller(EI2CBus e8Bus, uint8_t u8SclPin, uint8_t u8SdaPin, uint32_t u32tckPeriod) {
  // - param_config()
  // -- set_pin()
  // --- set_level()
  gpio_pin_out_on(u8SclPin);
  gpio_pin_out_on(u8SdaPin);
  // --- func_sel()
  // FUN_WPU
  // (no pull-down) ~(1 << 7)
  IomuxGpioConfReg rI2CConf = {.u1FunIE = 1, .u1FunWPU = 1, .u3McuSel = 2};
  iomux_set_gpioconf(u8SclPin, rI2CConf);
  iomux_set_gpioconf(u8SdaPin, rI2CConf);

  // --- set_direction()
  // output enable
  gpio_pin_enable(u8SclPin);
  gpio_pin_enable(u8SdaPin);
  // skipped : connect output signal (256)
  // ---
  if (true) {
    // io_mux
    gpio_matrix_out(u8SclPin, _scl_idx(e8Bus), 0, 0);
    gpio_matrix_in(u8SclPin, _scl_idx(e8Bus), 0);
    gpio_matrix_out(u8SdaPin, _sda_idx(e8Bus), 0, 0);
    gpio_matrix_in(u8SdaPin, _sda_idx(e8Bus), 0);
  }

  // -- i2c_hw_enable()
  // --- i2c_ll_enable_bus_clock()
  dport_regs()->PERIP_CLK_EN |= 1 << _dport_peri_bit(e8Bus);

  // --- i2c_ll_reset_register()
  dport_regs()->PERIP_RST_EN |= 1 << _dport_peri_bit(e8Bus);
  dport_regs()->PERIP_RST_EN &= ~(1 << _dport_peri_bit(e8Bus));

  // -- i2c_hal_init()
  // --- i2c_ll_enable_controller_clock()
  // NOP

  // -- i2c_hal_master_init()
  i2c_regs(e8Bus)->CTR = 1 << 4 | 1 << 8 | 3; // MASTER | ?? | FORCE_SCL | FORCE_SDA

  // -- i2c_hal_master_set_filter()
  // NOP / TODO

  // -- i2c_hal_set_bus_timing()
  i2c_settiming(i2c_regs(e8Bus), u32tckPeriod);

  // - i2c_driver_install()
  // -- i2c_hw_enable()
  // -- i2c_hal_init()

  i2c_regs(e8Bus)->INT_CLR = I2C_INT_MASK_ALL;
  i2c_regs(e8Bus)->INT_ENA = I2C_INT_MASK_ALL;
  i2c_regs(e8Bus)->FIFO_CONF |= (1 << 10); // nonfifo_enable
}
