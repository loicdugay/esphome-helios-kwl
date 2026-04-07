#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// helios_kwl.h — Composant ESPHome pour VMC Helios KWL EC 300 Pro
// Protocole RS485 Vallox/Helios — Refonte complète phase 1 — Corrigé
// ──────────────────────────────────────────────────────────────────────────────

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

// ──────────────────────────────────────────────────────────────────────────────
// Constantes du protocole RS485
// ──────────────────────────────────────────────────────────────────────────────

static constexpr uint8_t HELIOS_START_BYTE    = 0x01;
static constexpr uint8_t HELIOS_MAINBOARD     = 0x11;  // Adresse carte mère principale
static constexpr uint8_t HELIOS_BROADCAST_ALL = 0x10;  // Broadcast toutes cartes mères
static constexpr uint8_t HELIOS_BROADCAST_RC  = 0x20;  // Broadcast télécommandes
static constexpr uint8_t HELIOS_PACKET_LEN    = 6;

// Adresses télécommandes standard (notre ESP se fait passer pour une télécommande)
static constexpr uint8_t HELIOS_ADDR_DEFAULT  = 0x2F;  // Adresse par défaut ESP

// ──────────────────────────────────────────────────────────────────────────────
// Registres broadcastés (reçus passivement — carte mère → bus, toutes les ~12s)
// Annexe B : 2AH 2BH 2CH 32H 33H 34H 35H
// ──────────────────────────────────────────────────────────────────────────────

static constexpr uint8_t REG_CO2_HIGH         = 0x2B;
static constexpr uint8_t REG_CO2_LOW          = 0x2C;
static constexpr uint8_t REG_TEMP_OUTSIDE     = 0x32;  // NTC → °C
static constexpr uint8_t REG_TEMP_EXHAUST     = 0x33;  // NTC → °C
static constexpr uint8_t REG_TEMP_EXTRACT     = 0x34;  // NTC → °C
static constexpr uint8_t REG_TEMP_SUPPLY      = 0x35;  // NTC → °C

// ──────────────────────────────────────────────────────────────────────────────
// Registres à poller activement (rotation dans update())
// ──────────────────────────────────────────────────────────────────────────────

static constexpr uint8_t REG_FAN_SPEED        = 0x29;  // Vitesse actuelle (bitmask → 1-8)
static constexpr uint8_t REG_HUMIDITY1        = 0x2F;  // Sonde humidité 1
static constexpr uint8_t REG_HUMIDITY2        = 0x30;  // Sonde humidité 2
static constexpr uint8_t REG_STATES           = 0xA3;  // États/commandes (bitfield)
static constexpr uint8_t REG_IO_PORT          = 0x08;  // Port I/O physique (bitfield)
static constexpr uint8_t REG_ALARMS           = 0x6D;  // Alarmes CO₂ + gel (FLAGS 2)
static constexpr uint8_t REG_BOOST_STATE      = 0x71;  // FLAGS 6 — boost/cheminée
static constexpr uint8_t REG_BOOST_REMAINING  = 0x79;  // Timer boost (minutes)
static constexpr uint8_t REG_FAULT_CODE       = 0x36;  // Code erreur brut
static constexpr uint8_t REG_SERVICE_MONTHS   = 0xAB;  // Mois restants maintenance
static constexpr uint8_t REG_PROGRAM_VARS     = 0xAA;  // Variables programme (bitfield)
static constexpr uint8_t REG_BASIC_SPEED      = 0xA9;  // Vitesse de base (plancher)
static constexpr uint8_t REG_MAX_SPEED        = 0xA5;  // Vitesse maximale (plafond)
static constexpr uint8_t REG_BYPASS_TEMP      = 0xAF;  // Seuil température bypass
static constexpr uint8_t REG_DEFROST_TEMP     = 0xA7;  // Seuil dégivrage
static constexpr uint8_t REG_FROST_ALARM_TEMP = 0xA8;  // Seuil alerte givre
static constexpr uint8_t REG_FROST_HYSTERESIS = 0xB2;  // Hystérésis antigel
static constexpr uint8_t REG_SUPPLY_FAN_PCT   = 0xB0;  // % puissance soufflage (65-100)
static constexpr uint8_t REG_EXHAUST_FAN_PCT  = 0xB1;  // % puissance extraction (65-100)
static constexpr uint8_t REG_CO2_SETPOINT_H   = 0xB3;  // Seuil CO₂ high byte
static constexpr uint8_t REG_CO2_SETPOINT_L   = 0xB4;  // Seuil CO₂ low byte
static constexpr uint8_t REG_HUMIDITY_SET     = 0xAE;  // Seuil humidité (1-99%)
static constexpr uint8_t REG_SERVICE_INTERVAL = 0xA6;  // Intervalle entretien (mois)
static constexpr uint8_t REG_PROGRAM2         = 0xB5;  // Variables programme 2

