#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// helios_kwl_select.h — Classes select pour Helios KWL EC 300 Pro
// ──────────────────────────────────────────────────────────────────────────────

#include "esphome/components/select/select.h"
#include "../helios_kwl.h"

namespace esphome {
namespace helios_kwl {

// ── Classe de base ────────────────────────────────────────────────────────────
class HeliosKwlSelectBase : public select::Select {
 public:
  void set_parent(HeliosKwlComponent *parent) { parent_ = parent; }

 protected:
  HeliosKwlComponent *parent_{nullptr};
};

// ── Mode contact sec : Cheminée / Plein Air (0xAA bit 5) ─────────────────────
class HeliosKwlBoostFireplaceSelect : public HeliosKwlSelectBase {
 protected:
  void control(const std::string &value) override {
    if (parent_ != nullptr)
      parent_->control_boost_fireplace_mode(value == "Cycle Plein Air");
    publish_state(value);
  }
};

// ── Détection humidité auto (0xAA bit 4) ──────────────────────────────────────
class HeliosKwlHumidityAutoSelect : public HeliosKwlSelectBase {
 protected:
  void control(const std::string &value) override {
    if (parent_ != nullptr)
      parent_->control_humidity_auto_search(value == "Apprentissage auto");
    publish_state(value);
  }
};

// ── Vitesse max continue (0xB5 bit 0) ────────────────────────────────────────
class HeliosKwlMaxSpeedContSelect : public HeliosKwlSelectBase {
 protected:
  void control(const std::string &value) override {
    if (parent_ != nullptr)
      parent_->control_max_speed_continuous(value == "Ventilation maximale forcée");
    publish_state(value);
  }
};

}  // namespace helios_kwl
}  // namespace esphome
