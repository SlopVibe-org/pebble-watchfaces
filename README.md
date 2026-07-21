# Pebble Time 2 Watchface Development

Documentation complète pour développer des watchfaces pour Pebble Time 2 (plateforme **Emery**).

## Table des matières

1. [Spécifications matérielles — Pebble Time 2 (Emery)](#spécifications)
2. [Installation du SDK](#sdk-install)
3. [Anatomie d'un projet watchface](#anatomie)
4. [Structure de code de base](#code-base)
5. [Efficacité énergétique — Best Practices](#énergie)
6. [Compatibilité multi-plateformes](#compatibilité)
7. [Publication sur le Rebble Store](#publication)
8. [Ressources et liens](#ressources)

---

<a id="spécifications"></a>
## 1. Spécifications — Pebble Time 2 (Emery)

| Caractéristique | Valeur |
|----------------|--------|
| **Plateforme** | Emery |
| **SoC** | SiFli SF32LB52J |
| **CPU** | Star-MC1 (Cortex-M33-like) @ 240 MHz |
| **Écran** | Rectangulaire, 1.5", 200×228 px, 202 PPI |
| **Couleurs** | 64 couleurs |
| **Taille max app** | 128 KB (code + heap) |
| **Taille max ressources** | 256 KB |
| **Touch screen** | Oui |
| **Microphone** | Oui |
| **Speaker** | Oui |
| **Capteurs** | 6-axis IMU, Compass |
| **HRM** | Oui |
| **Vibration** | LRA (linear resonant actuator) |
| **Batterie** | ~14 jours (PebbleOS v4.9.91), ~30 jours théorique |
| **Étanchéité** | 20m |
| **Boutons** | 4 |

### Couleurs (GColor)
- 64 couleurs disponibles (6-bit: 2-bit red, 2-bit green, 2-bit blue)
- Utiliser `GColorFromRGB()` pour des couleurs personnalisées
- Constantes prédéfinies: `GColorBlack`, `GColorWhite`, `GColorRed`, etc.

---

<a id="sdk-install"></a>
## 2. Installation du SDK

### Dépendances (Ubuntu/Debian)
```bash
sudo apt install python3-pip python3-venv nodejs npm libsdl1.2debian libfdt1
```

### Installer le Pebble CLI
```bash
# Installer uv (fast Python package manager)
curl -LsSf https://astral.sh/uv/install.sh | sh

# Installer pebble-tool
uv tool install pebble-tool --python 3.13
```

### Premier projet
```bash
# Installer le SDK
pebble sdk install latest

# Créer un projet watchface
pebble new-project --simple my_watchface

# Compiler
cd my_watchface
pebble build

# Tester sur l'émulateur Emery (Pebble Time 2)
pebble install --emulator emery
```

---

<a id="anatomie"></a>
## 3. Anatomie d'un projet

```
my_watchface/
├── package.json          # Configuration du projet
├── appinfo.json          # (legacy) métadonnées de l'app
├── src/
│   └── main.c            # Code source principal
├── resources/
│   ├── images/           # Images PNG
│   └── fonts/            # Polices custom (.ttf → .pfo)
└── README.md
```

### package.json — champs clés
```json
{
  "name": "my-watchface",
  "author": "SlopVibe",
  "version": "1.0.0",
  "uuid": "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx",
  "sdkVersion": "3",
  "watchapp": {
    "watchface": true
  },
  "targetPlatforms": ["emery"],
  "resources": {
    "media": []
  }
}
```

> **UUID:** Doit être unique. Générer avec `uuidgen` ou python `uuid.uuid4()`.

---

<a id="code-base"></a>
## 4. Structure de code de base

```c
#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_time_layer;

// --- Update time ---
static void update_time() {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    static char s_buffer[8];
    strftime(s_buffer, sizeof(s_buffer),
        clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
    text_layer_set_text(s_time_layer, s_buffer);
}

// --- Tick handler ---
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();
}

// --- Window load ---
static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    s_time_layer = text_layer_create(
        GRect(0, PBL_IF_ROUND_ELSE(58, 52), bounds.size.w, 50));

    text_layer_set_background_color(s_time_layer, GColorClear);
    text_layer_set_text_color(s_time_layer, GColorBlack);
    text_layer_set_font(s_time_layer,
        fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

    layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
    update_time();
}

// --- Window unload ---
static void main_window_unload(Window *window) {
    text_layer_destroy(s_time_layer);
}

// --- Init ---
static void init() {
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });
    window_stack_push(s_main_window, true);

    // MINUTE_UNIT pour économiser la batterie!
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

// --- Deinit ---
static void deinit() {
    window_destroy(s_main_window);
}

// --- Main ---
int main(void) {
    init();
    app_event_loop();
    deinit();
}
```

---

<a id="énergie"></a>
## 5. ⚡ Efficacité énergétique — CRITIQUE

La Pebble Time 2 a une excellente autonomie (~14-30 jours), **mais seulement si le code respecte les règles**:

### 5.1 Tick frequency — Le #1 consommateur

| Fréquence | Effet batterie | Quand utiliser |
|-----------|---------------|----------------|
| `HOUR_UNIT` | Minimal | Watchfaces sans secondes/minutes |
| `MINUTE_UNIT` | Faible | **Recommandé par défaut** |
| `SECOND_UNIT` | ÉLEVÉ | Seulement si aiguille des secondes |

```c
// ✅ BON - tick par minute
tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

// ⚠️ À éviter sauf nécessité absolue
tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
```

### 5.2 Animations
- Éviter les animations continues (chaque seconde)
- Animer seulement au changement de minute, ou sur interaction utilisateur
- Utiliser `accel_tap_handler` pour animer sur mouvement de poignet

```c
// Animer seulement quand l'utilisateur bouge le poignet
static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
    play_animation();
}
accel_tap_service_subscribe(tap_handler);
```

### 5.3 Capteurs
- **Accéléromètre:** Batcher les samples
```c
const uint32_t num_samples = 10;
accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
accel_data_service_subscribe(num_samples, accel_data_handler);
// → Réveille l'app 1x/seconde au lieu de 10x
```

- **Compas:** Filtrer les changements insignifiants
```c
compass_service_set_heading_filter(TRIG_MAX_ANGLE / 36); // 10° minimum
```

### 5.4 Bluetooth / AppMessage
- Garder `SNIFF_INTERVAL_NORMAL` par défaut
- Retourner en mode low-power après chaque transfert
```c
app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
```
- Cacher les données avec `persist_write_*()` pour réduire les requêtes

### 5.5 Backlight
- Ne pas utiliser `light_enable(true)` longtemps
- Laisser le système gérer automatiquement
```c
// Retour au contrôle automatique ASAP
light_enable(false);
```

### 5.6 Vibration
- Minimiser l'usage du Vibes API
- Offrir l'option de désactiver les vibrations
- Raccourcir les séquences custom

### 5.7 Règles d'or
1. **Laisser la montre dormir** — chaque réveil = batterie
2. **MINUTE_UNIT par défaut** — passer à SECOND_UNIT seulement si nécessaire
3. **Batcher les données capteurs** — réduire la fréquence des callbacks
4. **Cacher les données network** — `persist_write_*()` au lieu de re-fetch
5. **Offrir des options** — laisser l'utilisateur désactiver animations/secondes
6. **Détruire tous les objets** — chaque `_create()` doit avoir son `_destroy()`

---

<a id="compatibilité"></a>
## 6. Compatibilité multi-plateformes

### Defines de compilation

| Define | Description |
|--------|-------------|
| `PBL_PLATFORM_EMERY` | Pebble Time 2 |
| `PBL_PLATFORM_BASALT` | Pebble Time / Time Steel |
| `PBL_PLATFORM_CHALK` | Pebble Time Round |
| `PBL_PLATFORM_DIORITE` | Pebble 2 |
| `PBL_COLOR` | Plateforme couleur (64 couleurs) |
| `PBL_BW` | Plateforme noir et blanc |
| `PBL_RECT` | Écran rectangulaire |
| `PBL_ROUND` | Écran rond |
| `PBL_DISPLAY_WIDTH` | Largeur en pixels |
| `PBL_DISPLAY_HEIGHT` | Hauteur en pixels |
| `PBL_HEALTH` | Support Health API |
| `PBL_MICROPHONE` | Micro disponible |

### Macros utiles
```c
// Valeur conditionnelle selon couleur
window_set_background_color(s_window, PBL_IF_COLOR_ELSE(GColorJaegerGreen, GColorBlack));

// Valeur selon forme d'écran
int y = PBL_IF_ROUND_ELSE(58, 52);

// Taille d'écran
#if PBL_DISPLAY_HEIGHT == 228
    // Code spécifique Emery (200×228)
#endif
```

### Layout dynamique — JAMAIS de valeurs hardcodées
```c
// ❌ MAUVAIS
s_layer = layer_create(GRect(0, 0, 144, 168));

// ✅ BON
Layer *window_layer = window_get_root_layer(window);
GRect bounds = layer_get_bounds(window_layer);
s_layer = layer_create(bounds);
```

### Ressources spécifiques par plateforme
- Suffixe `~color` pour les ressources couleur (Basalt, Chalk, Emery)
- Suffixe `~bw` pour noir et blanc (Aplite, Diorite)
- Propriété `targetPlatforms` dans package.json

---

<a id="publication"></a>
## 7. Publication sur le Rebble Store

### Prérequis
1. App terminée et stable
2. UUID unique et valide
3. Compilée avec un SDK non-beta
4. Fichier `.pbw` généré (`pebble build`)

### Étapes

1. **Créer un compte développeur** sur [dev-portal.rebble.io](https://dev-portal.rebble.io/)

2. **Click "Add a Watchface"**
   - Entrer le titre
   - URL du code source (GitHub)
   - Email de support

3. **Upload du .pbw**
   - Click "Add a release"
   - Uploader le fichier `.pbw`
   - Ajouter des release notes (optionnel)
   - Click "Save" puis "Publish"

4. **Asset Collections**
   - Créer une collection par plateforme supportée
   - Description (max 1600 caractères)
   - Jusqu'à 5 screenshots (PNG, GIF, ou GIF animé)
   - Marketing banner (optionnel)

5. **Publier**
   - "Publish" (public) ou "Publish Privately" (lien direct seulement)
   - ⚠️ Une fois public, ne peut pas redevenir privé

### Assets requis
| Asset | Watchface | Watchapp |
|-------|-----------|----------|
| Titre | ✅ | ✅ |
| .pbw | ✅ | ✅ |
| Asset collections | ✅ | ✅ |
| Catégorie | — | ✅ |
| Icônes (large/small) | — | ✅ |

### Promotion
- [Discord Rebble](https://discord.com/invite/aRUAYFN)
- [Reddit r/pebble](https://www.reddit.com/r/pebble)
- [Bluesky @rebble.io](https://bsky.app/profile/rebble.io)
- [Mastodon @rebble](https://mastodon.social/@rebble)

---

<a id="ressources"></a>
## 8. Ressources et liens

### Documentation officielle
- **SDK Install:** https://developer.rebble.io/sdk/
- **Guides:** https://developer.rebble.io/guides/
- **Tutorials C:** https://developer.rebble.io/tutorials/watchface-tutorial/part1
- **Best Practices:** https://developer.rebble.io/guides/best-practices/
- **Battery Conservation:** https://developer.rebble.io/guides/best-practices/conserving-battery-life/
- **Publishing:** https://developer.rebble.io/guides/appstore-publishing/
- **Hardware Info:** https://developer.rebble.io/guides/tools-and-resources/hardware-information/

### GitHub
- **Repo SlopVibe:** https://github.com/SlopVibe-org/pebble-watchfaces
- **Rebble org:** https://github.com/pebble-dev
- **Wiki communautaire:** https://github.com/pebble-dev/wiki/wiki

### Communauté
- **Discord:** https://discord.com/invite/aRUAYFN
- **Dev Portal:** https://dev-portal.rebble.io/
- **Appstore:** https://apps.rebble.io/

### Outils
- **Pebble CLI:** `uv tool install pebble-tool --python 3.13`
- **Émulateurs:** `pebble install --emulator emery` (ou basalt, chalk, diorite)
- **Clay (config UI):** https://github.com/pebble-dev/clay

---

*Documentation compilée par Ry pour SlopVibe — 2026-07-20*
