mod audio;
mod buttons;
mod config;
mod conversation;
mod deepgram_stt;
mod deepgram_tts;
mod groq_client;
mod hardware;
mod led;
mod llm;
mod pipeline;
mod prompt;
mod ring_buffer;
#[cfg(feature = "esp32")]
mod secrets;
mod stt;
mod tts;

#[cfg(feature = "esp32")]
fn main() -> anyhow::Result<()> {
    use crate::audio::{mono_to_stereo, peak_amplitude, stereo_to_mono};
    use crate::buttons::{ButtonEvent, VolumeControl};
    use crate::conversation::ConversationHistory;
    use crate::hardware::hw;
    use crate::led::neopixel::{state_to_color, NeoPixel};
    use crate::pipeline::{LedState, Pipeline, PipelineAction, PipelineEvent};
    use crate::ring_buffer::RingBuffer;

    use esp_idf_hal::i2c::I2cConfig;
    use esp_idf_hal::i2s::config::{DataBitWidth, StdConfig};
    use esp_idf_hal::i2s::I2sDriver;
    use esp_idf_hal::prelude::*;
    use esp_idf_hal::rmt::config::TransmitConfig;
    use esp_idf_hal::rmt::TxRmtDriver;

    use std::sync::mpsc;
    use std::sync::{Arc, Mutex};
    use std::thread;
    use std::time::Duration;

    // Init ESP-IDF
    esp_idf_svc::sys::link_patches();
    esp_idf_svc::log::EspLogger::initialize_default();
    log::info!("═══════════════════════════════════════");
    log::info!("  aidoll_v2 — Rust Voice Agent");
    log::info!("  Waveshare ESP32-S3-AUDIO-Board");
    log::info!("═══════════════════════════════════════");

    let peripherals = Peripherals::take()?;

    // ─── LED init ──────────────────────────────────────
    let rmt_config = TransmitConfig::new().clock_divider(1);
    let rmt_tx = TxRmtDriver::new(
        peripherals.rmt.channel0,
        peripherals.pins.gpio38,
        &rmt_config,
    )?;
    let led = Arc::new(Mutex::new(NeoPixel::new(rmt_tx)));
    {
        let mut l = led.lock().unwrap();
        let (r, g, b) = state_to_color(LedState::Connecting);
        let _ = l.set_all(r, g, b);
    }

    // ─── I2C init ──────────────────────────────────────
    let i2c = esp_idf_hal::i2c::I2cDriver::new(
        peripherals.i2c0,
        peripherals.pins.gpio11,
        peripherals.pins.gpio10,
        &I2cConfig::new().baudrate(100.kHz().into()),
    )?;
    let i2c = Arc::new(Mutex::new(i2c));

    // ─── Codec init ────────────────────────────────────
    {
        let mut i2c = i2c.lock().unwrap();
        hw::init_tca9555(&mut i2c)?;
        hw::init_es8311(&mut i2c, 70)?;
        hw::init_es7210(&mut i2c)?;
    }
    log::info!("[INIT] Audio codecs ready");

    // ─── I2S init ──────────────────────────────────────
    let i2s_config = StdConfig::philips(16000, DataBitWidth::Bits16);
    let mut i2s = I2sDriver::new_std_bidir(
        peripherals.i2s0,
        &i2s_config,
        peripherals.pins.gpio13, // BCLK
        peripherals.pins.gpio15, // DIN (mic)
        peripherals.pins.gpio16, // DOUT (speaker)
        Some(peripherals.pins.gpio12), // MCLK
        peripherals.pins.gpio14, // WS/LRCK
    )?;
    i2s.tx_enable()?;
    i2s.rx_enable()?;
    log::info!("[INIT] I2S ready");

    // ─── Playback buffer ───────────────────────────────
    let playback_buffer = RingBuffer::new(16000 * 2 * 30); // 30s

    // ─── WiFi ──────────────────────────────────────────
    let wifi_ssid = secrets::WIFI_SSID;
    let wifi_pass = secrets::WIFI_PASSWORD;
    let _wifi = hw::init_wifi(peripherals.modem, wifi_ssid, wifi_pass)?;

    // ─── Pipeline state ────────────────────────────────
    let pipeline = Arc::new(Mutex::new(Pipeline::new()));
    let mut history = ConversationHistory::new(10);
    let mut volume = VolumeControl::new(70, 10);

    // Load system prompt
    let system_prompt = include_str!("../../harry_potter_doll/laura_prompt.h");

    let dg_key = secrets::DEEPGRAM_API_KEY;
    let groq_key = secrets::GROQ_API_KEY;

    // ─── Deepgram STT ──────────────────────────────────
    let (stt_tx, stt_rx) = mpsc::channel();
    let mut stt_client = deepgram_stt::client::connect(&dg_key, stt_tx)?;

    // ─── Deepgram TTS ──────────────────────────────────
    let (tts_done_tx, tts_done_rx) = mpsc::channel();
    let mut tts_client = deepgram_tts::client::connect(
        &dg_key,
        "aura-2-luna-en",
        playback_buffer.clone(),
        tts_done_tx,
    )?;

    // Update LED to idle
    {
        let mut l = led.lock().unwrap();
        let (r, g, b) = state_to_color(LedState::Idle);
        let _ = l.set_all(r, g, b);
    }
    log::info!("[INIT] Ready! Start talking.");

    // Share I2S driver between mic and speaker threads via Mutex
    let i2s = Arc::new(Mutex::new(i2s));

    // ─── Mic thread ────────────────────────────────────
    let pipeline_mic = pipeline.clone();
    let i2s_mic = i2s.clone();
    thread::Builder::new().stack_size(8192).spawn(move || {
        let mut stereo_buf = [0u8; 1024];
        loop {
            let state = pipeline_mic.lock().unwrap().state();
            // Read from I2S mic
            let read_result = {
                let mut drv = i2s_mic.lock().unwrap();
                drv.read(&mut stereo_buf, 5000)
            };
            match read_result {
                Ok(n) if n > 0 => {
                    let stereo: &[i16] = unsafe {
                        std::slice::from_raw_parts(
                            stereo_buf.as_ptr() as *const i16,
                            n / 2,
                        )
                    };
                    let mono = stereo_to_mono(stereo);
                    let peak = peak_amplitude(&mono);

                    if state == pipeline::PipelineState::Speaking && peak <= 5000 {
                        continue; // filter speaker bleed
                    }

                    let mono_bytes: &[u8] = unsafe {
                        std::slice::from_raw_parts(
                            mono.as_ptr() as *const u8,
                            mono.len() * 2,
                        )
                    };
                    let _ = deepgram_stt::client::send_audio(&mut stt_client, mono_bytes);
                }
                _ => {}
            }
            thread::sleep(Duration::from_millis(20));
        }
    })?;

    // ─── Speaker thread ────────────────────────────────
    let pb = playback_buffer.clone();
    let i2s_spk = i2s.clone();
    thread::Builder::new().stack_size(4096).spawn(move || {
        let silence = [0u8; 512];
        let mut chunk = [0u8; 512];
        loop {
            let available = pb.available();
            if available >= 512 {
                let n = pb.read(&mut chunk);
                let mono: &[i16] = unsafe {
                    std::slice::from_raw_parts(chunk.as_ptr() as *const i16, n / 2)
                };
                let stereo = mono_to_stereo(mono);
                let stereo_bytes: &[u8] = unsafe {
                    std::slice::from_raw_parts(
                        stereo.as_ptr() as *const u8,
                        stereo.len() * 2,
                    )
                };
                let mut drv = i2s_spk.lock().unwrap();
                let _ = drv.write_all(stereo_bytes, 5000);
            } else {
                let mut drv = i2s_spk.lock().unwrap();
                let _ = drv.write_all(&silence, 5000);
            }
            thread::sleep(Duration::from_millis(5));
        }
    })?;

    // ─── Button thread ─────────────────────────────────
    let i2c_btn = i2c.clone();
    let led_btn = led.clone();
    let volume_ctl = Arc::new(Mutex::new(volume));
    let vol_for_btn = volume_ctl.clone();
    let i2c_vol = i2c.clone();
    thread::Builder::new().stack_size(4096).spawn(move || {
        let mut prev = [false; 3]; // KEY1, KEY2, KEY3
        loop {
            let mut i2c = i2c_btn.lock().unwrap();
            let keys = [
                hw::read_button(&mut i2c, 9).unwrap_or(false),
                hw::read_button(&mut i2c, 10).unwrap_or(false),
                hw::read_button(&mut i2c, 11).unwrap_or(false),
            ];
            drop(i2c);

            for (idx, &pressed) in keys.iter().enumerate() {
                if pressed && !prev[idx] {
                    // Rising edge — button just pressed
                    let event = match idx {
                        0 => ButtonEvent::VolumeUp,
                        1 => ButtonEvent::MuteToggle,
                        2 => ButtonEvent::VolumeDown,
                        _ => continue,
                    };

                    let mut vc = vol_for_btn.lock().unwrap();
                    let feedback = vc.handle_button(event);
                    let eff_vol = vc.effective_volume();
                    drop(vc);

                    // Set hardware volume
                    if let Ok(mut i2c) = i2c_vol.lock() {
                        let _ = hw::set_volume(&mut i2c, eff_vol);
                    }

                    // LED flash
                    let color = match feedback {
                        buttons::ButtonFeedback::VolumeUp(_) => (255, 255, 255),
                        buttons::ButtonFeedback::VolumeDown(_) => (60, 60, 60),
                        buttons::ButtonFeedback::Muted => (255, 0, 0),
                        buttons::ButtonFeedback::Unmuted(_) => (0, 255, 0),
                    };
                    if let Ok(mut l) = led_btn.lock() {
                        let _ = l.set_all(color.0, color.1, color.2);
                    }

                    log::info!("[BTN] {:?} → vol={}", feedback, eff_vol);
                }
            }
            prev = keys;
            thread::sleep(Duration::from_millis(50));
        }
    })?;

    // ─── Main pipeline loop ────────────────────────────
    let led_main = led.clone();
    loop {
        // Check for STT events
        while let Ok(stt_event) = stt_rx.try_recv() {
            let pipe_event = match stt_event {
                stt::SttEvent::PartialTranscript(t) => PipelineEvent::PartialTranscript(t),
                stt::SttEvent::FinalTranscript(t) => PipelineEvent::FinalTranscript(t),
                stt::SttEvent::UtteranceEnd => continue,
            };

            let actions = pipeline.lock().unwrap().handle_event(pipe_event);
            for action in actions {
                match action {
                    PipelineAction::SetLedState(state) => {
                        let (r, g, b) = state_to_color(state);
                        if let Ok(mut l) = led_main.lock() {
                            let _ = l.set_all(r, g, b);
                        }
                    }
                    PipelineAction::SendToLlm(text) => {
                        log::info!("[PIPE] LLM: {}", text);
                        history.add("user", &text);

                        // Groq completion (blocking)
                        let response = groq_client::client::complete(
                            &groq_key,
                            "api.groq.com",
                            "meta-llama/llama-4-scout-17b-16e-instruct",
                            80,
                            system_prompt,
                            history.turns(),
                            &text,
                            |token| {
                                let _ = pipeline.lock().unwrap().handle_event(
                                    PipelineEvent::LlmToken(token.to_string()),
                                );
                            },
                        );

                        match response {
                            Ok(full_response) => {
                                log::info!("[PIPE] Response: {}", full_response);
                                history.add("assistant", &full_response);
                                pipeline.lock().unwrap().handle_event(
                                    PipelineEvent::LlmDone(full_response.clone()),
                                );

                                // Send to TTS
                                let _ = deepgram_tts::client::send_text(
                                    &mut tts_client,
                                    &full_response,
                                );
                                let _ = deepgram_tts::client::flush(&mut tts_client);
                            }
                            Err(e) => {
                                log::error!("[PIPE] Groq error: {}", e);
                                pipeline.lock().unwrap().handle_event(PipelineEvent::LlmFailed);
                            }
                        }
                    }
                    PipelineAction::FlushAudioBuffer => {
                        playback_buffer.flush();
                    }
                    PipelineAction::CancelLlm => {
                        // Groq is blocking, can't cancel mid-request
                    }
                    PipelineAction::DisconnectTts => {
                        // TTS will auto-reconnect on next use
                    }
                    _ => {}
                }
            }
        }

        // Check for TTS done
        if let Ok(()) = tts_done_rx.try_recv() {
            let actions = pipeline.lock().unwrap().handle_event(PipelineEvent::TtsDone);
            for action in actions {
                if let PipelineAction::SetLedState(state) = action {
                    let (r, g, b) = state_to_color(state);
                    if let Ok(mut l) = led_main.lock() {
                        let _ = l.set_all(r, g, b);
                    }
                }
            }
        }

        // Check if speaker buffer drained
        if playback_buffer.available() == 0 {
            let actions = pipeline.lock().unwrap().handle_event(PipelineEvent::SpeakerDrained);
            for action in actions {
                if let PipelineAction::SetLedState(state) = action {
                    let (r, g, b) = state_to_color(state);
                    if let Ok(mut l) = led_main.lock() {
                        let _ = l.set_all(r, g, b);
                    }
                }
            }
        }

        thread::sleep(Duration::from_millis(10));
    }
}

#[cfg(not(feature = "esp32"))]
fn main() {
    println!("aidoll_v2 — run with --features esp32 for ESP32 target");
    println!("Run `cargo test --no-default-features --target aarch64-apple-darwin` for tests.");
}
