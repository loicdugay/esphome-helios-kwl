#pragma once
// helios_kwl.h — Phase 2C-fix — alternance S2/S3 + deduplication publications

#include "esphome/core/component.h"
#include "esphome/core/optional.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/button/button.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/fan/fan.h"
#include <array>
#include <cstdint>

namespace esphome {
namespace helios_kwl {

static constexpr uint8_t HELIOS_START_BYTE    = 0x01;
static constexpr uint8_t HELIOS_MAINBOARD     = 0x11;
static constexpr uint8_t HELIOS_BROADCAST_ALL = 0x10;
static constexpr uint8_t HELIOS_BROADCAST_RC  = 0x20;
static constexpr uint8_t HELIOS_PACKET_LEN    = 6;
static constexpr uint8_t HELIOS_ADDR_DEFAULT  = 0x2F;

// Strategie 1 (broadcast passif)
static constexpr uint8_t REG_CO2_HIGH         = 0x2B;
static constexpr uint8_t REG_CO2_LOW          = 0x2C;
static constexpr uint8_t REG_TEMP_OUTSIDE     = 0x32;
static constexpr uint8_t REG_TEMP_EXHAUST     = 0x33;
static constexpr uint8_t REG_TEMP_EXTRACT     = 0x34;
static constexpr uint8_t REG_TEMP_SUPPLY      = 0x35;

// Strategie 2 (6s)
static constexpr uint8_t REG_FAN_SPEED        = 0x29;
static constexpr uint8_t REG_HUMIDITY1        = 0x2F;
static constexpr uint8_t REG_HUMIDITY2        = 0x30;
static constexpr uint8_t REG_STATES           = 0xA3;
static constexpr uint8_t REG_IO_PORT          = 0x08;
static constexpr uint8_t REG_ALARMS           = 0x6D;
static constexpr uint8_t REG_BOOST_STATE      = 0x71;
static constexpr uint8_t REG_BOOST_REMAINING  = 0x79;

// Strategie 3 (init + 1h)
static constexpr uint8_t REG_CO2_SENSORS      = 0x2D;
static constexpr uint8_t REG_FAULT_CODE       = 0x36;
static constexpr uint8_t REG_POST_HEAT_ON     = 0x55;
static constexpr uint8_t REG_POST_HEAT_OFF    = 0x56;
static constexpr uint8_t REG_FLAGS_SYSTEM     = 0x6F;
static constexpr uint8_t REG_FLAGS_MODE       = 0x70;
static constexpr uint8_t REG_SERVICE_MONTHS   = 0xAB;
static constexpr uint8_t REG_PROGRAM_VARS     = 0xAA;
static constexpr uint8_t REG_BASIC_SPEED      = 0xA9;
static constexpr uint8_t REG_MAX_SPEED        = 0xA5;
static constexpr uint8_t REG_BYPASS_TEMP      = 0xAF;
static constexpr uint8_t REG_DEFROST_TEMP     = 0xA7;
static constexpr uint8_t REG_FROST_ALARM_TEMP = 0xA8;
static constexpr uint8_t REG_FROST_HYSTERESIS = 0xB2;
static constexpr uint8_t REG_SUPPLY_FAN_PCT   = 0xB0;
static constexpr uint8_t REG_EXHAUST_FAN_PCT  = 0xB1;
static constexpr uint8_t REG_CO2_SETPOINT_H   = 0xB3;
static constexpr uint8_t REG_CO2_SETPOINT_L   = 0xB4;
static constexpr uint8_t REG_HUMIDITY_SET     = 0xAE;
static constexpr uint8_t REG_SERVICE_INTERVAL = 0xA6;
static constexpr uint8_t REG_PROGRAM2         = 0xB5;

// Bits
static constexpr uint8_t BIT_POWER = 0, BIT_CO2_REG = 1, BIT_HUMIDITY_REG = 2, BIT_SUMMER_MODE = 3;
static constexpr uint8_t BIT_HEATING = 5, BIT_FAULT = 6, BIT_FILTER_MAINT = 7;
static constexpr uint8_t BIT_BYPASS_OPEN = 1, BIT_FAULT_RELAY = 2, BIT_SUPPLY_FAN = 3;
static constexpr uint8_t BIT_PREHEATING = 4, BIT_EXHAUST_FAN = 5, BIT_EXT_CONTACT = 6;
static constexpr uint8_t BIT_CO2_ALARM = 6, BIT_FREEZE_ALARM = 7;
static constexpr uint8_t BIT_BOOST_ACTIVATE = 5, BIT_BOOST_RUNNING = 6;
static constexpr uint8_t BIT_HUMIDITY_AUTO = 4, BIT_BOOST_FIRE_MODE = 5;
static constexpr uint8_t BIT_MAX_SPEED_CONT = 0;

static constexpr size_t RX_BUFFER_SIZE = 512;
static constexpr size_t REGISTER_COUNT = 256;
static constexpr size_t MAX_PENDING_WRITES = 8;
static constexpr uint32_t POLL_INTERVAL_S2 = 6000;
static constexpr uint32_t POLL_INTERVAL_S3 = 3600000;
static constexpr uint32_t BROADCAST_SALVE_MS = 80;

// Ratio alternance : 5 cycles S2 puis 1 cycle S3
static constexpr uint8_t S2_TURNS_BEFORE_S3 = 5;

struct RegisterCache { uint8_t value{0}; uint32_t last_update{0}; bool valid{false}; };
struct PollTask { uint8_t reg; uint32_t interval_ms; uint32_t last_polled; };
struct PendingWrite { uint8_t reg; uint8_t value; uint32_t written_at; uint8_t retries; bool active; };

class HeliosKwlFan;

class HeliosKwlComponent : public uart::UARTDevice, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_address(uint8_t a) { address_ = a; }

