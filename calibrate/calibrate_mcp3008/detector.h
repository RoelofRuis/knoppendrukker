#pragma once

/*
  Press event detection for one sensor channel.

  Plain C++ without Arduino dependencies, so the Pico sketch and the laptop
  replay tool (calibrate/replay) run exactly the same code.
  See calibrate/README.md for the rules.
*/

#include <stdint.h>

struct DetectorConfig {
  float vref = 3.3f;
  float thresholdV = 0.10f;          // a press starts above this
  float releaseV = 0.08f;            // a press ends below this (hysteresis)
  float tapRiseVPerMs = 0.08f;       // tap: rises at least this fast in one sample
  uint32_t onsetWindowUs = 2000;     // no fast rise within this time: touch
  uint32_t tapPeakMaxUs = 10000;     // send the tap by now even if still rising
  uint32_t holdIntervalUs = 10000;   // hold event interval while pressed
  uint32_t sampleIntervalUs = 1000;  // time between samples fed to update()
};

enum class EventType { Touch, Tap, Hold, Release };

inline const char *eventName(EventType type) {
  switch (type) {
    case EventType::Touch: return "touch";
    case EventType::Tap: return "tap";
    case EventType::Hold: return "hold";
    case EventType::Release: return "release";
  }
  return "?";
}

struct Event {
  uint32_t time;  // us; for touch/tap the start of the press
  EventType type;
  int value;      // raw ADC value; for tap the top of the spike
};

// touch/tap are timestamped at the start of the press, hold and release at the
// moment they are sent. A press that rises fast (the start of a spike) within
// onsetWindowUs is a tap, otherwise a touch. When in doubt it's a tap.
// A tap is sent once its spike stops rising, with the top of the spike as value.
class Detector {
 public:
  static const int MAX_EVENTS = 2;  // per update()

  explicit Detector(const DetectorConfig &config = DetectorConfig()) {
    configure(config);
  }

  void configure(const DetectorConfig &config) {
    config_ = config;
    thresholdRaw_ = config.thresholdV / config.vref * 1024;
    releaseRaw_ = config.releaseV / config.vref * 1024;
    tapRiseRaw_ = config.tapRiseVPerMs / config.vref * 1024 * config.sampleIntervalUs / 1000.0f;
  }

  // Feed one raw 10-bit sample taken at `now` (us, may wrap around).
  // Writes up to MAX_EVENTS events to `out` and returns how many.
  int update(int value, uint32_t now, Event *out) {
    int count = 0;
    int rise = value - previous_;
    previous_ = value;

    if (state_ == State::Idle) {
      if (value <= thresholdRaw_) {
        return 0;
      }
      state_ = State::Onset;
      startTime_ = now;
      peak_ = value;
    }

    if (value < releaseRaw_) {
      if (state_ == State::Onset || state_ == State::Rising) {
        out[count++] = {startTime_, EventType::Tap, peak_};  // released before it could be classified
      }
      out[count++] = {now, EventType::Release, value};
      state_ = State::Idle;
      return count;
    }

    if (state_ == State::Onset) {
      if (value > peak_) {
        peak_ = value;
      }
      if (rise >= tapRiseRaw_) {
        state_ = State::Rising;
      } else if (now - startTime_ >= config_.onsetWindowUs) {
        out[count++] = {startTime_, EventType::Touch, value};
        startHolding(now);
      }
    } else if (state_ == State::Rising) {
      if (value > peak_ && now - startTime_ < config_.tapPeakMaxUs) {
        peak_ = value;
      } else {
        out[count++] = {startTime_, EventType::Tap, peak_};
        startHolding(now);
      }
    } else if (now - lastHoldTime_ >= config_.holdIntervalUs) {
      out[count++] = {now, EventType::Hold, value};
      lastHoldTime_ += config_.holdIntervalUs;
    }
    return count;
  }

 private:
  // Scoped: Arduino already defines RISING
  enum class State { Idle, Onset, Rising, Pressed };

  void startHolding(uint32_t now) {
    state_ = State::Pressed;
    lastHoldTime_ = now;
  }

  DetectorConfig config_;
  int thresholdRaw_;
  int releaseRaw_;
  int tapRiseRaw_;

  State state_ = State::Idle;
  uint32_t startTime_ = 0;
  uint32_t lastHoldTime_ = 0;
  int previous_ = 0;
  int peak_ = 0;
};