// ──────────────────────────────────────────────────────────────────────────────
// Bits du registre 0xA3 (REG_STATES)
// ──────────────────────────────────────────────────────────────────────────────

static constexpr uint8_t BIT_POWER            = 0;  // 0=veille, 1=marche
static constexpr uint8_t BIT_CO2_REG          = 1;  // Régulation CO₂
static constexpr uint8_t BIT_HUMIDITY_REG     = 2;  // Régulation humidité
static constexpr uint8_t BIT_SUMMER_MODE      = 3;  // Mode fraîcheur (bypass)
// bit 4 : réservé
static constexpr uint8_t BIT_HEATING          = 5;  // Appoint de chaleur (post-échangeur)
static constexpr uint8_t BIT_FAULT            = 6;  // Défaut actif
static constexpr uint8_t BIT_FILTER_MAINT     = 7;  // Maintenance filtre requise

// ──────────────────────────────────────────────────────────────────────────────
// Bits du registre 0x08 (REG_IO_PORT)
// ──────────────────────────────────────────────────────────────────────────────

// bit 0 : réservé
static constexpr uint8_t BIT_BYPASS_OPEN      = 1;  // Volet bypass (1=ouvert/été)
static constexpr uint8_t BIT_FAULT_RELAY      = 2;  // Relais défaut (0=défaut, 1=normal)
static constexpr uint8_t BIT_SUPPLY_FAN       = 3;  // Ventilateur soufflage (0=marche, 1=arrêt)
static constexpr uint8_t BIT_PREHEATING       = 4;  // Préchauffage / dégivrage actif
static constexpr uint8_t BIT_EXHAUST_FAN      = 5;  // Ventilateur extraction (0=marche, 1=arrêt)
static constexpr uint8_t BIT_EXT_CONTACT      = 6;  // Contact externe (bouton S)

// ──────────────────────────────────────────────────────────────────────────────
// Bits du registre 0x6D (REG_ALARMS / FLAGS 2)
// ──────────────────────────────────────────────────────────────────────────────

static constexpr uint8_t BIT_CO2_ALARM        = 6;  // Alarme CO₂ >5000ppm
static constexpr uint8_t BIT_FREEZE_ALARM     = 7;  // Alarme gel échangeur

// ──────────────────────────────────────────────────────────────────────────────
// Bits du registre 0x71 (REG_BOOST_STATE / FLAGS 6)
// Source : Vallox Digit Protocol — 71H FLAGS 6
// bit 4 = remote control (read only)
// bit 5 = activating the fireplace switch (read + set to 1 pour activer)
// bit 6 = fireplace / boost function active (read only)
// ──────────────────────────────────────────────────────────────────────────────

static constexpr uint8_t BIT_BOOST_RC_ACTIVE  = 4;  // Télécommande active (read only)
static constexpr uint8_t BIT_BOOST_ACTIVATE   = 5;  // Activer cycle boost/cheminée (écriture)
static constexpr uint8_t BIT_BOOST_RUNNING    = 6;  // Cycle en cours (read only)

// ──────────────────────────────────────────────────────────────────────────────
// Bits du registre 0xAA (REG_PROGRAM_VARS)
// ──────────────────────────────────────────────────────────────────────────────

// bits 0-3 : intervalle mesure sondes (1-15 min)
static constexpr uint8_t BIT_HUMIDITY_AUTO    = 4;  // Recherche auto seuil humidité
static constexpr uint8_t BIT_BOOST_FIRE_MODE  = 5;  // Mode contact sec : 0=cheminée, 1=plein air

// ──────────────────────────────────────────────────────────────────────────────
// Bit du registre 0xB5 (REG_PROGRAM2)
// ──────────────────────────────────────────────────────────────────────────────

static constexpr uint8_t BIT_MAX_SPEED_CONT   = 0;  // Vitesse max forcée en continu

// ──────────────────────────────────────────────────────────────────────────────
// Taille du buffer RS485
// ──────────────────────────────────────────────────────────────────────────────

static constexpr size_t RX_BUFFER_SIZE        = 512;
static constexpr size_t REGISTER_COUNT        = 256;

