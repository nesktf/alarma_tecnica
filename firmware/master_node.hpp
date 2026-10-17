#ifndef MASTER_NODE_HPP
#define MASTER_NODE_HPP

#include "config.h"
#include "protocol.h"

struct SectorRuntime {
  uint8_t id;
  bool enabled;
  bool online;
  bool motion;
  bool panic;
  uint16_t distance_cm;
  uint32_t last_seen_ms;
  uint8_t consecutive_motions;
};

extern SectorRuntime sectors[TOTAL_SECTORS + 1];
extern uint32_t led_feedback_until_ms;
extern bool led_feedback_success;

AlarmState current_alarm_state();
TriggerReason current_trigger_reason();
uint8_t current_trigger_sector();
uint32_t arming_start_time();
uint32_t grace_start_time();

void set_alarm_state(AlarmState state);
void start_arming();
void disarm_system();
void trigger_grace_period(TriggerReason reason, uint8_t sector_id);
void trigger_alarm(TriggerReason reason, uint8_t sector_id);

#endif // MASTER_NODE_HPP
