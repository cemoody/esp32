mod audio;
mod config;
mod conversation;
mod llm;
mod pipeline;
mod prompt;
mod stt;
mod tts;

use esp_idf_svc::hal::prelude::Peripherals;
use esp_idf_svc::log::EspLogger;

fn main() -> anyhow::Result<()> {
    // Initialize ESP-IDF
    esp_idf_svc::sys::link_patches();
    EspLogger::initialize_default();

    log::info!("aidoll_v2 starting...");

    let _peripherals = Peripherals::take()?;

    log::info!("aidoll_v2 initialized. TODO: wire up WiFi, audio, and pipeline.");

    // Keep main alive
    loop {
        std::thread::sleep(std::time::Duration::from_secs(1));
    }
}