  bool write_register(uint8_t reg, uint8_t value);
  bool write_register_with_verify(uint8_t reg, uint8_t value, uint8_t retries = 2);
  bool set_register_bit(uint8_t reg, uint8_t bit, bool state);
  bool read_register_bit(uint8_t reg, uint8_t bit);
  optional<uint8_t> poll_register(uint8_t reg, uint8_t retries = 3);
  optional<uint8_t> get_cached_value(uint8_t reg);
  void set_fan_speed(uint8_t speed);
  void set_fan_on(bool on);

  // Setters (tous identiques aux phases precedentes)
  void set_temperature_outside_sensor(sensor::Sensor *s)     { temperature_outside_    = s; }
  void set_temperature_extract_sensor(sensor::Sensor *s)     { temperature_extract_    = s; }
  void set_temperature_supply_sensor(sensor::Sensor *s)      { temperature_supply_     = s; }
  void set_temperature_exhaust_sensor(sensor::Sensor *s)     { temperature_exhaust_    = s; }
  void set_humidity_sensor1(sensor::Sensor *s)               { humidity_sensor1_       = s; }
  void set_humidity_sensor2(sensor::Sensor *s)               { humidity_sensor2_       = s; }
  void set_co2_concentration_sensor(sensor::Sensor *s)       { co2_concentration_      = s; }
  void set_boost_remaining_sensor(sensor::Sensor *s)         { boost_remaining_        = s; }
  void set_fault_code_sensor(sensor::Sensor *s)              { fault_code_             = s; }
  void set_service_months_remaining_sensor(sensor::Sensor *s){ service_months_         = s; }
  void set_bypass_open_sensor(sensor::Sensor *s)             { bypass_open_            = s; }
  void set_fan_speed_sensor(sensor::Sensor *s)               { fan_speed_sensor_       = s; }
  void set_fault_indicator_sensor(sensor::Sensor *s)         { fault_indicator_sensor_ = s; }
  void set_boost_state_sensor(sensor::Sensor *s)             { boost_state_sensor_     = s; }
  void set_fault_description_text(text_sensor::TextSensor *s){ fault_description_      = s; }
  void set_boost_active_text(text_sensor::TextSensor *s)     { boost_active_text_      = s; }
  void set_bypass_state_text(text_sensor::TextSensor *s)     { bypass_state_text_      = s; }
  void set_preheating_active_sensor(binary_sensor::BinarySensor *s) { preheating_active_  = s; }
  void set_freeze_alarm_sensor(binary_sensor::BinarySensor *s)      { freeze_alarm_       = s; }
  void set_co2_alarm_sensor(binary_sensor::BinarySensor *s)         { co2_alarm_          = s; }
  void set_filter_maintenance_sensor(binary_sensor::BinarySensor *s){ filter_maintenance_ = s; }
  void set_heating_indicator_sensor(binary_sensor::BinarySensor *s) { heating_indicator_  = s; }
  void set_supply_fan_running_sensor(binary_sensor::BinarySensor *s){ supply_fan_running_ = s; }
  void set_exhaust_fan_running_sensor(binary_sensor::BinarySensor *s){exhaust_fan_running_= s; }
  void set_external_contact_sensor(binary_sensor::BinarySensor *s)  { external_contact_   = s; }
  void set_fault_relay_sensor(binary_sensor::BinarySensor *s)       { fault_relay_        = s; }
  void set_co2_regulation_switch(switch_::Switch *s)   { co2_regulation_  = s; }
  void set_humidity_regulation_switch(switch_::Switch *s){ humidity_regulation_ = s; }
  void set_summer_mode_switch(switch_::Switch *s)      { summer_mode_     = s; }
  void set_fan(HeliosKwlFan *f) { fan_ = f; }
  void set_basic_fan_speed_number(number::Number *n)      { basic_fan_speed_n_    = n; }
  void set_max_fan_speed_number(number::Number *n)        { max_fan_speed_n_      = n; }
  void set_bypass_temp_number(number::Number *n)          { bypass_temp_n_        = n; }
  void set_preheating_temp_number(number::Number *n)      { preheating_temp_n_    = n; }
  void set_frost_alarm_temp_number(number::Number *n)     { frost_alarm_temp_n_   = n; }
  void set_frost_hysteresis_number(number::Number *n)     { frost_hysteresis_n_   = n; }
  void set_co2_setpoint_number(number::Number *n)         { co2_setpoint_n_       = n; }
  void set_humidity_setpoint_number(number::Number *n)    { humidity_setpoint_n_  = n; }
  void set_regulation_interval_number(number::Number *n)  { regulation_interval_n_= n; }
  void set_supply_fan_percent_number(number::Number *n)   { supply_fan_pct_n_     = n; }
  void set_exhaust_fan_percent_number(number::Number *n)  { exhaust_fan_pct_n_    = n; }
  void set_service_interval_number(number::Number *n)     { service_interval_n_   = n; }
  void set_boost_fireplace_mode_select(select::Select *s) { boost_fireplace_sel_  = s; }
  void set_humidity_auto_search_select(select::Select *s) { humidity_auto_sel_    = s; }
  void set_max_speed_continuous_select(select::Select *s) { max_speed_cont_sel_   = s; }