// ──────────────────────────────────────────────────────────────────────────────
// Struct : cache d'un registre
// ──────────────────────────────────────────────────────────────────────────────

struct RegisterCache {
  uint8_t  value{0};
  uint32_t last_update{0};  // millis()
  bool     valid{false};
};

// ──────────────────────────────────────────────────────────────────────────────
// Struct : tâche de polling rotatif
// ──────────────────────────────────────────────────────────────────────────────

struct PollTask {
  uint8_t  reg;
  uint32_t interval_ms;   // Intervalle cible entre deux polls
  uint32_t last_polled;   // millis() du dernier poll réel
};

// ──────────────────────────────────────────────────────────────────────────────
// Déclarations anticipées des classes entités
// ──────────────────────────────────────────────────────────────────────────────

class HeliosKwlFan;

// ──────────────────────────────────────────────────────────────────────────────
// Classe principale : HeliosKwlComponent
// ──────────────────────────────────────────────────────────────────────────────

class HeliosKwlComponent : public uart::UARTDevice, public PollingComponent {
 public:
  // ── Cycle de vie ESPHome ──
  void setup() override;
  void loop() override;    // Écoute passive continue
  void update() override;  // Polling rotatif (1 registre par appel)
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // ── Configuration (appelé depuis __init__.py / to_code) ──
  void set_address(uint8_t address) { address_ = address; }

  // ── Écriture RS485 — séquence 3 messages Helios ──
  bool write_register(uint8_t reg, uint8_t value);

  // ── Helpers bits individuels ──
  bool set_register_bit(uint8_t reg, uint8_t bit, bool state);
  bool read_register_bit(uint8_t reg, uint8_t bit);

  // ── Lecture active (avec retry) ──
  optional<uint8_t> poll_register(uint8_t reg, uint8_t retries = 3);

  // ── Accès cache ──
  optional<uint8_t> get_cached_value(uint8_t reg);

  // ── Fan intégré ──
  void set_fan_speed(uint8_t speed);  // 1-8 ; 0 = arrêt
  void set_fan_on(bool on);

  // ────────────────────────────────────────────────────────────────────────────
  // Setters entités — appelés depuis les sous-plateformes Python (to_code)
  // ────────────────────────────────────────────────────────────────────────────

  // Sensors (14)
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

  // Text sensors (3)
  void set_fault_description_text(text_sensor::TextSensor *s){ fault_description_      = s; }
  void set_boost_active_text(text_sensor::TextSensor *s)     { boost_active_text_      = s; }
  void set_bypass_state_text(text_sensor::TextSensor *s)     { bypass_state_text_      = s; }

  // Binary sensors (9)
  void set_preheating_active_sensor(binary_sensor::BinarySensor *s) { preheating_active_  = s; }
  void set_freeze_alarm_sensor(binary_sensor::BinarySensor *s)      { freeze_alarm_       = s; }
  void set_co2_alarm_sensor(binary_sensor::BinarySensor *s)         { co2_alarm_          = s; }
  void set_filter_maintenance_sensor(binary_sensor::BinarySensor *s){ filter_maintenance_ = s; }
  void set_heating_indicator_sensor(binary_sensor::BinarySensor *s) { heating_indicator_  = s; }
  void set_supply_fan_running_sensor(binary_sensor::BinarySensor *s){ supply_fan_running_ = s; }
  void set_exhaust_fan_running_sensor(binary_sensor::BinarySensor *s){exhaust_fan_running_= s; }
  void set_external_contact_sensor(binary_sensor::BinarySensor *s)  { external_contact_   = s; }
  void set_fault_relay_sensor(binary_sensor::BinarySensor *s)       { fault_relay_        = s; }

  // Switches (3)
  void set_co2_regulation_switch(switch_::Switch *s)   { co2_regulation_  = s; }
  void set_humidity_regulation_switch(switch_::Switch *s){ humidity_regulation_ = s; }
  void set_summer_mode_switch(switch_::Switch *s)      { summer_mode_     = s; }

  // Fan natif (1)
  void set_fan(HeliosKwlFan *f) { fan_ = f; }

  // Numbers (13)
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

  // Selects (3)
  void set_boost_fireplace_mode_select(select::Select *s) { boost_fireplace_sel_  = s; }
  void set_humidity_auto_search_select(select::Select *s) { humidity_auto_sel_    = s; }
  void set_max_speed_continuous_select(select::Select *s) { max_speed_cont_sel_   = s; }

  // ────────────────────────────────────────────────────────────────────────────
  // Actions depuis les entités (number, select, button, switch, fan)
  // ────────────────────────────────────────────────────────────────────────────

