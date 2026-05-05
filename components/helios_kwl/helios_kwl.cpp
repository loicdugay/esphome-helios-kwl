// helios_kwl.cpp — Reecriture propre
// 4 primitives : read_register, write_register, write_bit, write_bits_masked
// write_bit utilise last_value_ si disponible (valeur du dernier poll S2, comme la vraie telecommande).
// write_register n'effectue pas de verification : la capture bus montre que la vraie telecommande
// ecrit et passe immediatement au poll suivant sans re-lire le registre.

#include "helios_kwl.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace helios_kwl {

static const char *const TAG = "helios_kwl";

static const int8_t NTC_TABLE[256] = {
  -74,-70,-66,-62,-59,-56,-54,-52,-50,-48,-47,-46,-44,-43,-42,-41,
  -40,-39,-38,-37,-36,-35,-34,-33,-33,-32,-31,-30,-30,-29,-28,-28,
  -27,-27,-26,-25,-25,-24,-24,-23,-23,-22,-22,-21,-21,-20,-20,-19,
  -19,-19,-18,-18,-17,-17,-16,-16,-16,-15,-15,-14,-14,-14,-13,-13,
  -12,-12,-12,-11,-11,-11,-10,-10, -9, -9, -9, -8, -8, -8, -7, -7,
   -7, -6, -6, -6, -5, -5, -5, -4, -4, -4, -3, -3, -3, -2, -2, -2,
   -1, -1, -1, -1,  0,  0,  0,  1,  1,  1,  2,  2,  2,  3,  3,  3,
    4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  7,  7,  7,  8,  8,  8,
    9,  9,  9, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14,
   14, 14, 15, 15, 15, 16, 16, 16, 17, 17, 18, 18, 18, 19, 19, 19,
   20, 20, 21, 21, 21, 22, 22, 22, 23, 23, 24, 24, 24, 25, 25, 26,
   26, 27, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 32, 32, 33, 33,
   34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39, 40, 40, 41, 41, 42,
   43, 43, 44, 45, 45, 46, 47, 48, 48, 49, 50, 51, 52, 53, 53, 54,
   55, 56, 57, 59, 60, 61, 62, 63, 65, 66, 68, 69, 71, 73, 75, 77,
   79, 81, 82, 86, 90, 93, 97,100,100,100,100,100,100,100,100,100,
};

static const char *fault_code_description(uint8_t code) {
  switch (code) {
    case 0x00: return "Aucun defaut";
    case 0x05: return "Sonde air souffle defectueuse";
    case 0x06: return "Alerte CO2 eleve";
    case 0x07: return "Sonde exterieure defectueuse";
    case 0x08: return "Sonde air interieur defectueuse";
    case 0x09: return "Risque gel circuit eau";
    case 0x0A: return "Sonde air rejete defectueuse";
    default:   return "Defaut inconnu";
  }
}

// ══ setup ══════════════════════════════════════════════════════════

void HeliosKwlComponent::setup() {
  ESP_LOGI(TAG, "Init Helios KWL — adresse 0x%02X", address_);
  for (auto &h : has_value_) h = false;

  s2_count_ = 0;
  s2_tasks_[s2_count_++] = {REG_FAN_SPEED,      POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_STATES,          POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_IO_PORT,         POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_BOOST_STATE,     POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_BOOST_REMAINING, POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_ALARMS,          POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_HUMIDITY1,       POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_HUMIDITY2,       POLL_INTERVAL_S2, 0};

  s3_count_ = 0;
  s3_tasks_[s3_count_++] = {REG_CO2_SENSORS,     POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_FAULT_CODE,      POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_POST_HEAT_ON,    POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_POST_HEAT_OFF,   POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_FLAGS_SYSTEM,    POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_FLAGS_MODE,      POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_SERVICE_MONTHS,  POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_PROGRAM_VARS,    POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_BASIC_SPEED,     POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_MAX_SPEED,       POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_BYPASS_TEMP,     POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_DEFROST_TEMP,    POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_FROST_ALARM_TEMP,POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_FROST_HYSTERESIS,POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_SUPPLY_FAN_PCT,  POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_EXHAUST_FAN_PCT, POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_CO2_SETPOINT_H,  POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_CO2_SETPOINT_L,  POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_HUMIDITY_SET,     POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_SERVICE_INTERVAL, POLL_INTERVAL_S3, 0};
  s3_tasks_[s3_count_++] = {REG_PROGRAM2,         POLL_INTERVAL_S3, 0};

  // Force tous S3 "dus" au boot
  uint32_t bt = millis();
  for (size_t i = 0; i < s3_count_; i++) s3_tasks_[i].last_polled = bt - POLL_INTERVAL_S3 - 1;

  last_rx_time_ = bt;
  ESP_LOGI(TAG, "S2: %u regs @6s | S3: %u regs @1h | ratio 5:1", s2_count_, s3_count_);
}

