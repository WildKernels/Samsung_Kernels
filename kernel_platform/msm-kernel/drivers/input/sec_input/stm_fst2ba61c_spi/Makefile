TARGET_I2C = fst2ba61c_i2c
TARGET_SPI = fst2ba61c_spi

$(TARGET_I2C)-objs := stm_i2c.o stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o
$(TARGET_SPI)-objs := stm_spi.o stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o stm_ioctl.o

ifneq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
  $(TARGET_I2C)-objs += stm_vendor.o
  $(TARGET_SPI)-objs += stm_vendor.o
endif

obj-$(CONFIG_TOUCHSCREEN_STM_FST2BA61C_I2C) += $(TARGET_I2C).o
obj-$(CONFIG_TOUCHSCREEN_STM_FST2BA61C_SPI) += $(TARGET_SPI).o

ccflags-y += -Wformat

