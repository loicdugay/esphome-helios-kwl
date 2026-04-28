# Référentiel Entités v7 — Helios KWL EC 300 Pro

> Mise à jour : reflète l'architecture finale avec 4 primitives (`read_register`, `write_register`, `write_bit`, `write_bits_masked`), dispatch strict des trames, et séquences de cycles corrigées.

## 🎛️ PILOTAGE VENTILATION

### Commande principale de la VMC

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Pilotage | ventilation_fan | Ventilation | fan (on/off + speed) | — | 0xA3 bit 0 + 0x29 | `write_bit` (power) + `write_register` (speed) | Oui — Arc + chiffre central | ON/OFF via 0xA3 bit 0. Vitesse 1-8 via 0x29. Garde au boot : ignore les commandes tant que le premier poll de 0xA3 n'est pas arrivé (~6s). Synchronisé en permanence avec l'état réel via polls S2. | État: Actif / Veille — Vitesse: 3/8 | Oui |
| Pilotage | fan_speed | Vitesse de ventilation | sensor | diagnostic | 0x29 | lecture S2 | Non | Sensor dédié à la vitesse (1-8). Mis à jour toutes les 6s. Ne force pas le fan ON quand la VMC est éteinte. | Valeur: 2 | Oui |

### Régulations automatiques

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Régulation | co2_regulation | Gestion intelligente (CO₂) | switch | — | 0xA3 bit 1 | `write_bit` | Non | Active la régulation auto CO₂. Vitesse varie entre base et max. | État: Dynamique / Standard | Oui |
| Régulation | humidity_regulation | Gestion intelligente (%HR) | switch | — | 0xA3 bit 2 | `write_bit` | Non | Active la régulation auto hygrométrique. | État: Dynamique / Standard | Oui |

## 🌡️ RÉGULATION THERMIQUE

### Mode saisonnier + bypass physique

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Thermique | summer_mode | Mode Fraîcheur | switch | — | 0xA3 bit 3 | `write_bit` | Oui — Toggle volet 2 | ON=été (bypass autorisé). OFF=hiver (récupération chaleur). | État: Fraîcheur activée / Chaleur conservée | Oui |
| Thermique | bypass_open | Gestion thermique (num) | sensor | diagnostic | 0x08 bit 1 | lecture S2 | Oui — Icône barre état | État physique volet bypass. 0=fermé (hiver), 1=ouvert (été). | Valeur: 0 ou 1 | Oui |
| Thermique | bypass_state | Gestion thermique | text_sensor | diagnostic | 0xA3 bit 3 | lecture S2 | Non | Traduction texte du mode saisonnier. | Texte: Air frais / Chaleur conservée | Non |
| Thermique | bypass_temp | Seuil de fraîcheur | number | config | 0xAF | `write_register` | Non | T° au-dessus de laquelle le bypass peut s'ouvrir. Défaut: 10°C. Plage: 0-25°C. | Valeur: 10 °C | Non |

## 🔥 CYCLES TEMPORAIRES

### Surventilation et compensation cheminée

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Cycles | boost_airflow | Cycle Plein Air | button | config | 0xAA+0x79+0x71 | `write_bit(0xAA,5,true)` → `write_register(0x79,45)` → `write_bit(0x71,5,true)` | Oui — Bouton volet 2 | Ventilateurs max 45 min. Le timer 0x79 est forcé à 45 car la VMC ne le reset pas toujours. | (bouton) | Non |
| Cycles | boost_fireplace | Cycle Cheminée | button | config | 0xAA+0x79+0x71 | `write_bit(0xAA,5,false)` → `write_register(0x79,15)` → `write_bit(0x71,5,true)` | Oui — Bouton volet 2 | Extraction coupée 15 min. Le timer 0x79 est forcé à 15 car la VMC ne le reset pas pour le mode Cheminée. | (bouton) | Non |
| Cycles | stop_boost | Arrêter le cycle | button | config | 0x79 | `write_register(0x79, 1)` | Oui — Bouton volet 2 | Écrit timer=1 min. La VMC termine naturellement le cycle en ~1 min et redémarre l'extracteur. Ne PAS écrire 0x00 (la VMC redéclenche un cycle). Ne PAS écrire directement 0x71=0x00 (l'extracteur ne redémarre pas). | (bouton) | Non |
| Cycles | boost_active | Cycle en cours | text_sensor | diagnostic | 0x71 bit 6 + 0xAA bit 5 | lecture S2 | Oui — Icône barre état | "Normal" / "Cycle Plein Air" / "Cycle Cheminée". | Texte | Oui |
| Cycles | boost_state | Cycle en cours (num) | sensor | diagnostic | 0x71 bit 6 + 0xAA bit 5 | lecture S2 | Non | 0=normal, 1=plein air, 2=cheminée. | Valeur: 0/1/2 | Oui |
| Cycles | boost_remaining | Fin du cycle dans… | sensor | diagnostic | 0x79 | lecture S2 | Oui — timer dans bouton | Minutes restantes. Pollé uniquement si cycle actif. | Valeur: 42 min | Non |

