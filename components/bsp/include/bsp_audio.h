// components/bsp/include/bsp_audio.h
// FMO-S3 port: MAX98357A I2S 数字功放(仅播放)。
// 无 I2C 控制口、无 codec 驱动;音量由 BSP 内部做 PCM 软件衰减。
// 头文件接口与 C3/ES8311 版保持兼容,应用层(fmo_audio.c)无需改动。
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

// 初始化 I2S 输出(MAX98357A)。本板无录音能力,全双工入口降级为仅播放。
// 由单一音频 worker 串行调用,可能阻塞,不得在按键回调或 LVGL 任务中调用。
// 重复调用幂等;初始化失败返回底层错误。
esp_err_t bsp_audio_init(void);

// 仅播放:本板默认路径,等价于 bsp_audio_init()。先初始化,再 set_format,最后 PCM I/O。
esp_err_t bsp_audio_init_playback(void);

// 设置采样格式。同格式重复调用是廉价的(不触发重配)。
// 仅支持 16bit 单声道;换采样率会重配 I2S 时钟。格式变化由本函数处理,
// 不存在 ES8311 版"codec 已打开不重配采样率"的坑。
esp_err_t bsp_audio_set_format(uint32_t hz, uint8_t bits, uint8_t ch);

// 播放 / 录音。bytes 为字节数(16bit 单声道时 = 采样数 x 2)。
// 阻塞操作,仅用于音频 worker;本板 read 恒返回 ESP_ERR_INVALID_STATE(无录音)。
esp_err_t bsp_audio_write(const void *pcm, size_t bytes);
esp_err_t bsp_audio_read(void *pcm, size_t bytes);

// 输出音量 0..100(%),软件 PCM 衰减。
void bsp_audio_set_volume(uint8_t percent);