// ══ loop + dispatch ═══════════════════════════════════════════════

void HeliosKwlComponent::loop() {
  loop_read_bus();
}

void HeliosKwlComponent::loop_read_bus() {
  // Accumule
  while (available()) {
    if (rx_buffer_len_ >= RX_BUFFER_SIZE) rx_buffer_len_ = 0;
    uint8_t b; read_byte(&b); rx_buffer_[rx_buffer_len_++] = b;
    last_rx_time_ = millis();
  }
  // Extrait paquets
  for (int i = 0; i < 32 && rx_buffer_len_ >= HELIOS_PACKET_LEN; i++) {
    if (!process_one_packet()) break;
  }
}

bool HeliosKwlComponent::process_one_packet() {
  // Chercher start byte
  size_t start = rx_buffer_len_;
  for (size_t i = 0; i + HELIOS_PACKET_LEN <= rx_buffer_len_; i++) {
    if (rx_buffer_[i] == HELIOS_START_BYTE) { start = i; break; }
  }
  if (start == rx_buffer_len_) {
    if (rx_buffer_len_ > 0) { rx_buffer_[0] = rx_buffer_[rx_buffer_len_-1]; rx_buffer_len_ = 1; }
    return false;
  }
  if (start > 0) {
    std::copy(rx_buffer_.begin()+start, rx_buffer_.begin()+rx_buffer_len_, rx_buffer_.begin());
    rx_buffer_len_ -= start;
  }
  if (rx_buffer_len_ < HELIOS_PACKET_LEN) return false;
  if (!verify_checksum(rx_buffer_.data(), HELIOS_PACKET_LEN)) {
    std::copy(rx_buffer_.begin()+1, rx_buffer_.begin()+rx_buffer_len_, rx_buffer_.begin());
    rx_buffer_len_--;
    return true;
  }
  uint8_t src = rx_buffer_[1], dst = rx_buffer_[2], reg = rx_buffer_[3], val = rx_buffer_[4];
  dispatch_packet(src, dst, reg, val);
  std::copy(rx_buffer_.begin()+HELIOS_PACKET_LEN, rx_buffer_.begin()+rx_buffer_len_, rx_buffer_.begin());
  rx_buffer_len_ -= HELIOS_PACKET_LEN;
  return true;
}

void HeliosKwlComponent::dispatch_packet(uint8_t src, uint8_t dst, uint8_t reg, uint8_t val) {
  // Publie UNIQUEMENT si c'est une vraie valeur de registre :
  // - dst 0x20 : broadcast CM vers toutes telecommandes
  // - dst == address_ : reponse CM a notre poll
  if (dst != HELIOS_BROADCAST_RC && dst != address_) return;
  last_value_[reg] = val;
  has_value_[reg] = true;
  publish_register(reg, val);
}

// ══ Silence bus ═══════════════════════════════════════════════════

void HeliosKwlComponent::wait_bus_silence() {
  uint32_t deadline = millis() + 200;  // garde-fou
  while (millis() < deadline) {
    // Consomme tout ce qui arrive entre temps
    while (available()) {
      uint8_t b; read_byte(&b);
      if (rx_buffer_len_ < RX_BUFFER_SIZE) rx_buffer_[rx_buffer_len_++] = b;
      last_rx_time_ = millis();
    }
    if ((millis() - last_rx_time_) >= BUS_SILENCE_MS) return;
    yield();
  }
}

// ══ read_register ═════════════════════════════════════════════════

