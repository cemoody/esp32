#[cfg(feature = "esp32")]
pub mod hw {
    use anyhow::Result;
    use esp_idf_hal::i2c::{I2cConfig, I2cDriver};
    use esp_idf_hal::peripheral::Peripheral;
    use esp_idf_hal::prelude::*;
    use esp_idf_svc::eventloop::EspSystemEventLoop;
    use esp_idf_svc::nvs::EspDefaultNvsPartition;
    use esp_idf_svc::wifi::{BlockingWifi, EspWifi};
    use embedded_svc::wifi::{AuthMethod, ClientConfiguration, Configuration};

    /// Connect to WiFi (blocking until connected)
    pub fn init_wifi(
        modem: impl Peripheral<P = esp_idf_svc::hal::modem::Modem> + 'static,
        ssid: &str,
        password: &str,
    ) -> Result<BlockingWifi<EspWifi<'static>>> {
        let sys_loop = EspSystemEventLoop::take()?;
        let nvs = EspDefaultNvsPartition::take()?;

        let mut wifi = BlockingWifi::wrap(
            EspWifi::new(modem, sys_loop.clone(), Some(nvs))?,
            sys_loop,
        )?;

        let config = Configuration::Client(ClientConfiguration {
            ssid: ssid.try_into().unwrap(),
            password: password.try_into().unwrap(),
            auth_method: AuthMethod::None, // Auto-detect
            ..Default::default()
        });

        wifi.set_configuration(&config)?;
        wifi.start()?;
        log::info!("[WIFI] Connecting to {}...", ssid);

