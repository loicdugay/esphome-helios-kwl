"""
Sous-plateforme switch — Helios KWL EC 300 Pro
3 interrupteurs : régulation CO₂, régulation humidité, mode fraîcheur (été)
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from .. import helios_kwl_ns, CONF_HELIOS_KWL_ID, HeliosKwlComponent

# ── Classes C++ des switches ──────────────────────────────────────────────────
HeliosKwlCo2RegSwitch      = helios_kwl_ns.class_("HeliosKwlCo2RegSwitch",      switch.Switch)
HeliosKwlHumidityRegSwitch = helios_kwl_ns.class_("HeliosKwlHumidityRegSwitch", switch.Switch)
HeliosKwlSummerModeSwitch  = helios_kwl_ns.class_("HeliosKwlSummerModeSwitch",  switch.Switch)

# ── Clés de configuration ──────────────────────────────────────────────────────
CONF_CO2_REGULATION      = "co2_regulation"
CONF_HUMIDITY_REGULATION = "humidity_regulation"
CONF_SUMMER_MODE         = "summer_mode"

# ── Schéma de la sous-plateforme ──────────────────────────────────────────────
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HELIOS_KWL_ID): cv.use_id(HeliosKwlComponent),

        # ── Régulation automatique CO₂ ──
        cv.Optional(CONF_CO2_REGULATION): switch.switch_schema(
            HeliosKwlCo2RegSwitch,
            icon="mdi:molecule-co2",
        ),

        # ── Régulation automatique humidité ──
        cv.Optional(CONF_HUMIDITY_REGULATION): switch.switch_schema(
            HeliosKwlHumidityRegSwitch,
            icon="mdi:water-percent",
        ),

        # ── Mode fraîcheur (été / bypass échangeur autorisé) ──
        cv.Optional(CONF_SUMMER_MODE): switch.switch_schema(
            HeliosKwlSummerModeSwitch,
            icon="mdi:sun-thermometer-outline",
        ),
    }
)


async def to_code(config):
    cg.add_global(cg.RawExpression('#include "helios_kwl_switch.h"'))
    parent = await cg.get_variable(config[CONF_HELIOS_KWL_ID])

    if CONF_CO2_REGULATION in config:
        sw = await switch.new_switch(config[CONF_CO2_REGULATION])
        cg.add(sw.set_parent(parent))
        cg.add(parent.set_co2_regulation_switch(sw))

    if CONF_HUMIDITY_REGULATION in config:
        sw = await switch.new_switch(config[CONF_HUMIDITY_REGULATION])
        cg.add(sw.set_parent(parent))
        cg.add(parent.set_humidity_regulation_switch(sw))

    if CONF_SUMMER_MODE in config:
        sw = await switch.new_switch(config[CONF_SUMMER_MODE])
        cg.add(sw.set_parent(parent))
        cg.add(parent.set_summer_mode_switch(sw))