optional<uint8_t> HeliosKwlComponent::read_register(uint8_t reg) {
  // Protocole section 3.1 : max 3 tentatives si pas de reponse
  for (int attempt = 0; attempt < 3; attempt++) {
    wait_bus_silence();
    while (available()) { uint8_t b; read_byte(&b); }
    rx_buffer_len_ = 0;
    uint8_t req[6] = {HELIOS_START_BYTE, address_, HELIOS_MAINBOARD, 0x00, reg, 0};
    req[5] = checksum(req, 5);
    write_array(req, 6);
    flush();
    uint8_t buf[6]; size_t got = 0;
    uint32_t deadline = millis() + 50;
    while (got < 6 && millis() < deadline) {
      if (available()) { read_byte(&buf[got]); got++; last_rx_time_ = millis(); }
      else yield();
    }
    if (got == 6 && verify_checksum(buf, 6) && buf[1] == HELIOS_MAINBOARD && buf[2] == address_ && buf[3] == reg) {
      last_value_[reg] = buf[4];
      has_value_[reg] = true;
      return buf[4];
    }
    if (attempt < 2) delay(5);
  }
  ESP_LOGW(TAG, "read_register 0x%02X : pas de reponse apres 3 tentatives", reg);
  return {};
}

// ══ write_register ════════════════════════════════════════════════

// Registres confirmés 3 messages (Annex B ou tests) — ajouter un case après vérification
// que la valeur persiste au poll S2/S3 suivant. Défaut : 4 messages (conservatif).
static bool needs_extra_mainboard_write(uint8_t reg) {
  switch (reg) {
    case 0x29:  // REG_FAN_SPEED        — test log ✓
    case 0x71:  // REG_BOOST_STATE      — test log ✓
    case 0x79:  // REG_BOOST_REMAINING  — test log ✓
    case 0xA9:  // REG_BASIC_SPEED      — Annex B ✓
    case 0xAA:  // REG_PROGRAM_VARS     — test boost ✓
      return false;
    default:
      return true;
  }
}

bool HeliosKwlComponent::write_register(uint8_t reg, uint8_t value) {
  wait_bus_silence();
  while (available()) { uint8_t b; read_byte(&b); }
  rx_buffer_len_ = 0;
  ESP_LOGD(TAG, "write 0x%02X = 0x%02X (%s)", reg, value, needs_extra_mainboard_write(reg) ? "4msg" : "3msg");
  uint8_t m1[6] = {HELIOS_START_BYTE, address_, HELIOS_BROADCAST_RC,  reg, value, 0}; m1[5] = checksum(m1, 5);
  write_array(m1, 6); flush(); delay(3);
  uint8_t m2[6] = {HELIOS_START_BYTE, address_, HELIOS_BROADCAST_ALL, reg, value, 0}; m2[5] = checksum(m2, 5);
  write_array(m2, 6); flush(); delay(3);
  uint8_t m3[6] = {HELIOS_START_BYTE, address_, HELIOS_MAINBOARD,     reg, value, 0}; m3[5] = checksum(m3, 5);
  if (needs_extra_mainboard_write(reg)) {
    write_array(m3, 6); flush(); delay(3);  // msg 3 : 0x11 sans CRC double (pattern Annex B 0xAF)
  }
  write_array(m3, 6); write_byte(m3[5]); flush();  // msg final : 0x11 avec CRC double
  delay(30);
  while (available()) { uint8_t b; read_byte(&b); last_rx_time_ = millis(); }
  rx_buffer_len_ = 0;
  last_value_[reg] = value;
  has_value_[reg]  = true;
  return true;
}

// ══ write_bit / write_bits_masked ═════════════════════════════════

// Target: Register | Modifie N bits via masque — source = last_value_ (VMC, max 6s)
bool HeliosKwlComponent::write_bits_masked(uint8_t reg, uint8_t mask, uint8_t value) {
  if (!has_value_[reg]) {
    ESP_LOGW(TAG, "write 0x%02X ignore — pas encore lu par S2/S3", reg);
    return false;
  }
  uint8_t nv = (last_value_[reg] & ~mask) | (value & mask);
  if (nv == last_value_[reg]) return true;
  return write_register(reg, nv);
}

// Target: Register | Wrapper 1 bit — delègue à write_bits_masked
bool HeliosKwlComponent::write_bit(uint8_t reg, uint8_t bit, bool state) {
  return write_bits_masked(reg, 1u << bit, state ? (1u << bit) : 0u);
}

