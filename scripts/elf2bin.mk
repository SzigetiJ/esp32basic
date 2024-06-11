
.elf.bin:
	esptool.py --chip=esp32 elf2image --flash_mode dio --flash_freq 40m --flash_size 4MB $<
