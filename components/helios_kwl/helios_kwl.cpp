// ──────────────────────────────────────────────────────────────────────────────
// helios_kwl.cpp — Composant ESPHome pour VMC Helios KWL EC 300 Pro
// Protocole RS485 Vallox/Helios — Refonte complète phase 1 — Corrigé
// ──────────────────────────────────────────────────────────────────────────────

#include "helios_kwl.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace helios_kwl {

static const char *const TAG = "helios_kwl";

// ──────────────────────────────────────────────────────────────────────────────
// Table NTC → °C (256 entrées, source : Vallox Digit bus protocol Annex A)
// ──────────────────────────────────────────────────────────────────────────────

static const int8_t NTC_TABLE[256] = {
  // 0x00–0x0F
   -74,  -70,  -66,  -62,  -59,  -56,  -54,  -52,  -50,  -48,  -47,  -46,  -44,  -43,  -42,  -41,
  // 0x10–0x1F
   -40,  -39,  -38,  -37,  -36,  -35,  -34,  -33,  -33,  -32,  -31,  -30,  -30,  -29,  -28,  -28,
  // 0x20–0x2F
   -27,  -27,  -26,  -25,  -25,  -24,  -24,  -23,  -23,  -22,  -22,  -21,  -21,  -20,  -20,  -19,
  // 0x30–0x3F
   -19,  -19,  -18,  -18,  -17,  -17,  -16,  -16,  -16,  -15,  -15,  -14,  -14,  -14,  -13,  -13,
  // 0x40–0x4F
   -12,  -12,  -12,  -11,  -11,  -11,  -10,  -10,   -9,   -9,   -9,   -8,   -8,   -8,   -7,   -7,
  // 0x50–0x5F
    -7,   -6,   -6,   -6,   -5,   -5,   -5,   -4,   -4,   -4,   -3,   -3,   -3,   -2,   -2,   -2,
  // 0x60–0x6F
    -1,   -1,   -1,   -1,    0,    0,    0,    1,    1,    1,    2,    2,    2,    3,    3,    3,
  // 0x70–0x7F
     4,    4,    4,    5,    5,    5,    5,    6,    6,    6,    7,    7,    7,    8,    8,    8,
  // 0x80–0x8F
     9,    9,    9,   10,   10,   10,   11,   11,   11,   12,   12,   12,   13,   13,   13,   14,
  // 0x90–0x9F
    14,   14,   15,   15,   15,   16,   16,   16,   17,   17,   18,   18,   18,   19,   19,   19,
  // 0xA0–0xAF
    20,   20,   21,   21,   21,   22,   22,   22,   23,   23,   24,   24,   24,   25,   25,   26,
  // 0xB0–0xBF
    26,   27,   27,   27,   28,   28,   29,   29,   30,   30,   31,   31,   32,   32,   33,   33,
  // 0xC0–0xCF
    34,   34,   35,   35,   36,   36,   37,   37,   38,   38,   39,   40,   40,   41,   41,   42,
  // 0xD0–0xDF
    43,   43,   44,   45,   45,   46,   47,   48,   48,   49,   50,   51,   52,   53,   53,   54,
  // 0xE0–0xEF
    55,   56,   57,   59,   60,   61,   62,   63,   65,   66,   68,   69,   71,   73,   75,   77,
  // 0xF0–0xFF
    79,   81,   82,   86,   90,   93,   97,  100,  100,  100,  100,  100,  100,  100,  100,  100,
};

// ──────────────────────────────────────────────────────────────────────────────
// Descriptions texte des codes d'erreur (registre 0x36)
// ──────────────────────────────────────────────────────────────────────────────

