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
    ('opt_title_fr', 'OPTIONS', 30, 'DejaVuSans.ttf'),
    ('opt_title_de', 'OPTIONEN', 30, 'DejaVuSans.ttf'),
    ('opt_title_es', 'OPCIONES', 30, 'DejaVuSans.ttf'),
    ('opt_title_it', 'OPZIONI', 30, 'DejaVuSans.ttf'),
    # P6.17 volume rows: label, then the step numbers 1..10 (0 is "OFF").
    # P6.18 rows: Encounter rate (OFF / 0.5x / 1x / 1.5x / 2x) and Text speed.
    # P7.9 row: Battle cursor (OFF / Command / Command+Tech).
    ('opt_1x', '1x', 26, 'DejaVuSans.ttf'),
    ('opt_05x', '0.5x', 26, 'DejaVuSans.ttf'),
    ('opt_15x', '1.5x', 26, 'DejaVuSans.ttf'),
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
    ('opt_key_b', 'B', 19, 'DejaVuSans-Bold.ttf'),
    ('opt_key_x', 'X', 19, 'DejaVuSans-Bold.ttf'),
]
# P9.9 Options screen: groups and sub-screens (docs/PORT_OPTIONS.md "Groups and
# sub-screens"), in the five languages of the EU disc (en is also the US disc's).
# Everything here is read as title_opt_<kind>_<key>_<lang>. Limits (checked
# below, the generator fails when a text is too wide): group names are bold
# plaque lettering under 330 px; row labels 290 px (the value cell starts 500 px
# from the plaque's left edge); values 236 px; descriptions 690 px (the message
# strip).
LANGS = ('en', 'fr', 'de', 'es', 'it')
GROUP_TEXT = {
    'display': {'name': ('Display', 'Affichage', 'Anzeige', 'Pantalla', 'Schermo'),
                'desc': ('How the picture fills the screen.',
                         "Comment l'image remplit l'écran.",
                         'Wie das Bild den Bildschirm füllt.',
                         'Cómo la imagen llena la pantalla.',
                         "Come l'immagine riempie lo schermo.")},
    'sound': {'name': ('Sound', 'Son', 'Ton', 'Sonido', 'Audio'),
              'desc': ('Music and sound effects volume.',
                       'Volume de la musique et des effets.',
                       'Lautstärke von Musik und Effekten.',
                       'Volumen de la música y los efectos.',
                       'Volume di musica ed effetti.')},
    'gameplay': {'name': ('Gameplay', 'Jouabilité', 'Gameplay', 'Jugabilidad', 'Gameplay'),
                 'desc': ('Speed, encounters, text and battle menu.',
                          'Vitesse, combats, texte et menu de combat.',
                          'Tempo, Kämpfe, Text und Kampfmenü.',
                          'Velocidad, combates, texto y menú de combate.',
                          'Velocità, scontri, testo e menu di battaglia.')},
    'saves': {'name': ('Saves', 'Sauvegardes', 'Speichern', 'Partidas', 'Salvataggi'),
              'desc': ('How and when the game saves.',
                       'Comment et quand le jeu sauvegarde.',
                       'Wie und wann das Spiel speichert.',
                       'Cómo y cuándo guarda el juego.',
                       'Come e quando il gioco salva.')},
    'cheats': {'name': ('Cheats', 'Triches', 'Cheats', 'Trucos', 'Trucchi'),
               'desc': ('Test menu. Everything is off until you turn it on.',
                        "Menu de test. Tout est désactivé par défaut.",
                        'Testmenü. Alles ist aus, bis du es einschaltest.',
                        'Menú de pruebas. Todo está apagado por defecto.',
                        'Menu di prova. Tutto è spento finché non lo attivi.')},
    # P6.30 Help: the group name and the message line under the group list.
    'help': {'name': ('Help', 'Aide', 'Hilfe', 'Ayuda', 'Aiuto'),
             'desc': ('Controls, hotkeys, saves and bug reports.',
                      'Commandes, raccourcis, sauvegardes, signalement.',
                      'Steuerung, Kürzel, Speichern und Fehlerberichte.',
                      'Controles, atajos, partidas e informes de errores.',
                      'Comandi, scorciatoie, salvataggi e segnalazioni.')},
}
OPEN_TEXT = ('Open', 'Ouvrir', 'Öffnen', 'Abrir', 'Apri')
CHANGE_TEXT = ('Change', 'Changer', 'Ändern', 'Cambiar', 'Cambia')
BACK_TEXT = ('Back', 'Retour', 'Zurück', 'Volver', 'Indietro')
# Registry rows, in registry order (port/options/port_options.c). Auto-save is
# P6.31's opt_as_* above. desc1 / desc2 differ on PC and Switch (pc / nx).
ROW_TEXT = (
    ('Widescreen', 'Écran large', 'Breitbild', 'Pantalla ancha', 'Schermo largo'),
    ('Fast-forward', 'Avance rapide', 'Schnellvorlauf', 'Avance rápido', 'Avanzamento rapido'),
    ('Restart hotkey', 'Raccourci de reset', 'Neustart-Kürzel', 'Atajo de reinicio', 'Scorciatoia reset'),
    ('Music volume', 'Volume musique', 'Musikvolumen', 'Volumen música', 'Volume musica'),
    ('SFX volume', 'Volume effets', 'Effektvolumen', 'Volumen efectos', 'Volume effetti'),
    ('Encounter rate', 'Taux de combats', 'Kampfhäufigkeit', 'Frec. de combates', 'Frequenza scontri'),
    ('Text speed', 'Vitesse du texte', 'Textgeschwindigkeit', 'Velocidad del texto', 'Velocità del testo'),
    ('Battle cursor', 'Curseur de combat', 'Kampfcursor', 'Cursor de combate', 'Cursore battaglia'),
)
DESC_TEXT = (
    ('Uses the full 16:9 screen where the game allows it.',
     "Utilise tout l'écran 16:9 quand le jeu le permet.",
     'Nutzt den ganzen 16:9-Schirm, wo das Spiel es kann.',
     'Usa toda la pantalla 16:9 donde el juego lo permite.',
     'Usa tutto lo schermo 16:9 dove il gioco lo consente.'),
    None,  # desc1: PC / Switch below
    None,  # desc2: PC / Switch below
    ('Left / Right change the music volume (10 = original).',
     'Gauche / Droite : volume musique (10 = original).',
     'Links / Rechts: Musikvolumen (10 = Original).',
     'Izq. / Der.: volumen de música (10 = original).',
     'Sinistra / Destra: volume musica (10 = originale).'),
    ('Left / Right change the effects volume (10 = original).',
     'Gauche / Droite : volume des effets (10 = original).',
     'Links / Rechts: Effektvolumen (10 = Original).',
     'Izq. / Der.: volumen de efectos (10 = original).',
     'Sinistra / Destra: volume effetti (10 = originale).'),
    ('Random battles only: OFF to 2x. Story battles stay.',
     'Combats aléatoires seulement : OFF à 2x.',
     'Nur Zufallskämpfe: OFF bis 2x. Story-Kämpfe bleiben.',
     'Solo combates aleatorios: de OFF a 2x.',
     'Solo scontri casuali: da OFF a 2x.'),
    ('How fast dialogue is typed out. Normal = original.',
     "Vitesse d'affichage des dialogues. Normal = original.",
     'Tempo der Dialoge. Normal = Original.',
     'Velocidad de los diálogos. Normal = original.',
     'Velocità dei dialoghi. Normale = originale.'),
    ('Battle menu opens on your last pick. Off = original.',
     "Le menu de combat s'ouvre sur votre dernier choix.",
     'Das Kampfmenü öffnet bei der letzten Wahl.',
     'El menú de combate abre en tu última elección.',
     "Il menu di battaglia si apre sull'ultima scelta."),
)
DESC1 = {'pc': ('Press R3 in the field or in battle to switch speed.',
                'R3 sur la carte ou en combat change la vitesse.',
                'R3 im Feld oder im Kampf wechselt das Tempo.',
                'R3 en el mapa o en combate cambia la velocidad.',
                'R3 sul campo o in battaglia cambia la velocità.'),
         'nx': ('Click the right stick in the field or in battle.',
                'Clic du stick droit sur la carte ou en combat.',
                'Rechten Stick drücken im Feld oder im Kampf.',
                'Pulsa el stick derecho en el mapa o en combate.',
                'Premi lo stick destro sul campo o in battaglia.')}
