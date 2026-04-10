// helios_kwl.cpp — Phase 2C-fix
// FIX 1: alternance S2/S3 (5 tours S2, 1 tour S3) pour eviter famine
// FIX 2: deduplication — ne publie que si la valeur du registre a change

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

// ══════════════════════════════════════════════════════════════
// setup()
// ══════════════════════════════════════════════════════════════

void HeliosKwlComponent::setup() {
  ESP_LOGI(TAG, "Init Helios KWL — adresse 0x%02X", address_);
  for (auto &e : register_cache_) { e.valid = false; e.value = 0; e.last_update = 0; }
  for (auto &pw : pending_writes_) { pw.active = false; }
  for (auto &hp : has_published_) { hp = false; }

  // S2 : polling cyclique 6s
  s2_count_ = 0;
  s2_tasks_[s2_count_++] = {REG_FAN_SPEED,      POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_STATES,          POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_IO_PORT,         POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_BOOST_STATE,     POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_BOOST_REMAINING, POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_ALARMS,          POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_HUMIDITY1,       POLL_INTERVAL_S2, 0};
  s2_tasks_[s2_count_++] = {REG_HUMIDITY2,       POLL_INTERVAL_S2, 0};
  s2_index_ = 0;

  // S3 : init + 1h
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
  s3_index_ = 0;
  s2_turn_counter_ = 0;

  // CRITICAL: forcer tous les S3 last_polled pour qu'ils soient "dus" immediatement au boot.
  // Avec last_polled=0 et interval=3600000, le check (millis()-0) >= 3600000 est false
  // pendant la premiere heure ! On utilise l'underflow unsigned pour forcer le premier poll.
  uint32_t boot_time = millis();
  for (size_t i = 0; i < s3_count_; i++) {
    s3_tasks_[i].last_polled = boot_time - POLL_INTERVAL_S3 - 1;
  }

  last_rx_time_ = millis();
  ESP_LOGI(TAG, "S2: %u regs @6s | S3: %u regs @1h | ratio 5:1", s2_count_, s3_count_);
}

// ══════════════════════════════════════════════════════════════
// loop() — ecoute passive (Strategie 1)
// ══════════════════════════════════════════════════════════════

void HeliosKwlComponent::loop() {
  accumulate_rx();
  const size_t max_p = RX_BUFFER_SIZE / HELIOS_PACKET_LEN;
  for (size_t i = 0; i < max_p && rx_buffer_len_ >= HELIOS_PACKET_LEN; i++) {
    if (!process_rx_buffer()) {
      if (rx_buffer_len_ > 0) { std::copy(rx_buffer_.begin()+1, rx_buffer_.begin()+rx_buffer_len_, rx_buffer_.begin()); rx_buffer_len_--; }
      break;
    }
  }
}

void HeliosKwlComponent::accumulate_rx() {
  for (size_t i = 0; i < RX_BUFFER_SIZE && available(); i++) {
    if (rx_buffer_len_ >= RX_BUFFER_SIZE) rx_buffer_len_ = 0;
    uint8_t b; read_byte(&b); rx_buffer_[rx_buffer_len_++] = b; last_rx_time_ = millis();
  }
}

