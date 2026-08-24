################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/Onoje/STM32Cube/Example/JPEG_DecodingUsingFs_DMA/Middlewares/Third_Party/FatFs/src/option/syscall.c \
C:/Users/Onoje/STM32Cube/Example/JPEG_DecodingUsingFs_DMA/Middlewares/Third_Party/FatFs/src/option/unicode.c 

OBJS += \
./Middlewares/FatFs/Options/syscall.o \
./Middlewares/FatFs/Options/unicode.o 

C_DEPS += \
./Middlewares/FatFs/Options/syscall.d \
./Middlewares/FatFs/Options/unicode.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/FatFs/Options/syscall.o: C:/Users/Onoje/STM32Cube/Example/JPEG_DecodingUsingFs_DMA/Middlewares/Third_Party/FatFs/src/option/syscall.c Middlewares/FatFs/Options/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DUSE_LCD_HDMI -DSTM32H747xx -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -DCORE_CM7 -c -I../../../Common/Inc -I../../../CM7/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/Fonts -I../../../Utilities/CPU -I../../../Utilities/lcd -I../../../Middlewares/Third_Party/FatFs/src -I../../../Middlewares/Third_Party/FatFs/src/drivers -I../../../CM7/LWIP/App -I../../../CM7/LWIP/Target -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/BSP/Components/lan8742 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
Middlewares/FatFs/Options/unicode.o: C:/Users/Onoje/STM32Cube/Example/JPEG_DecodingUsingFs_DMA/Middlewares/Third_Party/FatFs/src/option/unicode.c Middlewares/FatFs/Options/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DUSE_LCD_HDMI -DSTM32H747xx -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -DCORE_CM7 -c -I../../../Common/Inc -I../../../CM7/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/Fonts -I../../../Utilities/CPU -I../../../Utilities/lcd -I../../../Middlewares/Third_Party/FatFs/src -I../../../Middlewares/Third_Party/FatFs/src/drivers -I../../../CM7/LWIP/App -I../../../CM7/LWIP/Target -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/BSP/Components/lan8742 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-FatFs-2f-Options

clean-Middlewares-2f-FatFs-2f-Options:
	-$(RM) ./Middlewares/FatFs/Options/syscall.cyclo ./Middlewares/FatFs/Options/syscall.d ./Middlewares/FatFs/Options/syscall.o ./Middlewares/FatFs/Options/syscall.su ./Middlewares/FatFs/Options/unicode.cyclo ./Middlewares/FatFs/Options/unicode.d ./Middlewares/FatFs/Options/unicode.o ./Middlewares/FatFs/Options/unicode.su

.PHONY: clean-Middlewares-2f-FatFs-2f-Options

