#[cfg(feature = "esp32")]
pub mod client {
    use crate::stt::{parse_deepgram_message, SttEvent};
    use anyhow::Result;
    use esp_idf_svc::ws::client::{
        EspWebSocketClient, EspWebSocketClientConfig, WebSocketEventType, FrameType,
    };
    use std::sync::mpsc;
    use std::time::Duration;

    pub fn connect(
        api_key: &str,
        event_tx: mpsc::Sender<SttEvent>,
    ) -> Result<EspWebSocketClient<'static>> {
        let url = format!(
            "wss://api.deepgram.com/v1/listen?encoding=linear16&sample_rate=16000&channels=1&model=nova-3&punctuate=true&endpointing=300"
        );

        let headers = format!("Authorization: Token {}\r\n", api_key);

        let config = EspWebSocketClientConfig {
            headers: Some(Box::leak(headers.into_boxed_str())),
            use_global_ca_store: true,
            crt_bundle_attach: Some(esp_idf_svc::sys::esp_crt_bundle_attach),
            ..Default::default()
        };

        let client = EspWebSocketClient::new(
            &url,
            &config,
            Duration::from_secs(10),
            move |event| {
                if let Ok(event) = event {
                    match event.event_type {
                        WebSocketEventType::Text(text) => {
                            if let Some(stt_event) = parse_deepgram_message(text) {
                                let _ = event_tx.send(stt_event);
                            }
                        }
                        WebSocketEventType::Connected => {
                            log::info!("[STT] Connected to Deepgram");
                        }
                        WebSocketEventType::Disconnected => {
                            log::warn!("[STT] Disconnected from Deepgram");
                        }
                        _ => {}
                    }
                }
            },
        )?;

        log::info!("[STT] WebSocket client created");
        Ok(client)
    }

    pub fn send_audio(client: &mut EspWebSocketClient, pcm: &[u8]) -> Result<()> {
        client.send(FrameType::Binary(false), pcm)?;
        Ok(())
    }
}
