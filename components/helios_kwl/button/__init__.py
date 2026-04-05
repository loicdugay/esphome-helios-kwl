"""
Sous-plateforme button — Helios KWL EC 300 Pro
3 boutons d'action : cycle plein air (45 min), cycle cheminée (15 min), reset filtre
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG

from .. import helios_kwl_ns, CONF_HELIOS_KWL_ID, HeliosKwlComponent

# ── Classes C++ ───────────────────────────────────────────────────────────────
HeliosKwlBoostAirflowButton     = helios_kwl_ns.class_("HeliosKwlBoostAirflowButton",     button.Button)
HeliosKwlBoostFireplaceButton   = helios_kwl_ns.class_("HeliosKwlBoostFireplaceButton",   button.Button)
HeliosKwlAckMaintenanceButton   = helios_kwl_ns.class_("HeliosKwlAckMaintenanceButton",   button.Button)

# ── Clés de configuration ──────────────────────────────────────────────────────
CONF_BOOST_AIRFLOW        = "boost_airflow"
CONF_BOOST_FIREPLACE      = "boost_fireplace"
CONF_ACKNOWLEDGE_MAINTENANCE = "acknowledge_maintenance"

# ── Schéma de la sous-plateforme ──────────────────────────────────────────────
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HELIOS_KWL_ID): cv.use_id(HeliosKwlComponent),

        # ── Cycle Plein Air — 45 min à vitesse max ──
        cv.Optional(CONF_BOOST_AIRFLOW): button.button_schema(
            HeliosKwlBoostAirflowButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:weather-windy",
        ),

        # ── Cycle Cheminée — 15 min extraction coupée ──
        cv.Optional(CONF_BOOST_FIREPLACE): button.button_schema(
            HeliosKwlBoostFireplaceButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:fireplace",
        ),

        # ── Confirmer remplacement filtres (reset compteur) ──
        cv.Optional(CONF_ACKNOWLEDGE_MAINTENANCE): button.button_schema(
            HeliosKwlAckMaintenanceButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:air-filter",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HELIOS_KWL_ID])

    if CONF_BOOST_AIRFLOW in config:
        btn = await button.new_button(config[CONF_BOOST_AIRFLOW])
        cg.add(btn.set_parent(parent))

    if CONF_BOOST_FIREPLACE in config:
        btn = await button.new_button(config[CONF_BOOST_FIREPLACE])
        cg.add(btn.set_parent(parent))

    if CONF_ACKNOWLEDGE_MAINTENANCE in config:
        btn = await button.new_button(config[CONF_ACKNOWLEDGE_MAINTENANCE])
        cg.add(btn.set_parent(parent))
