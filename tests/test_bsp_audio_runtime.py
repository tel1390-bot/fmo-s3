"""Compile the production BSP audio driver (MAX98357A) against I2S stubs."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HEADER = r'''
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE -2
#define ESP_ERR_NOT_SUPPORTED -3
#define ESP_ERR_TIMEOUT -4
#define BSP_I2S_PORT 0
#define BSP_I2S_MCLK (-1)
#define BSP_I2S_BCLK 15
#define BSP_I2S_WS 16
#define BSP_I2S_DOUT 7
#define BSP_I2S_DIN (-1)
#define BSP_I2S_PA_CTRL (-1)
#define I2S_GPIO_UNUSED (-1)
#define I2S_ROLE_MASTER 1
#define I2S_CLK_SRC_DEFAULT 0
#define I2S_MCLK_MULTIPLE_256 256
#define I2S_DATA_BIT_WIDTH_16BIT 16
#define I2S_SLOT_BIT_WIDTH_AUTO 0
#define I2S_SLOT_MODE_STEREO 2
#define I2S_STD_SLOT_BOTH 3
#define portMAX_DELAY 0xFFFFFFFF
static inline void discard_log(const char *format, ...) {(void)format;}
#define ESP_LOGI(tag,...) ((void)(tag),discard_log(__VA_ARGS__))
#define ESP_LOGE(tag,...) ((void)(tag),discard_log(__VA_ARGS__))
#define ESP_LOGW(tag,...) ((void)(tag),discard_log(__VA_ARGS__))
const char *esp_err_to_name(esp_err_t err);
typedef struct {bool enabled;} channel_t;
typedef channel_t *i2s_chan_handle_t;
typedef struct {int id, role, dma_desc_num, dma_frame_num; bool auto_clear_after_cb, auto_clear_before_cb; int intr_priority;} i2s_chan_config_t;
typedef struct {
    struct {uint32_t sample_rate_hz; int clk_src, ext_clk_freq_hz, mclk_multiple;} clk_cfg;
    struct {int data_bit_width, slot_bit_width, slot_mode, slot_mask, ws_width; bool ws_pol, bit_shift, left_align, big_endian, bit_order_lsb;} slot_cfg;
    struct {int mclk, bclk, ws, dout, din; struct {bool mclk_inv,bclk_inv,ws_inv;} invert_flags;} gpio_cfg;
} i2s_std_config_t;
typedef struct {uint32_t sample_rate_hz; int clk_src, ext_clk_freq_hz, mclk_multiple;} i2s_std_clk_config_t;
esp_err_t i2s_new_channel(const i2s_chan_config_t *,i2s_chan_handle_t *,i2s_chan_handle_t *);
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t,const i2s_std_config_t *);
esp_err_t i2s_channel_enable(i2s_chan_handle_t);
esp_err_t i2s_channel_reconfig_std_clock(i2s_chan_handle_t,const i2s_std_clk_config_t *);
esp_err_t i2s_channel_write(i2s_chan_handle_t,const void *,size_t,size_t *,int);
'''
TEST = r'''
#include "audio_env.h"
#include "bsp_audio.h"
#include <stdio.h>
#include <string.h>
static channel_t tx;
static unsigned allocations, inits, reconfigs, writes;
static uint32_t expected_rate=8000;
static int write_error;
static const int16_t *last_buf;
static size_t last_bytes;
const char *esp_err_to_name(esp_err_t err) {(void)err;return "stub";}
esp_err_t i2s_new_channel(const i2s_chan_config_t *cfg,i2s_chan_handle_t *out,i2s_chan_handle_t *in) {
    assert(cfg->dma_desc_num==6 && cfg->dma_frame_num==240 && cfg->role==I2S_ROLE_MASTER);
    assert(cfg->auto_clear_after_cb && !cfg->auto_clear_before_cb);
    assert(out && in==NULL);++allocations;*out=&tx;return ESP_OK;
}
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t handle,const i2s_std_config_t *cfg) {
    assert(handle==&tx);
    assert(cfg->gpio_cfg.dout==BSP_I2S_DOUT && cfg->gpio_cfg.bclk==BSP_I2S_BCLK && cfg->gpio_cfg.ws==BSP_I2S_WS);
    assert(cfg->gpio_cfg.mclk==I2S_GPIO_UNUSED && cfg->gpio_cfg.din==I2S_GPIO_UNUSED);
    assert(cfg->slot_cfg.slot_mode==I2S_SLOT_MODE_STEREO && cfg->slot_cfg.slot_mask==I2S_STD_SLOT_BOTH);
    assert(cfg->slot_cfg.data_bit_width==I2S_DATA_BIT_WIDTH_16BIT);
    ++inits;return ESP_OK;
}
esp_err_t i2s_channel_enable(i2s_chan_handle_t handle) {assert(handle==&tx);tx.enabled=true;return ESP_OK;}
esp_err_t i2s_channel_reconfig_std_clock(i2s_chan_handle_t handle,const i2s_std_clk_config_t *clk) {
    assert(handle==&tx && clk->sample_rate_hz==expected_rate);++reconfigs;return ESP_OK;
}
esp_err_t i2s_channel_write(i2s_chan_handle_t handle,const void *buf,size_t bytes,size_t *written,int timeout) {
    assert(handle==&tx && timeout==portMAX_DELAY);assert(written);
    last_buf=(const int16_t*)buf;last_bytes=bytes;*written=bytes;++writes;return write_error;
}
int main(void) {
    int16_t pcm[160];for(int i=0;i<160;i++)pcm[i]=(int16_t)((i-80)*100);
    /* 未初始化时所有 I/O 拒绝 */
    assert(bsp_audio_set_format(8000,16,1)==ESP_ERR_INVALID_STATE);
    assert(bsp_audio_read(pcm,sizeof(pcm))==ESP_ERR_INVALID_STATE);
    assert(bsp_audio_write(pcm,sizeof(pcm))==ESP_ERR_INVALID_STATE);
    /* 初始化幂等;只建 TX(本板无录音) */
    assert(bsp_audio_init()==ESP_OK && bsp_audio_init()==ESP_OK && allocations==1 && inits==1);
    assert(bsp_audio_read(pcm,sizeof(pcm))==ESP_ERR_INVALID_STATE);
    /* 格式设置:同格式复用,换采样率重配 */
    assert(bsp_audio_set_format(8000,16,1)==ESP_OK);
    assert(bsp_audio_set_format(8000,16,1)==ESP_OK && reconfigs==1);
    expected_rate=16000;
    assert(bsp_audio_set_format(16000,16,1)==ESP_OK && reconfigs==2);
    expected_rate=8000;
    assert(bsp_audio_set_format(8000,16,1)==ESP_OK && reconfigs==3);
    assert(bsp_audio_set_format(8000,8,1)==ESP_ERR_NOT_SUPPORTED);
    assert(bsp_audio_set_format(8000,16,2)==ESP_ERR_NOT_SUPPORTED);
    /* 满音量:mono 160 → stereo 320 samples(640B),左右同数据 */
    bsp_audio_set_volume(100);
    assert(bsp_audio_write(pcm,sizeof(pcm))==ESP_OK && writes==1);
    assert(last_bytes==160*2*2);
    for(int i=0;i<160;i++){assert(last_buf[i*2]==pcm[i] && last_buf[i*2+1]==pcm[i]);}
    /* 软件音量:50% 衰减 */
    bsp_audio_set_volume(50);
    assert(bsp_audio_write(pcm,sizeof(pcm))==ESP_OK && writes==2);
    assert(last_buf[0]==(int16_t)((pcm[0]*50)/100));
    /* 静音:全 0 */
    bsp_audio_set_volume(0);
    assert(bsp_audio_write(pcm,sizeof(pcm))==ESP_OK && writes==3);
    assert(last_buf[0]==0 && last_buf[1]==0);
    /* 底层写错误透传 */
    write_error=-1;
    assert(bsp_audio_write(pcm,sizeof(pcm))==ESP_FAIL);
    assert(bsp_audio_read(pcm,sizeof(pcm))==ESP_ERR_INVALID_STATE);
    printf("BSP audio (MAX98357A): PASS (I2S out, format reopen, stereo expand, software volume)\n");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="bsp-audio-test-") as tmp:
    directory = Path(tmp)
    (directory / "audio_env.h").write_text(HEADER)
    for name in ("esp_err.h", "esp_log.h", "bsp_i2c.h", "bsp_pins.h",
                 "driver/i2s_std.h", "freertos/FreeRTOS.h", "freertos/task.h"):
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "audio_env.h"\n')
    (directory / "test.c").write_text(TEST)
    binary = directory / "bsp-audio-test"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(directory), "-I" + str(ROOT / "components/bsp/include"),
                    str(ROOT / "components/bsp/src/bsp_audio.c"), str(directory / "test.c"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=10)
