PROJECT_NAME=gatt_server
GDB_INIT_FILE=$(PROJECT_NAME).gdbinit

all: build

menuconfig:
	idf.py menuconfig

build:
	idf.py set-target esp32c6
	idf.py build

env:
	. ~/esp/esp-idf/export.sh

flash:
	idf.py -p /dev/ttyACM0 flash

flash_with_ocd:
	xterm -e 'openocd -f $(OPENOCD_CFG) -c "program_esp build/$(PROJECT_NAME).bin 0x10000 verify reset exit"'

$(GDB_INIT_FILE): gdbinit

gdbinit:
	@echo 'target remote localhost:3333' > $(GDB_INIT_FILE)
	@echo 'set remote hardware-watchpoint-limit 4' >> $(GDB_INIT_FILE)
	@echo 'mon reset halt' >> $(GDB_INIT_FILE)
	@echo 'maintenance flush register-cache' >> $(GDB_INIT_FILE)
	@echo 'thb app_main' >> $(GDB_INIT_FILE)
	@cat $(GDB_INIT_FILE)

ocd:
	xterm -e "openocd -f $(OPENOCD_CFG)" &

debug:  $(GDB_INIT_FILE) ocd
	xterm -e "riscv32-esp-elf-gdb -x $(GDB_INIT_FILE) build/$(PROJECT_NAME).elf" &

clean:
	idf.py clean
	rm -rf build


