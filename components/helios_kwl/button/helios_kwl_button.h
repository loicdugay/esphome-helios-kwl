#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// helios_kwl_button.h — Classes button pour Helios KWL EC 300 Pro
// ──────────────────────────────────────────────────────────────────────────────

#include "esphome/components/button/button.h"
#include "../helios_kwl.h"

namespace esphome {
namespace helios_kwl {

// ── Classe de base ────────────────────────────────────────────────────────────
class HeliosKwlButtonBase : public button::Button {
 public:
  void set_parent(HeliosKwlComponent *parent) { parent_ = parent; }

 protected:
  HeliosKwlComponent *parent_{nullptr};
};

// ── Cycle Plein Air : 45 min à vitesse max (0x71 bit 0) ──────────────────────
class HeliosKwlBoostAirflowButton : public HeliosKwlButtonBase {
 protected:
  void press_action() override {
    if (parent_ != nullptr)
      parent_->trigger_boost_airflow();
  }
};

// ── Cycle Cheminée : 15 min extraction coupée (0x71 bit 1) ───────────────────
class HeliosKwlBoostFireplaceButton : public HeliosKwlButtonBase {
 protected:
  void press_action() override {
    if (parent_ != nullptr)
      parent_->trigger_boost_fireplace();
  }
};

// ── Confirmer remplacement filtres — reset 0xA3 bit 7 ────────────────────────
class HeliosKwlAckMaintenanceButton : public HeliosKwlButtonBase {
 protected:
  void press_action() override {
    if (parent_ != nullptr)
      parent_->acknowledge_maintenance();
  }
};

}  // namespace helios_kwl
}  // namespace esphome
