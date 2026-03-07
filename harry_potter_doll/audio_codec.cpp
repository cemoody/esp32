#include "audio_codec.h"
#include "config.h"

#include <Wire.h>
#include "driver/i2s_std.h"
#include "es8311.h"
#include "es7210.h"
#include "TCA9555.h"

static es8311_handle_t es8311_handle = nullptr;
static es7210_dev_handle_t es7210_handle = nullptr;
static TCA9555 tca(TCA9555_ADDR);

static i2s_chan_handle_t tx_handle = nullptr;
static i2s_chan_handle_t rx_handle = nullptr;

static bool init_i2c() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
  return true;
}

static bool init_tca9555() {
  tca.begin(OUTPUT);
  // Enable PA amp
  tca.write1(PA_AMP_PIN, HIGH);
  delay(50);
  return true;
}

static bool init_es8311() {
  es8311_handle = es8311_create(I2C_NUM_0, ES8311_ADDR);
  if (!es8311_handle) {
    Serial.println("[CODEC] ES8311 create failed");
    return false;
  }

  es8311_clock_config_t clk = {};
  clk.mclk_inverted = false;
  clk.sclk_inverted = false;
  clk.mclk_from_mclk_pin = true;
  clk.mclk_frequency = AUDIO_SAMPLE_RATE * AUDIO_MCLK_MULTIPLE;
  clk.sample_frequency = AUDIO_SAMPLE_RATE;

  esp_err_t err = es8311_init(es8311_handle, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
  if (err != ESP_OK) {
    Serial.printf("[CODEC] ES8311 init failed: %d\n", err);
    return false;
  }

  es8311_voice_volume_set(es8311_handle, SPEAKER_VOLUME, nullptr);
  es8311_microphone_config(es8311_handle, false);

  return true;
}

static bool init_es7210() {
  es7210_i2c_config_t i2c_conf = {};
  i2c_conf.i2c_port = I2C_NUM_0;
  i2c_conf.i2c_addr = ES7210_ADDR;

  esp_err_t err = es7210_new_codec(&i2c_conf, &es7210_handle);
  if (err != ESP_OK) {
    Serial.printf("[CODEC] ES7210 create failed: %d\n", err);
    return false;
  }

  es7210_codec_config_t codec_conf = {};
  codec_conf.sample_rate_hz = AUDIO_SAMPLE_RATE;
  codec_conf.mclk_ratio = AUDIO_MCLK_MULTIPLE;
  codec_conf.i2s_format = ES7210_I2S_FMT_I2S;
  codec_conf.bit_width = ES7210_I2S_BITS_16B;
  codec_conf.mic_bias = ES7210_MIC_BIAS_2V87;
  codec_conf.mic_gain = (es7210_mic_gain_t)MIC_GAIN;
  codec_conf.flags.tdm_enable = true;

  err = es7210_config_codec(es7210_handle, &codec_conf);
  if (err != ESP_OK) {
    Serial.printf("[CODEC] ES7210 config failed: %d\n", err);
    return false;
  }

  es7210_config_volume(es7210_handle, 0);
  return true;
}

static bool init_i2s() {
  // Create I2S channel pair (TX for speaker, RX for mic)
  i2s_chan_config_t chan_cfg = {};
  chan_cfg.id = I2S_NUM_0;
  chan_cfg.role = I2S_ROLE_MASTER;
  chan_cfg.dma_desc_num = 6;
  chan_cfg.dma_frame_num = 240;
  chan_cfg.auto_clear_after_cb = true;
  chan_cfg.auto_clear_before_cb = false;
  chan_cfg.intr_priority = 0;

  esp_err_t err = i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);
  if (err != ESP_OK) {
    Serial.printf("[I2S] Channel create failed: %d\n", err);
    return false;
  }

  // Configure standard mode for both TX and RX
  i2s_std_config_t std_cfg = {};
  std_cfg.clk_cfg.sample_rate_hz = AUDIO_SAMPLE_RATE;
  std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
  std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

  std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
  std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
  std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
  std_cfg.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_16BIT;
  std_cfg.slot_cfg.ws_pol = false;
  std_cfg.slot_cfg.bit_shift = true;
  std_cfg.slot_cfg.left_align = true;
  std_cfg.slot_cfg.big_endian = false;
  std_cfg.slot_cfg.bit_order_lsb = false;

  std_cfg.gpio_cfg.mclk = (gpio_num_t)I2S_MCLK_PIN;
  std_cfg.gpio_cfg.bclk = (gpio_num_t)I2S_SCLK_PIN;
  std_cfg.gpio_cfg.ws = (gpio_num_t)I2S_LRCK_PIN;
  std_cfg.gpio_cfg.dout = (gpio_num_t)I2S_DOUT_PIN;
  std_cfg.gpio_cfg.din = (gpio_num_t)I2S_DSIN_PIN;
  std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
  std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
  std_cfg.gpio_cfg.invert_flags.ws_inv = false;

  err = i2s_channel_init_std_mode(tx_handle, &std_cfg);
  if (err != ESP_OK) {
    Serial.printf("[I2S] TX init failed: %d\n", err);
    return false;
  }

  err = i2s_channel_init_std_mode(rx_handle, &std_cfg);
  if (err != ESP_OK) {
    Serial.printf("[I2S] RX init failed: %d\n", err);
    return false;
  }

  err = i2s_channel_enable(tx_handle);
  if (err != ESP_OK) {
    Serial.printf("[I2S] TX enable failed: %d\n", err);
    return false;
  }

  err = i2s_channel_enable(rx_handle);
  if (err != ESP_OK) {
    Serial.printf("[I2S] RX enable failed: %d\n", err);
    return false;
  }

  return true;
}

