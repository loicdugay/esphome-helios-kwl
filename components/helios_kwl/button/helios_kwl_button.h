#pragma once
#include "esphome/components/button/button.h"
#include "../helios_kwl.h"

namespace esphome {
namespace helios_kwl {

class HeliosKwlButtonBase : public button::Button {
 public:
  void set_parent(HeliosKwlComponent *parent) { parent_ = parent; }
 protected:
  HeliosKwlComponent *parent_{nullptr};
};

class HeliosKwlBoostAirflowButton : public HeliosKwlButtonBase {
 protected:
  void press_action() override { if (parent_) parent_->trigger_boost_airflow(); }
};

class HeliosKwlBoostFireplaceButton : public HeliosKwlButtonBase {
 protected:
  void press_action() override { if (parent_) parent_->trigger_boost_fireplace(); }
};

class HeliosKwlStopBoostButton : public HeliosKwlButtonBase {
 protected:
  void press_action() override { if (parent_) parent_->stop_boost_cycle(); }
};

class HeliosKwlAckMaintenanceButton : public HeliosKwlButtonBase {
 protected:
  void press_action() override { if (parent_) parent_->acknowledge_maintenance(); }
};

}  // namespace helios_kwl
}  // namespace esphome