## 🌡️ CAPTEURS TEMPÉRATURE

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Température | temperature_outside | Température air extérieur | sensor | — | 0x32 | S1 broadcast passif | Oui — Grille volet 1 | Sonde NTC. Filtre EMA alpha=0.3, send_every=3. | Valeur: -3 °C | Oui |
| Température | temperature_extract | Température air intérieur | sensor | — | 0x34 | S1 broadcast passif | Oui — Grille volet 1 | Sonde NTC. Filtre EMA. | Valeur: 21 °C | Oui |
| Température | temperature_supply | Température air soufflé | sensor | — | 0x35 | S1 broadcast passif | Oui — Grille volet 1 | Sonde NTC. Filtre EMA. | Valeur: 18 °C | Oui |
| Température | temperature_exhaust | Température air rejeté | sensor | — | 0x33 | S1 broadcast passif | Oui — Grille volet 1 | Sonde NTC. Filtre EMA. | Valeur: 4 °C | Oui |

## 💧 QUALITÉ DE L'AIR

### Sondes humidité

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Qualité air | humidity_sensor1 | Humidité ${room_humidity1} | sensor | — | 0x2F | lecture S2 | Oui — Grille volet 1 | Nom pièce via substitutions. Formule: (val-51)/2.04. Filtre EMA. | Valeur: 58 % | Oui |
| Qualité air | humidity_sensor2 | Humidité ${room_humidity2} | sensor | — | 0x30 | lecture S2 | Oui — Grille volet 1 | Nom pièce via substitutions. Formule: (val-51)/2.04. Filtre EMA. | Valeur: 45 % | Oui |

### Sonde CO₂

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Qualité air | co2_concentration | Niveau CO₂ | sensor | — | 0x2B+0x2C | S1 broadcast passif | Non | 16 bits (high+low). Publié quand les 2 octets sont disponibles dans last_value_[]. | Valeur: 820 ppm | Oui |

## 🛡️ PROTECTION ANTIGEL

### Chaîne de protection : auto-dégivrage → coupure ventilateur → alerte givre

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Antigel | preheating_active | Auto-dégivrage | binary_sensor | diagnostic | 0x08 bit 4 | lecture S2 | Oui — Icône barre état | Résistance 1kW avant échangeur. Normal en hiver. | État: Dégivrage en cours / En veille | Oui |
| Antigel | freeze_alarm | Alerte Givre | binary_sensor | diagnostic | 0x6D bit 7 | lecture S2 | Oui — Icône barre (rouge) | Préchauffage insuffisant. Risque gel échangeur. | État: Alerte / Optimal | Oui |
| Antigel | preheating_temp | Seuil de dégivrage | number | config | 0xA7 | `write_register` | Non | Seuil activation auto-dégivrage. Plage: -6 à +15°C. Usine: -3°C. | Valeur: -3 °C | Non |
| Antigel | frost_alarm_temp | Seuil Alerte Givre | number | config | 0xA8 | `write_register` | Non | Seuil coupure ventilateur soufflage. Plage: -6 à +15°C. Usine: +3°C. | Valeur: 3 °C | Non |
| Antigel | frost_hysteresis | Hystérésis antigel | number | config | 0xB2 | `write_register` | Non | Écart entre arrêt et redémarrage ventilateur. Plage: 1-10°C. | Valeur: 3 °C | Non |

## 📊 ÉTAT PHYSIQUE VMC (registre 0x08)

