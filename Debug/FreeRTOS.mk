################################################################################
# Automatically-generated file. Do not edit!
# FreeRTOS kernel source files
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add FreeRTOS core source files
C_SRCS += \
../tasks.c \
../list.c \
../queue.c \
../timers.c \
../portable/GCC/ARM_CM3/port.c \
../portable/MemMang/heap_4.c

OBJS += \
./tasks.o \
./list.o \
./queue.o \
./timers.o \
./port.o \
./heap_4.o

C_DEPS += \
./tasks.d \
./list.d \
./queue.d \
./timers.d \
./port.d \
./heap_4.d

# FreeRTOS kernel objects compilation rules
%.o: ../%.c
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DSTM32F103RBTx -DSTM32 -DSTM32F1 -c -I../Inc -I../include -I../portable/GCC/ARM_CM3 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

port.o: ../portable/GCC/ARM_CM3/port.c
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DSTM32F103RBTx -DSTM32 -DSTM32F1 -c -I../Inc -I../include -I../portable/GCC/ARM_CM3 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

heap_4.o: ../portable/MemMang/heap_4.c
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DSTM32F103RBTx -DSTM32 -DSTM32F1 -c -I../Inc -I../include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-FreeRTOS

clean-FreeRTOS:
	-$(RM) ./tasks.o ./list.o ./queue.o ./timers.o ./port.o ./heap_4.o
	-$(RM) ./tasks.d ./list.d ./queue.d ./timers.d ./port.d ./heap_4.d
	-$(RM) ./tasks.su ./list.su ./queue.su ./timers.su ./port.su ./heap_4.su
	-$(RM) ./tasks.cyclo ./list.cyclo ./queue.cyclo ./timers.cyclo ./port.cyclo ./heap_4.cyclo

.PHONY: clean-FreeRTOS

