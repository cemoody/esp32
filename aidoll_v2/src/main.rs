mod audio;
mod buttons;
mod config;
mod conversation;
mod llm;
mod pipeline;
mod prompt;
mod stt;
mod tts;

#[cfg(feature = "esp32")]
fn main() -> anyhow::Result<()> {
    use esp_idf_svc::hal::prelude::Peripherals;
    use esp_idf_svc::log::EspLogger;

    esp_idf_svc::sys::link_patches();
    EspLogger::initialize_default();

    log::info!("aidoll_v2 starting...");

    let _peripherals = Peripherals::take()?;

    log::info!("aidoll_v2 initialized. TODO: wire up WiFi, audio, and pipeline.");

    loop {
        std::thread::sleep(std::time::Duration::from_secs(1));
    }
}

#[cfg(not(feature = "esp32"))]
fn main() {
    println!("aidoll_v2 — run with --features esp32 for ESP32 target");
    println!("Run `cargo test --no-default-features` to run tests on host.");
}
