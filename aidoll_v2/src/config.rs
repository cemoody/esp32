pub struct Config {
    pub deepgram_api_key: String,
    pub groq_api_key: String,
    pub deepgram_host: String,
    pub stt_model: String,
    pub tts_model: String,
    pub groq_host: String,
    pub groq_model: String,
    pub groq_max_tokens: u32,
    pub system_prompt: String,
    pub max_conversation_turns: usize,
    pub interrupt_peak_threshold: i16,
    // TCA9555 IO expander button pins
    pub key1_pin: u8,       // EXIO9 — Volume up
    pub key2_pin: u8,       // EXIO10 — Mute toggle
    pub key3_pin: u8,       // EXIO11 — Volume down
    pub volume_step: u8,    // Volume change per press
    pub initial_volume: u8, // Starting volume (0-100)
}

impl Config {
    pub fn from_env() -> Self {
        Config {
            deepgram_api_key: std::env::var("DEEPGRAM_API_KEY").unwrap_or_default(),
            groq_api_key: std::env::var("GROQ_API_KEY").unwrap_or_default(),
            deepgram_host: "api.deepgram.com".to_string(),
            stt_model: "nova-3".to_string(),
            tts_model: "aura-2-luna-en".to_string(),
            groq_host: "api.groq.com".to_string(),
            groq_model: "meta-llama/llama-4-scout-17b-16e-instruct".to_string(),
            groq_max_tokens: 80,
            system_prompt: String::new(),
            max_conversation_turns: 10,
            interrupt_peak_threshold: 5000,
            key1_pin: 9,
            key2_pin: 10,
            key3_pin: 11,
            volume_step: 10,
            initial_volume: 70,
        }
    }
}
