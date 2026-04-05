#pragma once

// ──────────────────────────────────────────────────────────────────────────────
// helios_kwl_number.h — Classe number générique pour Helios KWL EC 300 Pro
// Chaque instance reçoit un callback (lambda-free) vers la méthode de contrôle
// du composant principal.
// ──────────────────────────────────────────────────────────────────────────────

#include "esphome/components/number/number.h"
#include "helios_kwl.h"

namespace esphome {
namespace helios_kwl {

// Type pointeur de méthode pour les nombres flottants
using NumberFloatSetter = void (HeliosKwlComponent::*)(float);
// Type pointeur de méthode pour les nombres entiers 8 bits
using NumberUint8Setter = void (HeliosKwlComponent::*)(uint8_t);
// Type pointeur de méthode pour les nombres entiers 16 bits
using NumberUint16Setter = void (HeliosKwlComponent::*)(uint16_t);

// ── Classe générique ──────────────────────────────────────────────────────────
// Le YAML choisit le bon type via la clé de configuration.
// En pratique on utilise un seul type avec cast interne.

class HeliosKwlNumber : public number::Number {
 public:
  void set_parent(HeliosKwlComponent *parent) { parent_ = parent; }

  // Enregistrer le setter flottant (températures, %)
  void set_float_setter(NumberFloatSetter setter) { float_setter_ = setter; }

  // Enregistrer le setter uint8 (vitesses 1-8, mois, intervalles)
  void set_uint8_setter(NumberUint8Setter setter) { uint8_setter_ = setter; }

  // Enregistrer le setter uint16 (CO₂ setpoint en ppm)
  void set_uint16_setter(NumberUint16Setter setter) { uint16_setter_ = setter; }

 protected:
  void control(float value) override {
    if (parent_ == nullptr) return;

    if (float_setter_ != nullptr) {
      (parent_->*float_setter_)(value);
    } else if (uint8_setter_ != nullptr) {
      (parent_->*uint8_setter_)(static_cast<uint8_t>(value));
    } else if (uint16_setter_ != nullptr) {
      (parent_->*uint16_setter_)(static_cast<uint16_t>(value));
    }

    publish_state(value);
  }

 private:
  HeliosKwlComponent *parent_{nullptr};
  NumberFloatSetter   float_setter_{nullptr};
  NumberUint8Setter   uint8_setter_{nullptr};
  NumberUint16Setter  uint16_setter_{nullptr};
};

}  // namespace helios_kwl
}  // namespace esphome
