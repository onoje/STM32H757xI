################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/Drivers/BSP/STM32H747I-EVAL/stm32h747i_eval.c \
C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/Drivers/BSP/STM32H747I-EVAL/stm32h747i_eval_bus.c \
C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/Drivers/BSP/STM32H747I-EVAL/stm32h747i_eval_io.c 

OBJS += \
./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval.o \
./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_bus.o \
./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_io.o 

C_DEPS += \
./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval.d \
./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_bus.d \
./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_io.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval.o: C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/Drivers/BSP/STM32H747I-EVAL/stm32h747i_eval.c Drivers/BSP/STM32H747I_EVAL/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DSTM32H747xx -DCORE_CM4 -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -c -I../../../Common/Inc -I../../../CM4/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/lcd -I../../../Utilities/Fonts -I../../../Utilities/CPU -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_bus.o: C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/Drivers/BSP/STM32H747I-EVAL/stm32h747i_eval_bus.c Drivers/BSP/STM32H747I_EVAL/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DSTM32H747xx -DCORE_CM4 -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -c -I../../../Common/Inc -I../../../CM4/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/lcd -I../../../Utilities/Fonts -I../../../Utilities/CPU -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_io.o: C:/Users/Onoje/Desktop/HAVELSAN_STM32/JPEG_DecodingUsingFs_DMA/Drivers/BSP/STM32H747I-EVAL/stm32h747i_eval_io.c Drivers/BSP/STM32H747I_EVAL/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_STM32H747I_EVAL -DSTM32H747xx -DCORE_CM4 -DUSE_HAL_DRIVER -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_IOEXPANDER -c -I../../../Common/Inc -I../../../CM4/Inc -I../../../Drivers/CMSIS/Include -I../../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../../Drivers/BSP/STM32H747I-EVAL -I../../../Drivers/BSP/Components/Common -I../../../Utilities/lcd -I../../../Utilities/Fonts -I../../../Utilities/CPU -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-BSP-2f-STM32H747I_EVAL

clean-Drivers-2f-BSP-2f-STM32H747I_EVAL:
	-$(RM) ./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval.cyclo ./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval.d ./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval.o ./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval.su ./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_bus.cyclo ./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_bus.d ./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_bus.o ./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_bus.su ./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_io.cyclo ./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_io.d ./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_io.o ./Drivers/BSP/STM32H747I_EVAL/stm32h747i_eval_io.su

.PHONY: clean-Drivers-2f-BSP-2f-STM32H747I_EVAL