bool HeliosKwlComponent::process_rx_buffer() {
  if (rx_buffer_len_ < HELIOS_PACKET_LEN) return false;
  size_t start = rx_buffer_len_;
  for (size_t i = 0; i + HELIOS_PACKET_LEN <= rx_buffer_len_; i++) { if (rx_buffer_[i] == HELIOS_START_BYTE) { start = i; break; } }
  if (start == rx_buffer_len_) { if (rx_buffer_len_ > 0) { rx_buffer_[0] = rx_buffer_[rx_buffer_len_-1]; rx_buffer_len_ = 1; } return false; }
  if (start > 0) { std::copy(rx_buffer_.begin()+start, rx_buffer_.begin()+rx_buffer_len_, rx_buffer_.begin()); rx_buffer_len_ -= start; }
  if (rx_buffer_len_ < HELIOS_PACKET_LEN) return false;
  if (!verify_checksum(rx_buffer_.data(), HELIOS_PACKET_LEN)) {
    std::copy(rx_buffer_.begin()+1, rx_buffer_.begin()+rx_buffer_len_, rx_buffer_.begin()); rx_buffer_len_--; return true;
  }
  uint8_t src = rx_buffer_[1], dst = rx_buffer_[2], reg = rx_buffer_[3], val = rx_buffer_[4];
  register_cache_[reg] = {val, millis(), true};

  if (dst == HELIOS_BROADCAST_RC || dst == HELIOS_BROADCAST_ALL) handle_broadcast(src, reg, val);
  else if (dst == address_) handle_command(src, dst, reg, val);

  std::copy(rx_buffer_.begin()+HELIOS_PACKET_LEN, rx_buffer_.begin()+rx_buffer_len_, rx_buffer_.begin());
  rx_buffer_len_ -= HELIOS_PACKET_LEN; return true;
}

// Imperatif n°4 — methode robuste : silence reel du bus
// Le protocole dit : attendre que le bus soit silencieux avant d'emettre.
// On mesure le temps depuis le dernier octet recu (last_rx_time_).
// Si < 10ms : le bus est encore actif (broadcast ou autre trame en cours).
// Le timer fixe BROADCAST_SALVE_MS est supprime — on se base sur le silence reel.
bool HeliosKwlComponent::is_broadcast_salve_active() {
  // Bus considere libre si aucun octet recu depuis 10ms
  // A 9600 bps, 6 octets = ~6.3ms, donc 10ms = silence entre 2 paquets
  return (millis() - last_rx_time_) < 10;
}

void HeliosKwlComponent::handle_broadcast(uint8_t s, uint8_t reg, uint8_t val) { publish_register(reg, val); }
void HeliosKwlComponent::handle_command(uint8_t s, uint8_t r, uint8_t reg, uint8_t val) { publish_register(reg, val); }

// ══════════════════════════════════════════════════════════════
// should_publish() — deduplication
// Retourne true seulement si la valeur a change ou jamais publiee
// ══════════════════════════════════════════════════════════════

bool HeliosKwlComponent::should_publish(uint8_t reg, uint8_t value) {
  if (!has_published_[reg] || last_published_[reg] != value) {
    last_published_[reg] = value;
    has_published_[reg] = true;
    return true;
  }
  return false;
}

// ══════════════════════════════════════════════════════════════
// do_one_s2_poll() / do_one_s3_poll()
// Cherche la prochaine tache due, la polle, retourne true si fait
// ══════════════════════════════════════════════════════════════

bool HeliosKwlComponent::do_one_s2_poll() {
  uint32_t now = millis();
  for (size_t i = 0; i < s2_count_; i++) {
    size_t idx = (s2_index_ + i) % s2_count_;
    PollTask &t = s2_tasks_[idx];
    if ((now - t.last_polled) < t.interval_ms) continue;
    if (t.reg == REG_BOOST_REMAINING && !boost_cycle_active_) { t.last_polled = now; continue; }
    auto r = poll_register(t.reg, 1);
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
    auto r = poll_register(t.reg, 1);
    t.last_polled = now;
    if (r.has_value()) publish_register(t.reg, *r);
    s3_index_ = (idx + 1) % s3_count_;
    return true;
  }
  return false;
}

// ══════════════════════════════════════════════════════════════
// update() — alternance 5:1 entre S2 et S3
// Garantit que S3 avance meme quand S2 est toujours "du"
// ══════════════════════════════════════════════════════════════

