#[derive(Debug, Clone, Copy, PartialEq)]
pub enum PipelineState {
    Idle,
    Listening,
    Processing,
    Speaking,
}

#[derive(Debug)]
pub enum PipelineEvent {
    PartialTranscript(String),
    FinalTranscript(String),
    LlmToken(String),
    LlmDone(String),
    TtsAudioChunk(Vec<u8>),
    TtsDone,
    Interrupt,
    SpeakerDrained,
    LlmFailed,
}

pub struct Pipeline {
    state: PipelineState,
    tts_done: bool,
    llm_done: bool,
    buffer_drained: bool,
}

impl Pipeline {
    pub fn new() -> Self {
        Self {
            state: PipelineState::Idle,
            tts_done: false,
            llm_done: false,
            buffer_drained: true,
        }
    }

    pub fn state(&self) -> PipelineState {
        self.state
    }

    pub fn handle_event(&mut self, event: PipelineEvent) -> Vec<PipelineAction> {
        let mut actions = Vec::new();

        match event {
            PipelineEvent::PartialTranscript(_) => {
                if self.state == PipelineState::Idle {
                    self.state = PipelineState::Listening;
                    actions.push(PipelineAction::SetLedState(LedState::Listening));
                }
            }

            PipelineEvent::FinalTranscript(text) => {
                if self.state == PipelineState::Listening || self.state == PipelineState::Idle {
                    self.state = PipelineState::Processing;
                    self.tts_done = false;
                    self.llm_done = false;
                    self.buffer_drained = false;
                    actions.push(PipelineAction::SetLedState(LedState::Thinking));
                    actions.push(PipelineAction::SendToLlm(text));
                }
            }

            PipelineEvent::LlmToken(_token) => {
                if self.state == PipelineState::Processing {
                    self.state = PipelineState::Speaking;
                    actions.push(PipelineAction::SetLedState(LedState::Speaking));
                }
            }

            PipelineEvent::LlmDone(response) => {
                self.llm_done = true;
                if !response.is_empty() {
                    actions.push(PipelineAction::SendToTts(response));
                    actions.push(PipelineAction::FlushTts);
                }
            }

            PipelineEvent::TtsAudioChunk(_) => {
                // Audio chunk written to buffer by orchestrator
            }

            PipelineEvent::TtsDone => {
                self.tts_done = true;
                // Check if we can go idle
                self.maybe_go_idle(&mut actions);
            }

            PipelineEvent::SpeakerDrained => {
                self.buffer_drained = true;
                self.maybe_go_idle(&mut actions);
            }

            PipelineEvent::Interrupt => {
                if self.state == PipelineState::Speaking || self.state == PipelineState::Processing {
                    self.state = PipelineState::Listening;
                    self.tts_done = false;
                    self.llm_done = false;
                    actions.push(PipelineAction::CancelLlm);
                    actions.push(PipelineAction::DisconnectTts);
                    actions.push(PipelineAction::FlushAudioBuffer);
                    actions.push(PipelineAction::SetLedState(LedState::Listening));
                }
            }

            PipelineEvent::LlmFailed => {
                if self.state == PipelineState::Processing {
                    self.state = PipelineState::Idle;
                    actions.push(PipelineAction::DisconnectTts);
                    actions.push(PipelineAction::SetLedState(LedState::Idle));
                }
            }
        }

        actions
    }

    fn maybe_go_idle(&mut self, actions: &mut Vec<PipelineAction>) {
        if self.state == PipelineState::Speaking && self.tts_done && self.llm_done && self.buffer_drained {
            self.state = PipelineState::Idle;
            actions.push(PipelineAction::SetLedState(LedState::Idle));
        }
    }
}

/// Actions the pipeline requests the orchestrator to perform
#[derive(Debug, PartialEq)]
pub enum PipelineAction {
    StartListening,
    SendToLlm(String),
    SendToTts(String),
    FlushTts,
    CancelLlm,
    DisconnectTts,
    FlushAudioBuffer,
    SetLedState(LedState),
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum LedState {
    Idle,
    Listening,
    Thinking,
    Speaking,
    Error,
    Connecting,
}