// ══ update — polling rotatif S2/S3 5:1 ═══════════════════════════

bool HeliosKwlComponent::do_one_s2_poll() {
  uint32_t now = millis();
  for (size_t i = 0; i < s2_count_; i++) {
    size_t idx = (s2_index_ + i) % s2_count_;
    PollTask &t = s2_tasks_[idx];
    if ((now - t.last_polled) < t.interval_ms) continue;
    if (t.reg == REG_BOOST_REMAINING && !boost_cycle_active_) { t.last_polled = now; continue; }
    auto r = read_register(t.reg);
    t.last_polled = now;
    if (r.has_value()) publish_register(t.reg, *r);
    s2_index_ = (idx + 1) % s2_count_;
    return true;
  }
  return false;
}

bool HeliosKwlComponent::do_one_s3_poll() {
  uint32_t now = millis();
  for (size_t i = 0; i < s3_count_; i++) {
    size_t idx = (s3_index_ + i) % s3_count_;
    PollTask &t = s3_tasks_[idx];
    if ((now - t.last_polled) < t.interval_ms) continue;
    auto r = read_register(t.reg);
    t.last_polled = now;
    if (r.has_value()) publish_register(t.reg, *r);
    s3_index_ = (idx + 1) % s3_count_;
    return true;
  }
  return false;
}

void HeliosKwlComponent::update() {
  s2_turn_counter_++;
  if (s2_turn_counter_ > S2_TURNS_BEFORE_S3) {
    s2_turn_counter_ = 0;
    if (!do_one_s3_poll()) do_one_s2_poll();
  } else {
    if (!do_one_s2_poll()) do_one_s3_poll();
  }
}

void HeliosKwlComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Helios KWL EC 300 Pro — Reecrit");
  ESP_LOGCONFIG(TAG, "  Adresse : 0x%02X", address_);
  ESP_LOGCONFIG(TAG, "  S2: %u regs @6s | S3: %u regs @1h", s2_count_, s3_count_);
}

// ══ Publications ═════════════════════════════════════════════════

void HeliosKwlComponent::publish_register(uint8_t reg, uint8_t value) {
  switch (reg) {
    case REG_TEMP_OUTSIDE: case REG_TEMP_EXTRACT: case REG_TEMP_SUPPLY: case REG_TEMP_EXHAUST:
      publish_temperature(reg, value); break;
    case REG_CO2_HIGH: case REG_CO2_LOW:
      if (has_value_[REG_CO2_HIGH] && has_value_[REG_CO2_LOW])
        publish_co2(last_value_[REG_CO2_HIGH], last_value_[REG_CO2_LOW]);
      break;
    case REG_HUMIDITY1: case REG_HUMIDITY2: publish_humidity(reg, value); break;
    case REG_FAN_SPEED: publish_fan_speed(value); break;
    case REG_STATES: publish_states(value); break;
    case REG_IO_PORT: publish_io_port(value); break;
    case REG_ALARMS: publish_alarms(value); break;
    case REG_BOOST_STATE: publish_boost(value); break;
    case REG_BOOST_REMAINING: publish_boost_remaining(value); break;
    case REG_FAULT_CODE: publish_fault(value); break;
    case REG_SERVICE_MONTHS: if (service_months_) service_months_->publish_state((float)value); break;
    case REG_PROGRAM_VARS: publish_program_vars(value); break;
    case REG_BASIC_SPEED: if (basic_fan_speed_n_) basic_fan_speed_n_->publish_state((float)bitmask_to_speed(value)); break;
    case REG_MAX_SPEED: if (max_fan_speed_n_) max_fan_speed_n_->publish_state((float)bitmask_to_speed(value)); break;
    case REG_BYPASS_TEMP: if (bypass_temp_n_) bypass_temp_n_->publish_state(ntc_to_celsius(value)); break;
    case REG_DEFROST_TEMP: if (preheating_temp_n_) preheating_temp_n_->publish_state(ntc_to_celsius(value)); break;
    case REG_FROST_ALARM_TEMP: if (frost_alarm_temp_n_) frost_alarm_temp_n_->publish_state(ntc_to_celsius(value)); break;
    case REG_FROST_HYSTERESIS: if (frost_hysteresis_n_) frost_hysteresis_n_->publish_state((float)value); break;
    case REG_SUPPLY_FAN_PCT: if (supply_fan_pct_n_) supply_fan_pct_n_->publish_state((float)value); break;
    case REG_EXHAUST_FAN_PCT: if (exhaust_fan_pct_n_) exhaust_fan_pct_n_->publish_state((float)value); break;
    case REG_CO2_SETPOINT_H: case REG_CO2_SETPOINT_L:
      if (has_value_[REG_CO2_SETPOINT_H] && has_value_[REG_CO2_SETPOINT_L] && co2_setpoint_n_)
        co2_setpoint_n_->publish_state((float)bytes_to_co2(last_value_[REG_CO2_SETPOINT_H], last_value_[REG_CO2_SETPOINT_L]));
      break;
    case REG_HUMIDITY_SET: if (humidity_setpoint_n_) humidity_setpoint_n_->publish_state(raw_to_humidity(value)); break;
    case REG_SERVICE_INTERVAL: if (service_interval_n_) service_interval_n_->publish_state((float)value); break;
    case REG_PROGRAM2:
      if (max_speed_cont_sel_) max_speed_cont_sel_->publish_state((value >> BIT_MAX_SPEED_CONT) & 1 ? "Ventilation maximale forcee" : "Normal");
      break;
    default: break;
  }
}

