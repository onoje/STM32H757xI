################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/Src/decode_dma.c \
C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/LWIP/Target/ethernetif.c \
C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/LWIP/App/lwip.c \
C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/Src/main.c \
C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/Src/network_stream.c \
C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/Src/sd_diskio.c \
C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/Src/stm32h7xx_hal_msp.c \
C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/Src/stm32h7xx_it.c \
../Example/User/CM7/syscalls.c \
../Example/User/CM7/sysmem.c 

OBJS += \
./Example/User/CM7/decode_dma.o \
./Example/User/CM7/ethernetif.o \
./Example/User/CM7/lwip.o \
./Example/User/CM7/main.o \
./Example/User/CM7/network_stream.o \
./Example/User/CM7/sd_diskio.o \
./Example/User/CM7/stm32h7xx_hal_msp.o \
./Example/User/CM7/stm32h7xx_it.o \
./Example/User/CM7/syscalls.o \
./Example/User/CM7/sysmem.o 

C_DEPS += \
./Example/User/CM7/decode_dma.d \
./Example/User/CM7/ethernetif.d \
./Example/User/CM7/lwip.d \
./Example/User/CM7/main.d \
./Example/User/CM7/network_stream.d \
./Example/User/CM7/sd_diskio.d \
./Example/User/CM7/stm32h7xx_hal_msp.d \
./Example/User/CM7/stm32h7xx_it.d \
./Example/User/CM7/syscalls.d \
./Example/User/CM7/sysmem.d 


# Each subdirectory must supply rules for building sources it contributes
Example/User/CM7/decode_dma.o: C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/Src/decode_dma.c Example/User/CM7/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DUSE_LCD_HDMI -DSTM32H747xx -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -DCORE_CM7 -c -I../../../Common/Inc -I../../../CM7/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/Fonts -I../../../Utilities/CPU -I../../../Utilities/lcd -I../../../Middlewares/Third_Party/FatFs/src -I../../../Middlewares/Third_Party/FatFs/src/drivers -I../../../CM7/LWIP/App -I../../../CM7/LWIP/Target -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/BSP/Components/lan8742 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
Example/User/CM7/ethernetif.o: C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/LWIP/Target/ethernetif.c Example/User/CM7/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DUSE_LCD_HDMI -DSTM32H747xx -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -DCORE_CM7 -c -I../../../Common/Inc -I../../../CM7/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/Fonts -I../../../Utilities/CPU -I../../../Utilities/lcd -I../../../Middlewares/Third_Party/FatFs/src -I../../../Middlewares/Third_Party/FatFs/src/drivers -I../../../CM7/LWIP/App -I../../../CM7/LWIP/Target -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/BSP/Components/lan8742 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
Example/User/CM7/lwip.o: C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/LWIP/App/lwip.c Example/User/CM7/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DUSE_LCD_HDMI -DSTM32H747xx -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -DCORE_CM7 -c -I../../../Common/Inc -I../../../CM7/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/Fonts -I../../../Utilities/CPU -I../../../Utilities/lcd -I../../../Middlewares/Third_Party/FatFs/src -I../../../Middlewares/Third_Party/FatFs/src/drivers -I../../../CM7/LWIP/App -I../../../CM7/LWIP/Target -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/BSP/Components/lan8742 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
Example/User/CM7/main.o: C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/Src/main.c Example/User/CM7/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DUSE_LCD_HDMI -DSTM32H747xx -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -DCORE_CM7 -c -I../../../Common/Inc -I../../../CM7/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/Fonts -I../../../Utilities/CPU -I../../../Utilities/lcd -I../../../Middlewares/Third_Party/FatFs/src -I../../../Middlewares/Third_Party/FatFs/src/drivers -I../../../CM7/LWIP/App -I../../../CM7/LWIP/Target -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/BSP/Components/lan8742 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
Example/User/CM7/network_stream.o: C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/Src/network_stream.c Example/User/CM7/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DUSE_LCD_HDMI -DSTM32H747xx -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -DCORE_CM7 -c -I../../../Common/Inc -I../../../CM7/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/Fonts -I../../../Utilities/CPU -I../../../Utilities/lcd -I../../../Middlewares/Third_Party/FatFs/src -I../../../Middlewares/Third_Party/FatFs/src/drivers -I../../../CM7/LWIP/App -I../../../CM7/LWIP/Target -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/BSP/Components/lan8742 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
Example/User/CM7/sd_diskio.o: C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/Src/sd_diskio.c Example/User/CM7/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DUSE_LCD_HDMI -DSTM32H747xx -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -DCORE_CM7 -c -I../../../Common/Inc -I../../../CM7/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/Fonts -I../../../Utilities/CPU -I../../../Utilities/lcd -I../../../Middlewares/Third_Party/FatFs/src -I../../../Middlewares/Third_Party/FatFs/src/drivers -I../../../CM7/LWIP/App -I../../../CM7/LWIP/Target -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/BSP/Components/lan8742 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
Example/User/CM7/stm32h7xx_hal_msp.o: C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/Src/stm32h7xx_hal_msp.c Example/User/CM7/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DUSE_LCD_HDMI -DSTM32H747xx -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -DCORE_CM7 -c -I../../../Common/Inc -I../../../CM7/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/Fonts -I../../../Utilities/CPU -I../../../Utilities/lcd -I../../../Middlewares/Third_Party/FatFs/src -I../../../Middlewares/Third_Party/FatFs/src/drivers -I../../../CM7/LWIP/App -I../../../CM7/LWIP/Target -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/BSP/Components/lan8742 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
Example/User/CM7/stm32h7xx_it.o: C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/CM7/Src/stm32h7xx_it.c Example/User/CM7/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DUSE_LCD_HDMI -DSTM32H747xx -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -DCORE_CM7 -c -I../../../Common/Inc -I../../../CM7/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/Fonts -I../../../Utilities/CPU -I../../../Utilities/lcd -I../../../Middlewares/Third_Party/FatFs/src -I../../../Middlewares/Third_Party/FatFs/src/drivers -I../../../CM7/LWIP/App -I../../../CM7/LWIP/Target -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/BSP/Components/lan8742 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
Example/User/CM7/%.o Example/User/CM7/%.su Example/User/CM7/%.cyclo: ../Example/User/CM7/%.c Example/User/CM7/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DUSE_LCD_HDMI -DSTM32H747xx -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -DCORE_CM7 -c -I../../../Common/Inc -I../../../CM7/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/Fonts -I../../../Utilities/CPU -I../../../Utilities/lcd -I../../../Middlewares/Third_Party/FatFs/src -I../../../Middlewares/Third_Party/FatFs/src/drivers -I../../../CM7/LWIP/App -I../../../CM7/LWIP/Target -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/BSP/Components/lan8742 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Example-2f-User-2f-CM7

