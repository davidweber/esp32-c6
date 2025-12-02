#!/bin/bash
#
# @file   mk_esp32_proj.bash
# @author David Weber david.weber.dfw@gmail.com
# @date   09/05/2025
#
# @brief script to create esp32 project skeleton
#
# Copyright (C) 2025 David Weber
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

if [ $# -ne 1 ]; then
  BASENAME=`basename ${0}`
  echo "Usage: ${BASENAME} <project name>"
  exit -1
fi

PROJECT_NAME=${1}

if [ -d ${PROJECT_NAME} ]; then
  echo "${PROJECT_NAME} already exists!"
  exit -2
fi

idf.py create-project ${PROJECT_NAME} 
#mkdir ${PROJECT_NAME}

cd ${PROJECT_NAME}

cat >$PWD/Makefile <<EOL
PROJECT_NAME=${PROJECT_NAME}
GDB_INIT_FILE=\$(PROJECT_NAME).gdbinit
DEV_FILE=/dev/ttyESP32-0

all: build

menuconfig:
	idf.py menuconfig

build:
	idf.py set-target esp32c6
	idf.py build

env:
	@echo . ~/esp/esp-idf/export.sh

flash:
	idf.py -p $(DEV_FILE)

flash_with_ocd:
	xterm -e 'openocd -f \$(OPENOCD_CFG) -c "program_esp build/\$(PROJECT_NAME).bin 0x10000 verify reset exit"'

\$(GDB_INIT_FILE): gdbinit

gdbinit:
	@echo 'target remote localhost:3333' > \$(GDB_INIT_FILE)
	@echo 'set remote hardware-watchpoint-limit 4' >> \$(GDB_INIT_FILE)
	@echo 'mon reset halt' >> \$(GDB_INIT_FILE)
	@echo 'maintenance flush register-cache' >> \$(GDB_INIT_FILE)
	@echo 'thb app_main' >> \$(GDB_INIT_FILE)
	@cat \$(GDB_INIT_FILE)

ocd:
	xterm -e "openocd -f \$(OPENOCD_CFG)" &

debug:  \$(GDB_INIT_FILE) ocd
	xterm -e "riscv32-esp-elf-gdb -x \$(GDB_INIT_FILE) build/\$(PROJECT_NAME).elf" &

clean:
	idf.py clean
	rm -r build
EOL

mkdir -p main/src
mkdir -p main/inc
mv main/${PROJECT_NAME}.c main/src/main.c

#file(GLOB_RECURSE srcs "main.c" "src/*.c")
cat >$PWD/main/CMakeLists.txt <<EOL
file(GLOB_RECURSE srcs "src/*.c")
idf_component_register(SRCS "\${srcs}"
                       INCLUDE_DIRS "." "inc")
EOL


