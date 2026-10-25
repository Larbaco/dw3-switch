#!/usr/bin/env python3
"""Generate compiled antialiased lettering for the native PC title screen.

Requires Pillow and DejaVu Sans (Debian/Ubuntu fonts-dejavu-core).
No font or Python dependency is needed by the game at runtime.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
FONT_ROOT = Path('/usr/share/fonts/truetype/dejavu')
LETTERS = [
    ('start', 'PRESS START', 26, 'DejaVuSansCondensed-Bold.ttf'),
    # The title credits, transcribed from the original sprites (logo sprites
    # 5/6, identical on the US disc and in all seven EU banks): three lines,
    # nothing else. Keep the characters; only the spacing is typographic.
    ('credit1', '© Akiyoshi Hongo · Toei Animation', 12, 'DejaVuSansCondensed.ttf'),
    ('credit2', 'TM & © 1997–2002 Bandai.', 12, 'DejaVuSansCondensed.ttf'),
    ('credit3', '© 2002 Bandai Co.,Ltd', 12, 'DejaVuSansCondensed.ttf'),
    # The port-only Options row on the EU disc, which has no original sprite
    # for it. English and French both read "Options" (label_1).
    ('options_de', 'Optionen', 29, 'DejaVuSansCondensed-Bold.ttf'),
    ('options_es', 'Opciones', 29, 'DejaVuSansCondensed-Bold.ttf'),
    ('options_it', 'Opzioni', 29, 'DejaVuSansCondensed-Bold.ttf'),
    # The Port options screen, styled after the game's own windows: plain
    # white text, the window title in capitals as the memory card screen's.
    ('opt_title_en', 'OPTIONS', 30, 'DejaVuSans.ttf'),
    ('opt_title_de', 'OPTIONEN', 30, 'DejaVuSans.ttf'),
    ('opt_title_es', 'OPCIONES', 30, 'DejaVuSans.ttf'),
    ('opt_title_it', 'OPZIONI', 30, 'DejaVuSans.ttf'),
    ('opt_row0', 'Widescreen', 26, 'DejaVuSans.ttf'),
    ('opt_row1', 'Fast-forward', 26, 'DejaVuSans.ttf'),
    ('opt_row2', 'Restart hotkey', 26, 'DejaVuSans.ttf'),
    # P6.17 volume rows: label, then the step numbers 1..10 (0 is "OFF").
    ('opt_row3', 'Music volume', 26, 'DejaVuSans.ttf'),
    ('opt_row4', 'SFX volume', 26, 'DejaVuSans.ttf'),
    # P6.18 rows: Encounter rate (OFF / 0.5x / 1x / 1.5x / 2x) and Text speed.
    ('opt_row5', 'Encounter rate', 26, 'DejaVuSans.ttf'),
    ('opt_row6', 'Text speed', 26, 'DejaVuSans.ttf'),
    # P7.9 row: Battle cursor (OFF / Command / Command+Tech).
    ('opt_row7', 'Battle cursor', 26, 'DejaVuSans.ttf'),
    ('opt_1x', '1x', 26, 'DejaVuSans.ttf'),
    ('opt_05x', '0.5x', 26, 'DejaVuSans.ttf'),
    ('opt_15x', '1.5x', 26, 'DejaVuSans.ttf'),
    ('opt_normal', 'Normal', 26, 'DejaVuSans.ttf'),
    ('opt_fast', 'Fast', 26, 'DejaVuSans.ttf'),
    ('opt_instant', 'Instant', 26, 'DejaVuSans.ttf'),
    ('opt_command', 'Command', 26, 'DejaVuSans.ttf'),
    ('opt_command_tech', 'Command+Tech', 26, 'DejaVuSans.ttf'),
    ('opt_on', 'ON', 26, 'DejaVuSans.ttf'),
    ('opt_off', 'OFF', 26, 'DejaVuSans.ttf'),
    ('opt_2x', '2x', 26, 'DejaVuSans.ttf'),
    ('opt_3x', '3x', 26, 'DejaVuSans.ttf'),
    ('opt_4x', '4x', 26, 'DejaVuSans.ttf'),
    ('opt_n1', '1', 26, 'DejaVuSans.ttf'),
    ('opt_n2', '2', 26, 'DejaVuSans.ttf'),
    ('opt_n3', '3', 26, 'DejaVuSans.ttf'),
    ('opt_n4', '4', 26, 'DejaVuSans.ttf'),
    ('opt_n5', '5', 26, 'DejaVuSans.ttf'),
    ('opt_n6', '6', 26, 'DejaVuSans.ttf'),
    ('opt_n7', '7', 26, 'DejaVuSans.ttf'),
    ('opt_n8', '8', 26, 'DejaVuSans.ttf'),
    ('opt_n9', '9', 26, 'DejaVuSans.ttf'),
    ('opt_n10', '10', 26, 'DejaVuSans.ttf'),
    ('opt_desc0', 'Uses the full 16:9 screen where the game allows it.', 25, 'DejaVuSans.ttf'),
    ('opt_desc1_pc', 'Press R3 in the field or in battle to switch speed.', 25, 'DejaVuSans.ttf'),
    ('opt_desc1_nx', 'Click the right stick in the field or in battle.', 25, 'DejaVuSans.ttf'),
    ('opt_desc2_pc', 'L1 + R1 + Start + Select restarts the game.', 25, 'DejaVuSans.ttf'),
    ('opt_desc2_nx', 'L + R + Plus + Minus restarts the game.', 25, 'DejaVuSans.ttf'),
    ('opt_desc3', 'Left / Right change the music volume (10 = original).', 25, 'DejaVuSans.ttf'),
    ('opt_desc4', 'Left / Right change the effects volume (10 = original).', 25, 'DejaVuSans.ttf'),
    ('opt_desc5', 'Random battles only: OFF to 2x. Story battles stay.', 25, 'DejaVuSans.ttf'),
    ('opt_desc6', 'How fast dialogue is typed out. Normal = original.', 25, 'DejaVuSans.ttf'),
    ('opt_desc7', 'Battle menu opens on your last pick. Off = original.', 25, 'DejaVuSans.ttf'),
    ('opt_change', 'Change', 24, 'DejaVuSans.ttf'),
    ('opt_back', 'Back', 24, 'DejaVuSans.ttf'),
    ('opt_key_b', 'B', 19, 'DejaVuSans-Bold.ttf'),
    ('opt_key_x', 'X', 19, 'DejaVuSans-Bold.ttf'),
]
# P6.20 Cheats screen (docs/PORT_OPTIONS.md "Cheats"), in the five languages
# of the EU disc (en is also the US disc's). Emitted inside #ifdef DW3_CHEATS,
# so a build without cheats compiles none of it. ON / OFF and the numbers are
# the Options screen's words. Keep each description under 696 px (the message
# box) and each row label under 360 px (it must end before the value cell).
CHEAT_TEXT = {
    'en': {'title': 'CHEATS 1/2', 'title2': 'CHEATS 2/2', 'hint': 'Cheats', 'change': 'Change', 'back': 'Back',
           'ready': 'Ready', 'none': 'None',
           'rows': ['Cheats', 'Fill bag', 'Instant win', 'Battle now', 'Cheat saves'],
           'desc': ['OFF = the original game: no cheat does anything.',
                    'Next field menu: 99 of each item. Saving keeps them.',
                    'Every battle is won on its first turn.',
                    'A random battle on your next step in a battle area.',
                    'Saves written after a cheat changed the game.']},
    'fr': {'title': 'TRICHES 1/2', 'title2': 'TRICHES 2/2', 'hint': 'Triches', 'change': 'Changer', 'back': 'Retour',
           'ready': 'Prêt', 'none': 'Aucune',
           'rows': ['Triches', 'Remplir le sac', 'Victoire immédiate', 'Combat immédiat',
                    'Sauvegardes trichées'],
           'desc': ["OFF = le jeu original : aucune triche n'agit.",
                    'Prochain menu : 99 de chaque objet (sauvegardés).',
                    'Chaque combat est gagné dès le premier tour.',
                    'Combat aléatoire au prochain pas en zone hostile.',
                    'Sauvegardes faites après une triche.']},
    'de': {'title': 'CHEATS 1/2', 'title2': 'CHEATS 2/2', 'hint': 'Cheats', 'change': 'Ändern', 'back': 'Zurück',
           'ready': 'Bereit', 'none': 'Keine',
           'rows': ['Cheats', 'Tasche füllen', 'Sofortsieg', 'Kampf sofort',
                    'Cheat-Spielstände'],
           'desc': ['OFF = Originalspiel: kein Cheat wirkt.',
                    'Nächstes Menü: 99 von jedem Item (wird gespeichert).',
                    'Jeder Kampf ist in der ersten Runde gewonnen.',
                    'Zufallskampf beim nächsten Schritt im Kampfgebiet.',
                    'Spielstände, gespeichert nach einem Cheat.']},
    'es': {'title': 'TRUCOS 1/2', 'title2': 'TRUCOS 2/2', 'hint': 'Trucos', 'change': 'Cambiar', 'back': 'Volver',
           'ready': 'Listo', 'none': 'Ninguna',
           'rows': ['Trucos', 'Llenar bolsa', 'Victoria instantánea', 'Combate ya',
                    'Partidas con trucos'],
           'desc': ['OFF = el juego original: ningún truco actúa.',
                    'Próximo menú: 99 de cada objeto (se guardan).',
                    'Cada combate se gana en su primer turno.',
                    'Combate aleatorio en tu próximo paso en zona hostil.',
                    'Partidas guardadas tras usar un truco.']},
    'it': {'title': 'TRUCCHI 1/2', 'title2': 'TRUCCHI 2/2', 'hint': 'Trucchi', 'change': 'Cambia', 'back': 'Indietro',
           'ready': 'Pronto', 'none': 'Nessuno',
           'rows': ['Trucchi', 'Riempi borsa', 'Vittoria istantanea', 'Battaglia subito',
                    'Salvataggi con trucchi'],
           'desc': ['OFF = il gioco originale: nessun trucco agisce.',
                    'Prossimo menu: 99 di ogni oggetto. Salvando restano.',
                    'Ogni battaglia è vinta al primo turno.',
                    'Battaglia casuale al prossimo passo in zona ostile.',
                    'Salvataggi fatti dopo aver usato un trucco.']},
}

# P6.20 PR 2: page 2 of the Cheats screen (Money, Warp, Item id, Item count,
# Give item): row labels, message lines and the names of the Warp areas, in
# the order of port/options/cheats.c g_warp_modes. The area names are plain
# descriptive labels of the stage select's areas, not the game's own text.
CHEAT_PAGE2 = {'en': {'rows2': ['Money', 'Warp', 'Item id', 'Item count', 'Give item'], 'desc2': ['In the field: adds the amount, or MAX. Saving keeps it.', 'In the field: goes to this area, at its first start point.', 'Left / Right: item id (L / R: 10 at a time).', 'Left / Right: how many (L / R: 10 at a time).', 'In the field: adds the item to the bag. Saving keeps it.'], 'warp': ['Asuka City', 'Digimon Lab', 'Digimon Arena', 'Central Park', 'Wire Forest', 'Tyranno Valley', 'Chinlong City', 'Bulk Swamp', 'Suzaku City', 'Noise Desert', 'Byakko City', 'Snow Mountain', 'Genbu City']}, 'fr': {'rows2': ['Argent', 'Téléportation', "Id de l'objet", 'Quantité', "Donner l'objet"], 'desc2': ['Sur la carte : ajoute la somme, ou MAX. Sauvegardé.', 'Sur la carte : va dans cette zone, au point de départ.', "Gauche / Droite : id de l'objet (L / R : par 10).", 'Gauche / Droite : quantité (L / R : par 10).', "Sur la carte : ajoute l'objet au sac. Sauvegardé."], 'warp': ['Asuka City', 'Labo Digimon', 'Arène Digimon', 'Parc central', 'Forêt Wire', 'Vallée Tyranno', 'Chinlong City', 'Marais Bulk', 'Suzaku City', 'Désert Noise', 'Byakko City', 'Mont Snow', 'Genbu City']}, 'de': {'rows2': ['Geld', 'Teleport', 'Item-ID', 'Anzahl', 'Item geben'], 'desc2': ['Auf der Karte: addiert Betrag oder MAX (gespeichert).', 'Auf der Karte: springt ins Gebiet, zum Startpunkt.', 'Links / Rechts: Item-ID (L / R: je 10).', 'Links / Rechts: Anzahl (L / R: je 10).', 'Auf der Karte: Item in die Tasche (gespeichert).'], 'warp': ['Asuka City', 'Digimon-Labor', 'Digimon-Arena', 'Zentralpark', 'Wire-Wald', 'Tyranno-Tal', 'Chinlong City', 'Bulk-Sumpf', 'Suzaku City', 'Noise-Wüste', 'Byakko City', 'Schneeberg', 'Genbu City']}, 'es': {'rows2': ['Dinero', 'Teletransporte', 'ID de objeto', 'Cantidad', 'Dar objeto'], 'desc2': ['En el mapa: suma la cantidad o MAX (se guarda).', 'En el mapa: va a esta zona, a su punto de inicio.', 'Izq. / Der.: ID del objeto (L / R: de 10 en 10).', 'Izq. / Der.: cantidad (L / R: de 10 en 10).', 'En el mapa: añade el objeto a la bolsa (se guarda).'], 'warp': ['Asuka City', 'Lab. Digimon', 'Arena Digimon', 'Parque Central', 'Bosque Wire', 'Valle Tyranno', 'Chinlong City', 'Pantano Bulk', 'Suzaku City', 'Desierto Noise', 'Byakko City', 'Monte Snow', 'Genbu City']}, 'it': {'rows2': ['Denaro', 'Teletrasporto', 'ID oggetto', 'Quantità', 'Dai oggetto'], 'desc2': ['Sulla mappa: aggiunge la somma o MAX (si salva).', 'Sulla mappa: va in questa zona, al punto di partenza.', 'Sinistra / Destra: ID oggetto (L / R: di 10 in 10).', 'Sinistra / Destra: quantità (L / R: di 10 in 10).', "Sulla mappa: aggiunge l'oggetto alla borsa (si salva)."], 'warp': ['Asuka City', 'Lab Digimon', 'Arena Digimon', 'Parco centrale', 'Foresta Wire', 'Valle Tyranno', 'Chinlong City', 'Palude Bulk', 'Suzaku City', 'Deserto Noise', 'Byakko City', 'Monte Snow', 'Genbu City']}}
for _lang, _extra in CHEAT_PAGE2.items():
    CHEAT_TEXT[_lang].update(_extra)
CHEAT_LETTERS = [('opt_key_y', 'Y', 19, 'DejaVuSans-Bold.ttf')]
# Money values (the same in every language) and the digits of the two numbers
# of the Give item rows: a fixed advance (the 5th field), so that a number is
# the digits side by side (title_overlay_art.c, number()).
CHEAT_LETTERS += [('opt_cht_m10k', '+10,000', 26, 'DejaVuSans.ttf'),
                  ('opt_cht_m100k', '+100,000', 26, 'DejaVuSans.ttf'),
                  ('opt_cht_mmax', 'MAX', 26, 'DejaVuSans.ttf')]
CHEAT_LETTERS += [('opt_cht_d%d' % d, str(d), 26, 'DejaVuSans.ttf', 17) for d in range(10)]
for lang, t in CHEAT_TEXT.items():
    CHEAT_LETTERS.append(('opt_cht_title_' + lang, t['title'], 30, 'DejaVuSans.ttf'))
    CHEAT_LETTERS.append(('opt_cht_title2_' + lang, t['title2'], 30, 'DejaVuSans.ttf'))
    for i, row in enumerate(t['rows2']):
        CHEAT_LETTERS.append(('opt_cht_row%d_%s' % (i + 5, lang), row, 26, 'DejaVuSans.ttf'))
    for i, desc in enumerate(t['desc2']):
        CHEAT_LETTERS.append(('opt_cht_desc%d_%s' % (i + 5, lang), desc, 25, 'DejaVuSans.ttf'))
    for i, name in enumerate(t['warp']):
        CHEAT_LETTERS.append(('opt_cht_warp%d_%s' % (i, lang), name, 26, 'DejaVuSans.ttf'))
    for i, row in enumerate(t['rows']):
        CHEAT_LETTERS.append(('opt_cht_row%d_%s' % (i, lang), row, 26, 'DejaVuSans.ttf'))
    for i, desc in enumerate(t['desc']):
        CHEAT_LETTERS.append(('opt_cht_desc%d_%s' % (i, lang), desc, 25, 'DejaVuSans.ttf'))
    for key in ('ready', 'none'):
        CHEAT_LETTERS.append(('opt_cht_%s_%s' % (key, lang), t[key], 26, 'DejaVuSans.ttf'))
    for key in ('change', 'back', 'hint'):
        CHEAT_LETTERS.append(('opt_cht_%s_%s' % (key, lang), t[key], 24, 'DejaVuSans.ttf'))
lines = ['/* Generated by tools/generate_pc_title_lettering.py. DejaVu font license applies. */']
ENTRIES = [(e, False) for e in LETTERS] + [(e, True) for e in CHEAT_LETTERS]
for index, (entry, cheat) in enumerate(ENTRIES):
    name, text, size, font_name = entry[:4]
    advance = entry[4] if len(entry) > 4 else None
    if cheat and not ENTRIES[index - 1][1]:
        lines.append('#ifdef DW3_CHEATS')
    font = ImageFont.truetype(str(FONT_ROOT / font_name), size)
    bounds = font.getbbox(text)
    if advance is None:
        image = Image.new('L', (bounds[2]-bounds[0], bounds[3]-bounds[1]))
        ImageDraw.Draw(image).text((-bounds[0], -bounds[1]), text, font=font, fill=255)
    else:
        # Fixed advance: the glyph keeps its own side bearing.
        image = Image.new('L', (advance, bounds[3]-bounds[1]))
        ImageDraw.Draw(image).text((0, -bounds[1]), text, font=font, fill=255)
    pixels = image.tobytes()
    runs = []
    position = 0
    while position < len(pixels):
        count = 1
        while position+count < len(pixels) and pixels[position+count] == pixels[position] and count < 255:
            count += 1
        runs.extend((count, pixels[position]))
        position += count
    lines.append('static const unsigned char title_%s[] = {%s};' % (name, ','.join(map(str, runs))))
    lines.append('static const int title_%s_width=%d, title_%s_height=%d;' % (name, image.width, name, image.height))
    if name.startswith('opt_'):
        # Offset from the text origin, so words on one line share a baseline.
        lines.append('static const int title_%s_top=%d;' % (name, bounds[1]))
    if cheat and index == len(ENTRIES) - 1:
        lines.append('#endif /* DW3_CHEATS */')
(ROOT / 'platforms/pc/src/title_overlay_lettering.inc').write_text('\n'.join(lines)+'\n')