### Lecture complète du port I/O — états réels des actionneurs

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| État physique | supply_fan_running | Ventilateur soufflage | binary_sensor | diagnostic | 0x08 bit 3 | lecture S2 | Non | Logique inversée : 0=marche, 1=arrêt. Coupé pendant protection antigel. | État: En marche / À l'arrêt | Oui |
| État physique | exhaust_fan_running | Ventilateur extraction | binary_sensor | diagnostic | 0x08 bit 5 | lecture S2 | Non | Logique inversée : 0=marche, 1=arrêt. Coupé pendant Cycle Cheminée. La VMC met à jour ce bit ~30s après la fin du cycle. | État: En marche / À l'arrêt | Oui |
| État physique | external_contact | Contact externe | binary_sensor | diagnostic | 0x08 bit 6 | lecture S2 | Non | Bouton poussoir externe (bornes S). 0=ouvert, 1=fermé. | État: Déclenché / En attente | Non |
| État physique | fault_relay | Relais défaut | binary_sensor | diagnostic | 0x08 bit 2 | lecture S2 | Non | Relais signalisation. Logique inversée : 0=ouvert (défaut), 1=fermé (normal). | État: Normal / Défaut signalé | Non |

## ⚠️ ALERTES ET DIAGNOSTIC

### Santé globale du système

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Santé | fault_indicator | Santé du Système | sensor | diagnostic | 0xA3+0x36+0x6D | agrégé (last_value_[]) | Oui — Icône barre | 0=Optimal, 1=filtre, 2=défaut. Dédupliqué (ne publie que si changement). | Texte: Optimal / Maintenance filtres / Alerte | Oui |
| Santé | fault_code | Code de diagnostic | sensor | diagnostic | 0x36 | lecture S3 | Non | Code brut du dernier défaut. | Valeur: 0x00 | Oui |
| Santé | fault_description | Détail du diagnostic | text_sensor | diagnostic | 0x36 | lecture S3 | Non | Traduction code→texte FR : Aucun défaut / Sonde air soufflé défectueuse / Alerte CO₂ élevé / Sonde extérieure défectueuse / Sonde air intérieur défectueuse / Risque gel circuit eau / Sonde air rejeté défectueuse. | Texte | Non |
| Santé | co2_alarm | Alerte CO₂ | binary_sensor | diagnostic | 0x6D bit 6 | lecture S2 | Oui — Icône barre (rouge) | >5000 PPM. Danger. | État: Alerte / Aucun défaut | Oui |

## 🔧 ENTRETIEN FILTRE

### Trio préventif : état → décompte → reset

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Entretien | filter_maintenance | État des filtres | binary_sensor | diagnostic | 0xA3 bit 7 | lecture S2 | Oui — Icône barre (orange) | Remplacement nécessaire quand ON. | État: Optimal / Remplacement requis | Oui |
| Entretien | service_months_remaining | Prochain remplacement | sensor | diagnostic | 0xAB | lecture S3 | Oui — Label volet 2 | Décompte mois restants. | Valeur: 3 mois | Non |
| Entretien | acknowledge_maintenance | Confirmer remplacement filtres | button | config | 0xA6+0xAB | `read_register(0xA6)` → `write_register(0xAB, val)` | Oui — Bouton volet 2 | Lit l'intervalle d'entretien dans 0xA6 puis écrit cette valeur dans le compteur 0xAB. | (bouton) | Non |
| Entretien | service_interval | Intervalle entretien | number | config | 0xA6 | `write_register` | Non | Fréquence du rappel. Plage: 1-15 mois. Défaut: 4 mois. | Valeur: 4 mois | Non |

## 🔩 POSTCHAUFFAGE (compatibilité)

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Compat. | heating_indicator | Appoint de chaleur | binary_sensor | diagnostic | 0xA3 bit 5 | lecture S2 | Oui* — si installé | Non installé sur KWL EC 300 Pro. Prévu pour modèles avec batterie post-échangeur. | État: Actif / Non installé | Non |

## ⚙️ RÉGLAGES AVANCÉS (HA uniquement)

