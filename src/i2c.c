/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "dport.h"
#include "gpio.h"
#include "i2c.h"
#include "iomux.h"
#include "romfunctions.h"
#include "esp_attr.h"

// ============== Defines ==============
#define I2C0_SCL_IDX 29U
#define I2C0_SDA_IDX 30U
#define I2C1_SCL_IDX 95U
#define I2C1_SDA_IDX 96U
#define DPORT_I2C0_BIT 7U
#define DPORT_I2C1_BIT 18U

#define INTDISPATCHER_SLOTS 10U

// ============== Local types ==============

typedef struct {
  uint32_t au32IntMask[INTDISPATCHER_SLOTS];
  Isr afIsr[INTDISPATCHER_SLOTS];
  void *apvParam[INTDISPATCHER_SLOTS];
} SI2CIntDispatcher;

// ============== Local data ==============
SI2CIntDispatcher gasIntDispatcher[I2C_CHANNEL_NUM];

// ============== Internal function declarations ==============
void _dispatch_isr(void *pvParam);
static inline uint8_t _address(uint8_t u8RawAddr, bool bWrite);
static inline uint8_t _address_read(uint8_t u8RawAddr);
static inline uint8_t _address_write(uint8_t u8RawAddr);
static inline uint8_t _cmd_read_to_regs(Reg *prDest, uint8_t u8RxLen);
static inline uint8_t _scl_idx(EI2CBus eBus);
static inline uint8_t _sda_idx(EI2CBus eBus);
static inline uint8_t _dport_peri_bit(EI2CBus eBus);

// ============== Implementation ==============
// -------------- Internal functions --------------

IRAM_ATTR void _dispatch_isr(void *pvParam) {
  EI2CBus eBus = (int) pvParam;
  SI2CIntDispatcher *psTable = &gasIntDispatcher[eBus];
  uint32_t u32IntMask = i2c_regs(eBus)->INT_ST;
  for (uint8_t u8Idx = 0; u8Idx < INTDISPATCHER_SLOTS; ++u8Idx) {
    if (psTable->au32IntMask[u8Idx] & u32IntMask) {
      psTable->afIsr[u8Idx](psTable->apvParam[u8Idx]);
    } else {
      // not matching table entry
    }
  }
  i2c_regs(eBus)->INT_CLR = u32IntMask;
}

static inline uint8_t _address(uint8_t u8RawAddr, bool bRead) {
  return (u8RawAddr << 1) | (bRead ? 1 : 0);
}

static inline uint8_t _address_read(uint8_t u8RawAddr) {
  return _address(u8RawAddr, true);
}

static inline uint8_t _address_write(uint8_t u8RawAddr) {
  return _address(u8RawAddr, false);
}

static inline uint8_t _cmd_read_to_regs(Reg *prDest, uint8_t u8RxLen) {
  uint8_t u8MoreBytes = (1 < u8RxLen ? 1 : 0);
  prDest[0] = i2c_cmd_start();
  prDest[1] = i2c_cmd_write(true, 1);
  if (u8MoreBytes) {
    prDest[2] = i2c_cmd_read(false, u8RxLen - 1);
  }
  prDest[2 + u8MoreBytes] = i2c_cmd_read(true, 1);
  prDest[3 + u8MoreBytes] = i2c_cmd_stop();
  return 4 + u8MoreBytes;
}

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

// -------------- Interface functions --------------

/**
 * Initializes (clears) ISR dispatcher information table.
 */
void i2c_isr_init() {
  memset(&gasIntDispatcher, 0, sizeof (gasIntDispatcher));
}

/**
 * Binds i2c interrupt handler (i2c isr dispatcher) to an interrupt channel and a given CPU.
 * i2c_isr_init() must preceed this function. i2c_isr_register() can be invoked even after this function.
 * @param eCpu CPU that will run the ISR.
 * @param eBus I2C bus.
 * @param u8IntChannel Interrupt channel to use for I2C interrupts.
 */
void i2c_isr_start(ECpu eCpu, EI2CBus eBus, uint8_t u8IntChannel) {
  RegAddr prDportIntMap = (eCpu == CPU_PRO ? &dport_regs()->PRO_I2C_EXT0_INTR_MAP : &dport_regs()->APP_I2C_EXT0_INTR_MAP);
  if (eBus == I2C1) {
    ++prDportIntMap;
  }

  *prDportIntMap = u8IntChannel;
  _xtos_set_interrupt_handler_arg(u8IntChannel, _dispatch_isr, (int) eBus);
  ets_isr_unmask(1 << u8IntChannel);
}

