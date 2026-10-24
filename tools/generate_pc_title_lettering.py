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
    'en': {'title': 'CHEATS', 'hint': 'Cheats', 'change': 'Change', 'back': 'Back',
           'ready': 'Ready', 'none': 'None',
           'rows': ['Cheats', 'Fill bag', 'Instant win', 'Battle now', 'Cheat saves'],
           'desc': ['OFF = the original game: no cheat does anything.',
                    'Next field menu: 99 of each item. Saving keeps them.',
                    'Every battle is won on its first turn.',
                    'A random battle on your next step in a battle area.',
                    'Saves written after a cheat changed the game.']},
    'fr': {'title': 'TRICHES', 'hint': 'Triches', 'change': 'Changer', 'back': 'Retour',
           'ready': 'Prêt', 'none': 'Aucune',
           'rows': ['Triches', 'Remplir le sac', 'Victoire immédiate', 'Combat immédiat',
                    'Sauvegardes trichées'],
           'desc': ["OFF = le jeu original : aucune triche n'agit.",
                    'Prochain menu : 99 de chaque objet (sauvegardés).',
                    'Chaque combat est gagné dès le premier tour.',
                    'Combat aléatoire au prochain pas en zone hostile.',
                    'Sauvegardes faites après une triche.']},
    'de': {'title': 'CHEATS', 'hint': 'Cheats', 'change': 'Ändern', 'back': 'Zurück',
           'ready': 'Bereit', 'none': 'Keine',
           'rows': ['Cheats', 'Tasche füllen', 'Sofortsieg', 'Kampf sofort',
                    'Cheat-Spielstände'],
           'desc': ['OFF = Originalspiel: kein Cheat wirkt.',
                    'Nächstes Menü: 99 von jedem Item (wird gespeichert).',
                    'Jeder Kampf ist in der ersten Runde gewonnen.',
                    'Zufallskampf beim nächsten Schritt im Kampfgebiet.',
                    'Spielstände, gespeichert nach einem Cheat.']},
    'es': {'title': 'TRUCOS', 'hint': 'Trucos', 'change': 'Cambiar', 'back': 'Volver',
           'ready': 'Listo', 'none': 'Ninguna',
           'rows': ['Trucos', 'Llenar bolsa', 'Victoria instantánea', 'Combate ya',
                    'Partidas con trucos'],
           'desc': ['OFF = el juego original: ningún truco actúa.',
                    'Próximo menú: 99 de cada objeto (se guardan).',
                    'Cada combate se gana en su primer turno.',
                    'Combate aleatorio en tu próximo paso en zona hostil.',
                    'Partidas guardadas tras usar un truco.']},
    'it': {'title': 'TRUCCHI', 'hint': 'Trucchi', 'change': 'Cambia', 'back': 'Indietro',
           'ready': 'Pronto', 'none': 'Nessuno',
           'rows': ['Trucchi', 'Riempi borsa', 'Vittoria istantanea', 'Battaglia subito',
                    'Salvataggi con trucchi'],
           'desc': ['OFF = il gioco originale: nessun trucco agisce.',
                    'Prossimo menu: 99 di ogni oggetto. Salvando restano.',
                    'Ogni battaglia è vinta al primo turno.',
                    'Battaglia casuale al prossimo passo in zona ostile.',
                    'Salvataggi fatti dopo aver usato un trucco.']},
}
CHEAT_LETTERS = [('opt_key_y', 'Y', 19, 'DejaVuSans-Bold.ttf')]
for lang, t in CHEAT_TEXT.items():
    CHEAT_LETTERS.append(('opt_cht_title_' + lang, t['title'], 30, 'DejaVuSans.ttf'))
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
for index, ((name, text, size, font_name), cheat) in enumerate(ENTRIES):
    if cheat and not ENTRIES[index - 1][1]:
        lines.append('#ifdef DW3_CHEATS')
    font = ImageFont.truetype(str(FONT_ROOT / font_name), size)
    bounds = font.getbbox(text)
    image = Image.new('L', (bounds[2]-bounds[0], bounds[3]-bounds[1]))
    ImageDraw.Draw(image).text((-bounds[0], -bounds[1]), text, font=font, fill=255)
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
