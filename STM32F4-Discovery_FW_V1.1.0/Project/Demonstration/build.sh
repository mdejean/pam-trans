arm-none-eabi-gcc -Wall -Wno-strict-aliasing -fomit-frame-pointer \
-march=armv7-m -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mthumb -Os -fdata-sections -ffunction-sections  \
-I. \
-I../../Libraries/CMSIS/Include \
-I../../Libraries/CMSIS/ST/STM32F4xx/Include \
-I../../Libraries/STM32F4xx_StdPeriph_Driver/inc \
-I../../Utilities/STM32F4-Discovery \
-I../../Libraries/STM32_USB_OTG_Driver/inc \
-I../../Libraries/STM32_USB_Device_Library/Class/hid/inc \
-I../../Libraries/STM32_USB_Device_Library/Core/inc \
-DUSE_STDPERIPH_DRIVER -DSTM32F4XX -DMANGUSTA_DISCOVERY -DUSE_USB_OTG_FS -DHSE_VALUE=8000000 \
../../Libraries/STM32_USB_Device_Library/Class/hid/src/usbd_hid_core.c \
../../Libraries/STM32_USB_Device_Library/Core/src/usbd_core.c \
../../Libraries/STM32_USB_Device_Library/Core/src/usbd_ioreq.c \
../../Libraries/STM32_USB_Device_Library/Core/src/usbd_req.c \
../../Libraries/STM32_USB_OTG_Driver/src/usb_dcd_int.c \
../../Libraries/STM32_USB_OTG_Driver/src/usb_dcd.c \
../../Libraries/STM32_USB_OTG_Driver/src/usb_core.c \
../../Utilities/STM32F4-Discovery/*.c \
../../Libraries/STM32F4xx_StdPeriph_Driver/src/*.c \
../../Libraries/CMSIS/ST/STM32F4xx/Source/Templates/gcc_ride7/startup_stm32f4xx.s \
./*.c -Tlinker.ld -nostartfiles -o test.elf -Wl,--gc-sections -g