/**
 * Registers ISRs to an I2C channel.
 * If a given interrupt type should not be handled, the corresponding Isr parameter should be NULL.
 * @param eChannel Identifies the I2C channel
 * @param u32IntMask Interrupt mask.
 * @param fIsr Function to invoke in case of eIntType interrupt.
 * @param pvParam parameter passed to the Isr functions.
 * @return Index of the registered ISR in the dispatchers table (can be used for unregister). On failure: INTDISPATCHER_SLOTS.
 */
uint8_t i2c_isr_register(EI2CBus eBus, uint32_t u32IntMask, Isr fIsr, void *pvParam) {
  SI2CIntDispatcher *psTable = &gasIntDispatcher[eBus];
  // find an empty slot
  uint8_t u8Idx = 0;
  while (u8Idx < INTDISPATCHER_SLOTS) {
    if (psTable->au32IntMask[u8Idx] == 0) {
      break;
    }
    ++u8Idx;
  }
  if (u8Idx == INTDISPATCHER_SLOTS) { // no empty slot found
    return false;
  }

  psTable->au32IntMask[u8Idx] = u32IntMask;
  psTable->afIsr[u8Idx] = fIsr;
  psTable->apvParam[u8Idx] = pvParam;
  return u8Idx;
}

void i2c_isr_unregister(EI2CBus eBus, uint8_t u8Idx) {
  gasIntDispatcher[eBus].au32IntMask[u8Idx] = 0;
}

void i2c_write(EI2CBus eBus, uint8_t u8Addr, uint32_t u32Len, const uint8_t *pu8Dat) {
  I2C_Type *psI2C = i2c_regs(eBus);
  RegAddr prData = i2c_nonfifo(eBus);
  bool bMultiChunk = (254 < u32Len);

  i2c_reset_fifo(psI2C);

  // put data to be written into the buffer
  prData[0] = _address_write(u8Addr);  // slave addr
  // and the first chunk of the real data: max 31 bytes
  for (int i = 0; i < 31 && i < u32Len; ++i) {
    prData[i + 1] = pu8Dat[i];
  }

  psI2C->COMD[0] = i2c_cmd_start();
  if (!bMultiChunk) {
    psI2C->COMD[1] = i2c_cmd_write(true, u32Len + 1);
    psI2C->COMD[2] = i2c_cmd_stop();
  } else {
    psI2C->COMD[1] = i2c_cmd_write(true, 255);  // full first chunk
    u32Len -= 254;
    uint8_t u8ComdPtr = 2;
    while (u8ComdPtr < 15 && u32Len) {
      uint8_t u32ChunkSize = u32Len < 255 ? u32Len : 255;
      psI2C->COMD[u8ComdPtr] = i2c_cmd_write(true, u32ChunkSize);
      u32Len -= u32ChunkSize;
      ++u8ComdPtr;
    }
    psI2C->COMD[u8ComdPtr] = i2c_cmd_stop();
  }
  //  CTR
  psI2C->INT_CLR = I2C_INT_MASK_ALL;
  i2c_trans_start(psI2C);
}

void i2c_read(EI2CBus eBus, uint8_t u8Addr, uint8_t u8RxLen) {
  I2C_Type *psI2C = i2c_regs(eBus);
  RegAddr prData = i2c_nonfifo(eBus);

  i2c_reset_fifo(psI2C);

  // put data to be written into the buffer
  prData[0] = _address_read(u8Addr); // slave addr

  _cmd_read_to_regs(&psI2C->COMD[0], u8RxLen);

  //  CTR
  psI2C->INT_CLR = I2C_INT_MASK_ALL;
  i2c_trans_start(psI2C);
}

void i2c_read_mem(EI2CBus eBus, uint8_t u8Addr, uint8_t u8MemAddr, uint8_t u8RxLen) {
  I2C_Type *psI2C = i2c_regs(eBus);
  RegAddr prData = i2c_nonfifo(eBus);

  i2c_reset_fifo(psI2C);

  // put data to be written into the buffer
  prData[0] = _address_write(u8Addr); // slave addr (WR)
  prData[1] = u8MemAddr;
  prData[2] = _address_read(u8Addr);  // slave addr (RD)

  psI2C->COMD[0] = i2c_cmd_start();
  psI2C->COMD[1] = i2c_cmd_write(true, 2);  // prData[0..1]
  _cmd_read_to_regs(&psI2C->COMD[2], u8RxLen);

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
