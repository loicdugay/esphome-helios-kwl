// ──────────────────────────────────────────────────────────────────────────────
// helios_kwl.cpp — Composant ESPHome pour VMC Helios KWL EC 300 Pro
// Protocole RS485 Vallox/Helios — Refonte complète phase 1
// ──────────────────────────────────────────────────────────────────────────────

#include "helios_kwl.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace helios_kwl {

static const char *const TAG = "helios_kwl";

// ──────────────────────────────────────────────────────────────────────────────
// Table NTC → °C (256 entrées, source : Vallox Digit bus protocol Annex A)
// Index = valeur brute du registre, valeur = température en °C
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
  // Ordre : les registres les plus critiques (fréquence haute) en premier.
  poll_task_count_ = 0;

#define ADD_POLL(reg, interval) \
  poll_tasks_[poll_task_count_++] = PollTask{(reg), (interval), 0}

  ADD_POLL(REG_STATES,           2000);   // 0xA3 — power, régulations, mode, filtre
  ADD_POLL(REG_FAN_SPEED,        2000);   // 0x29 — vitesse actuelle
  ADD_POLL(REG_IO_PORT,          3000);   // 0x08 — bypass, préchauffe, ventilateurs
  // Températures et CO₂ reçus passivement via loop() (broadcasts VMC toutes les ~12s)
  ADD_POLL(REG_BOOST_STATE,      3000);   // 0x71 — boost/cheminée actif
  ADD_POLL(REG_BOOST_REMAINING,  3000);   // 0x79 — timer boost restant
  ADD_POLL(REG_ALARMS,           5000);   // 0x6D — alarmes CO₂ + gel
  ADD_POLL(REG_HUMIDITY1,        5000);   // 0x2F — sonde humidité 1
  ADD_POLL(REG_HUMIDITY2,        5000);   // 0x30 — sonde humidité 2
  ADD_POLL(REG_FAULT_CODE,      10000);   // 0x36 — code erreur
  ADD_POLL(REG_PROGRAM_VARS,    30000);   // 0xAA — variables programme (intervalle, modes)
  ADD_POLL(REG_BASIC_SPEED,     30000);   // 0xA9 — vitesse de base
  ADD_POLL(REG_MAX_SPEED,       30000);   // 0xA5 — vitesse maximale
  ADD_POLL(REG_BYPASS_TEMP,     30000);   // 0xAF — seuil température bypass
  ADD_POLL(REG_DEFROST_TEMP,    30000);   // 0xA7 — seuil dégivrage
  ADD_POLL(REG_FROST_ALARM_TEMP,30000);   // 0xA8 — seuil alerte givre
  ADD_POLL(REG_SERVICE_MONTHS,  60000);   // 0xAB — mois restants maintenance
  ADD_POLL(REG_FROST_HYSTERESIS,60000);   // 0xB2 — hystérésis antigel
  ADD_POLL(REG_SUPPLY_FAN_PCT,  60000);   // 0xB0 — % puissance soufflage
  ADD_POLL(REG_EXHAUST_FAN_PCT, 60000);   // 0xB1 — % puissance extraction
  ADD_POLL(REG_CO2_SETPOINT_H,  60000);   // 0xB3 — seuil CO₂ high byte
  ADD_POLL(REG_CO2_SETPOINT_L,  60000);   // 0xB4 — seuil CO₂ low byte
  ADD_POLL(REG_HUMIDITY_SET,    60000);   // 0xAE — seuil humidité
  ADD_POLL(REG_SERVICE_INTERVAL,120000);  // 0xA6 — intervalle entretien
  ADD_POLL(REG_PROGRAM2,        120000);  // 0xB5 — variables programme 2

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
// Appelé aussi souvent que possible par le scheduler ESPHome
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::loop() {
  // Écoute passive des broadcasts RS485 (températures, CO₂...)
  // La VMC broadcast ces registres toutes les ~12s sans qu'on les demande.
  // poll_register() appelle flush_rx() avant chaque TX, ce qui vide ce buffer
  // sans conflit — ESPHome est mono-thread, loop() et update() ne s'entrecroisent pas.
  accumulate_rx();

  // Règle NASA n°2 : max RX_BUFFER_SIZE/HELIOS_PACKET_LEN paquets par appel
  const size_t max_packets = RX_BUFFER_SIZE / HELIOS_PACKET_LEN;
  for (size_t i = 0; i < max_packets && rx_buffer_len_ >= HELIOS_PACKET_LEN; i++) {
    if (!process_rx_buffer()) {
      // Pas de paquet valide — décaler d'un octet et sortir
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
  // Règle NASA n°2 : borne supérieure explicite = RX_BUFFER_SIZE octets max par appel
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
// Retourne vrai si un paquet a été consommé (qu'il soit valide ou non)
// ──────────────────────────────────────────────────────────────────────────────

bool HeliosKwlComponent::process_rx_buffer() {
  if (rx_buffer_len_ < HELIOS_PACKET_LEN)
    return false;

  // Cherche l'octet de début 0x01
  size_t start = rx_buffer_len_;  // position du 0x01
  for (size_t i = 0; i + HELIOS_PACKET_LEN <= rx_buffer_len_; i++) {
    if (rx_buffer_[i] == HELIOS_START_BYTE) {
      start = i;
      break;
    }
  }

  if (start == rx_buffer_len_) {
    // Pas de 0x01 trouvé — vider tout ce qui précède le dernier octet
    // (il pourrait être le début d'un futur paquet)
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

  // Vérifier le checksum du paquet [0..4] → [5]
  if (!verify_checksum(rx_buffer_.data(), HELIOS_PACKET_LEN)) {
    ESP_LOGV(TAG, "CRC invalide : %02X %02X %02X %02X %02X %02X",
             rx_buffer_[0], rx_buffer_[1], rx_buffer_[2],
             rx_buffer_[3], rx_buffer_[4], rx_buffer_[5]);
    // Consommer cet octet de début invalide et continuer
    std::copy(rx_buffer_.begin() + 1,
              rx_buffer_.begin() + rx_buffer_len_,
              rx_buffer_.begin());
    rx_buffer_len_--;
    return true;  // on a avancé d'un octet → continuer l'itération
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
  // Ignorer les paquets adressés à d'autres appareils

  // Consommer le paquet du buffer
  std::copy(rx_buffer_.begin() + HELIOS_PACKET_LEN,
            rx_buffer_.begin() + rx_buffer_len_,
            rx_buffer_.begin());
  rx_buffer_len_ -= HELIOS_PACKET_LEN;

  return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// handle_broadcast() — paquets broadcast (carte mère → bus)
// Ces registres arrivent passivement toutes les ~12s sans qu'on les demande
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::handle_broadcast(uint8_t sender, uint8_t reg, uint8_t value) {
  ESP_LOGD(TAG, "Broadcast src=0x%02X reg=0x%02X val=0x%02X", sender, reg, value);
  publish_register(reg, value);
}

// ──────────────────────────────────────────────────────────────────────────────
// handle_command() — paquets adressés directement à cet ESP
// Réponse à nos propres polls
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::handle_command(uint8_t sender, uint8_t recipient,
                                         uint8_t reg, uint8_t value) {
  ESP_LOGD(TAG, "Commande src=0x%02X reg=0x%02X val=0x%02X", sender, reg, value);
  publish_register(reg, value);
}

// ──────────────────────────────────────────────────────────────────────────────
// update() — polling rotatif, appelé toutes les 1s par PollingComponent
// Stratégie : avancer dans la table de polling, ne poller que si l'intervalle
// cible est écoulé. Un seul poll par appel (pas de blocage du scheduler).
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::update() {
  if (poll_task_count_ == 0)
    return;

  uint32_t now = millis();

  // Recherche de la prochaine tâche dont l'intervalle est écoulé
  // On fait un tour complet maximum pour trouver un candidat
  for (size_t i = 0; i < poll_task_count_; i++) {
    size_t idx = (current_poll_index_ + i) % poll_task_count_;
    PollTask &task = poll_tasks_[idx];

    if ((now - task.last_polled) >= task.interval_ms) {
      // Cette tâche est due — la poller
      auto result = poll_register(task.reg, 2);
      if (result.has_value()) {
        task.last_polled = now;
        // publish_register() déjà appelé via handle_command()
        // Si la réponse est arrivée passivement, le cache est à jour
        // On publie quand même depuis le cache pour garantir la fraîcheur
        publish_register(task.reg, *result);
      } else {
        ESP_LOGW(TAG, "Poll timeout reg=0x%02X", task.reg);
        // Marquer quand même pour ne pas réessayer immédiatement
        task.last_polled = now;
      }

      // Avancer l'index pour le prochain appel
      current_poll_index_ = (idx + 1) % poll_task_count_;
      return;  // Un seul poll par update()
    }
  }

  // Aucune tâche due pour l'instant — avancer quand même l'index
  current_poll_index_ = (current_poll_index_ + 1) % poll_task_count_;
}

// ──────────────────────────────────────────────────────────────────────────────
// dump_config() — log de la configuration au démarrage
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
// Route chaque registre vers la bonne fonction de publication
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_register(uint8_t reg, uint8_t value) {
  switch (reg) {
    // ── Températures broadcastées ──
    case REG_TEMP_OUTSIDE:
    case REG_TEMP_EXTRACT:
    case REG_TEMP_SUPPLY:
    case REG_TEMP_EXHAUST:
      publish_temperature(reg, value);
      break;

    // ── CO₂ (16 bits — attend les deux octets dans le cache) ──
    case REG_CO2_HIGH:
    case REG_CO2_LOW: {
      auto h = get_cached_value(REG_CO2_HIGH);
      auto l = get_cached_value(REG_CO2_LOW);
      if (h.has_value() && l.has_value())
        publish_co2(*h, *l);
      break;
    }

    // ── Humidité ──
    case REG_HUMIDITY1:
    case REG_HUMIDITY2:
      publish_humidity(reg, value);
      break;

    // ── Vitesse ventilateur actuelle ──
    case REG_FAN_SPEED:
      publish_fan_speed(value);
      break;

    // ── États/commandes (power, régulations, mode été, filtre, défaut) ──
    case REG_STATES:
      publish_states(value);
      break;

    // ── Port I/O physique ──
    case REG_IO_PORT:
      publish_io_port(value);
      break;

    // ── Alarmes CO₂ + gel ──
    case REG_ALARMS:
      publish_alarms(value);
      break;

    // ── Boost / cheminée ──
    case REG_BOOST_STATE:
      publish_boost(value);
      break;

    // ── Timer boost restant ──
    case REG_BOOST_REMAINING:
      publish_boost_remaining(value);
      break;

    // ── Code erreur ──
    case REG_FAULT_CODE:
      publish_fault(value);
      break;

    // ── Mois restants maintenance ──
    case REG_SERVICE_MONTHS:
      if (service_months_ != nullptr)
        service_months_->publish_state((float) value);
      break;

    // ── Variables programme 0xAA ──
    case REG_PROGRAM_VARS:
      publish_program_vars(value);
      break;

    // ── Vitesse de base ──
    case REG_BASIC_SPEED:
      if (basic_fan_speed_n_ != nullptr)
        basic_fan_speed_n_->publish_state((float) bitmask_to_speed(value));
      break;

    // ── Vitesse maximale ──
    case REG_MAX_SPEED:
      if (max_fan_speed_n_ != nullptr)
        max_fan_speed_n_->publish_state((float) bitmask_to_speed(value));
      break;

    // ── Température bypass ──
    case REG_BYPASS_TEMP:
      if (bypass_temp_n_ != nullptr)
        bypass_temp_n_->publish_state((float) value);
      break;

    // ── Seuil dégivrage ──
    case REG_DEFROST_TEMP:
      if (preheating_temp_n_ != nullptr)
        preheating_temp_n_->publish_state(ntc_to_celsius(value));
      break;

    // ── Seuil alerte givre ──
    case REG_FROST_ALARM_TEMP:
      if (frost_alarm_temp_n_ != nullptr)
        frost_alarm_temp_n_->publish_state(ntc_to_celsius(value));
      break;

    // ── Hystérésis antigel ──
    case REG_FROST_HYSTERESIS:
      if (frost_hysteresis_n_ != nullptr)
        frost_hysteresis_n_->publish_state((float) value);
      break;

    // ── % soufflage ──
    case REG_SUPPLY_FAN_PCT:
      if (supply_fan_pct_n_ != nullptr)
        supply_fan_pct_n_->publish_state((float) value);
      break;

    // ── % extraction ──
    case REG_EXHAUST_FAN_PCT:
      if (exhaust_fan_pct_n_ != nullptr)
        exhaust_fan_pct_n_->publish_state((float) value);
      break;

    // ── Seuil CO₂ (16 bits) ──
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

    // ── Seuil humidité ──
    case REG_HUMIDITY_SET:
      if (humidity_setpoint_n_ != nullptr)
        humidity_setpoint_n_->publish_state(raw_to_humidity(value));
      break;

    // ── Intervalle entretien ──
    case REG_SERVICE_INTERVAL:
      if (service_interval_n_ != nullptr)
        service_interval_n_->publish_state((float) value);
      break;

    // ── Programme 2 (0xB5) ──
    case REG_PROGRAM2:
      if (max_speed_cont_sel_ != nullptr) {
        bool continuous = (value >> BIT_MAX_SPEED_CONT) & 0x01;
        max_speed_cont_sel_->publish_state(continuous ? "Ventilation maximale forcée" : "Normal");
      }
      break;

    default:
      // Registre non géré — log verbose uniquement
      ESP_LOGV(TAG, "Registre 0x%02X ignoré (val=0x%02X)", reg, value);
      break;
  }
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_temperature() — conversion NTC → °C et publication
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
// publish_humidity() — conversion raw → % et publication
// Formule Vallox : (x - 51) / 2.04  (0x33=0%, 0xFF=100%)
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
// publish_fan_speed() — bitmask → vitesse 1-8 et publication
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_fan_speed(uint8_t value) {
  uint8_t speed = bitmask_to_speed(value);
  ESP_LOGD(TAG, "Vitesse fan bitmask=0x%02X → %u/8", value, speed);

  // Publier uniquement le sensor de lecture — PAS fan.make_call()
  // fan.make_call() déclencherait HeliosKwlFan::control() → write_register()
  // → cache mis à jour → prochain poll → publish_fan_speed() → boucle infinie
  // La synchro fan ↔ sensor est gérée dans le YAML via sensor.on_value
  if (fan_speed_sensor_ != nullptr)
    fan_speed_sensor_->publish_state((float) speed);
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_states() — registre 0xA3, bitfield complet
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

  // fan.make_call() supprimé — déclenche write_register → boucle infinie
  // La synchro power ON/OFF ↔ fan est dans le YAML (fan.on_speed_set / sensor.on_value)
  // power_on est lu mais pas utilisé directement ici (la VMC gère l'état physique)
  (void) power_on;
  (void) fault;

  // Switches
  if (co2_regulation_ != nullptr)
    co2_regulation_->publish_state(co2_reg);
  if (humidity_regulation_ != nullptr)
    humidity_regulation_->publish_state(hum_reg);
  if (summer_mode_ != nullptr)
    summer_mode_->publish_state(summer);

  // Binary sensors
  if (heating_indicator_ != nullptr)
    heating_indicator_->publish_state(heating);
  if (filter_maintenance_ != nullptr)
    filter_maintenance_->publish_state(filter_maint);

  // Bypass état textuel
  if (bypass_state_text_ != nullptr)
    bypass_state_text_->publish_state(summer ? "Air frais" : "Chaleur conservée");

  // Indicateur santé global (recalcul à chaque changement d'état)
  update_health_indicator();
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_io_port() — registre 0x08, états physiques des actionneurs
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_io_port(uint8_t value) {
  ESP_LOGD(TAG, "Port I/O 0x08 = 0x%02X", value);

  bool bypass     = (value >> BIT_BYPASS_OPEN)  & 0x01;
  bool fault_rel  = (value >> BIT_FAULT_RELAY)  & 0x01;  // 0=défaut, 1=normal
  bool supply_off = (value >> BIT_SUPPLY_FAN)   & 0x01;  // logique inversée
  bool preheating = (value >> BIT_PREHEATING)   & 0x01;
  bool exhst_off  = (value >> BIT_EXHAUST_FAN)  & 0x01;  // logique inversée
  bool ext_cont   = (value >> BIT_EXT_CONTACT)  & 0x01;

  // Sensor bypass_open (0=hiver/fermé, 1=été/ouvert)
  if (bypass_open_ != nullptr)
    bypass_open_->publish_state((float)(bypass ? 1 : 0));

  // Binary sensors avec logique inversée pour les ventilateurs
  if (supply_fan_running_ != nullptr)
    supply_fan_running_->publish_state(!supply_off);  // 0=marche
  if (exhaust_fan_running_ != nullptr)
    exhaust_fan_running_->publish_state(!exhst_off);  // 0=marche
  if (preheating_active_ != nullptr)
    preheating_active_->publish_state(preheating);
  if (external_contact_ != nullptr)
    external_contact_->publish_state(ext_cont);
  if (fault_relay_ != nullptr)
    fault_relay_->publish_state(!fault_rel);  // 0=fermé=normal → on publie "défaut=false"
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
// publish_boost() — registre 0x71 (état boost/cheminée)
// bit 0 = boost plein air actif
// bit 1 = cycle cheminée actif
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_boost(uint8_t value) {
  ESP_LOGD(TAG, "Boost 0x71 = 0x%02X", value);

  bool fresh_air  = (value >> 0) & 0x01;
  bool fireplace  = (value >> 1) & 0x01;

  boost_cycle_active_ = (fresh_air || fireplace);

  const char *state_str = "Normal";
  if (fresh_air)      state_str = "Cycle Plein Air";
  else if (fireplace) state_str = "Cycle Cheminée";

  if (boost_active_text_ != nullptr)
    boost_active_text_->publish_state(state_str);
  if (boost_state_sensor_ != nullptr)
    boost_state_sensor_->publish_state((float)(fresh_air ? 1 : fireplace ? 2 : 0));
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_boost_remaining() — registre 0x79 (minutes restantes)
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_boost_remaining(uint8_t value) {
  if (boost_remaining_ != nullptr)
    boost_remaining_->publish_state((float) value);
}

// ──────────────────────────────────────────────────────────────────────────────
// publish_fault() — registre 0x36 (code erreur)
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
// publish_program_vars() — registre 0xAA (variables programme)
// bits 0-3 : intervalle mesure sondes (1-15 min)
// bit 4    : recherche auto seuil humidité
// bit 5    : mode contact sec boost
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_program_vars(uint8_t value) {
  ESP_LOGD(TAG, "Variables programme 0xAA = 0x%02X", value);

  uint8_t interval  = (value & 0x0F);  // bits 0-3
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
// publish_co2() — reconstruit la valeur 16 bits CO₂
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::publish_co2(uint8_t high, uint8_t low) {
  uint16_t ppm = bytes_to_co2(high, low);
  ESP_LOGD(TAG, "CO₂ high=0x%02X low=0x%02X → %u ppm", high, low, ppm);
  if (co2_concentration_ != nullptr)
    co2_concentration_->publish_state((float) ppm);
}

// ──────────────────────────────────────────────────────────────────────────────
// update_health_indicator() — calcule l'état agrégé "Santé du Système"
// Vert (0) = tout va bien
// Orange (1) = maintenance filtre requise
// Rouge (2) = défaut actif ou alarme
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::update_health_indicator() {
  // Lire depuis le cache pour éviter un poll supplémentaire
  auto states = get_cached_value(REG_STATES);
  auto alarms = get_cached_value(REG_ALARMS);
  auto fault  = get_cached_value(REG_FAULT_CODE);

  bool has_fault  = states.has_value() && ((states.value() >> BIT_FAULT)       & 0x01);
  bool has_filter = states.has_value() && ((states.value() >> BIT_FILTER_MAINT) & 0x01);
  bool has_co2al  = alarms.has_value() && ((alarms.value() >> BIT_CO2_ALARM)   & 0x01);
  bool has_freeze = alarms.has_value() && ((alarms.value() >> BIT_FREEZE_ALARM) & 0x01);
  bool has_fcode  = fault.has_value()  && (fault.value() != 0x00);

  const char *health;
  float       health_val;

  if (has_fault || has_co2al || has_freeze || has_fcode) {
    health     = "Défaut";
    health_val = 2.0f;
  } else if (has_filter) {
    health     = "Maintenance filtres";
    health_val = 1.0f;
  } else {
    health     = "Optimal";
    health_val = 0.0f;
  }

  if (fault_indicator_sensor_ != nullptr)
    fault_indicator_sensor_->publish_state(health_val);

  // fault_description_ est géré par publish_fault() — pas touché ici
  (void) health;  // utilisé uniquement pour LVGL plus tard
}

// ──────────────────────────────────────────────────────────────────────────────
// write_register() — séquence 3 messages Helios + vérification
// ──────────────────────────────────────────────────────────────────────────────

bool HeliosKwlComponent::write_register(uint8_t reg, uint8_t value) {
  ESP_LOGD(TAG, "Écriture reg=0x%02X val=0x%02X", reg, value);

  // 1. Silence bus pendant 5ms (non-bloquant sur 9600 baud)
  flush_rx(5);

  // 2. Message 1 : Broadcast télécommandes (0x20)
  uint8_t msg1[HELIOS_PACKET_LEN] = {HELIOS_START_BYTE, address_, HELIOS_BROADCAST_RC, reg, value, 0};
  msg1[5] = checksum(msg1, 5);
  write_array(msg1, HELIOS_PACKET_LEN);
  flush();
  delay(2);  // 6 octets @ 9600 baud ≈ 6ms → 2ms inter-message suffit

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

  // 5. Mise à jour optimiste du cache — la VMC broadcastera la confirmation dans ~12s
  //    Pas de vérification bloquante : delay(50)+poll_register() bloque le scheduler
  //    et empêche LVGL de rendre, causant des ralentissements à l'écran tactile
  register_cache_[reg] = {value, millis(), true};
  ESP_LOGD(TAG, "Écriture envoyée reg=0x%02X (cache optimiste)", reg);
  return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// set_register_bit() / read_register_bit() — opérations sur bits individuels
// Lit le registre depuis le cache, modifie le bit, réécrit
// ──────────────────────────────────────────────────────────────────────────────

bool HeliosKwlComponent::set_register_bit(uint8_t reg, uint8_t bit, bool state) {
  // Utiliser UNIQUEMENT le cache — poll_register() est bloquant (200ms)
  // et casse LVGL + le scheduler ESPHome quand déclenché depuis un callback tactile
  auto cached = get_cached_value(reg);
  if (!cached.has_value()) {
    ESP_LOGW(TAG, "set_register_bit : cache vide reg=0x%02X, skip (sera retransmis)", reg);
    return false;
  }
  uint8_t current = *cached;

  uint8_t new_val = state ? (current | (1u << bit)) : (current & ~(1u << bit));

  if (new_val == current) {
    ESP_LOGD(TAG, "set_register_bit : bit %u de reg=0x%02X déjà à %d", bit, reg, (int)state);
    return true;  // Rien à faire
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
// Protocole : envoyer un paquet avec val=0x00, la VMC répond avec la valeur
// ──────────────────────────────────────────────────────────────────────────────

optional<uint8_t> HeliosKwlComponent::poll_register(uint8_t reg, uint8_t retries) {
  for (uint8_t attempt = 0; attempt < retries; attempt++) {
    // Vider le buffer (même stratégie que le code legacy)
    flush_rx(5);

    // Format confirmé par le code legacy ET la doc Vallox Annexe B :
    // VARIABLE=0x00 (demande de lecture), DATA=registre demandé
    uint8_t req[HELIOS_PACKET_LEN] = {HELIOS_START_BYTE, address_, HELIOS_MAINBOARD, 0x00, reg, 0};
    req[5] = checksum(req, 5);
    write_array(req, HELIOS_PACKET_LEN);
    flush();

    // Lire la réponse — exactement comme le legacy : TX puis read_array<6>() direct
    // Pas de flush après TX : la réponse VMC arrive rapidement et ne doit pas être jetée
    auto response = read_array<HELIOS_PACKET_LEN>();
    if (response.has_value()) {
      const auto& arr = *response;
      if (verify_checksum(arr.data(), HELIOS_PACKET_LEN)) {
        // Vérifier : src=MAINBOARD, dst=nous, registre=celui demandé
        if (arr[1] == HELIOS_MAINBOARD && arr[2] == address_ && arr[3] == reg) {
          uint8_t val = arr[4];
          register_cache_[reg] = {val, millis(), true};
          ESP_LOGV(TAG, "Poll reg=0x%02X → 0x%02X (tentative %u)", reg, val, attempt + 1);
          return val;
        } else {
          ESP_LOGW(TAG, "Poll réponse inattendue : src=0x%02X dst=0x%02X reg=0x%02X (attendu reg=0x%02X)",
                   arr[1], arr[2], arr[3], reg);
        }
      } else {
        ESP_LOGW(TAG, "Poll CRC invalide reg=0x%02X", reg);
      }
    }

    ESP_LOGW(TAG, "Poll timeout reg=0x%02X (tentative %u/%u)", reg, attempt + 1, retries);
  }

  return {};  // Toutes les tentatives épuisées
}

// ──────────────────────────────────────────────────────────────────────────────
// get_cached_value() — retourne la valeur en cache si elle est valide
// ──────────────────────────────────────────────────────────────────────────────

optional<uint8_t> HeliosKwlComponent::get_cached_value(uint8_t reg) {
  const auto &entry = register_cache_[reg];
  if (entry.valid)
    return entry.value;
  return {};
}

// ──────────────────────────────────────────────────────────────────────────────
// flush_rx() — vide le buffer UART pendant timeout_ms ms
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::flush_rx(uint32_t timeout_ms) {
  // Reproduit le comportement du legacy flush_read_buffer() :
  // Boucle TANT QUE des octets arrivent + attend silence de timeout_ms.
  // Ceci garantit que les échos TX (RS485 half-duplex) sont complètement
  // purgés avant d'appeler read_array<6>() pour lire la vraie réponse.
  const uint32_t safety = millis() + 200;  // timeout de sécurité absolu
  uint32_t last_byte_time = millis();
  // Règle NASA n°2 : borne = 200 itérations max (200ms à 1ms/iter)
  for (uint32_t iter = 0; iter < 200; iter++) {
    if (millis() > safety) {
      ESP_LOGW(TAG, "flush_rx: safety timeout");
      break;
    }
    if (millis() - last_byte_time >= timeout_ms)
      break;
    for (size_t i = 0; i < RX_BUFFER_SIZE && available(); i++) {
      read();
      last_byte_time = millis();
    }
    yield();
  }
  rx_buffer_len_ = 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// checksum() — XOR de tous les octets (protocole Helios/Vallox)
// ──────────────────────────────────────────────────────────────────────────────

uint8_t HeliosKwlComponent::checksum(const uint8_t *data, size_t len) {
  // Protocole Helios/Vallox : somme arithmétique (pas XOR)
  // Identique au legacy : std::accumulate(begin, end, 0) tronqué à 8 bits
  uint8_t crc = 0;
  for (size_t i = 0; i < len; i++)
    crc += data[i];
  return crc;
}

bool HeliosKwlComponent::verify_checksum(const uint8_t *data, size_t len) {
  if (len < 2) return false;
  return checksum(data, len - 1) == data[len - 1];
}

// ──────────────────────────────────────────────────────────────────────────────
// count_ones() — compte les bits à 1 dans un octet
// ──────────────────────────────────────────────────────────────────────────────

uint8_t HeliosKwlComponent::count_ones(uint8_t byte) {
  // Règle NASA n°2 : borne = 8 bits max (uint8_t)
  uint8_t count = 0;
  for (uint8_t i = 0; i < 8u; i++) {
    count += (byte & 1u);
    byte >>= 1;
  }
  return count;
}

// ──────────────────────────────────────────────────────────────────────────────
// Conversions NTC ↔ °C
// ──────────────────────────────────────────────────────────────────────────────

float HeliosKwlComponent::ntc_to_celsius(uint8_t ntc) {
  return (float) NTC_TABLE[ntc];
}

uint8_t HeliosKwlComponent::celsius_to_ntc(float celsius) {
  // Recherche linéaire dans la table (la table n'est pas strictement monotone
  // aux extrêmes, mais l'est dans la plage utile -30..+60°C)
  int target = (int) celsius;
  // Chercher la valeur NTC la plus proche dans la plage utile (0x20..0xE0)
  uint8_t best_ntc = 0x64;  // 0°C par défaut
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

// ──────────────────────────────────────────────────────────────────────────────
// Conversions humidité : raw ↔ %
// Formule Vallox : %RH = (raw - 51) / 2.04  (0x33=0%, 0xFF=100%)
// ──────────────────────────────────────────────────────────────────────────────

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

// ──────────────────────────────────────────────────────────────────────────────
// Conversions vitesse : bitmask ↔ 1-8
// Vallox : 0x01=1, 0x03=2, 0x07=3, 0x0F=4, 0x1F=5, 0x3F=6, 0x7F=7, 0xFF=8
// Comptage des bits à 1 dans l'octet
// ──────────────────────────────────────────────────────────────────────────────

uint8_t HeliosKwlComponent::speed_to_bitmask(uint8_t speed) {
  if (speed == 0) return 0x00;
  if (speed > 8)  speed = 8;
  // (1 << speed) - 1 donne le bon masque pour 1-8
  return (uint8_t)((1u << speed) - 1u);
}

uint8_t HeliosKwlComponent::bitmask_to_speed(uint8_t mask) {
  return count_ones(mask);
}

// ──────────────────────────────────────────────────────────────────────────────
// Conversions CO₂ : 16 bits ↔ deux octets
// ──────────────────────────────────────────────────────────────────────────────

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
  // Écrire le bit ON/OFF dans 0xA3
  set_register_bit(REG_STATES, BIT_POWER, on);

  // Écrire la vitesse si fournie
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
  // Bits 0-3 de 0xAA — préserver les bits 4-7
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
// ──────────────────────────────────────────────────────────────────────────────

void HeliosKwlComponent::trigger_boost_airflow() {
  // Activer le cycle plein air (bit 0 de 0x71)
  ESP_LOGI(TAG, "Déclenchement Cycle Plein Air (45 min)");
  write_register(REG_BOOST_STATE, 0x01);
}

void HeliosKwlComponent::trigger_boost_fireplace() {
  // Activer le cycle cheminée (bit 1 de 0x71)
  ESP_LOGI(TAG, "Déclenchement Cycle Cheminée (15 min)");
  write_register(REG_BOOST_STATE, 0x02);
}

void HeliosKwlComponent::acknowledge_maintenance() {
  // Remettre le bit filtre à 0 dans 0xA3
  ESP_LOGI(TAG, "Acquittement maintenance filtres");
  set_register_bit(REG_STATES, BIT_FILTER_MAINT, false);
}

// ──────────────────────────────────────────────────────────────────────────────
// HeliosKwlFan — Entité fan native ESPHome
// ──────────────────────────────────────────────────────────────────────────────

fan::FanTraits HeliosKwlFan::get_traits() {
  // Constructeur FanTraits(oscillation, speed, direction, speed_count)
  // API ESPHome 2025+ : plus de setters individuels
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

  // Mettre à jour l'état local immédiatement (optimisme UI)
  if (call.get_state().has_value())
    this->state = *call.get_state();
  if (speed.has_value())
    this->speed = *speed;
  this->publish_state();
}

}  // namespace helios_kwl
}  // namespace esphome