void HeliosKwlComponent::update() {
  if (is_broadcast_salve_active()) return;

  enforce_desired_states();
  check_pending_writes();

  // Alternance : 5 tours S2, puis 1 tour S3
  s2_turn_counter_++;
  if (s2_turn_counter_ > S2_TURNS_BEFORE_S3) {
    // Tour S3
    s2_turn_counter_ = 0;
    if (!do_one_s3_poll()) {
      // Rien a faire en S3, donner le slot a S2
      do_one_s2_poll();
    }
  } else {
    // Tour S2
    if (!do_one_s2_poll()) {
      // Rien a faire en S2, donner le slot a S3
      do_one_s3_poll();
    }
  }
}

void HeliosKwlComponent::enforce_desired_states() {
  auto c = get_cached_value(REG_STATES); if (!c.has_value()) return;
  uint8_t cur = *c, desired = cur; bool need = false;
  auto apply = [&](int8_t want, uint8_t bit) {
    if (want < 0) return; bool w = (want==1), h = (cur>>bit)&1;
    if (w != h) { desired = w ? (desired|(1u<<bit)) : (desired&~(1u<<bit)); need = true; }
  };
  apply(desired_co2_reg_, BIT_CO2_REG); apply(desired_hum_reg_, BIT_HUMIDITY_REG); apply(desired_summer_, BIT_SUMMER_MODE);
  if (need) write_register(REG_STATES, desired);
}

void HeliosKwlComponent::check_pending_writes() {
  uint32_t now = millis();
  for (auto &pw : pending_writes_) {
    if (!pw.active || (now - pw.written_at) < 500) continue;
    auto c = get_cached_value(pw.reg);
    if (c.has_value() && *c == pw.value) { pw.active = false; continue; }
    if (pw.retries > 0) { pw.retries--; pw.written_at = now; write_register(pw.reg, pw.value); }
    else { ESP_LOGE(TAG, "Write failed reg=0x%02X", pw.reg); pw.active = false; }
  }
}

void HeliosKwlComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Helios KWL EC 300 Pro — Phase 2C-fix");
  ESP_LOGCONFIG(TAG, "  Adresse : 0x%02X", address_);
  ESP_LOGCONFIG(TAG, "  S2 (6s) : %u | S3 (1h) : %u | ratio %u:1", s2_count_, s3_count_, S2_TURNS_BEFORE_S3);
}

// ══════════════════════════════════════════════════════════════
// publish_register() — avec deduplication
// ══════════════════════════════════════════════════════════════

void HeliosKwlComponent::publish_register(uint8_t reg, uint8_t value) {
  // Dedup ciblee : seule update_health_indicator() est dedupliquee (voir plus bas).
  // Les publications registre passent toujours — ESPHome gere son propre dedup
  // via les filtres (EMA etc) cote YAML.

  switch (reg) {
    case REG_TEMP_OUTSIDE: case REG_TEMP_EXTRACT: case REG_TEMP_SUPPLY: case REG_TEMP_EXHAUST:
      publish_temperature(reg, value); break;
    case REG_CO2_HIGH: case REG_CO2_LOW: {
      auto h = get_cached_value(REG_CO2_HIGH); auto l = get_cached_value(REG_CO2_LOW);
      if (h.has_value() && l.has_value()) publish_co2(*h, *l); break; }
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
    case REG_CO2_SETPOINT_H: case REG_CO2_SETPOINT_L: {
      auto h = get_cached_value(REG_CO2_SETPOINT_H); auto l = get_cached_value(REG_CO2_SETPOINT_L);
      if (h.has_value() && l.has_value() && co2_setpoint_n_) co2_setpoint_n_->publish_state((float)bytes_to_co2(*h,*l)); break; }
    case REG_HUMIDITY_SET: if (humidity_setpoint_n_) humidity_setpoint_n_->publish_state(raw_to_humidity(value)); break;
    case REG_SERVICE_INTERVAL: if (service_interval_n_) service_interval_n_->publish_state((float)value); break;
    case REG_PROGRAM2:
      if (max_speed_cont_sel_) max_speed_cont_sel_->publish_state((value>>BIT_MAX_SPEED_CONT)&1 ? "Ventilation maximale forcee" : "Normal"); break;
    default: break;
  }
}

// ── Publications specialisees ──

