use std::collections::VecDeque;
use std::sync::{Arc, Mutex};

/// Thread-safe ring buffer for PCM audio data
#[derive(Clone)]
pub struct RingBuffer {
    inner: Arc<Mutex<VecDeque<u8>>>,
    capacity: usize,
}

impl RingBuffer {
    pub fn new(capacity: usize) -> Self {
        Self {
            inner: Arc::new(Mutex::new(VecDeque::with_capacity(capacity))),
            capacity,
        }
    }

    pub fn write(&self, data: &[u8]) -> usize {
        let mut buf = self.inner.lock().unwrap();
        let space = self.capacity - buf.len();
        let to_write = data.len().min(space);
        buf.extend(&data[..to_write]);
        to_write
    }

    pub fn read(&self, out: &mut [u8]) -> usize {
        let mut buf = self.inner.lock().unwrap();
        let to_read = out.len().min(buf.len());
        for i in 0..to_read {
            out[i] = buf.pop_front().unwrap();
        }
        to_read
    }

    pub fn available(&self) -> usize {
        self.inner.lock().unwrap().len()
    }

    pub fn flush(&self) {
        self.inner.lock().unwrap().clear();
    }
}