  // Actions
  void control_fan(bool on, optional<uint8_t> speed);
  void control_co2_regulation(bool e);
  void control_humidity_regulation(bool e);
  void control_summer_mode(bool e);
  void control_basic_fan_speed(uint8_t s);
  void control_max_fan_speed(uint8_t s);
  void control_bypass_temp(float c);
  void control_preheating_temp(float c);
  void control_frost_alarm_temp(float c);
  void control_frost_hysteresis(float c);
  void control_co2_setpoint(uint16_t p);
  void control_humidity_setpoint(uint8_t p);
  void control_regulation_interval(uint8_t m);
  void control_supply_fan_percent(uint8_t p);
  void control_exhaust_fan_percent(uint8_t p);
  void control_service_interval(uint8_t m);
  void control_boost_fireplace_mode(bool f);
  void control_humidity_auto_search(bool a);
  void control_max_speed_continuous(bool c);
  void trigger_boost_airflow();
  void trigger_boost_fireplace();
  void stop_boost_cycle();
  void acknowledge_maintenance();

  static float ntc_to_celsius(uint8_t n);
  static uint8_t celsius_to_ntc(float c);
  static float raw_to_humidity(uint8_t r);
  static uint8_t humidity_to_raw(float p);
  static uint8_t speed_to_bitmask(uint8_t s);
  static uint8_t bitmask_to_speed(uint8_t m);
  static uint16_t bytes_to_co2(uint8_t h, uint8_t l);
  static std::pair<uint8_t, uint8_t> co2_to_bytes(uint16_t p);

 protected:
  uint8_t address_{HELIOS_ADDR_DEFAULT};
  std::array<uint8_t, RX_BUFFER_SIZE> rx_buffer_{};
  size_t rx_buffer_len_{0};
  std::array<RegisterCache, REGISTER_COUNT> register_cache_{};

  // Tables de polling
  static constexpr size_t S2_TABLE_SIZE = 8;
  static constexpr size_t S3_TABLE_SIZE = 32;
  std::array<PollTask, S2_TABLE_SIZE> s2_tasks_{};
  size_t s2_count_{0}, s2_index_{0};
  std::array<PollTask, S3_TABLE_SIZE> s3_tasks_{};
  size_t s3_count_{0}, s3_index_{0};

  // Compteur alternance S2/S3
  uint8_t s2_turn_counter_{0};

  std::array<PendingWrite, MAX_PENDING_WRITES> pending_writes_{};
  bool broadcast_salve_active_{false};
  uint32_t broadcast_salve_start_{0};
  bool boost_cycle_active_{false};
  uint32_t last_rx_time_{0};
  HeliosKwlFan *fan_{nullptr};

  // Persistance switches
  int8_t desired_co2_reg_{-1}, desired_hum_reg_{-1}, desired_summer_{-1};

