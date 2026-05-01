#include "i2c.h"

//  void i2c_isr_init();
//  void i2c_isr_start(ECpu eCpu, EI2CBus eBus, uint8_t u8IntChannel);
//  uint8_t i2c_isr_register(EI2CBus eBus, uint32_t u32IntMask, Isr fIsr, void *pvParam);
//  void i2c_isr_unregister(EI2CBus eBus, uint8_t u8Idx);

void i2c_write(EI2CBus eBus, uint8_t u8Addr, uint32_t u32Len, const uint8_t *pu8Dat) {
  
}
//void i2c_read(EI2CBus eBus, uint8_t u8Addr, uint8_t u8RxLen);
void i2c_read_mem(EI2CBus eBus, uint8_t u8Addr, uint8_t u8MemAddr, uint8_t u8RxLen) {
  
}
//  void i2c_init_controller(EI2CBus e8Bus, uint8_t u8SclPin, uint8_t u8SdaPin, uint32_t u32tckPeriod);
