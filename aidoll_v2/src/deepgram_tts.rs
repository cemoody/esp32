#[cfg(feature = "esp32")]
pub mod client {
    use crate::ring_buffer::RingBuffer;
    use crate::tts::{build_flush_message, build_speak_message, parse_tts_message, TtsEvent};
    use anyhow::Result;
    use esp_idf_svc::ws::client::{
        EspWebSocketClient, EspWebSocketClientConfig, WebSocketEventType, FrameType,
    };
    use std::sync::mpsc;
    use std::time::Duration;

    pub fn connect(
        api_key: &str,
        model: &str,
        audio_buffer: RingBuffer,
        done_tx: mpsc::Sender<()>,
    ) -> Result<EspWebSocketClient<'static>> {
        let url = format!(
            "wss://api.deepgram.com/v1/speak?model={}&encoding=linear16&sample_rate=16000",
            model
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
                        WebSocketEventType::Binary(data) => {
                            // Raw PCM audio — write to playback buffer
                            audio_buffer.write(data);
                        }
                        WebSocketEventType::Text(text) => {
                            if let Some(TtsEvent::Flushed) = parse_tts_message(text) {
                                let _ = done_tx.send(());
                            }
                        }
                        WebSocketEventType::Connected => {
                            log::info!("[TTS] Connected to Deepgram TTS");
                        }
                        WebSocketEventType::Disconnected => {
                            log::warn!("[TTS] Disconnected");
                            let _ = done_tx.send(()); // force done on disconnect
                        }
                        _ => {}
                    }
                }
            },
        )?;

        log::info!("[TTS] WebSocket client created");
        Ok(client)
    }

    pub fn send_text(client: &mut EspWebSocketClient, text: &str) -> Result<()> {
        let msg = build_speak_message(text);
        client.send(FrameType::Text(false), msg.as_bytes())?;
        Ok(())
    }

    pub fn flush(client: &mut EspWebSocketClient) -> Result<()> {
        let msg = build_flush_message();
        client.send(FrameType::Text(false), msg.as_bytes())?;
        Ok(())
    }
}