  // Deduplication : derniere valeur publiee par registre
  std::array<uint8_t, REGISTER_COUNT> last_published_{};
  std::array<bool, REGISTER_COUNT> has_published_{};

  // Entites
  sensor::Sensor *temperature_outside_{nullptr}, *temperature_extract_{nullptr};
  sensor::Sensor *temperature_supply_{nullptr}, *temperature_exhaust_{nullptr};
  sensor::Sensor *humidity_sensor1_{nullptr}, *humidity_sensor2_{nullptr};
  sensor::Sensor *co2_concentration_{nullptr}, *boost_remaining_{nullptr};
  sensor::Sensor *fault_code_{nullptr}, *service_months_{nullptr};
  sensor::Sensor *bypass_open_{nullptr}, *fan_speed_sensor_{nullptr};
  sensor::Sensor *fault_indicator_sensor_{nullptr}, *boost_state_sensor_{nullptr};
  text_sensor::TextSensor *fault_description_{nullptr};
  text_sensor::TextSensor *boost_active_text_{nullptr};
  text_sensor::TextSensor *bypass_state_text_{nullptr};
  binary_sensor::BinarySensor *preheating_active_{nullptr}, *freeze_alarm_{nullptr};
  binary_sensor::BinarySensor *co2_alarm_{nullptr}, *filter_maintenance_{nullptr};
  binary_sensor::BinarySensor *heating_indicator_{nullptr};
  binary_sensor::BinarySensor *supply_fan_running_{nullptr}, *exhaust_fan_running_{nullptr};
  binary_sensor::BinarySensor *external_contact_{nullptr}, *fault_relay_{nullptr};
  switch_::Switch *co2_regulation_{nullptr}, *humidity_regulation_{nullptr}, *summer_mode_{nullptr};
  number::Number *basic_fan_speed_n_{nullptr}, *max_fan_speed_n_{nullptr};
  number::Number *bypass_temp_n_{nullptr}, *preheating_temp_n_{nullptr};
  number::Number *frost_alarm_temp_n_{nullptr}, *frost_hysteresis_n_{nullptr};
  number::Number *co2_setpoint_n_{nullptr}, *humidity_setpoint_n_{nullptr};
  number::Number *regulation_interval_n_{nullptr};
  number::Number *supply_fan_pct_n_{nullptr}, *exhaust_fan_pct_n_{nullptr};
  number::Number *service_interval_n_{nullptr};
  select::Select *boost_fireplace_sel_{nullptr}, *humidity_auto_sel_{nullptr};
  select::Select *max_speed_cont_sel_{nullptr};

  // Methodes privees
  void accumulate_rx();
  bool process_rx_buffer();
  void handle_broadcast(uint8_t s, uint8_t reg, uint8_t val);
  void handle_command(uint8_t s, uint8_t r, uint8_t reg, uint8_t val);
  void publish_register(uint8_t reg, uint8_t value);
  bool should_publish(uint8_t reg, uint8_t value);  // dedup
  void publish_temperature(uint8_t reg, uint8_t value);
  void publish_humidity(uint8_t reg, uint8_t value);
  void publish_fan_speed(uint8_t value);
  void publish_states(uint8_t value);
  void publish_io_port(uint8_t value);
  void publish_alarms(uint8_t value);
  void publish_boost(uint8_t value);
  void publish_boost_remaining(uint8_t value);
  void publish_fault(uint8_t value);
  void publish_program_vars(uint8_t value);
  void publish_co2(uint8_t h, uint8_t l);
  void update_health_indicator();
  void enforce_desired_states();
  void check_pending_writes();
  bool is_broadcast_salve_active();
  bool do_one_s2_poll();  // retourne true si un poll S2 a ete fait
  bool do_one_s3_poll();  // retourne true si un poll S3 a ete fait
  void flush_rx(uint32_t timeout_ms = 10);
  static uint8_t checksum(const uint8_t *d, size_t l);
  static bool verify_checksum(const uint8_t *d, size_t l);
  static uint8_t count_ones(uint8_t b);
};

class HeliosKwlFan : public fan::Fan, public Component {
 public:
  explicit HeliosKwlFan(HeliosKwlComponent *p) : parent_(p) {}
  fan::FanTraits get_traits() override;
  void control(const fan::FanCall &call) override;
 protected:
  HeliosKwlComponent *parent_;
};

}  // namespace helios_kwl
}  // namespace esphome

#include "switch/helios_kwl_switch.h"
#include "number/helios_kwl_number.h"
#include "select/helios_kwl_select.h"
#include "button/helios_kwl_button.h"