void HeliosKwlComponent::publish_temperature(uint8_t reg, uint8_t v) {
  float c = ntc_to_celsius(v);
  switch (reg) {
    case REG_TEMP_OUTSIDE: if (temperature_outside_) temperature_outside_->publish_state(c); break;
    case REG_TEMP_EXTRACT: if (temperature_extract_) temperature_extract_->publish_state(c); break;
    case REG_TEMP_SUPPLY:  if (temperature_supply_)  temperature_supply_->publish_state(c); break;
    case REG_TEMP_EXHAUST: if (temperature_exhaust_) temperature_exhaust_->publish_state(c); break;
    default: break;
  }
}
void HeliosKwlComponent::publish_humidity(uint8_t reg, uint8_t v) {
  float p = raw_to_humidity(v);
  if (reg == REG_HUMIDITY1 && humidity_sensor1_) humidity_sensor1_->publish_state(p);
  if (reg == REG_HUMIDITY2 && humidity_sensor2_) humidity_sensor2_->publish_state(p);
}
void HeliosKwlComponent::publish_fan_speed(uint8_t v) {
  uint8_t speed = bitmask_to_speed(v);
  if (fan_speed_sensor_) fan_speed_sensor_->publish_state((float)speed);
  // Synchroniser l'entite fan avec la vitesse reelle lue depuis la VMC
  if (fan_ && speed > 0) {
    fan_->speed = speed;
    if (!fan_->state) { fan_->state = true; }  // La VMC tourne
    fan_->publish_state();
  }
}

void HeliosKwlComponent::publish_states(uint8_t v) {
  bool power=(v>>BIT_POWER)&1;
  bool co2r=(v>>BIT_CO2_REG)&1, humr=(v>>BIT_HUMIDITY_REG)&1, summer=(v>>BIT_SUMMER_MODE)&1;
  // Synchroniser l'entite fan ON/OFF avec le bit power reel de la VMC
  if (fan_) {
    if (fan_->state != power) {
      fan_->state = power;
      fan_->publish_state();
    }
  }
  if (co2_regulation_)      co2_regulation_->publish_state(co2r);
  if (humidity_regulation_) humidity_regulation_->publish_state(humr);
  if (summer_mode_)         summer_mode_->publish_state(summer);
  if (heating_indicator_)   heating_indicator_->publish_state((v>>BIT_HEATING)&1);
  if (filter_maintenance_)  filter_maintenance_->publish_state((v>>BIT_FILTER_MAINT)&1);
  if (bypass_state_text_)   bypass_state_text_->publish_state(summer ? "Air frais" : "Chaleur conservee");
  update_health_indicator();
  if (desired_co2_reg_>=0 && co2r==(desired_co2_reg_==1)) desired_co2_reg_ = -1;
  if (desired_hum_reg_>=0 && humr==(desired_hum_reg_==1)) desired_hum_reg_ = -1;
  if (desired_summer_>=0  && summer==(desired_summer_==1)) desired_summer_ = -1;
}
void HeliosKwlComponent::publish_io_port(uint8_t v) {
  if (bypass_open_)         bypass_open_->publish_state((float)((v>>BIT_BYPASS_OPEN)&1));
  if (supply_fan_running_)  supply_fan_running_->publish_state(!((v>>BIT_SUPPLY_FAN)&1));
  if (exhaust_fan_running_) exhaust_fan_running_->publish_state(!((v>>BIT_EXHAUST_FAN)&1));
  if (preheating_active_)   preheating_active_->publish_state((v>>BIT_PREHEATING)&1);
  if (external_contact_)    external_contact_->publish_state((v>>BIT_EXT_CONTACT)&1);
  if (fault_relay_)         fault_relay_->publish_state(!((v>>BIT_FAULT_RELAY)&1));
}
void HeliosKwlComponent::publish_alarms(uint8_t v) {
  if (co2_alarm_)    co2_alarm_->publish_state((v>>BIT_CO2_ALARM)&1);
  if (freeze_alarm_) freeze_alarm_->publish_state((v>>BIT_FREEZE_ALARM)&1);
  update_health_indicator();
}
void HeliosKwlComponent::publish_boost(uint8_t v) {
  bool running = (v>>BIT_BOOST_RUNNING)&1; boost_cycle_active_ = running;
  const char *s = "Normal"; float fv = 0;
  if (running) { auto p = get_cached_value(REG_PROGRAM_VARS); bool ib = p.has_value()&&((*p>>BIT_BOOST_FIRE_MODE)&1);
    if (ib) { s = "Cycle Plein Air"; fv = 1; } else { s = "Cycle Cheminee"; fv = 2; } }
  if (boost_active_text_) boost_active_text_->publish_state(s);
  if (boost_state_sensor_) boost_state_sensor_->publish_state(fv);
}
void HeliosKwlComponent::publish_boost_remaining(uint8_t v) { if (boost_remaining_) boost_remaining_->publish_state((float)v); }
void HeliosKwlComponent::publish_fault(uint8_t v) {
  if (fault_code_) fault_code_->publish_state((float)v);
  if (fault_description_) fault_description_->publish_state(fault_code_description(v));
  update_health_indicator();
}
void HeliosKwlComponent::publish_program_vars(uint8_t v) {
  uint8_t iv = v & 0x0F; bool ha = (v>>BIT_HUMIDITY_AUTO)&1, fm = (v>>BIT_BOOST_FIRE_MODE)&1;
  if (regulation_interval_n_ && iv > 0) regulation_interval_n_->publish_state((float)iv);
  if (humidity_auto_sel_) humidity_auto_sel_->publish_state(ha ? "Apprentissage auto" : "Seuil manuel");
  if (boost_fireplace_sel_) boost_fireplace_sel_->publish_state(fm ? "Cycle Plein Air" : "Cycle Cheminee");
}
void HeliosKwlComponent::publish_co2(uint8_t h, uint8_t l) { if (co2_concentration_) co2_concentration_->publish_state((float)bytes_to_co2(h,l)); }