### Vitesses ventilateur

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Réglages | basic_fan_speed | Ventilation nominale | number | config | 0xA9 | `write_register` | Non | Vitesse de base (plancher). Plage: 1-8. Défaut: 1. Codage bitmask. | Valeur: 1 | Non |
| Réglages | max_fan_speed | Ventilation maximale | number | config | 0xA5 | `write_register` | Non | Plafond vitesse pendant régulations. Plage: 1-8. Défaut: 8. Codage bitmask. | Valeur: 8 | Non |

### Seuils de régulation

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Réglages | co2_setpoint | Seuil CO₂ | number | config | 0xB3+0xB4 | `write_register` ×2 | Non | Seuil régulation CO₂. Plage: 500-2000 ppm. 16 bits (high+low). Défaut: 900 ppm. | Valeur: 900 ppm | Non |
| Réglages | humidity_setpoint | Seuil Humidité | number | config | 0xAE | `write_register` | Non | Seuil régulation hygrométrique. Plage: 1-99%. Formule: val×2.04+51. | Valeur: 40 % | Non |
| Réglages | regulation_interval | Fréquence d'analyse des sondes | number | config | 0xAA bits 0-3 | `write_bits_masked(0xAA, 0x0F, m)` | Non | Périodicité mesure sondes. Plage: 1-15 min. Défaut: 10 min. Masque 4 bits de poids faible sans toucher aux bits 4-7. | Valeur: 10 min | Non |

### Puissance ventilateurs DC

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Réglages | supply_fan_percent | Ajustement Soufflage | number | config | 0xB0 | `write_register` | Non | Puissance DC ventilateur soufflage. Plage: 65-100%. Défaut: 100%. | Valeur: 100 % | Non |
| Réglages | exhaust_fan_percent | Ajustement Extraction | number | config | 0xB1 | `write_register` | Non | Puissance DC ventilateur extraction. Plage: 65-100%. Défaut: 100%. | Valeur: 100 % | Non |

### Modes avancés

| Groupe | Variable (code EN) | Nom HA (FR) | Type HA | entity_category | Registre | Primitive | Écran LVGL ? | Remarques / Logique | Affichage dans HA | Historique |
|---|---|---|---|---|---|---|---|---|---|---|
| Réglages | boost_fireplace_mode | Cycle commande murale | select | config | 0xAA bit 5 | `write_bit` | Non | Contact sec : Cheminée (0) / Plein Air (1). | Choix: Cycle Cheminée / Cycle Plein Air | Non |
| Réglages | humidity_auto_search | Détection humidité | select | config | 0xAA bit 4 | `write_bit` | Non | Recherche automatique du seuil hygrométrique. | Choix: Apprentissage auto / Seuil manuel | Non |
| Réglages | max_speed_continuous | Ventilation maximale forcée | select | config | 0xB5 bit 0 | `write_bit` | Non | Vitesse max en continu (ignore régulations). | Choix: Normal / Ventilation maximale forcée | Non |

---

## 📋 RÉSUMÉ COMPTAGE

| Type HA | Nombre |
|---|---|
| fan | 1 |
| sensor | 14 |
| text_sensor | 3 |
| binary_sensor | 9 |
| switch | 3 |
| number | 12 |
| select | 3 |
| button | 4 |
| **Total** | **49** |

## 📡 STRATÉGIES DE POLLING

| Stratégie | Fréquence | Source | Registres |
|---|---|---|---|
| S1 — Passive | ~12s (broadcasts CM) | `loop_read_bus()` → `dispatch_packet()` | 0x2B, 0x2C, 0x32, 0x33, 0x34, 0x35 |
| S2 — Cyclique | 6s | `update()` → `do_one_s2_poll()` | 0x29, 0xA3, 0x08, 0x71, 0x79, 0x6D, 0x2F, 0x30 |
| S3 — Init + 1h | Boot puis 1h | `update()` → `do_one_s3_poll()` | 0x2D, 0x36, 0x55, 0x56, 0x6F, 0x70, 0xAB, 0xAA, 0xA9, 0xA5, 0xAF, 0xA7, 0xA8, 0xB2, 0xB0, 0xB1, 0xB3, 0xB4, 0xAE, 0xA6, 0xB5 |

Alternance S2/S3 : ratio 5:1 (5 cycles S2 puis 1 cycle S3). Tous les S3 sont forcés "dus" au boot via underflow unsigned pour être pollés dans les ~2 premières minutes.
