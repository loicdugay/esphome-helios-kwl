"""
Sous-plateforme button — Helios KWL EC 300 Pro
4 boutons : plein air, cheminee, arret cycle, reset filtre
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import helios_kwl_ns, CONF_HELIOS_KWL_ID, HeliosKwlComponent

HeliosKwlBoostAirflowButton   = helios_kwl_ns.class_("HeliosKwlBoostAirflowButton",   button.Button)
HeliosKwlBoostFireplaceButton = helios_kwl_ns.class_("HeliosKwlBoostFireplaceButton", button.Button)
HeliosKwlStopBoostButton      = helios_kwl_ns.class_("HeliosKwlStopBoostButton",      button.Button)
HeliosKwlAckMaintenanceButton = helios_kwl_ns.class_("HeliosKwlAckMaintenanceButton", button.Button)

CONF_BOOST_AIRFLOW           = "boost_airflow"
CONF_BOOST_FIREPLACE         = "boost_fireplace"
CONF_STOP_BOOST              = "stop_boost"
CONF_ACKNOWLEDGE_MAINTENANCE = "acknowledge_maintenance"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HELIOS_KWL_ID): cv.use_id(HeliosKwlComponent),
        cv.Optional(CONF_BOOST_AIRFLOW): button.button_schema(
            HeliosKwlBoostAirflowButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:weather-windy",
        ),
        cv.Optional(CONF_BOOST_FIREPLACE): button.button_schema(
            HeliosKwlBoostFireplaceButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:fireplace",
        ),
        cv.Optional(CONF_STOP_BOOST): button.button_schema(
            HeliosKwlStopBoostButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:stop-circle-outline",
        ),
        cv.Optional(CONF_ACKNOWLEDGE_MAINTENANCE): button.button_schema(
            HeliosKwlAckMaintenanceButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:air-filter",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HELIOS_KWL_ID])

    for conf_key in [CONF_BOOST_AIRFLOW, CONF_BOOST_FIREPLACE, CONF_STOP_BOOST, CONF_ACKNOWLEDGE_MAINTENANCE]:
        if conf_key in config:
            btn = await button.new_button(config[conf_key])
            cg.add(btn.set_parent(parent))
