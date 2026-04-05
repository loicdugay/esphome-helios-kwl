"""
Sous-plateforme fan — Helios KWL EC 300 Pro
Fan natif ESPHome : ON/OFF + 8 vitesses
Remplace l'ancienne sous-plateforme output/
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan
from esphome.const import CONF_ID, CONF_NAME

from .. import helios_kwl_ns, CONF_HELIOS_KWL_ID, HeliosKwlComponent

# ── Classe C++ — définie dans helios_kwl.h (même namespace) ───────────────────
HeliosKwlFan = helios_kwl_ns.class_("HeliosKwlFan", fan.Fan, cg.Component)

# ── Clé de configuration ──────────────────────────────────────────────────────
CONF_VENTILATION_FAN = "ventilation_fan"

# ── Schéma : fan.fan_schema() est la nouvelle API depuis ESPHome 2025.11 ──────
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HELIOS_KWL_ID): cv.use_id(HeliosKwlComponent),
        cv.Optional(CONF_VENTILATION_FAN): fan.fan_schema(HeliosKwlFan).extend(
            cv.COMPONENT_SCHEMA
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HELIOS_KWL_ID])

    if CONF_VENTILATION_FAN in config:
        fan_conf = config[CONF_VENTILATION_FAN]
        fan_var = cg.new_Pvariable(fan_conf[CONF_ID], parent)
        await cg.register_component(fan_var, fan_conf)
        await fan.register_fan(fan_var, fan_conf)
        cg.add(parent.set_fan(fan_var))