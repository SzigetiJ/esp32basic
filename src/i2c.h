/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#ifndef I2C_H
#define I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "esp32types.h"

#define I2C_INT_END_DETECTED        0x0008  ///< END command
#define I2C_INT_ARB_LOSS            0x0020  ///< SDA mismatch while SCL high
#define I2C_INT_MASTER_TRANS_COMPL  0x0040  ///< as master: byte sent/recvd
#define I2C_INT_TRANS_COMPL         0x0080  ///< STOP bit detected
#define I2C_INT_TIMEOUT             0x0100  ///< SCL high or low longer than timeout
#define I2C_INT_TRANS_START         0x0200  ///< START bit detected
#define I2C_INT_ACK_ERR             0x0400  ///< recvd - not as expected (master); - 1 (slave)
#define I2C_INT_RX_REC_FULL         0x0800  ///< recv more than threshold
#define I2C_INT_TX_SEND_EMPTY       0x1000  ///< sent more than threshold

#define I2C_INT_MASK_ERR            (I2C_INT_ARB_LOSS | I2C_INT_TIMEOUT | I2C_INT_ACK_ERR)  ///< union of error flags
#define I2C_INT_MASK_ALL            0x1FE8  ///< union of all documented flags

  // ============ Types =====================

  typedef struct {
    Reg SCL_LOW_PERIOD; // 0x00, bits 0..13
    Reg CTR;
    Reg SR;
    Reg TO; // bits 0..19
    Reg SLAVE_ADDR; // 0x10 bit 31: 10/7 bit addr, bits 0..14
    Reg FIFO_ST;
    Reg FIFO_CONF;
    Reg resv1c;
    Reg INT_RAW; // 0x20
    Reg INT_CLR;
    Reg INT_ENA;
    Reg INT_ST;
    Reg SDA_HOLD; // 0x30, bits 0..9
    Reg SDA_SAMPLE; // bits 0..9
    Reg SCL_HIGH_PERIOD; // bits 0..13
    Reg rsvd1[1];
    Reg SCL_START_HOLD; // 0x40
    Reg SCL_RSTART_SETUP;
    Reg SCL_STOP_HOLD;
    Reg SCL_STOP_SETUP;
    Reg SCL_FILTER_CFG; // 0x50
    Reg SDA_FILTER_CFG;
    Reg COMD[16]; // bit 31: cmd done; bits 11..13 opcode, 10: ackval, 9: ack_exp, 8: ack_chk_en, 0..7 byte_num (length)
  } I2C_Type;

  typedef enum {
    I2C0 = 0,
    I2C1 = 1
  } EI2CBus;

  typedef enum {
    eRstart = 0,
    eWrite = 1,
    eRead = 2,
    eStop = 3,
    eEnd = 4
  } E_I2C_CMD;

  // ============ Global values =====================
  extern I2C_Type gsI2C0;
  extern I2C_Type gsI2C1;

  static inline I2C_Type *i2c_regs(EI2CBus u8Bus) {
    return u8Bus == I2C0 ? &gsI2C0 : &gsI2C1;
  }

  static inline RegAddr i2c_nonfifo(EI2CBus u8Bus) {
    return &((RegAddr) i2c_regs(u8Bus))[64];
  }

  static inline uint32_t i2c_cmd_start() {
    return eRstart << 11;
  }

  static inline uint32_t i2c_cmd_write(bool bAck, uint8_t u8Len) {
    return eWrite << 11 | bAck << 8 | u8Len;
  }

  static inline uint32_t i2c_cmd_read(bool bAck, uint8_t u8Len) {
    return eRead << 11 | bAck << 10 | 1 << 8 | u8Len;
  }

  static inline uint32_t i2c_cmd_stop() {
    return eStop << 11;
  }

  static inline void i2c_reset_fifo(I2C_Type *psI2C) {
    psI2C->FIFO_CONF |= (3 << 12); // reset
    psI2C->FIFO_CONF &= ~(3 << 12);
  }

  static inline void i2c_trans_start(I2C_Type *psI2C) {
    psI2C->CTR |= 1 << 5;
  }

  static inline bool i2c_isbusy(I2C_Type *psI2C) {
    return (0 != (psI2C->SR & 0x10));
  }

  static inline void i2c_settiming(I2C_Type *psI2C, uint32_t u32Period) {
    uint32_t u32HPeriod = u32Period / 2;
    uint32_t u32QPeriod = u32HPeriod / 2;

    psI2C->SCL_HIGH_PERIOD = u32HPeriod - 7;
    psI2C->SCL_LOW_PERIOD = u32HPeriod - 1;
    psI2C->SCL_RSTART_SETUP = u32QPeriod;
    psI2C->SCL_START_HOLD = u32QPeriod;
    psI2C->SCL_STOP_SETUP = u32QPeriod;
    psI2C->SCL_STOP_HOLD = u32QPeriod;
    psI2C->SDA_HOLD = u32QPeriod;
    psI2C->SDA_SAMPLE = u32QPeriod;
    psI2C->TO = 20 * u32Period;

  }

  void i2c_write(EI2CBus eBus, uint8_t u8Addr, uint8_t u8Len, const uint8_t *pu8Dat);
  void i2c_read(EI2CBus eBus, uint8_t u8Addr, uint8_t u8RxLen);
  void i2c_read_mem(EI2CBus eBus, uint8_t u8Addr, uint8_t u8MemAddr, uint8_t u8RxLen);
  void i2c_init_controller(EI2CBus e8Bus, uint8_t u8SclPin, uint8_t u8SdaPin, uint32_t u32tckPeriod);

#ifdef __cplusplus
}
#endif

#endif /* I2C_H */