void HeliosKwlComponent::publish_temperature(uint8_t reg, uint8_t v) {
  float c = ntc_to_celsius(v);
  if (reg == REG_TEMP_OUTSIDE && temperature_outside_) temperature_outside_->publish_state(c);
  else if (reg == REG_TEMP_EXTRACT && temperature_extract_) temperature_extract_->publish_state(c);
  else if (reg == REG_TEMP_SUPPLY && temperature_supply_) temperature_supply_->publish_state(c);
  else if (reg == REG_TEMP_EXHAUST && temperature_exhaust_) temperature_exhaust_->publish_state(c);
}

void HeliosKwlComponent::publish_humidity(uint8_t reg, uint8_t v) {
  float p = raw_to_humidity(v);
  if (reg == REG_HUMIDITY1 && humidity_sensor1_) humidity_sensor1_->publish_state(p);
  if (reg == REG_HUMIDITY2 && humidity_sensor2_) humidity_sensor2_->publish_state(p);
}

void HeliosKwlComponent::publish_fan_speed(uint8_t v) {
  uint8_t speed = bitmask_to_speed(v);
  if (fan_speed_sensor_) fan_speed_sensor_->publish_state((float)speed);
  // Synchroniser le fan entity — mais SEULEMENT si le bit power est ON.
  // Quand la VMC est eteinte, 0x29 garde la derniere vitesse (ex: 2) mais
  // on ne doit pas forcer fan_->state = true sinon ca bagote avec publish_states.
  if (fan_ && speed > 0) {
    bool power_on = has_value_[REG_STATES] && ((last_value_[REG_STATES] >> BIT_POWER) & 1);
    fan_->speed = speed;
    if (power_on && !fan_->state) {
      fan_->state = true;
      fan_->publish_state();
    } else if (power_on) {
      fan_->publish_state();  // speed a change, power deja ON
    }
    // Si power OFF, on met a jour speed silencieusement sans publier state=ON
  }
}

void HeliosKwlComponent::publish_states(uint8_t v) {
  bool power = (v >> BIT_POWER) & 1;
  bool co2r = (v >> BIT_CO2_REG) & 1;
  bool humr = (v >> BIT_HUMIDITY_REG) & 1;
  bool summer = (v >> BIT_SUMMER_MODE) & 1;
  fan_state_ready_ = true;
  if (fan_ && fan_->state != power) { fan_->state = power; fan_->publish_state(); }
  if (co2_regulation_)      co2_regulation_->publish_state(co2r);
  if (humidity_regulation_) humidity_regulation_->publish_state(humr);
  if (summer_mode_)         summer_mode_->publish_state(summer);
  if (heating_indicator_)   heating_indicator_->publish_state((v >> BIT_HEATING) & 1);
  if (filter_maintenance_)  filter_maintenance_->publish_state((v >> BIT_FILTER_MAINT) & 1);
  if (bypass_state_text_)   bypass_state_text_->publish_state(summer ? "Air frais" : "Chaleur conservee");
  update_health_indicator();
}

