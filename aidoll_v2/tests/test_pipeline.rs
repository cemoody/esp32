use aidoll_v2::pipeline::*;

#[test]
fn idle_to_listening_on_partial_transcript() {
    let mut p = Pipeline::new();
    assert_eq!(p.state(), PipelineState::Idle);
    let actions = p.handle_event(PipelineEvent::PartialTranscript("hel".into()));
    assert_eq!(p.state(), PipelineState::Listening);
    assert!(actions.contains(&PipelineAction::SetLedState(LedState::Listening)));
}

#[test]
fn listening_to_processing_on_final_transcript() {
    let mut p = Pipeline::new();
    p.handle_event(PipelineEvent::PartialTranscript("h".into()));
    assert_eq!(p.state(), PipelineState::Listening);

    let actions = p.handle_event(PipelineEvent::FinalTranscript("hello".into()));
    assert_eq!(p.state(), PipelineState::Processing);
    assert!(actions.contains(&PipelineAction::SetLedState(LedState::Thinking)));
    assert!(actions.contains(&PipelineAction::SendToLlm("hello".into())));
}

#[test]
fn processing_to_speaking_on_first_token() {
    let mut p = Pipeline::new();
    p.handle_event(PipelineEvent::PartialTranscript("h".into()));
    p.handle_event(PipelineEvent::FinalTranscript("hello".into()));
    assert_eq!(p.state(), PipelineState::Processing);

    let actions = p.handle_event(PipelineEvent::LlmToken("Hi".into()));
    assert_eq!(p.state(), PipelineState::Speaking);
    assert!(actions.contains(&PipelineAction::SetLedState(LedState::Speaking)));
}

#[test]
fn speaking_to_idle_when_tts_done_and_drained() {
    let mut p = Pipeline::new();
    p.handle_event(PipelineEvent::PartialTranscript("h".into()));
    p.handle_event(PipelineEvent::FinalTranscript("hello".into()));
    p.handle_event(PipelineEvent::LlmToken("Hi".into()));
    p.handle_event(PipelineEvent::LlmDone("Hi there!".into()));
    assert_eq!(p.state(), PipelineState::Speaking);

    p.handle_event(PipelineEvent::TtsDone);
    // Still speaking — buffer not drained yet
    assert_eq!(p.state(), PipelineState::Speaking);

    let actions = p.handle_event(PipelineEvent::SpeakerDrained);
    assert_eq!(p.state(), PipelineState::Idle);
    assert!(actions.contains(&PipelineAction::SetLedState(LedState::Idle)));
}

#[test]
fn speaking_stays_if_tts_not_done() {
    let mut p = Pipeline::new();
    p.handle_event(PipelineEvent::PartialTranscript("h".into()));
    p.handle_event(PipelineEvent::FinalTranscript("hello".into()));
    p.handle_event(PipelineEvent::LlmToken("Hi".into()));
    p.handle_event(PipelineEvent::LlmDone("Hi".into()));

    // Speaker drained but TTS not done yet
    let actions = p.handle_event(PipelineEvent::SpeakerDrained);
    assert_eq!(p.state(), PipelineState::Speaking);
}

#[test]
fn interrupt_during_speaking() {
    let mut p = Pipeline::new();
    p.handle_event(PipelineEvent::PartialTranscript("h".into()));
    p.handle_event(PipelineEvent::FinalTranscript("hello".into()));
    p.handle_event(PipelineEvent::LlmToken("Hi".into()));
    assert_eq!(p.state(), PipelineState::Speaking);

    let actions = p.handle_event(PipelineEvent::Interrupt);
    assert_eq!(p.state(), PipelineState::Listening);
    assert!(actions.contains(&PipelineAction::CancelLlm));
    assert!(actions.contains(&PipelineAction::DisconnectTts));
    assert!(actions.contains(&PipelineAction::FlushAudioBuffer));
    assert!(actions.contains(&PipelineAction::SetLedState(LedState::Listening)));
}

#[test]
fn llm_failure_returns_to_idle() {
    let mut p = Pipeline::new();
    p.handle_event(PipelineEvent::PartialTranscript("h".into()));
    p.handle_event(PipelineEvent::FinalTranscript("hello".into()));
    assert_eq!(p.state(), PipelineState::Processing);

    let actions = p.handle_event(PipelineEvent::LlmFailed);
    assert_eq!(p.state(), PipelineState::Idle);
    assert!(actions.contains(&PipelineAction::SetLedState(LedState::Idle)));
}

#[test]
fn full_conversation_cycle() {
    let mut p = Pipeline::new();
    assert_eq!(p.state(), PipelineState::Idle);

    // User speaks
    p.handle_event(PipelineEvent::PartialTranscript("hel".into()));
    assert_eq!(p.state(), PipelineState::Listening);

    // Transcript finalized
    p.handle_event(PipelineEvent::FinalTranscript("hello".into()));
    assert_eq!(p.state(), PipelineState::Processing);

    // LLM streams tokens
    p.handle_event(PipelineEvent::LlmToken("Hi".into()));
    assert_eq!(p.state(), PipelineState::Speaking);

    p.handle_event(PipelineEvent::LlmToken(" there".into()));
    p.handle_event(PipelineEvent::LlmDone("Hi there!".into()));

    // TTS finishes, speaker drains
    p.handle_event(PipelineEvent::TtsDone);
    assert_eq!(p.state(), PipelineState::Speaking);

    p.handle_event(PipelineEvent::SpeakerDrained);
    assert_eq!(p.state(), PipelineState::Idle);
}