  // Fan
  void control_fan(bool on, optional<uint8_t> speed);

  // Switches
  void control_co2_regulation(bool enabled);
  void control_humidity_regulation(bool enabled);
  void control_summer_mode(bool enabled);

  // Numbers — vitesses
  void control_basic_fan_speed(uint8_t speed);   // 1-8
  void control_max_fan_speed(uint8_t speed);     // 1-8

  // Numbers — températures
  void control_bypass_temp(float celsius);        // 0..+25°C
  void control_preheating_temp(float celsius);    // -6..+15°C
  void control_frost_alarm_temp(float celsius);   // -6..+15°C
  void control_frost_hysteresis(float celsius);   // 1..10°C

  // Numbers — qualité d'air
  void control_co2_setpoint(uint16_t ppm);        // 500..2000 ppm
  void control_humidity_setpoint(uint8_t percent);// 1..99 %

  // Numbers — entretien et configuration
  void control_regulation_interval(uint8_t minutes); // 1..15 min
  void control_supply_fan_percent(uint8_t percent);   // 65..100 %
  void control_exhaust_fan_percent(uint8_t percent);  // 65..100 %
  void control_service_interval(uint8_t months);      // 1..15 mois

  // Selects
  void control_boost_fireplace_mode(bool is_fresh_air); // false=cheminée, true=plein air
  void control_humidity_auto_search(bool auto_mode);    // false=manuel, true=auto
  void control_max_speed_continuous(bool continuous);   // false=normal, true=forcé

  // Buttons
  void trigger_boost_airflow();      // Plein air 45 min (0xAA bit5=1 puis 0x71 bit5=1)
  void trigger_boost_fireplace();    // Cheminée 15 min  (0xAA bit5=0 puis 0x71 bit5=1)
  void acknowledge_maintenance();    // Reset filtre (0xA3 bit 7 → 0)

  // ────────────────────────────────────────────────────────────────────────────
  // Fonctions de conversion (statiques, utilisables depuis les sous-plateformes)
  // ────────────────────────────────────────────────────────────────────────────

  static float     ntc_to_celsius(uint8_t ntc);
  static uint8_t   celsius_to_ntc(float celsius);
  static float     raw_to_humidity(uint8_t raw);
  static uint8_t   humidity_to_raw(float percent);
  static uint8_t   speed_to_bitmask(uint8_t speed);  // 1-8 → 0x01..0xFF
  static uint8_t   bitmask_to_speed(uint8_t mask);   // 0x01..0xFF → 1-8
  static uint16_t  bytes_to_co2(uint8_t high, uint8_t low);
  static std::pair<uint8_t, uint8_t> co2_to_bytes(uint16_t ppm);

 protected:
  // ── Adresse RS485 de cet ESP sur le bus ──
  uint8_t address_{HELIOS_ADDR_DEFAULT};

  // ── Buffer d'écoute passive (accumule les octets bruts) ──
  std::array<uint8_t, RX_BUFFER_SIZE> rx_buffer_{};
  size_t rx_buffer_len_{0};

  // ── Cache des 256 registres ──
  std::array<RegisterCache, REGISTER_COUNT> register_cache_{};

  // ── Table de polling rotatif (registres non broadcastés) ──
  // Statique : initialisée dans setup(), jamais réallouée (règle NASA n°3)
  static constexpr size_t POLL_TABLE_SIZE = 24;
  std::array<PollTask, POLL_TABLE_SIZE> poll_tasks_{};
  size_t poll_task_count_{0};   // Nombre de tâches actives
  size_t current_poll_index_{0};

  // ── Drapeaux d'état internes ──
  bool     boost_cycle_active_{false};  // Un cycle boost/cheminée est en cours
  uint32_t last_rx_time_{0};       // Pour détection silence bus

  // ── Fan natif (pointeur vers la sous-classe) ──
  HeliosKwlFan *fan_{nullptr};

  // ────────────────────────────────────────────────────────────────────────────
  // Pointeurs entités (45 au total)
  // ────────────────────────────────────────────────────────────────────────────