void HeliosKwlComponent::publish_io_port(uint8_t v) {
  if (bypass_open_)         bypass_open_->publish_state((float)((v >> BIT_BYPASS_OPEN) & 1));
  if (supply_fan_running_)  supply_fan_running_->publish_state(!((v >> BIT_SUPPLY_FAN) & 1));
  if (exhaust_fan_running_) exhaust_fan_running_->publish_state(!((v >> BIT_EXHAUST_FAN) & 1));
  if (preheating_active_)   preheating_active_->publish_state((v >> BIT_PREHEATING) & 1);
  if (external_contact_)    external_contact_->publish_state((v >> BIT_EXT_CONTACT) & 1);
  if (fault_relay_)         fault_relay_->publish_state(!((v >> BIT_FAULT_RELAY) & 1));
}

void HeliosKwlComponent::publish_alarms(uint8_t v) {
  if (co2_alarm_)    co2_alarm_->publish_state((v >> BIT_CO2_ALARM) & 1);
  if (freeze_alarm_) freeze_alarm_->publish_state((v >> BIT_FREEZE_ALARM) & 1);
  update_health_indicator();
}

void HeliosKwlComponent::publish_boost(uint8_t v) {
  bool running = (v >> BIT_BOOST_RUNNING) & 1;
  boost_cycle_active_ = running;
  const char *s = "Normal"; float fv = 0;
  if (running) {
    bool fresh_air = has_value_[REG_PROGRAM_VARS] && ((last_value_[REG_PROGRAM_VARS] >> BIT_BOOST_FIRE_MODE) & 1);
    if (fresh_air) { s = "Cycle Plein Air"; fv = 1; } else { s = "Cycle Cheminee"; fv = 2; }
  }
  if (boost_active_text_) boost_active_text_->publish_state(s);
  if (boost_state_sensor_) boost_state_sensor_->publish_state(fv);
}

void HeliosKwlComponent::publish_boost_remaining(uint8_t v) {
  if (boost_remaining_) boost_remaining_->publish_state((float)v);
}

void HeliosKwlComponent::publish_fault(uint8_t v) {
  if (fault_code_) fault_code_->publish_state((float)v);
  if (fault_description_) fault_description_->publish_state(fault_code_description(v));
  update_health_indicator();
}

void HeliosKwlComponent::publish_program_vars(uint8_t v) {
  uint8_t iv = v & 0x0F;
  bool ha = (v >> BIT_HUMIDITY_AUTO) & 1;
  bool fm = (v >> BIT_BOOST_FIRE_MODE) & 1;
  if (regulation_interval_n_ && iv > 0) regulation_interval_n_->publish_state((float)iv);
  if (humidity_auto_sel_) humidity_auto_sel_->publish_state(ha ? "Apprentissage auto" : "Seuil manuel");
  if (boost_fireplace_sel_) boost_fireplace_sel_->publish_state(fm ? "Cycle Plein Air" : "Cycle Cheminee");
}

void HeliosKwlComponent::publish_co2(uint8_t h, uint8_t l) {
  if (co2_concentration_) co2_concentration_->publish_state((float)bytes_to_co2(h, l));
}

void HeliosKwlComponent::update_health_indicator() {
  bool fault = has_value_[REG_STATES] && ((last_value_[REG_STATES] >> BIT_FAULT) & 1);
  bool filter = has_value_[REG_STATES] && ((last_value_[REG_STATES] >> BIT_FILTER_MAINT) & 1);
  bool co2a = has_value_[REG_ALARMS] && ((last_value_[REG_ALARMS] >> BIT_CO2_ALARM) & 1);
  bool freeze = has_value_[REG_ALARMS] && ((last_value_[REG_ALARMS] >> BIT_FREEZE_ALARM) & 1);
  bool fc = has_value_[REG_FAULT_CODE] && last_value_[REG_FAULT_CODE] != 0;
  uint8_t v = (fault || co2a || freeze || fc) ? 2 : (filter ? 1 : 0);
  if (v != last_health_) {
    last_health_ = v;
    if (fault_indicator_sensor_) fault_indicator_sensor_->publish_state((float)v);
  }
}

// ══ Helpers protocole ═════════════════════════════════════════════

