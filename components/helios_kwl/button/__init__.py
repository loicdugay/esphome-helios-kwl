"""button — 4 boutons"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import ENTITY_CATEGORY_CONFIG
from .. import helios_kwl_ns, CONF_HELIOS_KWL_ID, HeliosKwlComponent
HeliosKwlBoostAirflowButton = helios_kwl_ns.class_("HeliosKwlBoostAirflowButton", button.Button)
HeliosKwlBoostFireplaceButton = helios_kwl_ns.class_("HeliosKwlBoostFireplaceButton", button.Button)
HeliosKwlStopBoostButton = helios_kwl_ns.class_("HeliosKwlStopBoostButton", button.Button)
HeliosKwlAckMaintenanceButton = helios_kwl_ns.class_("HeliosKwlAckMaintenanceButton", button.Button)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_HELIOS_KWL_ID): cv.use_id(HeliosKwlComponent),
    cv.Optional("boost_airflow"): button.button_schema(HeliosKwlBoostAirflowButton, entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:weather-windy"),
    cv.Optional("boost_fireplace"): button.button_schema(HeliosKwlBoostFireplaceButton, entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:fireplace"),
    cv.Optional("stop_boost"): button.button_schema(HeliosKwlStopBoostButton, entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:stop-circle-outline"),
    cv.Optional("acknowledge_maintenance"): button.button_schema(HeliosKwlAckMaintenanceButton, entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:air-filter"),
})
async def to_code(config):
    parent = await cg.get_variable(config[CONF_HELIOS_KWL_ID])
    for key in ["boost_airflow", "boost_fireplace", "stop_boost", "acknowledge_maintenance"]:
        if key in config:
            btn = await button.new_button(config[key]); cg.add(btn.set_parent(parent))