  // Sensors (14)
  sensor::Sensor *temperature_outside_{nullptr};
  sensor::Sensor *temperature_extract_{nullptr};
  sensor::Sensor *temperature_supply_{nullptr};
  sensor::Sensor *temperature_exhaust_{nullptr};
  sensor::Sensor *humidity_sensor1_{nullptr};
  sensor::Sensor *humidity_sensor2_{nullptr};
  sensor::Sensor *co2_concentration_{nullptr};
  sensor::Sensor *boost_remaining_{nullptr};
  sensor::Sensor *fault_code_{nullptr};
  sensor::Sensor *service_months_{nullptr};
  sensor::Sensor *bypass_open_{nullptr};
  sensor::Sensor *fan_speed_sensor_{nullptr};
  sensor::Sensor *fault_indicator_sensor_{nullptr};
  sensor::Sensor *boost_state_sensor_{nullptr};

  // Text sensors (3)
  text_sensor::TextSensor *fault_description_{nullptr};
  text_sensor::TextSensor *boost_active_text_{nullptr};
  text_sensor::TextSensor *bypass_state_text_{nullptr};

  // Binary sensors (9)
  binary_sensor::BinarySensor *preheating_active_{nullptr};
  binary_sensor::BinarySensor *freeze_alarm_{nullptr};
  binary_sensor::BinarySensor *co2_alarm_{nullptr};
  binary_sensor::BinarySensor *filter_maintenance_{nullptr};
  binary_sensor::BinarySensor *heating_indicator_{nullptr};
  binary_sensor::BinarySensor *supply_fan_running_{nullptr};
  binary_sensor::BinarySensor *exhaust_fan_running_{nullptr};
  binary_sensor::BinarySensor *external_contact_{nullptr};
  binary_sensor::BinarySensor *fault_relay_{nullptr};

  // Switches (3)
  switch_::Switch *co2_regulation_{nullptr};
  switch_::Switch *humidity_regulation_{nullptr};
  switch_::Switch *summer_mode_{nullptr};

  // Numbers (13)
  number::Number *basic_fan_speed_n_{nullptr};
  number::Number *max_fan_speed_n_{nullptr};
  number::Number *bypass_temp_n_{nullptr};
  number::Number *preheating_temp_n_{nullptr};
  number::Number *frost_alarm_temp_n_{nullptr};
  number::Number *frost_hysteresis_n_{nullptr};
  number::Number *co2_setpoint_n_{nullptr};
  number::Number *humidity_setpoint_n_{nullptr};
  number::Number *regulation_interval_n_{nullptr};
  number::Number *supply_fan_pct_n_{nullptr};
  number::Number *exhaust_fan_pct_n_{nullptr};
  number::Number *service_interval_n_{nullptr};

  // Selects (3)
  select::Select *boost_fireplace_sel_{nullptr};
  select::Select *humidity_auto_sel_{nullptr};
  select::Select *max_speed_cont_sel_{nullptr};

  // ────────────────────────────────────────────────────────────────────────────
  // Méthodes privées — écoute passive et parsing
  // ────────────────────────────────────────────────────────────────────────────

  void accumulate_rx();
  bool process_rx_buffer();
  void handle_broadcast(uint8_t sender, uint8_t reg, uint8_t value);
  void handle_command(uint8_t sender, uint8_t recipient, uint8_t reg, uint8_t value);

  // ────────────────────────────────────────────────────────────────────────────
  // Méthodes privées — publication vers les entités HA
  // ────────────────────────────────────────────────────────────────────────────

  void publish_register(uint8_t reg, uint8_t value);
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
  void publish_co2(uint8_t high, uint8_t low);
  void update_health_indicator();

  // ────────────────────────────────────────────────────────────────────────────
  // Méthodes privées — communication RS485
  // ────────────────────────────────────────────────────────────────────────────

  void flush_rx(uint32_t timeout_ms = 10);

  // Calcule le checksum du protocole Helios (somme arithmétique 8 bits)
  static uint8_t  checksum(const uint8_t *data, size_t len);
  static bool     verify_checksum(const uint8_t *data, size_t len);
  static uint8_t  count_ones(uint8_t byte);
};

// ──────────────────────────────────────────────────────────────────────────────
// HeliosKwlFan — Entité fan native ESPHome
// ──────────────────────────────────────────────────────────────────────────────

class HeliosKwlFan : public fan::Fan, public Component {
 public:
  explicit HeliosKwlFan(HeliosKwlComponent *parent) : parent_(parent) {}

  fan::FanTraits get_traits() override;
  void control(const fan::FanCall &call) override;

 protected:
  HeliosKwlComponent *parent_;
};

}  // namespace helios_kwl
}  // namespace esphome

// ── Includes des sous-plateformes C++ ────────────────────────────────────────
#include "switch/helios_kwl_switch.h"
#include "number/helios_kwl_number.h"
#include "select/helios_kwl_select.h"
#include "button/helios_kwl_button.h"