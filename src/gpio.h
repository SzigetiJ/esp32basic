/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef GPIO_H
#define GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32types.h"

  // Based on:
  // https://github.com/espressif/esp-idf/blob/6b3da6b1882f3b72e904cc90be67e9c4e3f369a9/components/soc/esp32/include/soc/gpio_reg.h
  typedef volatile union {

    volatile struct {
      uint32_t rsvd0 : 2;
      uint32_t u1PinPadDriver : 1;
      uint32_t rsvd4 : 4;
      uint32_t u3PinIntType : 3;
      uint32_t bWakeUpEn : 1;
      uint32_t rsvd11 : 2;
      uint32_t u5PinIntEn : 5;
      uint32_t rsvd18 : 14;
    };
    volatile uint32_t raw;
  } SGpioPinReg;

  typedef struct {
    Reg BT_SELECT;
    Reg OUT;
    Reg OUT_W1TS;
    Reg OUT_W1TC;
    Reg OUT1;
    Reg OUT1_W1TS;
    Reg OUT1_W1TC;
    Reg SDIO_SELECT;
    Reg ENABLE;
    Reg ENABLE_W1TS;
    Reg ENABLE_W1TC;
    Reg ENABLE1;
    Reg ENABLE1_W1TS;
    Reg ENABLE1_W1TC;
    Reg STRAP;
    Reg IN;
    Reg IN1;
    Reg STATUS;
    Reg STATUS_W1TS;
    Reg STATUS_W1TC;
    Reg STATUS1;
    Reg STATUS1_W1TS;
    Reg STATUS1_W1TC;
    Reg _reserved0;
    Reg ACPU_INT;
    Reg ACPU_NMI_INT;
    Reg PCPU_INT;
    Reg PCPU_NMI_INT;
    Reg CPUSDIO_INT;
    Reg ACPU_INT1;
    Reg ACPU_NMI_INT1;
    Reg PCPU_INT1;
    Reg PCPU_NMI_INT1;
    Reg CPUSDIO_INT1;
    SGpioPinReg PIN[40];
    Reg cali_conf;
    Reg cali_data;
    Reg FUNC_IN_SEL_CFG[256];
    Reg FUNC_OUT_SEL_CFG[40];
  } GPIO_Type;

  extern GPIO_Type gsGPIO;

  static inline GPIO_Type *gpio_regs() {
    return &gsGPIO;
  }

  /**
   * Tells the register of any gpio pin in range [0..39].
   * Generally, there is a base register for gpio pins [0..31],
   * and there is another register for pins [32..39].
   * This second register is either directly after the base register, e.g., .IN1 follows .IN,
   * or a group of base register is followed by a group of second registers, e.g., OUT, OUT_W1TS, OUT_W1TC followed by their pairs.
   * @param prRegBase Base register.
   * @param u8Shift Difference between the base and the second register addresses (measured in register width).
   * @param u8Pin GPIO pin. The only thing that matters here is whether u8Pin is less than 32 or not.
   * @return Register to use for the given GPIO pin.
   */
  static inline RegAddr gpio_reg_anypin(RegAddr prRegBase, uint8_t u8Shift, uint8_t u8Pin) {
    return prRegBase + (u8Pin < 32 ? 0 : u8Shift);
  }

  static inline uint8_t gpio_pin_read(uint8_t u8Pin) {
    return 0 < (*gpio_reg_anypin(&gsGPIO.IN, 1, u8Pin) & (1 << (u8Pin & 0x1f)));
  }

  /**
   * Provides bit access for GPIO pins in range [0..39] for the following register groups: OUT, ENABLE, STATUS
   * @param prReg Base register, e.g., OUT, OUT_W1TS, OUT_W1TS, etc.
   * @param u8Pin GPIO pin.
   */
  static inline void gpio_reg_setbit(RegAddr prReg, uint8_t u8Pin) {
    prReg[u8Pin < 32 ? 0 : 3] = 1 << (u8Pin & 0x1f);
  }

  static inline void gpio_pin_enable(uint8_t u8Pin) {
    gpio_reg_setbit(&gsGPIO.ENABLE_W1TS, u8Pin);
  }

  static inline void gpio_pin_disable(uint8_t u8Pin) {
    gpio_reg_setbit(&gsGPIO.ENABLE_W1TC, u8Pin);
  }

  static inline void gpio_pin_out_on(uint8_t u8Pin) {
    gpio_reg_setbit(&gsGPIO.OUT_W1TS, u8Pin);
  }

  static inline void gpio_pin_out_off(uint8_t u8Pin) {
    gpio_reg_setbit(&gsGPIO.OUT_W1TC, u8Pin);
  }

#ifdef __cplusplus
}
#endif

#endif /* GPIO_H */
