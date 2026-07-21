# SlopVibe Watchface — Pebble Time 2 (Emery)

Watchface fonctionnelle pour Pebble Time 2 avec heure, date, météo, calendrier et tracking de pas.

## Layout (200×228 px)

```
┌──────────────────────────────┐
│██ Battery Bar (2px)        ██│ ← Vert >50%, Jaune >20%, Rouge <20%
│                              │
│                              │
│         12:34                │ ← Heure (FONT_KEY_LECO_42_NUMBERS)
│         (50% écran)          │
│                              │
│  ☀    21 - 07 - 26    23°   │ ← Date row (20%): icône | date | température
│                              │
│  Réunion équipe              │ ← Calendar event (20%)
│  14:00                       │
├──────────────────────────────┤
│██ Step Bar (2px)           ░░│ ← Bleu (en cours), Vert (objectif atteint)
└──────────────────────────────┘
```

## Fonctionnalités

- **Heure** — Format 24h/12h (selon préférence système), police LECO 42 Bold
- **Date** — Format JJ - MM - YY, police Gothic 24 Bold
- **Météo** — Open-Meteo API (gratuit, pas de key), refresh 60 min
  - Icônes dessinées par code (soleil, nuage, pluie, neige, orage, brouillard)
  - Température en Celsius selon géolocalisation
  - Dernière valeur conservée si pas de connexion
- **Calendrier** — Prochains évènements via Rebble Timeline API, refresh 60 min
  - Affiche le prochain évènement (titre + heure)
  - Garde 2 évènements en mémoire
  - Évènement passé → automatiquement remplacé par le suivant
- **Barre de batterie** — 2px en haut, couleur selon le niveau
- **Barre de pas** — 2px en bas, HealthService API

## Installation

### Prérequis
```bash
# SDK Pebble
uv tool install pebble-tool --python 3.13
pebble sdk install latest
```

### Build et installation
```bash
git clone https://github.com/SlopVibe-org/pebble-watchfaces.git
cd pebble-watchfaces
pebble build
pebble install --emulator emery   # Émulateur
pebble install                     # Montre connectée
```

## Architecture

### Fichiers
| Fichier | Description |
|---------|-------------|
| `src/main.c` | Code C natif — UI, layers, handlers |
| `src/pkjs.js` | PebbleKit JS — météo + calendrier |
| `package.json` | Configuration du projet |

### Données persistées (sur la montre)
| Key | Donnée |
|-----|--------|
| `PERSIST_WEATHER_TEMP` | Température (int) |
| `PERSIST_WEATHER_CODE` | Code météo WMO (int) |
| `PERSIST_CAL_1_TITLE` | Titre évènement 1 (string) |
| `PERSIST_CAL_1_TIME` | Timestamp évènement 1 (uint32) |
| `PERSIST_CAL_2_TITLE` | Titre évènement 2 (string) |
| `PERSIST_CAL_2_TIME` | Timestamp évènement 2 (uint32) |

### Efficacité énergétique
- `MINUTE_UNIT` tick (pas de `SECOND_UNIT`)
- Pas d'animations continues
- Météo/calendrier refresh 60 min seulement
- Toutes les couches sont détruites dans `unload`
- Données cachées via `persist_write_*()`

## APIs externes
- **Météo:** [Open-Meteo](https://open-meteo.com/) — gratuit, pas de key
- **Calendrier:** Rebble Timeline API

## Publication (Rebble Store)
1. `pebble build` → génère `.pbw`
2. Créer un compte sur [dev-portal.rebble.io](https://dev-portal.rebble.io/)
3. Upload du `.pbw` + screenshots

---

*SlopVibe — 2026-07-21*