uint8_t HeliosKwlComponent::checksum(const uint8_t *d, size_t l) {
  uint8_t c = 0; for (size_t i = 0; i < l; i++) c += d[i]; return c;
}
bool HeliosKwlComponent::verify_checksum(const uint8_t *d, size_t l) {
  return l >= 2 && checksum(d, l-1) == d[l-1];
}
uint8_t HeliosKwlComponent::count_ones(uint8_t b) {
  uint8_t c = 0; for (uint8_t i = 0; i < 8; i++) { c += b & 1; b >>= 1; } return c;
}

float HeliosKwlComponent::ntc_to_celsius(uint8_t n) { return (float)NTC_TABLE[n]; }
uint8_t HeliosKwlComponent::celsius_to_ntc(float c) {
  if (c < -30) c = -30; if (c > 60) c = 60;
  int t = (int)c; uint8_t best = 0x64; int bd = 255;
  for (int i = 0x20; i <= 0xE0; i++) { int d = abs((int)NTC_TABLE[i] - t); if (d < bd) { bd = d; best = i; } }
  return best;
}
float HeliosKwlComponent::raw_to_humidity(uint8_t r) {
  if (r < 51) return 0; float p = (r - 51.0f) / 2.04f; return p > 100 ? 100 : p;
}
uint8_t HeliosKwlComponent::humidity_to_raw(float p) {
  if (p < 0) p = 0; if (p > 100) p = 100; return (uint8_t)(p * 2.04f + 51);
}
uint8_t HeliosKwlComponent::speed_to_bitmask(uint8_t s) {
  return s == 0 ? 0 : (uint8_t)((1u << (s > 8 ? 8 : s)) - 1u);
}
uint8_t HeliosKwlComponent::bitmask_to_speed(uint8_t m) { return count_ones(m); }
uint16_t HeliosKwlComponent::bytes_to_co2(uint8_t h, uint8_t l) { return ((uint16_t)h << 8) | l; }
std::pair<uint8_t,uint8_t> HeliosKwlComponent::co2_to_bytes(uint16_t p) {
  return {(uint8_t)(p >> 8), (uint8_t)(p & 0xFF)};
}

// ══ Actions ═══════════════════════════════════════════════════════

void HeliosKwlComponent::control_fan(bool on, optional<uint8_t> spd) {
  ESP_LOGI(TAG, "[HA] Ventilation -> %s%s", on ? "ON" : "OFF",
           spd.has_value() ? (" vitesse " + std::to_string(*spd)).c_str() : "");
  write_bit(REG_STATES, BIT_POWER, on);
  if (spd.has_value() && *spd >= 1 && *spd <= 8) write_register(REG_FAN_SPEED, speed_to_bitmask(*spd));
}
void HeliosKwlComponent::set_fan_speed(uint8_t s) {
  ESP_LOGI(TAG, "[HA] Vitesse -> %d", s);
  if (s == 0) write_bit(REG_STATES, BIT_POWER, false);
  else write_register(REG_FAN_SPEED, speed_to_bitmask(s));
}
void HeliosKwlComponent::set_fan_on(bool on) {
  ESP_LOGI(TAG, "[HA] Alimentation -> %s", on ? "ON" : "OFF");
  write_bit(REG_STATES, BIT_POWER, on);
}

void HeliosKwlComponent::control_co2_regulation(bool e) {
  ESP_LOGI(TAG, "[HA] Regulation CO2 -> %s", e ? "ON" : "OFF");
  write_bit(REG_STATES, BIT_CO2_REG, e);
}
void HeliosKwlComponent::control_humidity_regulation(bool e) {
  ESP_LOGI(TAG, "[HA] Regulation HR -> %s", e ? "ON" : "OFF");
  write_bit(REG_STATES, BIT_HUMIDITY_REG, e);
}
void HeliosKwlComponent::control_summer_mode(bool e) {
  ESP_LOGI(TAG, "[HA] Mode ete -> %s", e ? "ON" : "OFF");
  write_bit(REG_STATES, BIT_SUMMER_MODE, e);
}