void HeliosKwlComponent::update_health_indicator() {
  auto st=get_cached_value(REG_STATES); auto al=get_cached_value(REG_ALARMS); auto ft=get_cached_value(REG_FAULT_CODE);
  bool fault=st.has_value()&&((st.value()>>BIT_FAULT)&1), filter=st.has_value()&&((st.value()>>BIT_FILTER_MAINT)&1);
  bool co2a=al.has_value()&&((al.value()>>BIT_CO2_ALARM)&1), freeze=al.has_value()&&((al.value()>>BIT_FREEZE_ALARM)&1);
  bool fc=ft.has_value()&&ft.value()!=0;
  float v = (fault||co2a||freeze||fc) ? 2.0f : (filter ? 1.0f : 0.0f);
  // Dedup ciblee : ne publier que si la valeur sante a change
  uint8_t vi = (uint8_t)v;
  if (!has_published_[0xFF] || last_published_[0xFF] != vi) {
    last_published_[0xFF] = vi; has_published_[0xFF] = true;
    if (fault_indicator_sensor_) fault_indicator_sensor_->publish_state(v);
  }
}

// ══════════════════════════════════════════════════════════════
// write_register + verify + helpers
// ══════════════════════════════════════════════════════════════

bool HeliosKwlComponent::write_register(uint8_t reg, uint8_t value) {
  if (is_broadcast_salve_active()) { uint32_t w = BROADCAST_SALVE_MS-(millis()-broadcast_salve_start_); if (w>0&&w<200) delay(w); }
  flush_rx(5);
  uint8_t m1[6]={HELIOS_START_BYTE,address_,HELIOS_BROADCAST_RC,reg,value,0}; m1[5]=checksum(m1,5); write_array(m1,6); flush(); delay(2);
  uint8_t m2[6]={HELIOS_START_BYTE,address_,HELIOS_BROADCAST_ALL,reg,value,0}; m2[5]=checksum(m2,5); write_array(m2,6); flush(); delay(2);
  uint8_t m3[6]={HELIOS_START_BYTE,address_,HELIOS_MAINBOARD,reg,value,0}; m3[5]=checksum(m3,5);
  write_array(m3,6); write_byte(m3[5]); flush();
  if (reg == REG_BYPASS_TEMP) { delay(2); write_array(m3,6); write_byte(m3[5]); flush(); }
  register_cache_[reg] = {value, millis(), true};
  return true;
}

