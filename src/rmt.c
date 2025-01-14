/*
 * Copyright 2024 SZIGETI János
 *
 * This file is part of Bilis ESP32 Basic, which is released under GNU General Public License.version 3.
 * See LICENSE or <https://www.gnu.org/licenses/> for full license details.
 */
#include <string.h>
#include "dport.h"
#include "esp_attr.h"
#include "gpio.h"
#include "iomux.h"
#include "rmt.h"
#include "romfunctions.h"

// ============= Local types ===============

typedef struct {
  Isr aafIsr[8][4];
  void *apvParam[8];
  uint8_t abChannelEn;
} SRmtIntDispatcher;

// ==================== Local data ================
SRmtIntDispatcher gsIntDispatcher;

// ============== Internal function declarations ==============
void _dispatch_isr(void *pvParam);


// ============== Internal functions ==============

IRAM_ATTR void _dispatch_isr(void *pvParam) {
  SRmtIntDispatcher *psParam = (SRmtIntDispatcher*)pvParam;
  for (int iChannel = 0; iChannel < RMT_CHANNEL_NUM; ++iChannel) {
    if (psParam->abChannelEn & (1 << iChannel)) {
      for (int iInt = 0; iInt < 4; ++iInt) {
        if (gpsRMT->arInt[RMT_INT_ST] & rmt_int_bit(iChannel, iInt)) {
          gpsRMT->arInt[RMT_INT_CLR] = rmt_int_bit(iChannel, iInt);
          if (NULL != psParam->aafIsr[iChannel][iInt]) {
            psParam->aafIsr[iChannel][iInt](psParam->apvParam[iChannel]);
          } else {
            // unhandled exception with registered RMT channel
          }
        } else {
          // interrupt flag (iChannel, iInt) is not active
        }
      }
    } else {
      // RMT channel not registered
    }
  }
}

// ============== Interface functions ==============

/**
 * Initializes (clears) ISR dispatcher information table.
 */
void rmt_isr_init() {
  memset(&gsIntDispatcher, 0, sizeof (gsIntDispatcher));
}

/**
 * Binds rmt interrupt handler (rmt isr dispatcher) to an interrupt channel and a given CPU.
 * rmt_isr_init() must preceed this function. rmt_isr_register() can be invoked even after this function.
 * @param eCpu CPU that will run the ISR.
 * @param u8IntChannel Interrupt channel to use for RMT interrupts.
 */
void rmt_isr_start(ECpu eCpu, uint8_t u8IntChannel) {
  RegAddr prDportIntMap = (eCpu == CPU_PRO ? &dport_regs()->PRO_RMT_INTR_MAP : &dport_regs()->APP_RMT_INTR_MAP);

  *prDportIntMap = u8IntChannel;
  _xtos_set_interrupt_handler_arg(u8IntChannel, _dispatch_isr, (int) &gsIntDispatcher);
  ets_isr_unmask(1 << u8IntChannel);

}

/**
 * Registers ISRs to an RMT channel.
 * If a given interrupt type should not be handled, the corresponding Isr parameter should be NULL.
 * @param eChannel Identifies the RMT channel.
 * @param fTxEndIsr function to invoke in case of TXEND interrupt.
 * @param fRxEndIsr function to invoke in case of RXEND interrupt.
 * @param fTxThresholdIsr function to invoke in case of TXTHRESH interrupt.
 * @param fErrorIsr function to invoke in case of ERR interrupt.
 * @param pvParam parameter passed to the Isr functions.
 */
void rmt_isr_register(ERmtChannel eChannel, Isr fTxEndIsr, Isr fRxEndIsr, Isr fTxThresholdIsr, Isr fErrorIsr, void *pvParam) {
  gsIntDispatcher.abChannelEn |= (1 << eChannel);

  gsIntDispatcher.aafIsr[eChannel][RMT_INT_TXEND] = fTxEndIsr;
  gsIntDispatcher.aafIsr[eChannel][RMT_INT_RXEND] = fRxEndIsr;
  gsIntDispatcher.aafIsr[eChannel][RMT_INT_TXTHRES] = fTxThresholdIsr;
  gsIntDispatcher.aafIsr[eChannel][RMT_INT_ERR] = fErrorIsr;

  gsIntDispatcher.apvParam[eChannel] = pvParam;

  Reg rIntMask = 0;
  for (int iInt = 0; iInt < 4; ++iInt) {
    if (NULL != gsIntDispatcher.aafIsr[eChannel][iInt]) rIntMask |= rmt_int_bit(eChannel, iInt);
  }
  gpsRMT->arInt[RMT_INT_ENA] |= rIntMask;
}

/**
 * Global RMT peripherial initialization.
 * Enables peripheral in DPORT and sets basic ApbConf attributes.
 * @param bMemAccessEn attribute to set in rApb.
 * @param bMemTxWrapEn attribute to set in rApb.
 */
void rmt_init_controller(bool bMemAccessEn, bool bMemTxWrapEn) {
  dport_regs()->PERIP_CLK_EN |= 1 << DPORT_PERIP_BIT_RMT;

  dport_regs()->PERIP_RST_EN |= 1 << DPORT_PERIP_BIT_RMT;
  dport_regs()->PERIP_RST_EN &= ~(1 << DPORT_PERIP_BIT_RMT);

  SRmtApbConfReg rApbConf = {.bMemAccessEn = bMemAccessEn, .bMemTxWrapEn = bMemTxWrapEn};
  gpsRMT->rApb = rApbConf;

}

/**
 * Initializes an RMT channel by configuring IOMUX and GPIO registers.
 * @param eChannel Identifies RMT channel.
 * @param u8Pin Identifies GPIO pin.
 * @param bInitLevel Initial output level on the GPIO pin (before/after RMT TX is running).
 */
void rmt_init_channel(ERmtChannel eChannel, uint8_t u8Pin, bool bInitLevel) {
  // gpio & iomux
  bInitLevel ? gpio_pin_out_on(u8Pin) : gpio_pin_out_off(u8Pin); // set GPIO level when not bound to RMT (optional)

  IomuxGpioConfReg rRmtConf = {.u1FunIE = 1, .u1FunWPU = 1, .u3McuSel = 2}; // input enable, pull-up, iomux function
  iomux_set_gpioconf(u8Pin, rRmtConf);

  gpio_pin_enable(u8Pin);
  gpio_matrix_out(u8Pin, rmt_out_signal(eChannel), 0, 0);
  gpio_matrix_in(u8Pin, rmt_in_signal(eChannel), 0);
}
