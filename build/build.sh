FIRMWARE_PACKAGE="./STM32F4-Discovery_FW_V1.1.0"

arm-none-eabi-gcc -O3 -std=gnu99 -static -nostdlib -Wall -Wno-strict-aliasing -fomit-frame-pointer \
-mfloat-abi=hard -fsingle-precision-constant -Wdouble-promotion -mfpu=fpv4-sp-d16 \
-fno-math-errno -mcpu=cortex-m4 -mthumb  \
-Iinc \
-I$FIRMWARE_PACKAGE/Libraries/CMSIS/Include \
-I$FIRMWARE_PACKAGE/Libraries/CMSIS/ST/STM32F4xx/Include \
-I$FIRMWARE_PACKAGE/Libraries/STM32F4xx_StdPeriph_Driver/inc \
-I$FIRMWARE_PACKAGE/Utilities/STM32F4-Discovery \
-I$FIRMWARE_PACKAGE/Libraries/STM32_USB_OTG_Driver/inc \
-I$FIRMWARE_PACKAGE/Libraries/STM32_USB_Device_Library/Class/hid/inc \
-I$FIRMWARE_PACKAGE/Libraries/STM32_USB_Device_Library/Core/inc \
-DUSE_STDPERIPH_DRIVER -DSTM32F4XX -DMANGUSTA_DISCOVERY -DUSE_USB_OTG_FS -DHSE_VALUE=8000000 \
$FIRMWARE_PACKAGE/Libraries/STM32F4xx_StdPeriph_Driver/src/*.c \
$FIRMWARE_PACKAGE/Libraries/CMSIS/ST/STM32F4xx/Source/Templates/gcc_ride7/startup_stm32f4xx.s \
$FIRMWARE_PACKAGE/Libraries/CMSIS/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c \
src/*.c -Tbuild/linker.ld -o bin/pam-trans.elf -g #-fdata-sections -ffunction-sections -Wl,--gc-sections

