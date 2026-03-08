/// Button events from the TCA9555 IO expander
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ButtonEvent {
    VolumeUp,
    VolumeDown,
    MuteToggle,
}

/// LED flash feedback for button presses
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ButtonFeedback {
    VolumeUp(u8),    // new volume level
    VolumeDown(u8),  // new volume level
    Muted,
    Unmuted(u8),     // restored volume level
}

/// Manages volume state and mute toggle
pub struct VolumeControl {
    volume: u8,
    muted: bool,
    step: u8,
}

impl VolumeControl {
    pub fn new(initial_volume: u8, step: u8) -> Self {
        Self {
            volume: initial_volume.min(100),
            muted: false,
            step,
        }
    }

    pub fn handle_button(&mut self, event: ButtonEvent) -> ButtonFeedback {
        match event {
            ButtonEvent::VolumeUp => {
                if self.muted {
                    self.muted = false;
                    return ButtonFeedback::Unmuted(self.volume);
                }
                self.volume = (self.volume + self.step).min(100);
                ButtonFeedback::VolumeUp(self.volume)
            }
            ButtonEvent::VolumeDown => {
                if self.muted {
                    self.muted = false;
                    return ButtonFeedback::Unmuted(self.volume);
                }
                self.volume = self.volume.saturating_sub(self.step);
                ButtonFeedback::VolumeDown(self.volume)
            }
            ButtonEvent::MuteToggle => {
                self.muted = !self.muted;
                if self.muted {
                    ButtonFeedback::Muted
                } else {
                    ButtonFeedback::Unmuted(self.volume)
                }
            }
        }
    }

    /// Returns the effective volume (0 if muted)
    pub fn effective_volume(&self) -> u8 {
        if self.muted { 0 } else { self.volume }
    }

    pub fn volume(&self) -> u8 {
        self.volume
    }

    pub fn is_muted(&self) -> bool {
        self.muted
    }
}