        for attempt in 1..=5 {
            match wifi.connect() {
                Ok(_) => break,
                Err(e) => {
                    log::warn!("[WIFI] Attempt {} failed: {}", attempt, e);
                    if attempt == 5 {
                        anyhow::bail!("WiFi connection failed after 5 attempts");
                    }
                    std::thread::sleep(std::time::Duration::from_secs(2));
                }
            }
        }
        wifi.wait_netif_up()?;
        log::info!("[WIFI] Connected!");
        Ok(wifi)
    }

    // ─── TCA9555 IO Expander ─────────────────────────────

    const TCA9555_ADDR: u8 = 0x20;
    const TCA9555_INPUT_PORT0: u8 = 0x00;
    const TCA9555_INPUT_PORT1: u8 = 0x01;
    const TCA9555_OUTPUT_PORT0: u8 = 0x02;
    const TCA9555_OUTPUT_PORT1: u8 = 0x03;
    const TCA9555_CONFIG_PORT0: u8 = 0x06;
    const TCA9555_CONFIG_PORT1: u8 = 0x07;

    /// Configure TCA9555: pin 8 as output (PA amp), pins 9-11 as input (buttons)
    pub fn init_tca9555(i2c: &mut I2cDriver) -> Result<()> {
        // Port 1 config: bit 0 (pin 8) = output (0), bits 1-3 (pins 9-11) = input (1)
        // 0b00001110 = 0x0E
        i2c.write(TCA9555_ADDR, &[TCA9555_CONFIG_PORT1, 0x0E], 1000)?;
        // Port 0: all outputs (for other IO)
        i2c.write(TCA9555_ADDR, &[TCA9555_CONFIG_PORT0, 0x00], 1000)?;
        // Enable PA amp (pin 8 = port1 bit 0 = HIGH)
        i2c.write(TCA9555_ADDR, &[TCA9555_OUTPUT_PORT1, 0x01], 1000)?;
        log::info!("[HW] TCA9555 OK, PA enabled");
        Ok(())
    }

    /// Read button state from TCA9555 port 1
    /// Returns true if button is pressed (active low)
    pub fn read_button(i2c: &mut I2cDriver, pin: u8) -> Result<bool> {
        let mut buf = [0u8];
        i2c.write_read(TCA9555_ADDR, &[TCA9555_INPUT_PORT1], &mut buf, 1000)?;
        let bit = pin - 8; // pins 8-15 are on port 1
        Ok((buf[0] >> bit) & 1 == 0) // active low
    }

    // ─── ES8311 Speaker Codec ────────────────────────────

    const ES8311_ADDR: u8 = 0x18;

    pub fn init_es8311(i2c: &mut I2cDriver, volume: u8) -> Result<()> {
        // Reset
        i2c.write(ES8311_ADDR, &[0x00, 0x1F], 1000)?;
        std::thread::sleep(std::time::Duration::from_millis(20));
        i2c.write(ES8311_ADDR, &[0x00, 0x00], 1000)?;

        // Clock config for 16kHz, MCLK=256*fs
        i2c.write(ES8311_ADDR, &[0x01, 0x30], 1000)?; // CLK manager 1
        i2c.write(ES8311_ADDR, &[0x02, 0x10], 1000)?; // CLK manager 2
        i2c.write(ES8311_ADDR, &[0x03, 0x10], 1000)?; // CLK manager 3
        i2c.write(ES8311_ADDR, &[0x16, 0x24], 1000)?; // CLK manager 4
        i2c.write(ES8311_ADDR, &[0x04, 0x10], 1000)?; // CLK manager 5
        i2c.write(ES8311_ADDR, &[0x05, 0x00], 1000)?; // CLK manager 6

        // I2S format: 16-bit, standard I2S
        i2c.write(ES8311_ADDR, &[0x09, 0x0C], 1000)?; // SDP In
        i2c.write(ES8311_ADDR, &[0x0A, 0x0C], 1000)?; // SDP Out

        // DAC config
        i2c.write(ES8311_ADDR, &[0x0C, 0x00], 1000)?; // System
        i2c.write(ES8311_ADDR, &[0x0D, 0x01], 1000)?; // System
        i2c.write(ES8311_ADDR, &[0x0E, 0x02], 1000)?; // System
        i2c.write(ES8311_ADDR, &[0x0F, 0x44], 1000)?; // System
        i2c.write(ES8311_ADDR, &[0x10, 0x08], 1000)?; // System
        i2c.write(ES8311_ADDR, &[0x11, 0x00], 1000)?; // System

        // Volume (0x00 = -95.5dB, 0xBF = 0dB, 0xFF = +32dB)
        let vol_reg = ((volume as u16) * 255 / 100) as u8;
        i2c.write(ES8311_ADDR, &[0x32, vol_reg], 1000)?; // DAC volume

        // Power up
        i2c.write(ES8311_ADDR, &[0x13, 0x10], 1000)?; // ADC/DAC power
        i2c.write(ES8311_ADDR, &[0x14, 0x1A], 1000)?; // Analog power

        log::info!("[HW] ES8311 OK (speaker), volume={}", volume);
        Ok(())
    }

    pub fn set_volume(i2c: &mut I2cDriver, volume: u8) -> Result<()> {
        let vol = volume.min(100);
        let vol_reg = ((vol as u16) * 255 / 100) as u8;
        i2c.write(ES8311_ADDR, &[0x32, vol_reg], 1000)?;
        Ok(())
    }

    // ─── ES7210 Microphone Codec ─────────────────────────

    const ES7210_ADDR: u8 = 0x40;

    pub fn init_es7210(i2c: &mut I2cDriver) -> Result<()> {
        // Software reset
        i2c.write(ES7210_ADDR, &[0x00, 0xFF], 1000)?;
        std::thread::sleep(std::time::Duration::from_millis(10));
        i2c.write(ES7210_ADDR, &[0x00, 0x32], 1000)?;

        // Init timing
        i2c.write(ES7210_ADDR, &[0x09, 0x30], 1000)?;
        i2c.write(ES7210_ADDR, &[0x0A, 0x30], 1000)?;

        // HPF config
        i2c.write(ES7210_ADDR, &[0x23, 0x2A], 1000)?;
        i2c.write(ES7210_ADDR, &[0x22, 0x0A], 1000)?;
        i2c.write(ES7210_ADDR, &[0x21, 0x2A], 1000)?;
        i2c.write(ES7210_ADDR, &[0x20, 0x0A], 1000)?;

        // I2S format + TDM enable
        i2c.write(ES7210_ADDR, &[0x11, 0x60], 1000)?; // 16-bit I2S
        i2c.write(ES7210_ADDR, &[0x12, 0x02], 1000)?; // TDM enable

        // Clock: 16kHz with 256x MCLK
        i2c.write(ES7210_ADDR, &[0x07, 0x20], 1000)?; // OSR
        i2c.write(ES7210_ADDR, &[0x02, 0x01], 1000)?; // Main clock
        i2c.write(ES7210_ADDR, &[0x04, 0x01], 1000)?; // LRCK high
        i2c.write(ES7210_ADDR, &[0x05, 0x00], 1000)?; // LRCK low

        // Analog power
        i2c.write(ES7210_ADDR, &[0x40, 0xC3], 1000)?;

        // Mic bias 2.87V
        i2c.write(ES7210_ADDR, &[0x41, 0x70], 1000)?;
        i2c.write(ES7210_ADDR, &[0x42, 0x70], 1000)?;

        // Mic gain 37.5dB (0x1E = 14 | 0x10 enable)
        i2c.write(ES7210_ADDR, &[0x43, 0x1E], 1000)?;
        i2c.write(ES7210_ADDR, &[0x44, 0x1E], 1000)?;
        i2c.write(ES7210_ADDR, &[0x45, 0x1E], 1000)?;
        i2c.write(ES7210_ADDR, &[0x46, 0x1E], 1000)?;

        // Power on mics
        i2c.write(ES7210_ADDR, &[0x47, 0x08], 1000)?;
        i2c.write(ES7210_ADDR, &[0x48, 0x08], 1000)?;
        i2c.write(ES7210_ADDR, &[0x49, 0x08], 1000)?;
        i2c.write(ES7210_ADDR, &[0x4A, 0x08], 1000)?;

        // Power down DLL
        i2c.write(ES7210_ADDR, &[0x06, 0x04], 1000)?;

        // Power on MIC bias & ADC & PGA
        i2c.write(ES7210_ADDR, &[0x4B, 0x0F], 1000)?;
        i2c.write(ES7210_ADDR, &[0x4C, 0x0F], 1000)?;

        // Enable
        i2c.write(ES7210_ADDR, &[0x00, 0x71], 1000)?;
        i2c.write(ES7210_ADDR, &[0x00, 0x41], 1000)?;

        // Volume 0dB
        i2c.write(ES7210_ADDR, &[0x1B, 0xBF], 1000)?;
        i2c.write(ES7210_ADDR, &[0x1C, 0xBF], 1000)?;
        i2c.write(ES7210_ADDR, &[0x1D, 0xBF], 1000)?;
        i2c.write(ES7210_ADDR, &[0x1E, 0xBF], 1000)?;

        log::info!("[HW] ES7210 OK (mic), gain=37.5dB");
        Ok(())
    }
}
