# Changelog

Toutes les modifications notables de ce fork sont documentées ici.

## [Unreleased] — Corrections et améliorations

### Corrections
- **Polling rotatif** : un seul registre par cycle `update()` au lieu de tous en séquence (résout le blocage de 400ms)
- **Boucle flush bornée** : `flush_read_buffer()` a maintenant un timeout de sécurité de 200ms
- **Humidité clampée** : les valeurs brutes < 0x33 retournent 0% au lieu d'un nombre négatif
- **Séquence d'écriture RS485** : envoi des 3 messages requis par le protocole Helios (broadcast 0x20, broadcast 0x10, direct 0x11 + checksum doublé)

### Ajouts
- Alarme CO2 élevé (registre 0x6D bit 6) — `binary_sensor`
- Risque gel échangeur (registre 0x6D bit 7) — `binary_sensor`

## [humiditysensors] — Fork loicdugay

### Ajouts
- Support des sondes d'humidité KWL-FF (registres 0x2F et 0x30)
- Sous-plateforme `sensor` pour `humidity1` et `humidity2`

### Modifications
- Adresse du composant changée de 0x21 à 0x2F (évite le conflit avec la télécommande physique)
- Polling séquentiel de tous les registres dans `update()` (avec `delay(20)` entre chaque)
- Mise à jour de l'état du switch `winter_mode` depuis la VMC

## [Original] — Cyril Jaquier (lostcontrol)

### Fonctionnalités initiales
- Lecture des 4 sondes de température via table NTC
- Lecture de la vitesse ventilateur (registre 0x29, codage masque bits)
- Lecture des états : alimentation, bypass, chauffage, défaut, maintenance (registre 0xA3)
- Écriture de la vitesse ventilateur
- Switch mode hiver (registre 0xA3 bit 3)
- Output float pour intégration avec le composant `fan:` ESPHome
