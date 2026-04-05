"""
Sous-plateforme text_sensor — Helios KWL EC 300 Pro
3 capteurs textuels : description du défaut, état boost, état bypass
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import helios_kwl_ns, CONF_HELIOS_KWL_ID, HeliosKwlComponent

# ── Clés de configuration ──────────────────────────────────────────────────────
CONF_FAULT_DESCRIPTION  = "fault_description"
CONF_BOOST_ACTIVE       = "boost_active"
CONF_BYPASS_STATE       = "bypass_state"

# ── Schéma de la sous-plateforme ──────────────────────────────────────────────
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HELIOS_KWL_ID): cv.use_id(HeliosKwlComponent),

        # ── Traduction du code défaut en texte FR ──
        cv.Optional(CONF_FAULT_DESCRIPTION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:alert-circle-outline",
        ),

        # ── État du cycle actif : Normal / Cycle Plein Air / Cycle Cheminée ──
        cv.Optional(CONF_BOOST_ACTIVE): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:weather-windy",
        ),

        # ── État bypass : Air frais / Chaleur conservée ──
        cv.Optional(CONF_BYPASS_STATE): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:valve",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HELIOS_KWL_ID])

    if CONF_FAULT_DESCRIPTION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_FAULT_DESCRIPTION])
        cg.add(parent.set_fault_description_text(sens))

    if CONF_BOOST_ACTIVE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_BOOST_ACTIVE])
        cg.add(parent.set_boost_active_text(sens))

    if CONF_BYPASS_STATE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_BYPASS_STATE])
        cg.add(parent.set_bypass_state_text(sens))
