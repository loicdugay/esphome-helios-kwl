#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// helios_kwl_switch.h — Classes switch pour Helios KWL EC 300 Pro
// Chaque classe hérite de switch_::Switch et délègue l'écriture au composant
// principal via le pointeur parent_.
// ──────────────────────────────────────────────────────────────────────────────

#include "esphome/components/switch/switch.h"
#include "helios_kwl.h"

namespace esphome {
namespace helios_kwl {

// ── Classe de base commune ────────────────────────────────────────────────────

class HeliosKwlSwitchBase : public switch_::Switch {
 public:
  void set_parent(HeliosKwlComponent *parent) { parent_ = parent; }

 protected:
  HeliosKwlComponent *parent_{nullptr};
};

// ── Régulation CO₂ (0xA3 bit 1) ─────────────────────────────────────────────

class HeliosKwlCo2RegSwitch : public HeliosKwlSwitchBase {
 protected:
  void write_state(bool state) override {
    if (parent_ != nullptr)
      parent_->control_co2_regulation(state);
    publish_state(state);
  }
};

// ── Régulation humidité (0xA3 bit 2) ─────────────────────────────────────────

class HeliosKwlHumidityRegSwitch : public HeliosKwlSwitchBase {
 protected:
  void write_state(bool state) override {
    if (parent_ != nullptr)
      parent_->control_humidity_regulation(state);
    publish_state(state);
  }
};

// ── Mode fraîcheur / été (0xA3 bit 3) ────────────────────────────────────────

class HeliosKwlSummerModeSwitch : public HeliosKwlSwitchBase {
 protected:
  void write_state(bool state) override {
    if (parent_ != nullptr)
      parent_->control_summer_mode(state);
    publish_state(state);
  }
};

}  // namespace helios_kwl
}  // namespace esphome
