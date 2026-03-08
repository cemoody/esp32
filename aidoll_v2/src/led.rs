#[cfg(feature = "esp32")]
pub mod neopixel {
    use anyhow::Result;
    use esp_idf_hal::rmt::config::TransmitConfig;
    use esp_idf_hal::rmt::{FixedLengthSignal, PinState, Pulse, TxRmtDriver};
    use std::time::Duration;

    const NUM_LEDS: usize = 7;

    pub struct NeoPixel<'d> {
        tx: TxRmtDriver<'d>,
    }

    impl<'d> NeoPixel<'d> {
        pub fn new(tx: TxRmtDriver<'d>) -> Self {
            Self { tx }
        }

        /// Set all LEDs to one color (RGB order)
        pub fn set_all(&mut self, r: u8, g: u8, b: u8) -> Result<()> {
            let ticks_hz = self.tx.counter_clock()?;
            let t0h = Pulse::new_with_duration(ticks_hz, PinState::High, &Duration::from_nanos(350))?;
            let t0l = Pulse::new_with_duration(ticks_hz, PinState::Low, &Duration::from_nanos(800))?;
            let t1h = Pulse::new_with_duration(ticks_hz, PinState::High, &Duration::from_nanos(700))?;
            let t1l = Pulse::new_with_duration(ticks_hz, PinState::Low, &Duration::from_nanos(600))?;

            // WS2812 expects GRB order on wire
            // But our board uses RGB order (NEO_RGB from C++ code)
            let pixel: u32 = ((r as u32) << 16) | ((g as u32) << 8) | (b as u32);

            // 7 LEDs × 24 bits = 168 pulses
            let mut signal = FixedLengthSignal::<168>::new();

            for led in 0..NUM_LEDS {
                for bit in (0..24).rev() {
                    let idx = led * 24 + (23 - bit);
                    let is_one = (pixel >> bit) & 1 != 0;
                    let pair = if is_one { (t1h, t1l) } else { (t0h, t0l) };
                    signal.set(idx, &pair)?;
                }
            }

            self.tx.start_blocking(&signal)?;
            Ok(())
        }

        pub fn off(&mut self) -> Result<()> {
            self.set_all(0, 0, 0)
        }
    }

    use crate::pipeline::LedState;

    /// Map pipeline LED state to RGB color
    pub fn state_to_color(state: LedState) -> (u8, u8, u8) {
        match state {
            LedState::Idle => (255, 180, 0),       // Gold
            LedState::Listening => (0, 100, 255),   // Blue
            LedState::Thinking => (180, 0, 255),    // Purple
            LedState::Speaking => (0, 255, 80),     // Green
            LedState::Error => (255, 0, 0),         // Red
            LedState::Connecting => (100, 100, 100), // Dim white
        }
    }
}
