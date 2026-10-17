// components/bsp/include/bsp_pins.h
// FMO-S3 port: ESP32-S3-DevKitC-1 + 小智面包板 DIY 套件
// (1.59" SPI-LCD + INMP441 + MAX98357A)。
// 引脚以飞书教程《小智AI终端DIY面包板WIFI-LCD彩屏接线教程》2.1 节表格为准。
#pragma once

#include "driver/spi_master.h"
#include "driver/i2c_types.h"
#include "hal/adc_types.h"

// ============================================================================
// 显示:1.59" SPI-LCD,4-line SPI(小智面包板方案)
//
//   ESP32-S3        LCD
//   GPIO21  --->  SCL(SCK)  时钟
//   GPIO47  --->  SDA(MOSI) 数据
//   GPIO45  --->  RES       复位
//   GPIO40  --->  DC        数据/命令
//   GPIO41  --->  CS        片选
//   GPIO42  --->  BLK       背光
//   GND/3V3 --->  GND/VCC
//
// 屏幕分辨率与显存参数由 menuconfig 的 FMO_LCD_* 配置(见 main/Kconfig.projbuild)。
// 默认 240x280(1.59" GC9107/ST7789 兼容);若你的屏是 240x240,改 FMO_LCD_HEIGHT。
// ============================================================================
#define BSP_LCD_W            240
#define BSP_LCD_H            CONFIG_FMO_LCD_HEIGHT
#define BSP_LCD_SPI_HOST     SPI2_HOST
#define BSP_LCD_MOSI         47
#define BSP_LCD_SCLK         21
#define BSP_LCD_CS           41
#define BSP_LCD_DC           40
#define BSP_LCD_RST          45          // 45 是 S3 strapping 脚,作 RES 输出没问题
#define BSP_LCD_BL           42          // 背光,LEDC PWM 调光
#define BSP_LCD_PCLK_HZ      (40 * 1000 * 1000)
// ST7789 系(含 GC9107)SCK 空闲低、上升沿采样 → SPI mode 0。
#define BSP_LCD_SPI_MODE     0
// 反色/镜像/偏移统一走 menuconfig,换屏无需改代码。
#if defined(CONFIG_FMO_LCD_INVERT)
#define BSP_LCD_INVERT_COLOR 1
#else
#define BSP_LCD_INVERT_COLOR 0
#endif
#if defined(CONFIG_FMO_LCD_MIRROR_X)
#define BSP_LCD_MIRROR_X     1
#else
#define BSP_LCD_MIRROR_X     0
#endif
#if defined(CONFIG_FMO_LCD_MIRROR_Y)
#define BSP_LCD_MIRROR_Y     1
#else
#define BSP_LCD_MIRROR_Y     0
#endif
#define BSP_LCD_GAP_X        CONFIG_FMO_LCD_GAP_X
#define BSP_LCD_GAP_Y        CONFIG_FMO_LCD_GAP_Y

// 背光 LEDC 参数
#define BSP_BL_LEDC_TIMER    LEDC_TIMER_0
#define BSP_BL_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define BSP_BL_LEDC_CHANNEL  LEDC_CHANNEL_0
#define BSP_BL_LEDC_RES      LEDC_TIMER_10_BIT
#define BSP_BL_LEDC_FREQ_HZ  5000

// ============================================================================
// 按键:单键 = 开发板板载 BOOT 键(GPIO0,按下为低)。
// 语义映射为"确定键"(OK):短按开关音频、长按切换台站/返回、配网页翻页。
// 上/下键(音量调节、长按进网络菜单)在本板上无硬件,相关快捷键不触发。
// ============================================================================
#define BSP_BTN_GPIO          0
#define BSP_BTN_COUNT         1

// ============================================================================
// I2C:小智面包板套件无 I2C 设备。保留总线以维持 main.c 的电池降级逻辑,
// 引脚挪到未占用的 GPIO8/9(与功放 DIN=GPIO7 冲突,故不可再用 7)。
// CW2017 不应答 → bsp_battery 自动降级,UI 电量显示 "--"。
// ============================================================================
#define BSP_I2C_PORT         I2C_NUM_0
#define BSP_I2C_SDA          8
#define BSP_I2C_SCL          9
#define BSP_I2C_ES8311_ADDR  0x18    // 7 位地址(保留,未接线)
#define BSP_I2C_CW2017_ADDR  0x63    // 7 位地址(保留,未接线)

// ============================================================================
// 音频:MAX98357A I2S 数字功放(仅播放,无 MCLK 引脚、无 I2C 控制口)
//
//   ESP32-S3        MAX98357A
//   GPIO7   --->  DIN      数字信号
//   GPIO15  --->  BCLK     位时钟
//   GPIO16  --->  LRC      左右时钟
//   3V3/GND --->  Vin/GND  (SD 悬空或接 3V3 使能;GAIN 接 GND=9dB)
// 软件音量:BSP 内部对 PCM 做衰减(MAX98357A 无音量寄存器)。
// ============================================================================
#define BSP_I2S_PORT         I2S_NUM_0
#define BSP_I2S_BCLK         15
#define BSP_I2S_WS           16
#define BSP_I2S_DOUT         7        // 播放:MCU → MAX98357A DIN
// MAX98357A 无 MCLK;录音(INMP441)不在本固件使用。
#define BSP_I2S_MCLK         (-1)
#define BSP_I2S_DIN          (-1)
#define BSP_I2S_PA_CTRL      (-1)
