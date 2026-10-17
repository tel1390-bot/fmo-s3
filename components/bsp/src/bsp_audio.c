// components/bsp/src/bsp_audio.c
// FMO-S3 port: MAX98357A I2S 数字功放(仅播放)。
// MAX98357A 无 MCLK 引脚、无 I2C 控制口:
//   - 音量用软件衰减(PCM 乘以 0..100 系数);
//   - 应用层给 16bit mono,内部展开为 stereo 左右同数据(I2S 标准 64fs 帧)。
// 头文件接口保持与 C3/ES8311 版一致,应用层(fmo_audio.c)无需修改。
#include "bsp_audio.h"
#include "bsp_pins.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stddef.h>
#include <stdint.h>

static const char *TAG = "bsp_audio";

#define BSP_AUDIO_CHUNK_SAMPLES 160   // 与 FMO_PCM_CHUNK_SAMPLES 相同的块上限

static i2s_chan_handle_t s_tx;
static bool s_init_attempted;
static bool s_opened;
static uint32_t s_hz;
static uint8_t  s_bits, s_ch;
static volatile uint8_t s_volume = 100;   // 0..100,软件衰减
static int16_t s_stereo[BSP_AUDIO_CHUNK_SAMPLES * 2];   // 单音频 worker 专用,非重入

static esp_err_t i2s_init(void)
{
    i2s_chan_config_t chan = {
        .id = BSP_I2S_PORT,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    esp_err_t e = i2s_new_channel(&chan, &s_tx, NULL);
    if (e != ESP_OK) { ESP_LOGE(TAG, "i2s_new_channel 失败: %s", esp_err_to_name(e)); return e; }

    // MAX98357A:I2S 标准格式,STEREO 双 slot(左右同数据),16bit。
    // BCLK = 2 x fs x 16,标准 64fs 帧,芯片无条件兼容。
    i2s_std_config_t std = {
        .clk_cfg = {
            .sample_rate_hz = 16000,          // 实际速率由 set_format 重配
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false,
        },
        .gpio_cfg = {
            .mclk = BSP_I2S_MCLK, .bclk = BSP_I2S_BCLK, .ws = BSP_I2S_WS,
            .dout = BSP_I2S_DOUT, .din = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    if ((e = i2s_channel_init_std_mode(s_tx, &std)) != ESP_OK) {
        ESP_LOGE(TAG, "i2s tx 初始化失败: %s", esp_err_to_name(e)); return e;
    }
    // 先 enable 一次,让后续 set_format 里的 disable/re-enable 序列合法。
    i2s_channel_enable(s_tx);
    return ESP_OK;
}

esp_err_t bsp_audio_init_playback(void)
{
    if (s_init_attempted) return s_tx ? ESP_OK : ESP_FAIL;
    s_init_attempted = true;
    esp_err_t e = i2s_init();
    if (e != ESP_OK) ESP_LOGE(TAG, "MAX98357A I2S 初始化失败: %s", esp_err_to_name(e));
    return e;
}

// 本板无录音 codec,全双工入口降级为仅播放。
esp_err_t bsp_audio_init(void)
{
    return bsp_audio_init_playback();
}

esp_err_t bsp_audio_set_format(uint32_t hz, uint8_t bits, uint8_t ch)
{
    if (!s_tx) return ESP_ERR_INVALID_STATE;
    if (s_opened && s_hz == hz && s_bits == bits && s_ch == ch) return ESP_OK;
    if (bits != 16 || ch != 1) {
        ESP_LOGW(TAG, "仅支持 16bit mono,收到 %ubit/%uch", bits, ch);
        return ESP_ERR_NOT_SUPPORTED;
    }
    i2s_std_clk_config_t clk = {
        .sample_rate_hz = hz,
        .clk_src = I2S_CLK_SRC_DEFAULT,
        .ext_clk_freq_hz = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    // reconfig 内部会 disable → 改时钟 → enable,无需手动停启。
    esp_err_t e = i2s_channel_reconfig_std_clock(s_tx, &clk);
    if (e != ESP_OK) { ESP_LOGE(TAG, "重配采样率失败: %s", esp_err_to_name(e)); return e; }
    s_opened = true; s_hz = hz; s_bits = bits; s_ch = ch;
    ESP_LOGI(TAG, "MAX98357A 就绪 %luHz/16bit/mono", (unsigned long)hz);
    return ESP_OK;
}

esp_err_t bsp_audio_write(const void *pcm, size_t bytes)
{
    if (!s_tx || !s_opened) return ESP_ERR_INVALID_STATE;
    const int16_t *in = (const int16_t *)pcm;
    size_t remaining = bytes / 2;          // 16bit mono → 采样数
    uint32_t vol = s_volume;
    while (remaining) {
        size_t n = remaining > BSP_AUDIO_CHUNK_SAMPLES ? BSP_AUDIO_CHUNK_SAMPLES : remaining;
        for (size_t i = 0; i < n; i++) {
            int32_t s = in[i];
            if (vol < 100) s = (s * (int32_t)vol) / 100;   // 软件音量
            if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
            s_stereo[i * 2]     = (int16_t)s;   // 左声道
            s_stereo[i * 2 + 1] = (int16_t)s;   // 右声道(同数据)
        }
        size_t written = 0;
        esp_err_t e = i2s_channel_write(s_tx, s_stereo, n * 4, &written, portMAX_DELAY);
        if (e != ESP_OK) return e;
        if (written < n * 4) return ESP_ERR_TIMEOUT;   // 正常不会发生(阻塞写)
        in += n; remaining -= n;
    }
    return ESP_OK;
}

// 仅播放:录音返回 ESP_ERR_INVALID_STATE(与应用层约定一致)。
esp_err_t bsp_audio_read(void *pcm, size_t bytes)
{
    (void)pcm; (void)bytes;
    return ESP_ERR_INVALID_STATE;
}

void bsp_audio_set_volume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    s_volume = percent;
}
