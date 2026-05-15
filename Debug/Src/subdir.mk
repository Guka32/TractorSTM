################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/EngTrModel.c \
../Src/EngTrModel_data.c \
../Src/lcd.c \
../Src/main_uart.c \
../Src/rtGetInf.c \
../Src/rtGetNaN.c \
../Src/rt_nonfinite.c \
../Src/syscall.c \
../Src/sysmem.c \
../Src/user_adc.c \
../Src/user_motor.c \
../Src/user_pwm.c \
../Src/user_timer.c \
../Src/user_uart.c 

OBJS += \
./Src/EngTrModel.o \
./Src/EngTrModel_data.o \
./Src/lcd.o \
./Src/main_uart.o \
./Src/rtGetInf.o \
./Src/rtGetNaN.o \
./Src/rt_nonfinite.o \
./Src/syscall.o \
./Src/sysmem.o \
./Src/user_adc.o \
./Src/user_motor.o \
./Src/user_pwm.o \
./Src/user_timer.o \
./Src/user_uart.o 

C_DEPS += \
./Src/EngTrModel.d \
./Src/EngTrModel_data.d \
./Src/lcd.d \
./Src/main_uart.d \
./Src/rtGetInf.d \
./Src/rtGetNaN.d \
./Src/rt_nonfinite.d \
./Src/syscall.d \
./Src/sysmem.d \
./Src/user_adc.d \
./Src/user_motor.d \
./Src/user_pwm.d \
./Src/user_timer.d \
./Src/user_uart.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o Src/%.su Src/%.cyclo: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DSTM32F103RBTx -DSTM32 -DSTM32F1 -c -I../Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Src

clean-Src:
	-$(RM) ./Src/EngTrModel.cyclo ./Src/EngTrModel.d ./Src/EngTrModel.o ./Src/EngTrModel.su ./Src/EngTrModel_data.cyclo ./Src/EngTrModel_data.d ./Src/EngTrModel_data.o ./Src/EngTrModel_data.su ./Src/lcd.cyclo ./Src/lcd.d ./Src/lcd.o ./Src/lcd.su ./Src/main_uart.cyclo ./Src/main_uart.d ./Src/main_uart.o ./Src/main_uart.su ./Src/rtGetInf.cyclo ./Src/rtGetInf.d ./Src/rtGetInf.o ./Src/rtGetInf.su ./Src/rtGetNaN.cyclo ./Src/rtGetNaN.d ./Src/rtGetNaN.o ./Src/rtGetNaN.su ./Src/rt_nonfinite.cyclo ./Src/rt_nonfinite.d ./Src/rt_nonfinite.o ./Src/rt_nonfinite.su ./Src/syscall.cyclo ./Src/syscall.d ./Src/syscall.o ./Src/syscall.su ./Src/sysmem.cyclo ./Src/sysmem.d ./Src/sysmem.o ./Src/sysmem.su ./Src/user_adc.cyclo ./Src/user_adc.d ./Src/user_adc.o ./Src/user_adc.su ./Src/user_motor.cyclo ./Src/user_motor.d ./Src/user_motor.o ./Src/user_motor.su ./Src/user_pwm.cyclo ./Src/user_pwm.d ./Src/user_pwm.o ./Src/user_pwm.su ./Src/user_timer.cyclo ./Src/user_timer.d ./Src/user_timer.o ./Src/user_timer.su ./Src/user_uart.cyclo ./Src/user_uart.d ./Src/user_uart.o ./Src/user_uart.su

.PHONY: clean-Src