bool HeliosKwlComponent::write_register_with_verify(uint8_t reg, uint8_t value, uint8_t retries) {
  write_register(reg, value);
  for (auto &pw : pending_writes_) { if (!pw.active) { pw={reg,value,millis(),retries,true}; return true; } }
  return true;
}

bool HeliosKwlComponent::set_register_bit(uint8_t reg, uint8_t bit, bool state) {
  auto c=get_cached_value(reg); if (!c.has_value()) return false;
  uint8_t cur=*c, nv=state?(cur|(1u<<bit)):(cur&~(1u<<bit));
  if (nv==cur) return true; return write_register(reg, nv);
}
bool HeliosKwlComponent::read_register_bit(uint8_t reg, uint8_t bit) {
  auto c=get_cached_value(reg); if (c.has_value()) return (*c>>bit)&1;
  auto p=poll_register(reg,1); if (p.has_value()) return (*p>>bit)&1; return false;
}

optional<uint8_t> HeliosKwlComponent::poll_register(uint8_t reg, uint8_t retries) {
  if (is_broadcast_salve_active()) return {};
  for (uint8_t a=0; a<retries; a++) {
    flush_rx(5);
    uint8_t req[6]={HELIOS_START_BYTE,address_,HELIOS_MAINBOARD,0x00,reg,0}; req[5]=checksum(req,5); write_array(req,6); flush();
    // Lecture non-bloquante : max 50ms, abandon silencieux si incomplet
    uint8_t buf[6]; size_t got = 0;
    uint32_t deadline = millis() + 50;
    while (got < 6 && millis() < deadline) {
      if (available()) { read_byte(&buf[got]); got++; }
      else { yield(); }
    }
    if (got == 6 && verify_checksum(buf, 6) && buf[1]==HELIOS_MAINBOARD && buf[2]==address_ && buf[3]==reg) {
      register_cache_[reg] = {buf[4], millis(), true};
      return buf[4];
    }
  }
  return {};
}

optional<uint8_t> HeliosKwlComponent::get_cached_value(uint8_t reg) { return register_cache_[reg].valid ? optional<uint8_t>(register_cache_[reg].value) : optional<uint8_t>(); }

void HeliosKwlComponent::flush_rx(uint32_t tmo) {
  uint32_t sf=millis()+200, lt=millis();
  for (uint32_t i=0;i<200;i++) { if (millis()>sf||millis()-lt>=tmo) break; for (size_t j=0;j<RX_BUFFER_SIZE&&available();j++){read();lt=millis();} yield(); }
  rx_buffer_len_=0;
}

uint8_t HeliosKwlComponent::checksum(const uint8_t *d, size_t l) { uint8_t c=0; for(size_t i=0;i<l;i++) c+=d[i]; return c; }
bool HeliosKwlComponent::verify_checksum(const uint8_t *d, size_t l) { return l>=2&&checksum(d,l-1)==d[l-1]; }
uint8_t HeliosKwlComponent::count_ones(uint8_t b) { uint8_t c=0; for(uint8_t i=0;i<8;i++){c+=b&1;b>>=1;} return c; }

