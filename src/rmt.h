#ifndef RMT_H
#define RMT_H

#include "esp32types.h"


#ifdef __cplusplus
extern "C" {
#endif

#define RMT_SIGNAL1 (0x8000)
#define RMT_SIGNAL0 (0x0000)

#define RMT_CHANNEL_NUM     8U
#define RMT_RAM_BLOCK_SIZE  64U

  typedef enum {
    RMT_INT_RAW = 0, ///< Raw interrupt status (RO)
    RMT_INT_ST = 1, ///< Masked interrupt status (RO)
    RMT_INT_ENA = 2, ///< Setting a bit enables the given interrupt (RW)
    RMT_INT_CLR = 3 ///< Setting a bit disables the given interrupt (WO)
  } E_RMT_INT_REG; ///< Types of interrupt registers.

  typedef enum {
    RMT_INT_TXEND = 0, ///< TX is done.
    RMT_INT_RXEND = 1, ///< RX is done.
    RMT_INT_ERR = 2, ///< An error occured.
    RMT_INT_TXTHRES = 3 ///< TX threshold is reached.
  } E_RMT_INT_TYPE; ///< Types of RMT interrupt.

  typedef enum {
    RMT_CH0 = 0,
    RMT_CH1,
    RMT_CH2,
    RMT_CH3,
    RMT_CH4,
    RMT_CH5,
    RMT_CH6,
    RMT_CH7
  } E_RMT_CHANNEL;

  typedef volatile union {

    volatile struct {
      uint32_t u8DivCnt : 8; ///< The divisor for the channel clock of channel n. (R/W)

      uint32_t u16IdleThres : 16; ///< In receive mode, when no edge is detected on the input signal for longer than REG_IDLE_THRES_CHn channel clock cycles, the receive process is finished. (R/W)

      uint32_t u4MemSize : 4; ///< The amount of memory blocks allocated to channel n. (R/W)
      uint32_t bCarrierEn : 1; ///< Carrier modulation is enabled with 1, while carrier modulation is disabled with 0. (R/W)
      uint32_t bCarrierOutLvl : 1; ///< Used when the carrier wave is being transmitted. Transmit on low output level with 0, and transmit on high output level with 1. (R/W)
      uint32_t bMemPd : 1; ///< Power down the entire RMT RAM block.Reset (It only exists in RMT_CH0CONF0). 1: power down memory; 0: power up memory. (R/W)
      uint32_t rsvd31 : 1;
    };
    volatile uint32_t raw;
  } S_RMT_CHCONF0_REG;

  typedef volatile union {

    volatile struct {
      uint32_t bTxStart : 1; ///< Set this bit to start sending data on channel n. (R/W)
      uint32_t bRxEn : 1; ///< Set this bit to enable receiving data on channel n. (R/W)
      uint32_t bMemWrRst : 1; ///< Set this bit to reset the write-RAM address for channel n by accessing the receiver. (R/W)
      uint32_t bMemRdRst : 1; ///< Set this bit to reset the read-RAM address for channel n by accessing the transmitter. (R/W)
      uint32_t bFifoRst : 1; ///< Undocumented bit. Probably resets FIFO internal counters (R/W).
      uint32_t bMemOwner : 1; ///< Number 1 indicates that the receiver is using the RAM, while 0 indicates that the transmitter is using the RAM. (R/W)
      uint32_t bTxContiMode : 1; ///< If this bit is set, instead of going to an idle state when transmission ends, the transmitter will restart transmission. This results in a repeating output signal. (R/W)
      uint32_t bRxFilterEn : 1; ///< receive filter’s enable-bit for channel n. (R/W)

      uint32_t u8RxFilterThres : 8; ///< In receive mode, channel n ignores input pulse when the pulse width is smaller than this value in APB clock periods. (R/W)

      uint32_t bRefCntRst : 1; ///< Setting this bit resets the clock divider of channel n. (R/W)
      uint32_t bRefAlwaysOn : 1; ///< Select the channel’s base clock. 1:clk_apb; 0:clk_ref. (R/W)
      uint32_t bIdleOutLvl : 1; ///< The level of output signals in channel n when the latter is in IDLE state. (R/W)
      uint32_t bIdleOutEn : 1; ///< Output enable-control bit for channel n in IDLE state. (R/W)
      uint32_t rsvd20 : 12; ///< Reserved
    };
    volatile uint32_t raw;
  } S_RMT_CHCONF1_REG;

  typedef struct {
    S_RMT_CHCONF0_REG r0;
    S_RMT_CHCONF1_REG r1;
  } S_RMT_CHCONF;

  typedef struct {
    uint32_t u16Low : 16; ///< If carrier is enabled, this value defines the length of low level in the carrier wave (APB/REF ticks).
    uint32_t u16High : 16; ///< If carrier is enabled, this value defines the length of high level in the carrier wave (APB/REF ticks).
  } S_RMT_CHCARRIER_DUTY_REG;

  typedef union {

    volatile struct {
      uint32_t u9Val : 9; ///< After sending this amount of TX entries, TX_THR_EVENT interrupt is produced.
      uint32_t rsvd9 : 23;
    };
    volatile uint32_t raw;
  } S_RMT_CH_TX_LIM_REG;

  typedef union {

    volatile struct {
      uint32_t bMemAccessEn : 1; ///< Direct (1) / FIFO (0) access of RMT RAM.
      uint32_t bMemTxWrapEn : 1; ///< Enable/disable TX wraparound.
      uint32_t rsvd2 : 30;
    };
    volatile uint32_t raw;
  } S_RMT_APB_CONF_REG;

  // ---------------- undocumented register types -------------------

  typedef Reg S_RMT_STATUS_REG;

  typedef struct {
    uint16_t u16TxOffset;
    uint16_t u16RxOffset;
  } S_RMT_FIFO_OFFSET_REG;

  typedef struct {
    Reg arFifo[8]; ///< Undocumented registers. Probably RAM access (R/W) as FIFO for each channel.
    S_RMT_CHCONF asChConf[RMT_CHANNEL_NUM];
    S_RMT_STATUS_REG asStatus[RMT_CHANNEL_NUM];
    S_RMT_FIFO_OFFSET_REG asFifoOffset[RMT_CHANNEL_NUM];
    Reg arInt[4];
    S_RMT_CHCARRIER_DUTY_REG arCarrierDuty[RMT_CHANNEL_NUM];
    S_RMT_CH_TX_LIM_REG arTxLim[RMT_CHANNEL_NUM];
    S_RMT_APB_CONF_REG rApb;
    Reg rsvd61;
    Reg rsvd62;
    Reg rVersion;
  } RMT_Type;

  extern RMT_Type gsRMT;
  extern Reg grRMTRAM[RMT_CHANNEL_NUM * RMT_RAM_BLOCK_SIZE];
  static RMT_Type *gpsRMT = &gsRMT;
  static RegAddr gpsRMTRAM = (RegAddr) grRMTRAM;

  /**
   * Tells the RMT RAM base address of an RMT channel.
   * @param eChannel Identifies the RMT channel.
   * @return Base address of the RMT RAM of the given channel.
   */
  static inline RegAddr rmt_ram_block(E_RMT_CHANNEL eChannel) {
    return gpsRMTRAM + RMT_RAM_BLOCK_SIZE * eChannel;
  }

  /**
   * Tells the bit position of an interrupt in RMT interrupt registers (RAW / ST / ENA / CLR).
   * @param eChannel Identifies the RMT channel.
   * @param eType Type of the RMT interrupt.
   * @return Bit position in the interrupt registers.
   */
  static inline uint8_t rmt_int_idx(E_RMT_CHANNEL eChannel, E_RMT_INT_TYPE eType) {
    return eType == RMT_INT_TXTHRES ?
            24U + (eChannel & 7) :
            3U * (eChannel & 7) + (uint8_t) eType;
  }

  /**
   * Tells the masking value of an interrupt in RMT interrupt registers.
   * @param eChannel Identifies the RMT channel.
   * @param eType Type of the RMT interrupt.
   * @return Masking value of the given interrupt.
   */
  static inline uint32_t rmt_int_bit(E_RMT_CHANNEL eChannel, E_RMT_INT_TYPE eType) {
    return 1 << rmt_int_idx(eChannel, eType);
  }

  /**
   * Tells the output signal number of an RMT channel in the GPIO matrix.
   * @param eChannel Identifies the RMT channel.
   * @return GPIO output signal number.
   */
  static inline uint8_t rmt_out_signal(E_RMT_CHANNEL eChannel) {
    return 87 + eChannel;
  }

  /**
   * Tells the input signal number of an RMT channel in the GPIO matrix.
   * @param eChannel Identifies the RMT channel.
   * @return GPIO input signal number.
   */
  static inline uint8_t rmt_in_signal(E_RMT_CHANNEL eChannel) {
    return 83 + eChannel;
  }

  /**
   * Start data transmit.
   * @param eChannel Identifies the channel.
   * @param bMemRdRst Should the controller reset its memory read address for the channel.
   */
  static inline void rmt_start_tx(E_RMT_CHANNEL eChannel, bool bMemRdRst) {
    S_RMT_CHCONF1_REG rConf1 = {.bTxStart = 1, .bMemRdRst = bMemRdRst};
    gpsRMT->asChConf[eChannel].r1.raw |= rConf1.raw;
  }

  /**
   * Start receiving data.
   * @param eChannel Identifies the channel.
   * @param bMemWrRst Should the controller reset its memory write address for the channel.
   */
  static inline void rmt_start_rx(E_RMT_CHANNEL eChannel, bool bMemWrRst) {
    S_RMT_CHCONF1_REG rConf1 = {.bRxEn = 1, .bMemWrRst = bMemWrRst};
    gpsRMT->asChConf[eChannel].r1.raw |= rConf1.raw;
  }

#undef RMT_CHANNEL_NUM
#undef RMT_RAM_BLOCK_SIZE

#ifdef __cplusplus
}
#endif

#endif /* RMT_H */