bool audio_codec_init() {
  Serial.println("[CODEC] Initializing audio...");

  if (!init_i2c()) {
    Serial.println("[CODEC] I2C init failed");
    return false;
  }
  Serial.println("[CODEC] I2C OK");

  if (!init_tca9555()) {
    Serial.println("[CODEC] TCA9555 init failed");
    return false;
  }
  Serial.println("[CODEC] TCA9555 OK, PA enabled");

  if (!init_es8311()) {
    Serial.println("[CODEC] ES8311 init failed");
    return false;
  }
  Serial.println("[CODEC] ES8311 OK (speaker)");

  if (!init_es7210()) {
    Serial.println("[CODEC] ES7210 init failed");
    return false;
  }
  Serial.println("[CODEC] ES7210 OK (mic)");

  if (!init_i2s()) {
    Serial.println("[CODEC] I2S init failed");
    return false;
  }
  Serial.println("[CODEC] I2S OK");

  return true;
}

size_t audio_speaker_write(const uint8_t* data, size_t len) {
  if (!tx_handle || !data || len == 0) return 0;

  size_t bytes_written = 0;
  // Convert mono to stereo for I2S (duplicate each sample)
  // Input: mono 16-bit, Output: stereo 16-bit
  const int16_t* mono = (const int16_t*)data;
  size_t samples = len / 2;

  // Process in chunks to avoid large stack allocation
  const size_t chunk_samples = 128;
  int16_t stereo[chunk_samples * 2];
  size_t total_written = 0;

  for (size_t i = 0; i < samples; i += chunk_samples) {
    size_t n = (samples - i < chunk_samples) ? (samples - i) : chunk_samples;
    for (size_t j = 0; j < n; j++) {
      stereo[j * 2] = mono[i + j];
      stereo[j * 2 + 1] = mono[i + j];
    }
    size_t written = 0;
    i2s_channel_write(tx_handle, stereo, n * 4, &written, pdMS_TO_TICKS(100));
    total_written += written / 2;  // Report mono byte equivalent
  }

  return total_written;
}

size_t audio_mic_read(uint8_t* data, size_t len) {
  if (!rx_handle || !data || len == 0) return 0;

  size_t bytes_read = 0;
  esp_err_t err = i2s_channel_read(rx_handle, data, len, &bytes_read, pdMS_TO_TICKS(100));
  if (err != ESP_OK) return 0;
  return bytes_read;
}

void audio_set_volume(uint8_t volume) {
  if (volume > 100) volume = 100;
  if (es8311_handle) {
    es8311_voice_volume_set(es8311_handle, volume, nullptr);
  }
}

void audio_pa_enable(bool enable) {
  tca.write1(PA_AMP_PIN, enable ? HIGH : LOW);
}