float HeliosKwlComponent::ntc_to_celsius(uint8_t n) { return (float)NTC_TABLE[n]; }
uint8_t HeliosKwlComponent::celsius_to_ntc(float c) { if(c<-30)c=-30;if(c>60)c=60; int t=(int)c; uint8_t best=0x64; int bd=255; for(int i=0x20;i<=0xE0;i++){int d=abs((int)NTC_TABLE[i]-t);if(d<bd){bd=d;best=i;}} return best; }
float HeliosKwlComponent::raw_to_humidity(uint8_t r) { if(r<51)return 0; float p=(r-51.0f)/2.04f; return p>100?100:p; }
uint8_t HeliosKwlComponent::humidity_to_raw(float p) { if(p<0)p=0;if(p>100)p=100; return(uint8_t)(p*2.04f+51); }
uint8_t HeliosKwlComponent::speed_to_bitmask(uint8_t s) { return s==0?0:(uint8_t)((1u<<(s>8?8:s))-1u); }
uint8_t HeliosKwlComponent::bitmask_to_speed(uint8_t m) { return count_ones(m); }
uint16_t HeliosKwlComponent::bytes_to_co2(uint8_t h, uint8_t l) { return((uint16_t)h<<8)|l; }
std::pair<uint8_t,uint8_t> HeliosKwlComponent::co2_to_bytes(uint16_t p) { return{(uint8_t)(p>>8),(uint8_t)(p&0xFF)}; }

// ══════════════════════════════════════════════════════════════
// Actions
// ══════════════════════════════════════════════════════════════

void HeliosKwlComponent::control_fan(bool on, optional<uint8_t> spd) { set_register_bit(REG_STATES,BIT_POWER,on); if(spd.has_value()&&*spd>=1&&*spd<=8) write_register(REG_FAN_SPEED,speed_to_bitmask(*spd)); }
void HeliosKwlComponent::set_fan_speed(uint8_t s) { if(s==0)set_register_bit(REG_STATES,BIT_POWER,false);else write_register(REG_FAN_SPEED,speed_to_bitmask(s)); }
void HeliosKwlComponent::set_fan_on(bool on) { set_register_bit(REG_STATES,BIT_POWER,on); }

void HeliosKwlComponent::control_co2_regulation(bool e) { desired_co2_reg_=e?1:0; set_register_bit(REG_STATES,BIT_CO2_REG,e); }
void HeliosKwlComponent::control_humidity_regulation(bool e) { desired_hum_reg_=e?1:0; set_register_bit(REG_STATES,BIT_HUMIDITY_REG,e); }
void HeliosKwlComponent::control_summer_mode(bool e) { desired_summer_=e?1:0; set_register_bit(REG_STATES,BIT_SUMMER_MODE,e); }

void HeliosKwlComponent::control_basic_fan_speed(uint8_t s) { write_register_with_verify(REG_BASIC_SPEED,speed_to_bitmask(s)); }
void HeliosKwlComponent::control_max_fan_speed(uint8_t s) { write_register_with_verify(REG_MAX_SPEED,speed_to_bitmask(s)); }
void HeliosKwlComponent::control_bypass_temp(float c) { write_register_with_verify(REG_BYPASS_TEMP,celsius_to_ntc(c)); }
void HeliosKwlComponent::control_preheating_temp(float c) { write_register_with_verify(REG_DEFROST_TEMP,celsius_to_ntc(c)); }
void HeliosKwlComponent::control_frost_alarm_temp(float c) { write_register_with_verify(REG_FROST_ALARM_TEMP,celsius_to_ntc(c)); }
void HeliosKwlComponent::control_frost_hysteresis(float c) { write_register_with_verify(REG_FROST_HYSTERESIS,(uint8_t)c); }
void HeliosKwlComponent::control_co2_setpoint(uint16_t p) { auto b=co2_to_bytes(p); write_register_with_verify(REG_CO2_SETPOINT_H,b.first); write_register_with_verify(REG_CO2_SETPOINT_L,b.second); }
void HeliosKwlComponent::control_humidity_setpoint(uint8_t p) { write_register_with_verify(REG_HUMIDITY_SET,humidity_to_raw((float)p)); }
void HeliosKwlComponent::control_regulation_interval(uint8_t m) { auto c=get_cached_value(REG_PROGRAM_VARS); uint8_t cur=c.has_value()?*c:0; write_register_with_verify(REG_PROGRAM_VARS,(cur&0xF0)|(m&0x0F)); }
void HeliosKwlComponent::control_supply_fan_percent(uint8_t p) { write_register_with_verify(REG_SUPPLY_FAN_PCT,p); }
void HeliosKwlComponent::control_exhaust_fan_percent(uint8_t p) { write_register_with_verify(REG_EXHAUST_FAN_PCT,p); }
void HeliosKwlComponent::control_service_interval(uint8_t m) { write_register_with_verify(REG_SERVICE_INTERVAL,m); }