static const char *fault_code_description(uint8_t code) {
  switch (code) {
    case 0x00: return "Aucun défaut";
    case 0x05: return "Sonde air soufflé défectueuse";
    case 0x06: return "Alerte CO₂ élevé";
    case 0x07: return "Sonde extérieure défectueuse";
    case 0x08: return "Sonde air intérieur défectueuse";
    case 0x09: return "Risque gel circuit eau";
    case 0x0A: return "Sonde air rejeté défectueuse";
    default:   return "Défaut inconnu";
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// setup() — initialisation, construction de la table de polling
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::setup() {
  ESP_LOGI(TAG, "Initialisation Helios KWL — adresse bus 0x%02X", address_);

  // Vide le cache registres
  for (auto &entry : register_cache_) {
    entry.valid       = false;
    entry.value       = 0;
    entry.last_update = 0;
  }

  // ── Construction de la table de polling rotatif ──
  // Règle NASA n°3 : taille statique, allouée ici une seule fois.
  // Températures, CO₂ et humidité max (0x2A) sont broadcastés → pas de poll.
  // Humidité sondes 1 et 2 (0x2F, 0x30) ne sont PAS broadcastés → poll nécessaire.
  poll_task_count_ = 0;

#define ADD_POLL(reg, interval) \
  poll_tasks_[poll_task_count_++] = PollTask{(reg), (interval), 0}

  // Registres critiques — intervalle court
  ADD_POLL(REG_STATES,           2000);   // 0xA3 — power, régulations, mode, filtre
  ADD_POLL(REG_FAN_SPEED,        2000);   // 0x29 — vitesse actuelle
  ADD_POLL(REG_IO_PORT,          3000);   // 0x08 — bypass, préchauffe, ventilateurs
  ADD_POLL(REG_BOOST_STATE,      3000);   // 0x71 — boost/cheminée actif
  ADD_POLL(REG_BOOST_REMAINING,  5000);   // 0x79 — timer boost (pollé seulement si actif)

  // Registres moyennement critiques
  ADD_POLL(REG_ALARMS,           5000);   // 0x6D — alarmes CO₂ + gel
  ADD_POLL(REG_HUMIDITY1,       10000);   // 0x2F — sonde humidité 1 (non broadcasté)
  ADD_POLL(REG_HUMIDITY2,       10000);   // 0x30 — sonde humidité 2 (non broadcasté)
  ADD_POLL(REG_FAULT_CODE,      10000);   // 0x36 — code erreur

  // Paramètres de configuration — intervalle long (changent rarement)
  ADD_POLL(REG_PROGRAM_VARS,    60000);   // 0xAA — variables programme
  ADD_POLL(REG_BASIC_SPEED,     60000);   // 0xA9 — vitesse de base
  ADD_POLL(REG_MAX_SPEED,       60000);   // 0xA5 — vitesse maximale
  ADD_POLL(REG_BYPASS_TEMP,     60000);   // 0xAF — seuil température bypass
  ADD_POLL(REG_DEFROST_TEMP,    60000);   // 0xA7 — seuil dégivrage
  ADD_POLL(REG_FROST_ALARM_TEMP,60000);   // 0xA8 — seuil alerte givre
  ADD_POLL(REG_SERVICE_MONTHS,  60000);   // 0xAB — mois restants maintenance

  // Paramètres avancés — intervalle très long
  ADD_POLL(REG_FROST_HYSTERESIS,120000);  // 0xB2 — hystérésis antigel
  ADD_POLL(REG_SUPPLY_FAN_PCT, 120000);   // 0xB0 — % puissance soufflage
  ADD_POLL(REG_EXHAUST_FAN_PCT,120000);   // 0xB1 — % puissance extraction
  ADD_POLL(REG_CO2_SETPOINT_H, 120000);   // 0xB3 — seuil CO₂ high byte
  ADD_POLL(REG_CO2_SETPOINT_L, 120000);   // 0xB4 — seuil CO₂ low byte
  ADD_POLL(REG_HUMIDITY_SET,   120000);   // 0xAE — seuil humidité
  ADD_POLL(REG_SERVICE_INTERVAL,120000);  // 0xA6 — intervalle entretien
  ADD_POLL(REG_PROGRAM2,       120000);   // 0xB5 — variables programme 2

#undef ADD_POLL

  // Sécurité : ne pas dépasser la taille statique
  if (poll_task_count_ > POLL_TABLE_SIZE) {
    ESP_LOGE(TAG, "POLL_TABLE_SIZE dépassé ! Augmenter la constante.");
    poll_task_count_ = POLL_TABLE_SIZE;
  }

  current_poll_index_ = 0;
  last_rx_time_       = millis();

  ESP_LOGI(TAG, "Table de polling : %u registres initialisés", poll_task_count_);
}

// ──────────────────────────────────────────────────────────────────────────────
// loop() — écoute passive continue du bus RS485
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::loop() {
  accumulate_rx();

  // Règle NASA n°2 : max RX_BUFFER_SIZE/HELIOS_PACKET_LEN paquets par appel
  const size_t max_packets = RX_BUFFER_SIZE / HELIOS_PACKET_LEN;
  for (size_t i = 0; i < max_packets && rx_buffer_len_ >= HELIOS_PACKET_LEN; i++) {
    if (!process_rx_buffer()) {
      if (rx_buffer_len_ > 0) {
        std::copy(rx_buffer_.begin() + 1,
                  rx_buffer_.begin() + rx_buffer_len_,
                  rx_buffer_.begin());
        rx_buffer_len_--;
      }
      break;
    }
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// accumulate_rx() — lit les octets UART disponibles dans rx_buffer_
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::accumulate_rx() {
  for (size_t i = 0; i < RX_BUFFER_SIZE && available(); i++) {
    if (rx_buffer_len_ >= RX_BUFFER_SIZE) {
      ESP_LOGW(TAG, "RX buffer plein — reset");
      rx_buffer_len_ = 0;
    }
    uint8_t byte;
    read_byte(&byte);
    rx_buffer_[rx_buffer_len_++] = byte;
    last_rx_time_ = millis();
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// process_rx_buffer() — tente de parser un paquet depuis rx_buffer_
// Format Helios : [0x01][src][dst][reg][val][crc] = 6 octets
// ──────────────────────────────────────────────────────────────────────────────

bool HeliosKwlComponent::process_rx_buffer() {
  if (rx_buffer_len_ < HELIOS_PACKET_LEN)
    return false;

  // Cherche l'octet de début 0x01
  size_t start = rx_buffer_len_;
  for (size_t i = 0; i + HELIOS_PACKET_LEN <= rx_buffer_len_; i++) {
    if (rx_buffer_[i] == HELIOS_START_BYTE) {
      start = i;
      break;
    }
  }

  if (start == rx_buffer_len_) {
    if (rx_buffer_len_ > 0) {
      rx_buffer_[0] = rx_buffer_[rx_buffer_len_ - 1];
      rx_buffer_len_ = 1;
    }
    return false;
  }

  // Décaler jusqu'au 0x01
  if (start > 0) {
    std::copy(rx_buffer_.begin() + start,
              rx_buffer_.begin() + rx_buffer_len_,
              rx_buffer_.begin());
    rx_buffer_len_ -= start;
  }

  if (rx_buffer_len_ < HELIOS_PACKET_LEN)
    return false;

  // Vérifier le checksum
  if (!verify_checksum(rx_buffer_.data(), HELIOS_PACKET_LEN)) {
    ESP_LOGV(TAG, "CRC invalide : %02X %02X %02X %02X %02X %02X",
             rx_buffer_[0], rx_buffer_[1], rx_buffer_[2],
             rx_buffer_[3], rx_buffer_[4], rx_buffer_[5]);
    std::copy(rx_buffer_.begin() + 1,
              rx_buffer_.begin() + rx_buffer_len_,
              rx_buffer_.begin());
    rx_buffer_len_--;
    return true;
  }

  // Paquet valide : extraire les champs
  uint8_t src = rx_buffer_[1];
  uint8_t dst = rx_buffer_[2];
  uint8_t reg = rx_buffer_[3];
  uint8_t val = rx_buffer_[4];

  ESP_LOGV(TAG, "Paquet reçu : src=0x%02X dst=0x%02X reg=0x%02X val=0x%02X",
           src, dst, reg, val);

  // Mettre à jour le cache
  register_cache_[reg] = {val, millis(), true};

  // Dispatcher
  if (dst == HELIOS_BROADCAST_RC || dst == HELIOS_BROADCAST_ALL) {
    handle_broadcast(src, reg, val);
  } else if (dst == address_) {
    handle_command(src, dst, reg, val);
  }

  // Consommer le paquet du buffer
  std::copy(rx_buffer_.begin() + HELIOS_PACKET_LEN,
            rx_buffer_.begin() + rx_buffer_len_,
            rx_buffer_.begin());
  rx_buffer_len_ -= HELIOS_PACKET_LEN;

  return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// handle_broadcast() / handle_command()
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::handle_broadcast(uint8_t sender, uint8_t reg, uint8_t value) {
  ESP_LOGD(TAG, "Broadcast src=0x%02X reg=0x%02X val=0x%02X", sender, reg, value);
  publish_register(reg, value);
}

void HeliosKwlComponent::handle_command(uint8_t sender, uint8_t recipient,
                                         uint8_t reg, uint8_t value) {
  ESP_LOGD(TAG, "Commande src=0x%02X reg=0x%02X val=0x%02X", sender, reg, value);
  publish_register(reg, value);
}

// ──────────────────────────────────────────────────────────────────────────────
// update() — polling rotatif, appelé toutes les 1s par PollingComponent
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::update() {
  if (poll_task_count_ == 0)
    return;

  uint32_t now = millis();

  for (size_t i = 0; i < poll_task_count_; i++) {
    size_t idx = (current_poll_index_ + i) % poll_task_count_;
    PollTask &task = poll_tasks_[idx];

    if ((now - task.last_polled) >= task.interval_ms) {
      // Skip boost_remaining si aucun cycle actif (évite poll inutile)
      if (task.reg == REG_BOOST_REMAINING && !boost_cycle_active_) {
        task.last_polled = now;
        continue;
      }

      auto result = poll_register(task.reg, 2);
      if (result.has_value()) {
        task.last_polled = now;
        publish_register(task.reg, *result);
      } else {
        ESP_LOGW(TAG, "Poll timeout reg=0x%02X", task.reg);
        task.last_polled = now;
      }

      current_poll_index_ = (idx + 1) % poll_task_count_;
      return;  // Un seul poll par update()
    }
  }

  current_poll_index_ = (current_poll_index_ + 1) % poll_task_count_;
}

// ──────────────────────────────────────────────────────────────────────────────
// dump_config()
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Helios KWL EC 300 Pro");
  ESP_LOGCONFIG(TAG, "  Adresse RS485 : 0x%02X", address_);
  ESP_LOGCONFIG(TAG, "  Registres en polling : %u", poll_task_count_);
  LOG_SENSOR("  ", "Temp. extérieure",  temperature_outside_);
  LOG_SENSOR("  ", "Temp. intérieure",  temperature_extract_);
  LOG_SENSOR("  ", "Temp. soufflée",    temperature_supply_);
  LOG_SENSOR("  ", "Temp. rejetée",     temperature_exhaust_);
  LOG_SENSOR("  ", "Humidité sonde 1",  humidity_sensor1_);
  LOG_SENSOR("  ", "Humidité sonde 2",  humidity_sensor2_);
  LOG_SENSOR("  ", "Niveau CO₂",        co2_concentration_);
  LOG_SENSOR("  ", "Vitesse fan",        fan_speed_sensor_);
  LOG_SENSOR("  ", "Boost restant",     boost_remaining_);
  LOG_SENSOR("  ", "Code défaut",        fault_code_);
  LOG_SENSOR("  ", "Maintenance dans",  service_months_);
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_register() — dispatch central
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_register(uint8_t reg, uint8_t value) {
  switch (reg) {
    case REG_TEMP_OUTSIDE:
    case REG_TEMP_EXTRACT:
    case REG_TEMP_SUPPLY:
    case REG_TEMP_EXHAUST:
      publish_temperature(reg, value);
      break;

    case REG_CO2_HIGH:
    case REG_CO2_LOW: {
      auto h = get_cached_value(REG_CO2_HIGH);
      auto l = get_cached_value(REG_CO2_LOW);
      if (h.has_value() && l.has_value())
        publish_co2(*h, *l);
      break;
    }

    case REG_HUMIDITY1:
    case REG_HUMIDITY2:
      publish_humidity(reg, value);
      break;

    case REG_FAN_SPEED:
      publish_fan_speed(value);
      break;

    case REG_STATES:
      publish_states(value);
      break;

    case REG_IO_PORT:
      publish_io_port(value);
      break;

    case REG_ALARMS:
      publish_alarms(value);
      break;

    case REG_BOOST_STATE:
      publish_boost(value);
      break;

    case REG_BOOST_REMAINING:
      publish_boost_remaining(value);
      break;

    case REG_FAULT_CODE:
      publish_fault(value);
      break;

    case REG_SERVICE_MONTHS:
      if (service_months_ != nullptr)
        service_months_->publish_state((float) value);
      break;

    case REG_PROGRAM_VARS:
      publish_program_vars(value);
      break;

    case REG_BASIC_SPEED:
      if (basic_fan_speed_n_ != nullptr)
        basic_fan_speed_n_->publish_state((float) bitmask_to_speed(value));
      break;

    case REG_MAX_SPEED:
      if (max_fan_speed_n_ != nullptr)
        max_fan_speed_n_->publish_state((float) bitmask_to_speed(value));
      break;

    case REG_BYPASS_TEMP:
      if (bypass_temp_n_ != nullptr)
        bypass_temp_n_->publish_state((float) value);
      break;

    case REG_DEFROST_TEMP:
      if (preheating_temp_n_ != nullptr)
        preheating_temp_n_->publish_state(ntc_to_celsius(value));
      break;

    case REG_FROST_ALARM_TEMP:
      if (frost_alarm_temp_n_ != nullptr)
        frost_alarm_temp_n_->publish_state(ntc_to_celsius(value));
      break;

    case REG_FROST_HYSTERESIS:
      if (frost_hysteresis_n_ != nullptr)
        frost_hysteresis_n_->publish_state((float) value);
      break;

    case REG_SUPPLY_FAN_PCT:
      if (supply_fan_pct_n_ != nullptr)
        supply_fan_pct_n_->publish_state((float) value);
      break;

    case REG_EXHAUST_FAN_PCT:
      if (exhaust_fan_pct_n_ != nullptr)
        exhaust_fan_pct_n_->publish_state((float) value);
      break;

    case REG_CO2_SETPOINT_H:
    case REG_CO2_SETPOINT_L: {
      auto h = get_cached_value(REG_CO2_SETPOINT_H);
      auto l = get_cached_value(REG_CO2_SETPOINT_L);
      if (h.has_value() && l.has_value() && co2_setpoint_n_ != nullptr) {
        uint16_t ppm = bytes_to_co2(*h, *l);
        co2_setpoint_n_->publish_state((float) ppm);
      }
      break;
    }

    case REG_HUMIDITY_SET:
      if (humidity_setpoint_n_ != nullptr)
        humidity_setpoint_n_->publish_state(raw_to_humidity(value));
      break;

    case REG_SERVICE_INTERVAL:
      if (service_interval_n_ != nullptr)
        service_interval_n_->publish_state((float) value);
      break;

    case REG_PROGRAM2:
      if (max_speed_cont_sel_ != nullptr) {
        bool continuous = (value >> BIT_MAX_SPEED_CONT) & 0x01;
        max_speed_cont_sel_->publish_state(continuous ? "Ventilation maximale forcée" : "Normal");
      }
      break;

    default:
      ESP_LOGV(TAG, "Registre 0x%02X ignoré (val=0x%02X)", reg, value);
      break;
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_temperature()
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_temperature(uint8_t reg, uint8_t value) {
  float celsius = ntc_to_celsius(value);
  ESP_LOGD(TAG, "Température reg=0x%02X ntc=0x%02X → %.0f°C", reg, value, celsius);

  switch (reg) {
    case REG_TEMP_OUTSIDE:
      if (temperature_outside_ != nullptr)
        temperature_outside_->publish_state(celsius);
      break;
    case REG_TEMP_EXTRACT:
      if (temperature_extract_ != nullptr)
        temperature_extract_->publish_state(celsius);
      break;
    case REG_TEMP_SUPPLY:
      if (temperature_supply_ != nullptr)
        temperature_supply_->publish_state(celsius);
      break;
    case REG_TEMP_EXHAUST:
      if (temperature_exhaust_ != nullptr)
        temperature_exhaust_->publish_state(celsius);
      break;
    default:
      break;
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_humidity()
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_humidity(uint8_t reg, uint8_t value) {
  float pct = raw_to_humidity(value);
  ESP_LOGD(TAG, "Humidité reg=0x%02X raw=0x%02X → %.1f%%", reg, value, pct);

  switch (reg) {
    case REG_HUMIDITY1:
      if (humidity_sensor1_ != nullptr)
        humidity_sensor1_->publish_state(pct);
      break;
    case REG_HUMIDITY2:
      if (humidity_sensor2_ != nullptr)
        humidity_sensor2_->publish_state(pct);
      break;
    default:
      break;
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_fan_speed()
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_fan_speed(uint8_t value) {
  uint8_t speed = bitmask_to_speed(value);
  ESP_LOGD(TAG, "Vitesse fan bitmask=0x%02X → %u/8", value, speed);

  // Publier uniquement le sensor de lecture — PAS fan.make_call()
  // La synchro fan ↔ sensor est gérée dans le YAML via sensor.on_value
  if (fan_speed_sensor_ != nullptr)
    fan_speed_sensor_->publish_state((float) speed);
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_states() — registre 0xA3
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_states(uint8_t value) {
  ESP_LOGD(TAG, "États 0xA3 = 0x%02X", value);

  bool power_on      = (value >> BIT_POWER)        & 0x01;
  bool co2_reg       = (value >> BIT_CO2_REG)      & 0x01;
  bool hum_reg       = (value >> BIT_HUMIDITY_REG) & 0x01;
  bool summer        = (value >> BIT_SUMMER_MODE)  & 0x01;
  bool heating       = (value >> BIT_HEATING)      & 0x01;
  bool fault         = (value >> BIT_FAULT)        & 0x01;
  bool filter_maint  = (value >> BIT_FILTER_MAINT) & 0x01;

  (void) power_on;
  (void) fault;

  if (co2_regulation_ != nullptr)
    co2_regulation_->publish_state(co2_reg);
  if (humidity_regulation_ != nullptr)
    humidity_regulation_->publish_state(hum_reg);
  if (summer_mode_ != nullptr)
    summer_mode_->publish_state(summer);

  if (heating_indicator_ != nullptr)
    heating_indicator_->publish_state(heating);
  if (filter_maintenance_ != nullptr)
    filter_maintenance_->publish_state(filter_maint);

  if (bypass_state_text_ != nullptr)
    bypass_state_text_->publish_state(summer ? "Air frais" : "Chaleur conservée");

  update_health_indicator();
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_io_port() — registre 0x08
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_io_port(uint8_t value) {
  ESP_LOGD(TAG, "Port I/O 0x08 = 0x%02X", value);

  bool bypass     = (value >> BIT_BYPASS_OPEN)  & 0x01;
  bool fault_rel  = (value >> BIT_FAULT_RELAY)  & 0x01;
  bool supply_off = (value >> BIT_SUPPLY_FAN)   & 0x01;
  bool preheating = (value >> BIT_PREHEATING)   & 0x01;
  bool exhst_off  = (value >> BIT_EXHAUST_FAN)  & 0x01;
  bool ext_cont   = (value >> BIT_EXT_CONTACT)  & 0x01;

  if (bypass_open_ != nullptr)
    bypass_open_->publish_state((float)(bypass ? 1 : 0));

  if (supply_fan_running_ != nullptr)
    supply_fan_running_->publish_state(!supply_off);
  if (exhaust_fan_running_ != nullptr)
    exhaust_fan_running_->publish_state(!exhst_off);
  if (preheating_active_ != nullptr)
    preheating_active_->publish_state(preheating);
  if (external_contact_ != nullptr)
    external_contact_->publish_state(ext_cont);
  if (fault_relay_ != nullptr)
    fault_relay_->publish_state(!fault_rel);
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_alarms() — registre 0x6D
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_alarms(uint8_t value) {
  ESP_LOGD(TAG, "Alarmes 0x6D = 0x%02X", value);

  bool co2_alarm    = (value >> BIT_CO2_ALARM)   & 0x01;
  bool freeze_alarm = (value >> BIT_FREEZE_ALARM) & 0x01;

  if (co2_alarm_ != nullptr)
    co2_alarm_->publish_state(co2_alarm);
  if (freeze_alarm_ != nullptr)
    freeze_alarm_->publish_state(freeze_alarm);

  update_health_indicator();
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_boost() — registre 0x71 (FLAGS 6)
// Vallox : bit 5 = activation, bit 6 = cycle en cours (read only)
// Le mode (plein air vs cheminée) est dans 0xAA bit 5
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_boost(uint8_t value) {
  ESP_LOGD(TAG, "Boost 0x71 = 0x%02X", value);

  bool running = (value >> BIT_BOOST_RUNNING) & 0x01;
  boost_cycle_active_ = running;

  const char *state_str = "Normal";
  float state_val = 0.0f;

  if (running) {
    // Lire le mode depuis 0xAA bit 5 pour savoir si c'est plein air ou cheminée
    auto prog = get_cached_value(REG_PROGRAM_VARS);
    bool is_fresh_air = prog.has_value() && ((*prog >> BIT_BOOST_FIRE_MODE) & 0x01);
    if (is_fresh_air) {
      state_str = "Cycle Plein Air";
      state_val = 1.0f;
    } else {
      state_str = "Cycle Cheminée";
      state_val = 2.0f;
    }
  }

  if (boost_active_text_ != nullptr)
    boost_active_text_->publish_state(state_str);
  if (boost_state_sensor_ != nullptr)
    boost_state_sensor_->publish_state(state_val);
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_boost_remaining() — registre 0x79 (minutes restantes)
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_boost_remaining(uint8_t value) {
  if (boost_remaining_ != nullptr)
    boost_remaining_->publish_state((float) value);
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_fault() — registre 0x36
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_fault(uint8_t value) {
  ESP_LOGD(TAG, "Code défaut 0x36 = 0x%02X", value);

  if (fault_code_ != nullptr)
    fault_code_->publish_state((float) value);
  if (fault_description_ != nullptr)
    fault_description_->publish_state(fault_code_description(value));

  update_health_indicator();
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_program_vars() — registre 0xAA
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_program_vars(uint8_t value) {
  ESP_LOGD(TAG, "Variables programme 0xAA = 0x%02X", value);

  uint8_t interval  = (value & 0x0F);
  bool    hum_auto  = (value >> BIT_HUMIDITY_AUTO)   & 0x01;
  bool    fire_mode = (value >> BIT_BOOST_FIRE_MODE) & 0x01;

  if (regulation_interval_n_ != nullptr && interval > 0)
    regulation_interval_n_->publish_state((float) interval);

  if (humidity_auto_sel_ != nullptr)
    humidity_auto_sel_->publish_state(hum_auto ? "Apprentissage auto" : "Seuil manuel");

  if (boost_fireplace_sel_ != nullptr)
    boost_fireplace_sel_->publish_state(fire_mode ? "Cycle Plein Air" : "Cycle Cheminée");
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_co2()
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_co2(uint8_t high, uint8_t low) {
  uint16_t ppm = bytes_to_co2(high, low);
  ESP_LOGD(TAG, "CO₂ high=0x%02X low=0x%02X → %u ppm", high, low, ppm);
  if (co2_concentration_ != nullptr)
    co2_concentration_->publish_state((float) ppm);
}

// ──────────────────────────────────────────────────────────────────────────────
// update_health_indicator()
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::update_health_indicator() {
  auto states = get_cached_value(REG_STATES);
  auto alarms = get_cached_value(REG_ALARMS);
  auto fault  = get_cached_value(REG_FAULT_CODE);

  bool has_fault  = states.has_value() && ((states.value() >> BIT_FAULT)       & 0x01);
  bool has_filter = states.has_value() && ((states.value() >> BIT_FILTER_MAINT) & 0x01);
  bool has_co2al  = alarms.has_value() && ((alarms.value() >> BIT_CO2_ALARM)   & 0x01);
  bool has_freeze = alarms.has_value() && ((alarms.value() >> BIT_FREEZE_ALARM) & 0x01);
  bool has_fcode  = fault.has_value()  && (fault.value() != 0x00);

  float health_val;
  if (has_fault || has_co2al || has_freeze || has_fcode) {
    health_val = 2.0f;
  } else if (has_filter) {
    health_val = 1.0f;
  } else {
    health_val = 0.0f;
  }

  if (fault_indicator_sensor_ != nullptr)
    fault_indicator_sensor_->publish_state(health_val);
}

// ──────────────────────────────────────────────────────────────────────────────
// write_register() — séquence 3 messages Helios + cache optimiste
// Annexe B : broadcast 0x20, broadcast 0x10, direct 0x11 + CRC doublé
// ──────────────────────────────────────────────────────────────────────────────

bool HeliosKwlComponent::write_register(uint8_t reg, uint8_t value) {
  ESP_LOGD(TAG, "Écriture reg=0x%02X val=0x%02X", reg, value);

  // 1. Silence bus pendant 5ms
  flush_rx(5);

  // 2. Message 1 : Broadcast télécommandes (0x20)
  uint8_t msg1[HELIOS_PACKET_LEN] = {HELIOS_START_BYTE, address_, HELIOS_BROADCAST_RC, reg, value, 0};
  msg1[5] = checksum(msg1, 5);
  write_array(msg1, HELIOS_PACKET_LEN);
  flush();
  delay(2);

  // 3. Message 2 : Broadcast cartes mères (0x10)
  uint8_t msg2[HELIOS_PACKET_LEN] = {HELIOS_START_BYTE, address_, HELIOS_BROADCAST_ALL, reg, value, 0};
  msg2[5] = checksum(msg2, 5);
  write_array(msg2, HELIOS_PACKET_LEN);
  flush();
  delay(2);

  // 4. Message 3 : Direct carte mère 1 (0x11) + CRC doublé (requis protocole Helios)
  uint8_t msg3[HELIOS_PACKET_LEN] = {HELIOS_START_BYTE, address_, HELIOS_MAINBOARD, reg, value, 0};
  msg3[5] = checksum(msg3, 5);
  write_array(msg3, HELIOS_PACKET_LEN);
  write_byte(msg3[5]);
  flush();

  // 5. Cache optimiste — la VMC broadcastera la confirmation dans ~12s
  register_cache_[reg] = {value, millis(), true};
  ESP_LOGD(TAG, "Écriture envoyée reg=0x%02X (cache optimiste)", reg);
  return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// set_register_bit() / read_register_bit()
// ──────────────────────────────────────────────────────────────────────────────

bool HeliosKwlComponent::set_register_bit(uint8_t reg, uint8_t bit, bool state) {
  auto cached = get_cached_value(reg);
  if (!cached.has_value()) {
    ESP_LOGW(TAG, "set_register_bit : cache vide reg=0x%02X, skip", reg);
    return false;
  }
  uint8_t current = *cached;
  uint8_t new_val = state ? (current | (1u << bit)) : (current & ~(1u << bit));

  if (new_val == current) {
    ESP_LOGD(TAG, "set_register_bit : bit %u de reg=0x%02X déjà à %d", bit, reg, (int)state);
    return true;
  }

  return write_register(reg, new_val);
}

bool HeliosKwlComponent::read_register_bit(uint8_t reg, uint8_t bit) {
  auto cached = get_cached_value(reg);
  if (cached.has_value())
    return (*cached >> bit) & 0x01;

  auto polled = poll_register(reg, 2);
  if (polled.has_value())
    return (*polled >> bit) & 0x01;

  return false;
}

// ──────────────────────────────────────────────────────────────────────────────
// poll_register() — envoie une demande de lecture et attend la réponse
// Protocole Vallox : VARIABLE=0x00, DATA=registre demandé
// ──────────────────────────────────────────────────────────────────────────────

optional<uint8_t> HeliosKwlComponent::poll_register(uint8_t reg, uint8_t retries) {
  for (uint8_t attempt = 0; attempt < retries; attempt++) {
    flush_rx(5);

    uint8_t req[HELIOS_PACKET_LEN] = {HELIOS_START_BYTE, address_, HELIOS_MAINBOARD, 0x00, reg, 0};
    req[5] = checksum(req, 5);
    write_array(req, HELIOS_PACKET_LEN);
    flush();

    auto response = read_array<HELIOS_PACKET_LEN>();
    if (response.has_value()) {
      const auto& arr = *response;
      if (verify_checksum(arr.data(), HELIOS_PACKET_LEN)) {
        if (arr[1] == HELIOS_MAINBOARD && arr[2] == address_ && arr[3] == reg) {
          uint8_t val = arr[4];
          register_cache_[reg] = {val, millis(), true};
          ESP_LOGV(TAG, "Poll reg=0x%02X → 0x%02X (tentative %u)", reg, val, attempt + 1);
          return val;
        } else {
          ESP_LOGW(TAG, "Poll réponse inattendue : src=0x%02X dst=0x%02X reg=0x%02X",
                   arr[1], arr[2], arr[3]);
        }
      } else {
        ESP_LOGW(TAG, "Poll CRC invalide reg=0x%02X", reg);
      }
    }

    ESP_LOGW(TAG, "Poll timeout reg=0x%02X (tentative %u/%u)", reg, attempt + 1, retries);
  }

  return {};
}

// ──────────────────────────────────────────────────────────────────────────────
// get_cached_value()
// ──────────────────────────────────────────────────────────────────────────────

optional<uint8_t> HeliosKwlComponent::get_cached_value(uint8_t reg) {
  const auto &entry = register_cache_[reg];
  if (entry.valid)
    return entry.value;
  return {};
}

// ──────────────────────────────────────────────────────────────────────────────
// flush_rx()
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::flush_rx(uint32_t timeout_ms) {
  const uint32_t safety = millis() + 200;
  uint32_t last_byte_time = millis();
  for (uint32_t iter = 0; iter < 200; iter++) {
    if (millis() > safety) break;
    if (millis() - last_byte_time >= timeout_ms) break;
    for (size_t i = 0; i < RX_BUFFER_SIZE && available(); i++) {
      read();
      last_byte_time = millis();
    }
    yield();
  }
  rx_buffer_len_ = 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// checksum() — somme arithmétique 8 bits (protocole Vallox)
// ──────────────────────────────────────────────────────────────────────────────

uint8_t HeliosKwlComponent::checksum(const uint8_t *data, size_t len) {
  uint8_t crc = 0;
  for (size_t i = 0; i < len; i++)
    crc += data[i];
  return crc;
}

bool HeliosKwlComponent::verify_checksum(const uint8_t *data, size_t len) {
  if (len < 2) return false;
  return checksum(data, len - 1) == data[len - 1];
}

uint8_t HeliosKwlComponent::count_ones(uint8_t byte) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < 8u; i++) {
    count += (byte & 1u);
    byte >>= 1;
  }
  return count;
}

// ──────────────────────────────────────────────────────────────────────────────
// Conversions
// ──────────────────────────────────────────────────────────────────────────────

float HeliosKwlComponent::ntc_to_celsius(uint8_t ntc) {
  return (float) NTC_TABLE[ntc];
}

uint8_t HeliosKwlComponent::celsius_to_ntc(float celsius) {
  int target = (int) celsius;
  uint8_t best_ntc = 0x64;
  int     best_diff = 255;
  for (int i = 0x20; i <= 0xE0; i++) {
    int diff = abs((int) NTC_TABLE[i] - target);
    if (diff < best_diff) {
      best_diff = diff;
      best_ntc  = (uint8_t) i;
    }
  }
  return best_ntc;
}

float HeliosKwlComponent::raw_to_humidity(uint8_t raw) {
  if (raw < 51) return 0.0f;
  float pct = (raw - 51.0f) / 2.04f;
  if (pct > 100.0f) pct = 100.0f;
  return pct;
}

uint8_t HeliosKwlComponent::humidity_to_raw(float percent) {
  if (percent < 0.0f)   percent = 0.0f;
  if (percent > 100.0f) percent = 100.0f;
  return (uint8_t)(percent * 2.04f + 51.0f);
}

uint8_t HeliosKwlComponent::speed_to_bitmask(uint8_t speed) {
  if (speed == 0) return 0x00;
  if (speed > 8)  speed = 8;
  return (uint8_t)((1u << speed) - 1u);
}

uint8_t HeliosKwlComponent::bitmask_to_speed(uint8_t mask) {
  return count_ones(mask);
}

uint16_t HeliosKwlComponent::bytes_to_co2(uint8_t high, uint8_t low) {
  return ((uint16_t) high << 8) | low;
}

std::pair<uint8_t, uint8_t> HeliosKwlComponent::co2_to_bytes(uint16_t ppm) {
  return {(uint8_t)(ppm >> 8), (uint8_t)(ppm & 0xFF)};
}

// ──────────────────────────────────────────────────────────────────────────────
// Actions depuis les entités HA — Fan
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::control_fan(bool on, optional<uint8_t> speed) {
  set_register_bit(REG_STATES, BIT_POWER, on);
  if (speed.has_value() && *speed >= 1 && *speed <= 8) {
    write_register(REG_FAN_SPEED, speed_to_bitmask(*speed));
  }
}

void HeliosKwlComponent::set_fan_speed(uint8_t speed) {
  if (speed == 0) {
    set_register_bit(REG_STATES, BIT_POWER, false);
  } else {
    write_register(REG_FAN_SPEED, speed_to_bitmask(speed));
  }
}

void HeliosKwlComponent::set_fan_on(bool on) {
  set_register_bit(REG_STATES, BIT_POWER, on);
}

// ──────────────────────────────────────────────────────────────────────────────
// Actions depuis les entités HA — Switches
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::control_co2_regulation(bool enabled) {
  set_register_bit(REG_STATES, BIT_CO2_REG, enabled);
}

void HeliosKwlComponent::control_humidity_regulation(bool enabled) {
  set_register_bit(REG_STATES, BIT_HUMIDITY_REG, enabled);
}

void HeliosKwlComponent::control_summer_mode(bool enabled) {
  set_register_bit(REG_STATES, BIT_SUMMER_MODE, enabled);
}

// ──────────────────────────────────────────────────────────────────────────────
// Actions depuis les entités HA — Numbers
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::control_basic_fan_speed(uint8_t speed) {
  write_register(REG_BASIC_SPEED, speed_to_bitmask(speed));
}

void HeliosKwlComponent::control_max_fan_speed(uint8_t speed) {
  write_register(REG_MAX_SPEED, speed_to_bitmask(speed));
}

void HeliosKwlComponent::control_bypass_temp(float celsius) {
  write_register(REG_BYPASS_TEMP, (uint8_t) celsius);
}

void HeliosKwlComponent::control_preheating_temp(float celsius) {
  write_register(REG_DEFROST_TEMP, celsius_to_ntc(celsius));
}

void HeliosKwlComponent::control_frost_alarm_temp(float celsius) {
  write_register(REG_FROST_ALARM_TEMP, celsius_to_ntc(celsius));
}

void HeliosKwlComponent::control_frost_hysteresis(float celsius) {
  write_register(REG_FROST_HYSTERESIS, (uint8_t) celsius);
}

void HeliosKwlComponent::control_co2_setpoint(uint16_t ppm) {
  auto bytes = co2_to_bytes(ppm);
  write_register(REG_CO2_SETPOINT_H, bytes.first);
  write_register(REG_CO2_SETPOINT_L, bytes.second);
}

void HeliosKwlComponent::control_humidity_setpoint(uint8_t percent) {
  write_register(REG_HUMIDITY_SET, humidity_to_raw((float) percent));
}

void HeliosKwlComponent::control_regulation_interval(uint8_t minutes) {
  auto cached = get_cached_value(REG_PROGRAM_VARS);
  uint8_t current = cached.has_value() ? *cached : 0;
  uint8_t new_val = (current & 0xF0) | (minutes & 0x0F);
  write_register(REG_PROGRAM_VARS, new_val);
}

void HeliosKwlComponent::control_supply_fan_percent(uint8_t percent) {
  write_register(REG_SUPPLY_FAN_PCT, percent);
}

void HeliosKwlComponent::control_exhaust_fan_percent(uint8_t percent) {
  write_register(REG_EXHAUST_FAN_PCT, percent);
}

void HeliosKwlComponent::control_service_interval(uint8_t months) {
  write_register(REG_SERVICE_INTERVAL, months);
}

// ──────────────────────────────────────────────────────────────────────────────
// Actions depuis les entités HA — Selects
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::control_boost_fireplace_mode(bool is_fresh_air) {
  set_register_bit(REG_PROGRAM_VARS, BIT_BOOST_FIRE_MODE, is_fresh_air);
}

void HeliosKwlComponent::control_humidity_auto_search(bool auto_mode) {
  set_register_bit(REG_PROGRAM_VARS, BIT_HUMIDITY_AUTO, auto_mode);
}

void HeliosKwlComponent::control_max_speed_continuous(bool continuous) {
  set_register_bit(REG_PROGRAM2, BIT_MAX_SPEED_CONT, continuous);
}

// ──────────────────────────────────────────────────────────────────────────────
// Actions depuis les entités HA — Buttons
// CORRIGÉ : Vallox protocole — 0x71 bit 5 active le cycle, 0xAA bit 5 choisit le mode
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::trigger_boost_airflow() {
  ESP_LOGI(TAG, "Déclenchement Cycle Plein Air (45 min)");
  // 1. Configurer mode plein air dans 0xAA bit 5 = 1
  set_register_bit(REG_PROGRAM_VARS, BIT_BOOST_FIRE_MODE, true);
  delay(5);
  // 2. Activer le cycle via 0x71 bit 5 = 1
  set_register_bit(REG_BOOST_STATE, BIT_BOOST_ACTIVATE, true);
}

void HeliosKwlComponent::trigger_boost_fireplace() {
  ESP_LOGI(TAG, "Déclenchement Cycle Cheminée (15 min)");
  // 1. Configurer mode cheminée dans 0xAA bit 5 = 0
  set_register_bit(REG_PROGRAM_VARS, BIT_BOOST_FIRE_MODE, false);
  delay(5);
  // 2. Activer le cycle via 0x71 bit 5 = 1
  set_register_bit(REG_BOOST_STATE, BIT_BOOST_ACTIVATE, true);
}

void HeliosKwlComponent::acknowledge_maintenance() {
  ESP_LOGI(TAG, "Acquittement maintenance filtres");
  set_register_bit(REG_STATES, BIT_FILTER_MAINT, false);
}

// ──────────────────────────────────────────────────────────────────────────────
// HeliosKwlFan — Entité fan native ESPHome
// ──────────────────────────────────────────────────────────────────────────────

fan::FanTraits HeliosKwlFan::get_traits() {
  return fan::FanTraits(false, true, false, 8);
}

void HeliosKwlFan::control(const fan::FanCall &call) {
  if (parent_ == nullptr) return;

  optional<uint8_t> speed;
  if (call.get_speed().has_value())
    speed = (uint8_t) *call.get_speed();

  bool on = this->state;
  if (call.get_state().has_value())
    on = *call.get_state();

  parent_->control_fan(on, speed);

  if (call.get_state().has_value())
    this->state = *call.get_state();
  if (speed.has_value())
    this->speed = *speed;
  this->publish_state();
}

}  // namespace helios_kwl
}  // namespace esphome