void HeliosKwlComponent::control_basic_fan_speed(uint8_t s)   { write_register(REG_BASIC_SPEED, speed_to_bitmask(s)); }
void HeliosKwlComponent::control_max_fan_speed(uint8_t s)     { write_register(REG_MAX_SPEED, speed_to_bitmask(s)); }
void HeliosKwlComponent::control_bypass_temp(float c)         { write_register(REG_BYPASS_TEMP, celsius_to_ntc(c)); }
void HeliosKwlComponent::control_preheating_temp(float c)     { write_register(REG_DEFROST_TEMP, celsius_to_ntc(c)); }
void HeliosKwlComponent::control_frost_alarm_temp(float c)    { write_register(REG_FROST_ALARM_TEMP, celsius_to_ntc(c)); }
void HeliosKwlComponent::control_frost_hysteresis(float c)    { write_register(REG_FROST_HYSTERESIS, (uint8_t)c); }
void HeliosKwlComponent::control_co2_setpoint(uint16_t p) {
  auto b = co2_to_bytes(p);
  write_register(REG_CO2_SETPOINT_H, b.first);
  write_register(REG_CO2_SETPOINT_L, b.second);
}
void HeliosKwlComponent::control_humidity_setpoint(uint8_t p) { write_register(REG_HUMIDITY_SET, humidity_to_raw((float)p)); }
void HeliosKwlComponent::control_regulation_interval(uint8_t m) { write_bits_masked(REG_PROGRAM_VARS, 0x0F, m & 0x0F); }
void HeliosKwlComponent::control_supply_fan_percent(uint8_t p)  { write_register(REG_SUPPLY_FAN_PCT, p); }
void HeliosKwlComponent::control_exhaust_fan_percent(uint8_t p) { write_register(REG_EXHAUST_FAN_PCT, p); }
void HeliosKwlComponent::control_service_interval(uint8_t m)    { write_register(REG_SERVICE_INTERVAL, m); }

void HeliosKwlComponent::control_boost_fireplace_mode(bool f) { write_bit(REG_PROGRAM_VARS, BIT_BOOST_FIRE_MODE, f); }
void HeliosKwlComponent::control_humidity_auto_search(bool a) { write_bit(REG_PROGRAM_VARS, BIT_HUMIDITY_AUTO, a); }
void HeliosKwlComponent::control_max_speed_continuous(bool c) { write_bit(REG_PROGRAM2, BIT_MAX_SPEED_CONT, c); }

void HeliosKwlComponent::trigger_boost_airflow() {
  ESP_LOGI(TAG, "Cycle Plein Air (45 min)");
  write_bit(REG_PROGRAM_VARS, BIT_BOOST_FIRE_MODE, true);
  write_register(REG_BOOST_REMAINING, 45);
  write_bit(REG_BOOST_STATE, BIT_BOOST_ACTIVATE, true);
}
void HeliosKwlComponent::trigger_boost_fireplace() {
  ESP_LOGI(TAG, "Cycle Cheminee (15 min)");
  write_bit(REG_PROGRAM_VARS, BIT_BOOST_FIRE_MODE, false);
  write_register(REG_BOOST_REMAINING, 15);
  write_bit(REG_BOOST_STATE, BIT_BOOST_ACTIVATE, true);
}
void HeliosKwlComponent::stop_boost_cycle() {
  ESP_LOGI(TAG, "Arret cycle — timer 0x79 a 1");
  write_register(REG_BOOST_REMAINING, 1);
}
void HeliosKwlComponent::acknowledge_maintenance() {
  ESP_LOGI(TAG, "Reset filtres");
  auto iv = read_register(REG_SERVICE_INTERVAL);
  write_register(REG_SERVICE_MONTHS, iv.has_value() ? *iv : 4);
}

// ══ Fan entity ════════════════════════════════════════════════════

fan::FanTraits HeliosKwlFan::get_traits() { return fan::FanTraits(false, true, false, 8); }

void HeliosKwlFan::control(const fan::FanCall &call) {
  if (!parent_) return;
  if (!parent_->fan_is_ready()) {
    ESP_LOGW(TAG, "Fan control ignore — VMC pas encore lue");
    return;
  }
  optional<uint8_t> spd;
  if (call.get_speed().has_value()) spd = (uint8_t)*call.get_speed();
  bool on = this->state;
  if (call.get_state().has_value()) on = *call.get_state();
  parent_->control_fan(on, spd);
  if (call.get_state().has_value()) this->state = *call.get_state();
  if (spd.has_value()) this->speed = *spd;
  this->publish_state();
}

}  // namespace helios_kwl
}  // namespace esphome
