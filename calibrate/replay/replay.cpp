/*
  Replay a recorded samples CSV (from live_view.py) through detector.h, the
  same detection code that runs on the Pico, and print the events as CSV.

  Build:  g++ -std=c++17 -O2 -Wall -o calibrate/replay/replay calibrate/replay/replay.cpp
  Usage:  calibrate/replay/replay readings_20261029_101500.csv > events.csv

  The output has the same format as the <name>_events.csv written by
  live_view.py, so you can diff them or try different settings offline.
*/

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "../calibrate_mcp3008/detector.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <samples.csv>\n", argv[0]);
    return 2;
  }
  std::ifstream in(argv[1]);
  if (!in) {
    fprintf(stderr, "cannot open %s\n", argv[1]);
    return 1;
  }

  std::vector<Detector> detectors;
  std::map<std::string, int> counts;
  std::string line;
  long samples = 0;

  printf("time_us,channel,event,value\n");
  while (std::getline(in, line)) {
    if (line.empty() || line[0] < '0' || line[0] > '9') {
      continue;  // header
    }
    const char *p = line.c_str();
    char *end;
    int64_t time = strtoll(p, &end, 10);
    std::vector<int> values;
    while (*end == ',') {
      values.push_back(strtol(end + 1, &end, 10));
    }
    if (detectors.empty()) {
      detectors.resize(values.size());
    }
    if (values.size() != detectors.size()) {
      continue;
    }
    samples++;

    // The recording's time is unwrapped to 64 bit; the Pico works with 32 bit
    uint32_t now = (uint32_t)time;
    for (size_t ch = 0; ch < values.size(); ch++) {
      Event events[Detector::MAX_EVENTS];
      int count = detectors[ch].update(values[ch], now, events);
      for (int i = 0; i < count; i++) {
        int64_t eventTime = time - (uint32_t)(now - events[i].time);
        printf("%lld,%zu,%s,%d\n", (long long)eventTime, ch,
               eventName(events[i].type), events[i].value);
        counts[eventName(events[i].type)]++;
      }
    }
  }

  fprintf(stderr, "%ld samples, %zu channels:", samples, detectors.size());
  for (const auto &c : counts) {
    fprintf(stderr, " %s %d", c.first.c_str(), c.second);
  }
  fprintf(stderr, "\n");
  return 0;
}