void HeliosKwlComponent::control_boost_fireplace_mode(bool f) { set_register_bit(REG_PROGRAM_VARS,BIT_BOOST_FIRE_MODE,f); }
void HeliosKwlComponent::control_humidity_auto_search(bool a) { set_register_bit(REG_PROGRAM_VARS,BIT_HUMIDITY_AUTO,a); }
void HeliosKwlComponent::control_max_speed_continuous(bool c) { set_register_bit(REG_PROGRAM2,BIT_MAX_SPEED_CONT,c); }

void HeliosKwlComponent::trigger_boost_airflow() { ESP_LOGI(TAG,"Cycle Plein Air"); set_register_bit(REG_PROGRAM_VARS,BIT_BOOST_FIRE_MODE,true); delay(5); set_register_bit(REG_BOOST_STATE,BIT_BOOST_ACTIVATE,true); }
void HeliosKwlComponent::trigger_boost_fireplace() { ESP_LOGI(TAG,"Cycle Cheminee"); set_register_bit(REG_PROGRAM_VARS,BIT_BOOST_FIRE_MODE,false); delay(5); set_register_bit(REG_BOOST_STATE,BIT_BOOST_ACTIVATE,true); }
void HeliosKwlComponent::stop_boost_cycle() {
  ESP_LOGI(TAG,"Arret cycle — force timer 0x79 a 1 min (fin naturelle dans ~1 min)");
  // Astuce : au lieu de forcer timer=0 (que la VMC interprete comme un glitch et
  // redeclenche un nouveau cycle), on met timer=1. La VMC va naturellement
  // decompter a 0 dans ~1 min et executer sa sequence de fin native :
  // efface 0x71, redemarre l'extracteur, met a jour 0x08. C'est propre.
  write_register(REG_BOOST_REMAINING, 0x01);
  // On ne touche pas a boost_cycle_active_ ni aux text_sensors :
  // les prochains polls S2 de 0x71/0x79 refleteront naturellement la fin du cycle
  // quand la VMC l'aura effectivement terminee.
}
void HeliosKwlComponent::acknowledge_maintenance() { ESP_LOGI(TAG,"Reset filtres"); auto iv=get_cached_value(REG_SERVICE_INTERVAL); write_register_with_verify(REG_SERVICE_MONTHS,iv.has_value()?*iv:4); }

fan::FanTraits HeliosKwlFan::get_traits() { return fan::FanTraits(false,true,false,8); }
void HeliosKwlFan::control(const fan::FanCall &call) {
  if(!parent_)return;
  // Ignorer les commandes fan tant que la VMC n'a pas ete lue (empeche le restore ESPHome
  // d'ecraser l'etat reel de la VMC avec speed=8 au boot/flash)
  if (!parent_->get_cached_value(REG_STATES).has_value()) {
    ESP_LOGW(TAG, "Fan control ignore — VMC pas encore lue");
    return;
  }
  optional<uint8_t> spd; if(call.get_speed().has_value())spd=(uint8_t)*call.get_speed();
  bool on=this->state; if(call.get_state().has_value())on=*call.get_state();
  parent_->control_fan(on,spd); if(call.get_state().has_value())this->state=*call.get_state(); if(spd.has_value())this->speed=*spd; this->publish_state();
}

}  // namespace helios_kwl
}  // namespace esphome
