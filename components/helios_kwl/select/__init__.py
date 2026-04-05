"""
Sous-plateforme select — Helios KWL EC 300 Pro
3 sélecteurs : mode boost/cheminée, détection humidité auto, vitesse max forcée
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import helios_kwl_ns, CONF_HELIOS_KWL_ID, HeliosKwlComponent

# ── Classes C++ ───────────────────────────────────────────────────────────────
HeliosKwlBoostFireplaceSelect  = helios_kwl_ns.class_("HeliosKwlBoostFireplaceSelect",  select.Select)
HeliosKwlHumidityAutoSelect    = helios_kwl_ns.class_("HeliosKwlHumidityAutoSelect",    select.Select)
HeliosKwlMaxSpeedContSelect    = helios_kwl_ns.class_("HeliosKwlMaxSpeedContSelect",    select.Select)

# ── Clés de configuration ──────────────────────────────────────────────────────
CONF_BOOST_FIREPLACE_MODE  = "boost_fireplace_mode"
CONF_HUMIDITY_AUTO_SEARCH  = "humidity_auto_search"
CONF_MAX_SPEED_CONTINUOUS  = "max_speed_continuous"

# ── Options des sélecteurs ────────────────────────────────────────────────────
OPTIONS_BOOST_FIREPLACE = ["Cycle Cheminée", "Cycle Plein Air"]
OPTIONS_HUMIDITY_AUTO   = ["Seuil manuel", "Apprentissage auto"]
OPTIONS_MAX_SPEED       = ["Normal", "Ventilation maximale forcée"]

# ── Schéma de la sous-plateforme ──────────────────────────────────────────────
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HELIOS_KWL_ID): cv.use_id(HeliosKwlComponent),

        # ── Contact sec : Cheminée (0) ou Plein Air (1) ──
        cv.Optional(CONF_BOOST_FIREPLACE_MODE): select.select_schema(
            HeliosKwlBoostFireplaceSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:fireplace",
        ),

        # ── Recherche automatique du seuil hygrométrique ──
        cv.Optional(CONF_HUMIDITY_AUTO_SEARCH): select.select_schema(
            HeliosKwlHumidityAutoSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:water-percent-alert",
        ),

        # ── Vitesse max en continu (ignore les régulations) ──
        cv.Optional(CONF_MAX_SPEED_CONTINUOUS): select.select_schema(
            HeliosKwlMaxSpeedContSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:fan-speed-3",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HELIOS_KWL_ID])

    if CONF_BOOST_FIREPLACE_MODE in config:
        sel = await select.new_select(
            config[CONF_BOOST_FIREPLACE_MODE],
            options=OPTIONS_BOOST_FIREPLACE,
        )
        cg.add(sel.set_parent(parent))
        cg.add(parent.set_boost_fireplace_mode_select(sel))

    if CONF_HUMIDITY_AUTO_SEARCH in config:
        sel = await select.new_select(
            config[CONF_HUMIDITY_AUTO_SEARCH],
            options=OPTIONS_HUMIDITY_AUTO,
        )
        cg.add(sel.set_parent(parent))
        cg.add(parent.set_humidity_auto_search_select(sel))

    if CONF_MAX_SPEED_CONTINUOUS in config:
        sel = await select.new_select(
            config[CONF_MAX_SPEED_CONTINUOUS],
            options=OPTIONS_MAX_SPEED,
        )
        cg.add(sel.set_parent(parent))
        cg.add(parent.set_max_speed_continuous_select(sel))
