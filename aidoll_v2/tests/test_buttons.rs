use aidoll_v2::buttons::*;

#[test]
fn volume_up_increases() {
    let mut vc = VolumeControl::new(50, 10);
    let fb = vc.handle_button(ButtonEvent::VolumeUp);
    assert_eq!(fb, ButtonFeedback::VolumeUp(60));
    assert_eq!(vc.volume(), 60);
    assert_eq!(vc.effective_volume(), 60);
}

#[test]
fn volume_down_decreases() {
    let mut vc = VolumeControl::new(50, 10);
    let fb = vc.handle_button(ButtonEvent::VolumeDown);
    assert_eq!(fb, ButtonFeedback::VolumeDown(40));
    assert_eq!(vc.volume(), 40);
}

#[test]
fn volume_clamps_at_100() {
    let mut vc = VolumeControl::new(95, 10);
    let fb = vc.handle_button(ButtonEvent::VolumeUp);
    assert_eq!(fb, ButtonFeedback::VolumeUp(100));
    assert_eq!(vc.volume(), 100);
}

#[test]
fn volume_clamps_at_0() {
    let mut vc = VolumeControl::new(5, 10);
    let fb = vc.handle_button(ButtonEvent::VolumeDown);
    assert_eq!(fb, ButtonFeedback::VolumeDown(0));
    assert_eq!(vc.volume(), 0);
}

#[test]
fn mute_toggle_on() {
    let mut vc = VolumeControl::new(70, 10);
    let fb = vc.handle_button(ButtonEvent::MuteToggle);
    assert_eq!(fb, ButtonFeedback::Muted);
    assert!(vc.is_muted());
    assert_eq!(vc.effective_volume(), 0);
    assert_eq!(vc.volume(), 70); // underlying volume preserved
}

#[test]
fn mute_toggle_off() {
    let mut vc = VolumeControl::new(70, 10);
    vc.handle_button(ButtonEvent::MuteToggle); // mute
    let fb = vc.handle_button(ButtonEvent::MuteToggle); // unmute
    assert_eq!(fb, ButtonFeedback::Unmuted(70));
    assert!(!vc.is_muted());
    assert_eq!(vc.effective_volume(), 70);
}

#[test]
fn volume_up_while_muted_unmutes() {
    let mut vc = VolumeControl::new(50, 10);
    vc.handle_button(ButtonEvent::MuteToggle); // mute
    assert!(vc.is_muted());

    let fb = vc.handle_button(ButtonEvent::VolumeUp);
    assert_eq!(fb, ButtonFeedback::Unmuted(50)); // unmutes at current volume
    assert!(!vc.is_muted());
}

#[test]
fn volume_down_while_muted_unmutes() {
    let mut vc = VolumeControl::new(50, 10);
    vc.handle_button(ButtonEvent::MuteToggle);
    let fb = vc.handle_button(ButtonEvent::VolumeDown);
    assert_eq!(fb, ButtonFeedback::Unmuted(50));
    assert!(!vc.is_muted());
}

#[test]
fn multiple_volume_ups() {
    let mut vc = VolumeControl::new(0, 10);
    for _ in 0..5 {
        vc.handle_button(ButtonEvent::VolumeUp);
    }
    assert_eq!(vc.volume(), 50);
}

#[test]
fn initial_volume_clamped() {
    let vc = VolumeControl::new(200, 10);
    assert_eq!(vc.volume(), 100);
}