DESC2 = {'pc': ('L1 + R1 + Start + Select restarts the game.',
                'L1 + R1 + Start + Select redémarre le jeu.',
                'L1 + R1 + Start + Select startet das Spiel neu.',
                'L1 + R1 + Start + Select reinicia el juego.',
                'L1 + R1 + Start + Select riavvia il gioco.'),
         'nx': ('L + R + Plus + Minus restarts the game.',
                'L + R + Plus + Moins redémarre le jeu.',
                'L + R + Plus + Minus startet das Spiel neu.',
                'L + R + Más + Menos reinicia el juego.',
                'L + R + Più + Meno riavvia il gioco.')}
VALUE_TEXT = {
    'normal': ('Normal', 'Normal', 'Normal', 'Normal', 'Normale'),
    'fast': ('Fast', 'Rapide', 'Schnell', 'Rápido', 'Veloce'),
    'instant': ('Instant', 'Instantané', 'Sofort', 'Instantáneo', 'Istantaneo'),
    'command': ('Command', 'Commande', 'Befehl', 'Comando', 'Comando'),
    'command_tech': ('Command+Tech', 'Commande+Tech', 'Befehl+Tech', 'Comando+Téc.', 'Comando+Tec.'),
}
LIMITS = {'grp': 330, 'row': 290, 'val': 236, 'desc': 690}
for _i, _lang in enumerate(LANGS):
    for _key, _t in GROUP_TEXT.items():
        LETTERS.append(('opt_grp_%s_%s' % (_key, _lang), _t['name'][_i], 29, 'DejaVuSansCondensed-Bold.ttf'))
        LETTERS.append(('opt_grpdesc_%s_%s' % (_key, _lang), _t['desc'][_i], 25, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_open_' + _lang, OPEN_TEXT[_i], 24, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_change_' + _lang, CHANGE_TEXT[_i], 24, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_back_' + _lang, BACK_TEXT[_i], 24, 'DejaVuSans.ttf'))
    for _r, _row in enumerate(ROW_TEXT):
        LETTERS.append(('opt_row%d_%s' % (_r, _lang), _row[_i], 26, 'DejaVuSans.ttf'))
    for _r, _desc in enumerate(DESC_TEXT):
        if _desc is not None:
            LETTERS.append(('opt_desc%d_%s' % (_r, _lang), _desc[_i], 25, 'DejaVuSans.ttf'))
    for _plat in ('pc', 'nx'):
        LETTERS.append(('opt_desc1_%s_%s' % (_plat, _lang), DESC1[_plat][_i], 25, 'DejaVuSans.ttf'))
        LETTERS.append(('opt_desc2_%s_%s' % (_plat, _lang), DESC2[_plat][_i], 25, 'DejaVuSans.ttf'))
    for _key, _t in VALUE_TEXT.items():
        LETTERS.append(('opt_val_%s_%s' % (_key, _lang), _t[_i], 26, 'DejaVuSans.ttf'))
# P6.27 / P6.28 title Quit plaque and the Yes / No confirmation (docs/PORT_OPTIONS.md
# "Quit and return to title"), in the five languages of the EU disc (en is also
# the US disc's). The plaque word is bold like the other plaques; the window
# title is in capitals like the Options screen's. Keep each message line under
# 650 px (the window body) and the words short enough for their button row.
# The code reads them as title_quit_<lang> (the plaque) and
# title_opt_dlg_<key>_<lang>.
DIALOG_TEXT = {
    'en': {'plaque': 'Quit game', 'quit_title': 'QUIT GAME', 'ret_title': 'TITLE SCREEN',
           'quit_q': 'Quit the game?', 'ret_q': 'Return to the title screen?',
           'warn': 'Progress since your last save will be lost.',
           'yes': 'Yes', 'no': 'No', 'select': 'Select', 'cancel': 'Cancel'},
    'fr': {'plaque': 'Quitter', 'quit_title': 'QUITTER', 'ret_title': 'ÉCRAN TITRE',
           'quit_q': 'Quitter le jeu ?', 'ret_q': "Retourner à l'écran titre ?",
           'warn': 'La progression non sauvegardée sera perdue.',
           'yes': 'Oui', 'no': 'Non', 'select': 'Choisir', 'cancel': 'Annuler'},
    'de': {'plaque': 'Beenden', 'quit_title': 'BEENDEN', 'ret_title': 'TITELBILD',
           'quit_q': 'Spiel beenden?', 'ret_q': 'Zurück zum Titelbild?',
           'warn': 'Nicht gespeicherter Fortschritt geht verloren.',
           'yes': 'Ja', 'no': 'Nein', 'select': 'Wählen', 'cancel': 'Abbrechen'},
    'es': {'plaque': 'Salir', 'quit_title': 'SALIR', 'ret_title': 'PANTALLA DE TÍTULO',
           'quit_q': '¿Salir del juego?', 'ret_q': '¿Volver a la pantalla de título?',
           'warn': 'Se perderá el progreso no guardado.',
           'yes': 'Sí', 'no': 'No', 'select': 'Elegir', 'cancel': 'Cancelar'},
    'it': {'plaque': 'Esci', 'quit_title': 'ESCI', 'ret_title': 'SCHERMATA TITOLO',
           'quit_q': 'Uscire dal gioco?', 'ret_q': 'Tornare alla schermata del titolo?',
           'warn': 'I progressi non salvati andranno persi.',
           'yes': 'Sì', 'no': 'No', 'select': 'Scegli', 'cancel': 'Annulla'},
}
for _lang, _t in DIALOG_TEXT.items():
    LETTERS.append(('quit_' + _lang, _t['plaque'], 29, 'DejaVuSansCondensed-Bold.ttf'))
    for _key, _size in (('quit_title', 30), ('ret_title', 30), ('quit_q', 26), ('ret_q', 26),
                        ('warn', 24), ('yes', 26), ('no', 26), ('select', 24), ('cancel', 24)):
        LETTERS.append(('opt_dlg_%s_%s' % (_key, _lang), _t[_key], _size, 'DejaVuSans.ttf'))
# P6.31 Auto-save row of the Options screen (docs/PORT_OPTIONS.md "Auto-save",
# docs/P6.31_AUTOSAVE.md), in the five languages of the EU disc (en is also the
# US disc's). OFF is the Options screen's word. Keep the label under 360 px, the
# value under 214 px (the value cell is right-aligned at x 780 from x 560, and the
# widest word, Command+Tech, is 214 px) and the description under 696 px.
AUTOSAVE_TEXT = {
    'en': {'row': 'Auto-save', 'area': 'Each area',
           'desc': 'Saves to its own file at each new area. Use Continue.'},
    'fr': {'row': 'Sauvegarde auto', 'area': 'Chaque zone',
           'desc': "Sauvegarde dans son propre fichier. Voir Continuer."},
    'de': {'row': 'Auto-Speichern', 'area': 'Jedes Gebiet',
           'desc': 'Eigene Datei bei jedem Gebiet. Laden mit Weiter.'},
    'es': {'row': 'Autoguardado', 'area': 'Cada zona',
           'desc': 'Guarda en su propio archivo. Carga con Continuar.'},
    'it': {'row': 'Salvataggio auto', 'area': 'Ogni area',
           'desc': "Salva in un file a parte. Si carica con Continua."},
}
# P6.31 follow-up: the title menu's Continue plaque (it loads the auto-save;
# shown, with the game's own label moved to a Load Game row, only when an
# auto-save file exists). Same bold condensed face as the Quit plaque; keep each
# under 420 px.
CONTINUE_TEXT = {'en': 'Continue', 'fr': 'Continuer', 'de': 'Weiter',
                 'es': 'Continuar', 'it': 'Continua'}
# The Load Game plaque beside it (the old Continue: the slot list). The EU
# disc's own label for that row reads "Continue", so with two rows it would say
# it twice: both discs use this lettering while the Continue row is there.
LOAD_TEXT = {'en': 'Load Game', 'fr': 'Charger', 'de': 'Laden',
             'es': 'Cargar', 'it': 'Carica'}
# The first plaque on the EU disc: its own words are "Start" (en, de),
# "Commencer", "Inizia", "Comenzar". The disc's sprites are a white bubble with
# a dark word inside, so the plaque looked unlike the port's other plaques;
# every plaque now uses this lettering (same face and shadow).
NEWGAME_TEXT = {'en': 'Start', 'fr': 'Commencer', 'de': 'Start',
                'es': 'Comenzar', 'it': 'Inizia'}
for _lang, _text in NEWGAME_TEXT.items():
    LETTERS.append(('newgame_' + _lang, _text, 29, 'DejaVuSansCondensed-Bold.ttf'))
for _lang, _text in CONTINUE_TEXT.items():
    LETTERS.append(('continue_' + _lang, _text, 29, 'DejaVuSansCondensed-Bold.ttf'))
for _lang, _text in LOAD_TEXT.items():
    LETTERS.append(('loadgame_' + _lang, _text, 29, 'DejaVuSansCondensed-Bold.ttf'))
for _lang, _t in AUTOSAVE_TEXT.items():
    LETTERS.append(('opt_as_row_' + _lang, _t['row'], 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_as_area_' + _lang, _t['area'], 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_as_desc_' + _lang, _t['desc'], 25, 'DejaVuSans.ttf'))
# P9.7 Options screen opened from the field menu (docs/PORT_OPTIONS.md "Options
# in game"): the message line of a row that cannot change in game (Widescreen),
# in the five languages of the EU disc (en is also the US disc's). Keep it under
# 696 px (the message box).
LOCK_TEXT = {
    'en': 'Locked in game: change it at the title screen.',
    'fr': "Verrouill\u00e9 en jeu : \u00e0 changer \u00e0 l'\u00e9cran titre.",
    'de': 'Im Spiel gesperrt: im Titelbild \u00e4ndern.',
    'es': 'Bloqueado en juego: c\u00e1mbialo en el t\u00edtulo.',
    'it': 'Bloccato in gioco: cambialo nel titolo.',
}
for _lang, _text in LOCK_TEXT.items():
    LETTERS.append(('opt_lock_' + _lang, _text, 25, 'DejaVuSans.ttf'))
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
TOO_WIDE = []
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
    limit = (LIMITS['grp'] if name.startswith('opt_grp_') else LIMITS['row'] if name.startswith(('opt_row', 'opt_cht_row')) else
             LIMITS['val'] if name.startswith(('opt_val_', 'opt_cht_warp', 'opt_cht_ready', 'opt_cht_none',
                                               'opt_cht_m10k', 'opt_cht_m100k', 'opt_cht_mmax')) else
             LIMITS['desc'] if name.startswith(('opt_desc', 'opt_grpdesc', 'opt_lock', 'opt_cht_desc')) else None)
    if limit is not None and image.width > limit:
        TOO_WIDE.append('%s is %d px wide (limit %d): %r' % (name, image.width, limit, text))
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
if TOO_WIDE:
    raise SystemExit('\n'.join(TOO_WIDE))