clean-Example-2f-User-2f-CM7:
	-$(RM) ./Example/User/CM7/decode_dma.cyclo ./Example/User/CM7/decode_dma.d ./Example/User/CM7/decode_dma.o ./Example/User/CM7/decode_dma.su ./Example/User/CM7/ethernetif.cyclo ./Example/User/CM7/ethernetif.d ./Example/User/CM7/ethernetif.o ./Example/User/CM7/ethernetif.su ./Example/User/CM7/lwip.cyclo ./Example/User/CM7/lwip.d ./Example/User/CM7/lwip.o ./Example/User/CM7/lwip.su ./Example/User/CM7/main.cyclo ./Example/User/CM7/main.d ./Example/User/CM7/main.o ./Example/User/CM7/main.su ./Example/User/CM7/network_stream.cyclo ./Example/User/CM7/network_stream.d ./Example/User/CM7/network_stream.o ./Example/User/CM7/network_stream.su ./Example/User/CM7/sd_diskio.cyclo ./Example/User/CM7/sd_diskio.d ./Example/User/CM7/sd_diskio.o ./Example/User/CM7/sd_diskio.su ./Example/User/CM7/stm32h7xx_hal_msp.cyclo ./Example/User/CM7/stm32h7xx_hal_msp.d ./Example/User/CM7/stm32h7xx_hal_msp.o ./Example/User/CM7/stm32h7xx_hal_msp.su ./Example/User/CM7/stm32h7xx_it.cyclo ./Example/User/CM7/stm32h7xx_it.d ./Example/User/CM7/stm32h7xx_it.o ./Example/User/CM7/stm32h7xx_it.su ./Example/User/CM7/syscalls.cyclo ./Example/User/CM7/syscalls.d ./Example/User/CM7/syscalls.o ./Example/User/CM7/syscalls.su ./Example/User/CM7/sysmem.cyclo ./Example/User/CM7/sysmem.d ./Example/User/CM7/sysmem.o ./Example/User/CM7/sysmem.su

.PHONY: clean-Example-2f-User-2f-CM7

