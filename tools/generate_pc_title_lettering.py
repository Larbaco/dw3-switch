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
    # Button caps of the hint row (title_overlay_art.c button()): one per
    # physical Switch button a PS button can come from (P10.6 fix-now, the cap
    # follows the button map). Not under DW3_CHEATS: every hint row uses them.
    ('opt_key_a', 'A', 19, 'DejaVuSans-Bold.ttf'),
    ('opt_key_y', 'Y', 19, 'DejaVuSans-Bold.ttf'),
    ('opt_key_l', 'L', 19, 'DejaVuSans-Bold.ttf'),
    ('opt_key_r', 'R', 19, 'DejaVuSans-Bold.ttf'),
    ('opt_key_zl', 'ZL', 15, 'DejaVuSans-Bold.ttf'),
    ('opt_key_zr', 'ZR', 15, 'DejaVuSans-Bold.ttf'),
    ('opt_key_l3', 'L3', 15, 'DejaVuSans-Bold.ttf'),
    ('opt_key_r3', 'R3', 15, 'DejaVuSans-Bold.ttf'),
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
    # P9.8 Controls: the Button layout row lives here (docs/CONTROLS_BUNDLE.md).
    'controls': {'name': ('Controls', 'Commandes', 'Steuerung', 'Controles', 'Comandi'),
                 'desc': ('Which button confirms and which cancels.',
                          'Quel bouton valide et quel bouton annule.',
                          'Welche Taste bestätigt und welche bricht ab.',
                          'Qué botón confirma y cuál cancela.',
                          'Quale pulsante conferma e quale annulla.')},
    # P6.12 Customize buttons: the header of the sub-screen and the message
    # line of its rows. FR / DE / IT / ES are machine-drafted, needs native
    # review.
    'custom': {'name': ('Customize', 'Personnaliser', 'Anpassen', 'Personalizar', 'Personalizza'),
               'desc': ('Confirm, then press a button. Left / Right picks one.',
                        'Valider, puis appuyer. Gauche / Droite choisit.',
                        'Bestätigen, dann Taste drücken. Links / Rechts wählt.',
                        'Confirma y pulsa un botón. Izquierda / Derecha elige.',
                        'Conferma, poi premi un tasto. Sinistra / Destra sceglie.')},
    # P6.30 Help: the group name and the message line under the group list.
    'help': {'name': ('Help', 'Aide', 'Hilfe', 'Ayuda', 'Aiuto'),
             'desc': ('Controls, hotkeys, saves and bug reports.',
                      'Commandes, raccourcis, sauvegardes, signalement.',
                      'Steuerung, Kürzel, Speichern und Fehlerberichte.',
                      'Controles, atajos, partidas e informes de errores.',
                      'Comandi, scorciatoie, salvataggi e segnalazioni.')},
    # P6.35 Walkthrough: the group name and the message line under the group
    # list. FR / DE / IT / ES are machine-drafted, needs native review.
    'walkthrough': {'name': ('Walkthrough', 'Pas à pas', 'Komplettlösung', 'Guía', 'Soluzione'),
                    'desc': ('Where to go next. Hints first, answers on request.',
                             "Où aller ensuite. Indices d'abord, solution après.",
                             'Nächstes Ziel. Hinweise zuerst, Lösung auf Wunsch.',
                             'Adónde ir ahora. Pistas primero, solución si la pides.',
                             'Dove andare ora. Prima indizi, soluzioni a richiesta.')},
    # D1 (docs/OPTIONS_REORG.md): the Menus group (the five Items-menu switches),
    # the Advanced group (the stage sweep, a test aid) and the Map buttons
    # sub-screen of Controls. FR / DE / IT / ES are machine-drafted, needs native
    # review.
    'menus': {'name': ('Menus', 'Menus', 'Menüs', 'Menús', 'Menu'),
              'desc': ('The Items menu and its equipment shortcuts.',
                       "Le menu Objets et ses raccourcis d'équipement.",
                       'Das Itemmenü und seine Ausrüstungs-Kürzel.',
                       'El menú de objetos y sus atajos de equipo.',
                       'Il menu oggetti e le scorciatoie di equipaggiamento.')},
    'advanced': {'name': ('Advanced', 'Avancé', 'Erweitert', 'Avanzado', 'Avanzate'),
                 'desc': ('Test aids for developers.',
                          'Outils de test pour les développeurs.',
                          'Testhilfen für Entwickler.',
                          'Ayudas de prueba para desarrolladores.',
                          'Strumenti di prova per sviluppatori.')},
    'mapbtn': {'name': ('Map buttons', 'Boutons carte', 'Kartentasten', 'Botones del mapa', 'Tasti mappa'),
               'desc': ('Used by the full-screen map. Left / Right pick a button.',
                        'Pour la carte plein écran. Gauche / Droite choisit.',
                        'Für die Vollbildkarte. Links / Rechts wählt.',
                        'Para el mapa completo. Izquierda / Derecha elige.',
                        'Per la mappa a schermo. Sinistra / Destra sceglie.')},
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
LIMITS = {'grp': 330, 'row': 290, 'val': 236, 'desc': 690, 'dlg': 650}
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
# D1 (docs/OPTIONS_REORG.md section 3): the rows that were file-only keys and the
# Map buttons screen. Keys are the short names the code reads as
# title_opt_row_x_<key>_<lang> / title_opt_desc_x_<key>_<lang>. FR / DE / IT / ES
# are machine-drafted, needs native review. Row labels under 290 px, values
# under 236 px, message lines under 690 px (the generator fails when wider).
# Button-neutral wording: the PlayStation and the Switch names differ.
EXTRA_ROWS = {
    'eq': {'row': ('Equip from Items', 'Équiper depuis Objets', 'Aus Items ausrüsten',
                   'Equipar desde Ítems', 'Equipaggia da menu'),
           'desc': ('Equips gear from the Items menu. OFF = original.',
                    'Équipe depuis le menu Objets. OFF = original.',
                    'Ausrüsten aus dem Itemmenü. OFF = Original.',
                    'Equipar desde el menú Objetos. OFF = original.',
                    'Equipaggia dal menu Oggetti. OFF = originale.')},
    'st': {'row': ('Equip stays on list', 'Rester sur la liste', 'Auf der Liste bleiben',
                   'Seguir en la lista', 'Restare nella lista'),
           'desc': ('Equipping returns to the slot list. OFF = the long fades.',
                    'Équiper revient à la liste. OFF = longs fondus.',
                    'Ausrüsten bleibt in der Liste. OFF = lange Blenden.',
                    'Equipar vuelve a la lista. OFF = fundidos largos.',
                    'Equipaggiare resta nella lista. OFF = dissolvenze.')},
    'bg': {'row': ('Whole bag', 'Tout le sac', 'Ganze Tasche', 'Bolsa entera', 'Intera borsa'),
           'desc': ('The whole bag in the equip list. OFF = original.',
                    'Tout le sac dans la liste. OFF = original.',
                    'Ganze Tasche in der Liste. OFF = Original.',
                    'Toda la bolsa en la lista. OFF = original.',
                    'Tutta la borsa nella lista. OFF = originale.')},
    'us': {'row': ('Use item stays', 'Rester après usage', 'Nach Benutzen offen',
                   'Seguir tras usar', "Restare dopo l'uso"),
           'desc': ('A use keeps the partner step open. OFF = original.',
                    'Après usage, le partenaire reste. OFF = original.',
                    'Nach Benutzen bleibt die Partnerwahl. OFF = Original.',
                    'Tras usarlo sigue la elección. OFF = original.',
                    "Dopo l'uso resta la scelta. OFF = originale.")},
    'ps': {'row': ('Partner switch', 'Autre partenaire', 'Partnerwechsel',
                   'Cambiar compañero', 'Cambio compagno'),
           'desc': ('ZL / ZR switch the Digimon in Equip. OFF = original.',
                    'ZL / ZR changent de Digimon. OFF = original.',
                    'ZL / ZR wechseln das Digimon. OFF = Original.',
                    'ZL / ZR cambian de Digimon. OFF = original.',
                    'ZL / ZR cambiano Digimon. OFF = originale.')},
    'vr': {'row': ('Version label', 'Étiquette de version', 'Versionsanzeige',
                   'Etiqueta de versión', 'Etichetta versione'),
           'desc': ('Port version on the title screen. OFF hides it.',
                    "Version du port sur l'écran titre. OFF = masquée.",
                    'Port-Version im Titelbild. OFF = versteckt.',
                    'Versión del port en el título. OFF = oculta.',
                    'Versione del port nel titolo. OFF = nascosta.')},
    'mm': {'row': ('Map mod pack', 'Pack de mod carte', 'Karten-Modpaket',
                   'Pack de mod mapa', 'Pacchetto mappa'),
           'desc': ('Uses a map-upscale pack. OFF = the original map.',
                    "Utilise un pack de carte. OFF = carte d'origine.",
                    'Nutzt ein Karten-Paket. OFF = Originalkarte.',
                    'Usa un pack de mapa. OFF = mapa original.',
                    'Usa un pacchetto mappa. OFF = mappa originale.')},
    'ss': {'row': ('Shared saves (exp.)', 'Saves partagées', 'Gem. Speicher (exp.)',
                   'Partidas comp. (exp.)', 'Salvat. cond. (exp.)'),
           'desc': ('Experimental: US and EU share saves. Next start.',
                    'Expérimental : US et EU partagent. Au redémarrage.',
                    'Experimentell: US und EU teilen. Beim Neustart.',
                    'Experimental: US y EU comparten. Al reiniciar.',
                    'Sperimentale: US ed EU condividono. Al riavvio.')},
    'sw': {'row': ('Stage sweep', 'Balayage des zones', 'Stufen-Durchlauf',
                   'Barrido de etapas', 'Scansione stage'),
           'desc': ('Test aid. Walks every stage after the next start.',
                    'Outil de test. Parcourt les stages au redémarrage.',
                    'Testhilfe. Geht nach dem nächsten Start jede Stufe ab.',
                    'Prueba. Recorre todas las etapas en el próximo inicio.',
                    'Prova. Percorre ogni stage al prossimo avvio.')},
    'sf': {'row': ('Frames per stage', 'Frames par stage', 'Frames pro Stufe',
                   'Fotogramas por etapa', 'Frame per stage'),
           'desc': ('Frames spent in each stage. Left / Right: 30. L / R: 300.',
                    'Images par stage. Gauche / Droite : 30. L / R : 300.',
                    'Frames pro Stufe. Links / Rechts: 30. L / R: 300.',
                    'Fotogramas por etapa. Izq. / Der.: 30. L / R: 300.',
                    'Frame per stage. Sinistra / Destra: 30. L / R: 300.')},
    'mx': {'row': ('Instant menu exit', 'Sortie rapide menu', 'Sofort aus dem Menü',
                   'Salida rápida menú', 'Uscita rapida menu'),
           'desc': ('Leaving a menu skips the fade. OFF = original.',
                    'Quitter un menu saute le fondu. OFF = original.',
                    'Menü verlassen ohne Überblendung. OFF = Original.',
                    'Salir del menú sin fundido. OFF = original.',
                    'Uscire dal menu salta la dissolvenza. OFF = originale.')},
    'mb': {'row': ('Map buttons', 'Boutons carte', 'Kartentasten', 'Botones del mapa', 'Tasti mappa'),
           'desc': ('Which buttons drive the full-screen map.',
                    'Quels boutons pilotent la carte plein écran.',
                    'Welche Tasten die Vollbildkarte steuern.',
                    'Qué botones manejan el mapa completo.',
                    'Quali tasti guidano la mappa a schermo.')},
    # D5 (registry row 27, Display): the full-screen map's picture scaler. The values
    # are algorithm names, the same in every language (SCALER_VALUES below).
    'sc': {'row': ('Map scaler', 'Filtre de carte', 'Karten-Skalierer', 'Escalado del mapa',
                   'Scaler mappa'),
           'desc': ('Picture filter of the full-screen map. Next time it opens.',
                    'Filtre de la carte plein écran. À la prochaine ouverture.',
                    'Skalierung der Vollbildkarte. Beim nächsten Öffnen.',
                    'Escalado del mapa completo. Al abrirlo otra vez.',
                    'Scaler della mappa a schermo. Alla prossima apertura.')},
}
# D5: the words of the Map scaler row (the stored order: nearest, sharp, lanczos2,
# fsr1, bilinear), product names the same in every language.
SCALER_VALUES = (('nearest', 'Nearest'), ('sharp', 'Sharp bilinear'),
                 ('lanczos2', 'Lanczos2'), ('fsr1', 'FSR 1'), ('bilinear', 'Bilinear'))
# The Stage sweep values besides OFF (the Options screen's word).
SWEEP_VALUES = {
    'run': ('Run', 'Lancer', 'Starten', 'Ejecutar', 'Avvia'),
    'over': ('Start over', 'Recommencer', 'Neu beginnen', 'Reiniciar', 'Ricomincia'),
}
# Map buttons screen (Controls): the row of each of the seven bindings (map_view_core.h
# MV_ACT_* order), the Reset action, and the three message lines.
MAPBTN_ROWS = (
    ('Open the map', 'Ouvrir la carte', 'Karte öffnen', 'Abrir el mapa', 'Apri la mappa'),
    ('Zoom out', 'Zoom arrière', 'Verkleinern', 'Alejar', 'Riduci'),
    ('Zoom in', 'Zoom avant', 'Vergrößern', 'Acercar', 'Ingrandisci'),
    ('Centre on my area', 'Centrer sur ma zone', 'Auf mich zentrieren',
     'Centrar en mi zona', 'Centra sulla mia area'),
    ('Previous scaler', 'Filtre précédent', 'Voriger Skalierer', 'Escalador anterior',
     'Scaler precedente'),
    ('Next scaler', 'Filtre suivant', 'Nächster Skalierer', 'Escalador siguiente',
     'Scaler successivo'),
    ('Open from the field', 'Ouvrir du terrain', 'Aus dem Feld öffnen',
     'Abrir desde el campo', 'Apri dal campo'),
    ('Reset map buttons', 'Réinit. boutons carte', 'Tasten zurücksetzen',
     'Restablecer botones', 'Ripristina tasti mappa'),
)
MAPBTN_DESC = {
    'bind': ('Used by the full-screen map. Left / Right pick a button.',
             'Pour la carte plein écran. Gauche / Droite choisit.',
             'Für die Vollbildkarte. Links / Rechts wählt.',
             'Para el mapa completo. Izquierda / Derecha elige.',
             'Per la mappa a schermo. Sinistra / Destra sceglie.'),
    'field': ('Opens the map in the field. Left / Right pick.',
              'Ouvre la carte hors combat. Gauche / Droite choisit.',
              'Karte im Feld öffnen. Links / Rechts wählt.',
              'Mapa desde el campo. Izq. / Der. elige.',
              'Apre la mappa dal campo. Sinistra / Destra sceglie.'),
    'reset': ('Puts the seven default buttons back. Applies now.',
              'Remet les sept boutons par défaut. Effet immédiat.',
              'Setzt die sieben Standardtasten zurück. Sofort aktiv.',
              'Restablece los siete botones. Se aplica ya.',
              'Ripristina i sette tasti predefiniti. Subito attivo.'),
}
# Select (PC) / Minus (Switch): the eighth bindable button, not a PS button of the map.
MAPBTN_SELECT = {'select': 'Select', 'minus': 'Minus'}
# The Yes / No windows of Shared saves and Stage sweep (port/options/confirm.c,
# kinds DW3_DIALOG_SHARED_SAVES and DW3_DIALOG_SWEEP): a title, the question and
# one amber warning line. Each line under 650 px (the window body).
ASK_TEXT = {
    'ss': {'title': ('SHARED SAVES', 'SAUVEGARDES COMMUNES', 'GEMEINSAMER SPEICHER',
                     'PARTIDAS COMPARTIDAS', 'SALVATAGGI CONDIVISI'),
           'q': ('Share one save store between US and EU?',
                 'Partager un seul stock entre US et EU ?',
                 'Einen Speicher für US und EU teilen?',
                 '¿Compartir un almacén entre US y EU?',
                 'Condividere un archivio tra US ed EU?'),
           'warn': ('Experimental. Back up your saves first.',
                    'Expérimental. Sauvegardez vos données avant.',
                    'Experimentell. Vorher Sicherung anlegen.',
                    'Experimental. Haz una copia antes.',
                    'Sperimentale. Fai prima un backup.')},
    'sw': {'title': ('STAGE SWEEP', 'BALAYAGE DES ZONES', 'STUFEN-DURCHLAUF',
                     'BARRIDO DE ETAPAS', 'SCANSIONE STAGE'),
           'q': ('Turn on the stage sweep?', 'Activer le balayage des zones ?',
                 'Stufen-Durchlauf einschalten?', '¿Activar el barrido de etapas?',
                 'Attivare la scansione degli stage?'),
           'warn': ('The next start walks every stage by itself.',
                    'Au prochain démarrage, le jeu parcourt tout seul.',
                    'Beim nächsten Start läuft das Spiel allein durch.',
                    'Al reiniciar, el juego recorre todo solo.',
                    'Al prossimo avvio il gioco percorre tutto da solo.')},
}
for _i, _lang in enumerate(LANGS):
    for _key, _t in EXTRA_ROWS.items():
        LETTERS.append(('opt_row_x_%s_%s' % (_key, _lang), _t['row'][_i], 26, 'DejaVuSans.ttf'))
        LETTERS.append(('opt_desc_x_%s_%s' % (_key, _lang), _t['desc'][_i], 25, 'DejaVuSans.ttf'))
    for _key, _t in SWEEP_VALUES.items():
        LETTERS.append(('opt_val_sw_%s_%s' % (_key, _lang), _t[_i], 26, 'DejaVuSans.ttf'))
    for _r, _row in enumerate(MAPBTN_ROWS):
        LETTERS.append(('opt_row_mb%d_%s' % (_r, _lang), _row[_i], 26, 'DejaVuSans.ttf'))
    for _key, _t in MAPBTN_DESC.items():
        LETTERS.append(('opt_desc_mb_%s_%s' % (_key, _lang), _t[_i], 25, 'DejaVuSans.ttf'))
    for _key, _t in ASK_TEXT.items():
        LETTERS.append(('opt_dlg_ask_title_%s_%s' % (_key, _lang), _t['title'][_i], 30, 'DejaVuSans.ttf'))
        LETTERS.append(('opt_dlg_ask_q_%s_%s' % (_key, _lang), _t['q'][_i], 26, 'DejaVuSans.ttf'))
        LETTERS.append(('opt_dlg_ask_warn_%s_%s' % (_key, _lang), _t['warn'][_i], 24, 'DejaVuSans.ttf'))
for _key, _name in MAPBTN_SELECT.items():
    LETTERS.append(('opt_val_btn_' + _key, _name, 26, 'DejaVuSans.ttf'))
for _key, _name in SCALER_VALUES:
    LETTERS.append(('opt_val_sc_' + _key, _name, 26, 'DejaVuSans.ttf'))
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
# P6.38 restart into the chosen region (docs/PORT_OPTIONS.md "Restart into the chosen
# region"): the same window with "Restart in EU now?" (US and EU are product names, the
# same in every language), its title, and the one-button notice for a host that cannot
# relaunch. The progress warning line is DIALOG_TEXT's `warn`. Keep each line under 650 px.
RESTART_TEXT = {
    'en': {'title': 'RESTART', 'q': 'Restart in %s now?', 'note': 'Restart the game to apply.'},
    'fr': {'title': 'RED\u00c9MARRER', 'q': 'Red\u00e9marrer en %s maintenant ?',
           'note': 'Red\u00e9marrez le jeu pour appliquer.'},
    'de': {'title': 'NEUSTART', 'q': 'Jetzt in %s neu starten?',
           'note': 'Spiel neu starten, um es zu \u00fcbernehmen.'},
    'es': {'title': 'REINICIAR', 'q': '\u00bfReiniciar en %s ahora?',
           'note': 'Reinicia el juego para aplicar.'},
    'it': {'title': 'RIAVVIA', 'q': 'Riavviare in %s ora?',
           'note': 'Riavvia il gioco per applicare.'},
}
for _lang, _t in RESTART_TEXT.items():
    LETTERS.append(('opt_dlg_rst_title_' + _lang, _t['title'], 30, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_dlg_rst_q_us_' + _lang, _t['q'] % 'US', 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_dlg_rst_q_eu_' + _lang, _t['q'] % 'EU', 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_dlg_rst_note_' + _lang, _t['note'], 26, 'DejaVuSans.ttf'))
LETTERS.append(('opt_dlg_ok', 'OK', 26, 'DejaVuSans.ttf'))
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
# P9.8 Button layout row of the Controls group (docs/CONTROLS_BUNDLE.md), in the
# five languages of the EU disc (en is also the US disc's). The two values are
# product names, the same in every language. Keep the label under 290 px, the
# values under 236 px and the description under 690 px.
LAYOUT_TEXT = {
    'en': {'row': 'Button layout',
           'desc': 'Nintendo: A confirms, B cancels. Applies now.'},
    'fr': {'row': 'Disposition boutons',
           'desc': 'Nintendo : A valide, B annule. Effet immédiat.'},
    'de': {'row': 'Tastenbelegung',
           'desc': 'Nintendo: A bestätigt, B bricht ab. Sofort aktiv.'},
    'es': {'row': 'Botones',
           'desc': 'Nintendo: A confirma, B cancela. Se aplica ya.'},
    'it': {'row': 'Disposizione tasti',
           'desc': 'Nintendo: A conferma, B annulla. Subito attivo.'},
}
for _lang, _t in LAYOUT_TEXT.items():
    LETTERS.append(('opt_row_bl_' + _lang, _t['row'], 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_val_bl_ps_' + _lang, 'PlayStation', 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_val_bl_nin_' + _lang, 'Nintendo', 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_desc_bl_' + _lang, _t['desc'], 25, 'DejaVuSans.ttf'))
# P6.36 Turbo music row of the Sound group (docs/P6.36_TURBO_MUSIC.md), in the
# five languages of the EU disc (en is also the US disc's). The values are the
# Options screen's "1x" and "Fast" words. Label under 290 px, description under
# 690 px.
TURBO_MUSIC_TEXT = {
    'en': {'row': 'Turbo music',
           'desc': 'Music keeps its speed in turbo. Fast = as before.'},
    'fr': {'row': 'Musique turbo',
           'desc': 'Musique normale en turbo. Rapide = comme avant.'},
    'de': {'row': 'Turbo-Musik',
           'desc': 'Musik bleibt im Turbo normal. Schnell = wie bisher.'},
    'es': {'row': 'Música turbo',
           'desc': 'Música normal en turbo. Rápido = como antes.'},
    'it': {'row': 'Musica turbo',
           'desc': 'Musica normale in turbo. Veloce = come prima.'},
}
for _lang, _t in TURBO_MUSIC_TEXT.items():
    LETTERS.append(('opt_row_tm_' + _lang, _t['row'], 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_desc_tm_' + _lang, _t['desc'], 25, 'DejaVuSans.ttf'))
# P10.6 Switch glyphs row of the Controls group (docs/P10.6_GLYPHS.md), Switch
# only, in the five languages of the EU disc (en is also the US disc's). The
# values are the Options screen's OFF / ON words. Label under 290 px,
# description under 690 px. FR / DE / IT / ES are machine-drafted, needs native
# review.
SWITCH_GLYPHS_TEXT = {
    'en': {'row': 'Switch glyphs',
           'desc': 'Switch buttons in game text. OFF = original icons.'},
    'fr': {'row': 'Symboles Switch',
           'desc': "Boutons Switch dans les textes. OFF = d'origine."},
    'de': {'row': 'Switch-Symbole',
           'desc': 'Switch-Tasten im Spieltext. OFF = Originalsymbole.'},
    'es': {'row': 'Símbolos Switch',
           'desc': 'Botones Switch en el texto. OFF = iconos originales.'},
    'it': {'row': 'Simboli Switch',
           'desc': 'Pulsanti Switch nei testi di gioco. OFF = icone originali.'},
}
for _lang, _t in SWITCH_GLYPHS_TEXT.items():
    LETTERS.append(('opt_row_sg_' + _lang, _t['row'], 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_desc_sg_' + _lang, _t['desc'], 25, 'DejaVuSans.ttf'))
# P6.32 phase 4 Full-screen map row of the Display group (docs/P6.32_MAP_ZOOM.md,
# section 16), in the five languages of the EU disc (en is also the US disc's). The
# values are the Options screen's OFF / ON words. Label under 290 px, description
# under 690 px. FR / DE / IT / ES are machine-drafted, needs native review.
MAP_SCREEN_TEXT = {
    'en': {'row': 'Full-screen map',
           'desc': 'Opens a full-screen map from the Map screen.'},
    'fr': {'row': 'Carte plein écran',
           'desc': 'Ouvre une carte plein écran depuis la Carte.'},
    'de': {'row': 'Vollbildkarte',
           'desc': 'Öffnet eine Vollbildkarte aus der Karte.'},
    'es': {'row': 'Mapa completo',
           'desc': 'Abre un mapa a pantalla completa desde el Mapa.'},
    'it': {'row': 'Mappa a schermo',
           'desc': 'Apre una mappa a schermo intero dalla Mappa.'},
}
for _lang, _t in MAP_SCREEN_TEXT.items():
    LETTERS.append(('opt_row_ms_' + _lang, _t['row'], 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_desc_ms_' + _lang, _t['desc'], 25, 'DejaVuSans.ttf'))
# P6.12 Controls group, phase 2 (docs/CONTROLS_BUNDLE.md): the Customize buttons
# and Reset to default rows, the Custom value of Button layout, the prompt of the
# Customize screen. FR / DE / IT / ES are machine-drafted, needs native review.
# The PlayStation button names and the Switch button names are the same in every
# language. Keep labels under 290 px, values under 236 px, descriptions under 690.
CUSTOM_TEXT = {
    'en': {'row': 'Customize buttons', 'rowdesc': 'Pick which button makes each PlayStation button.',
           'reset': 'Reset to default', 'resetdesc': 'Back to the Nintendo layout. Applies now.',
           'prompt': 'Press a button', 'promptdesc': 'Press the new button. Start or Select cancels.',
           'custom': 'Custom'},
    'fr': {'row': 'Personnaliser', 'rowdesc': 'Choisir le bouton de chaque touche PlayStation.',
           'reset': 'Réinitialiser', 'resetdesc': 'Retour à la disposition Nintendo. Effet immédiat.',
           'prompt': 'Appuyez', 'promptdesc': 'Appuyez sur le nouveau bouton. Start/Select annule.',
           'custom': 'Perso'},
    'de': {'row': 'Tasten anpassen', 'rowdesc': 'Wähle, welche Taste jede PlayStation-Taste auslöst.',
           'reset': 'Zurücksetzen', 'resetdesc': 'Zurück zur Nintendo-Belegung. Sofort aktiv.',
           'prompt': 'Taste drücken', 'promptdesc': 'Neue Taste drücken. Start oder Select bricht ab.',
           'custom': 'Eigene'},
    'es': {'row': 'Personalizar botones', 'rowdesc': 'Elige qué botón hace cada botón de PlayStation.',
           'reset': 'Restablecer', 'resetdesc': 'Vuelve a la disposición Nintendo. Se aplica ya.',
           'prompt': 'Pulsa un botón', 'promptdesc': 'Pulsa el botón nuevo. Start o Select cancela.',
           'custom': 'Otra'},
    'it': {'row': 'Personalizza tasti', 'rowdesc': 'Scegli quale tasto fa ogni tasto PlayStation.',
           'reset': 'Ripristina', 'resetdesc': 'Torna alla disposizione Nintendo. Subito attivo.',
           'prompt': 'Premi un tasto', 'promptdesc': 'Premi il nuovo tasto. Start o Select annulla.',
           'custom': 'Personale'},
}
for _lang, _t in CUSTOM_TEXT.items():
    LETTERS.append(('opt_row_cz_' + _lang, _t['row'], 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_desc_cz_' + _lang, _t['rowdesc'], 25, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_row_rs_' + _lang, _t['reset'], 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_desc_rs_' + _lang, _t['resetdesc'], 25, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_val_cp_' + _lang, _t['prompt'], 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_desc_cp_' + _lang, _t['promptdesc'], 25, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_val_bl_cus_' + _lang, _t['custom'], 26, 'DejaVuSans.ttf'))
# The rows of the Customize screen: the ten PS buttons (map order) and the Switch
# button that can make each one (port/options/button_map.c, g_src_label).
for _name in ('Cross', 'Circle', 'Triangle', 'Square', 'L1', 'R1', 'L2', 'R2', 'L3', 'R3'):
    LETTERS.append(('opt_row_ps_' + _name.lower(), _name, 26, 'DejaVuSans.ttf'))
for _key, _name in (('b', 'B'), ('a', 'A'), ('x', 'X'), ('y', 'Y'), ('l', 'L'), ('r', 'R'),
                    ('zl', 'ZL'), ('zr', 'ZR'), ('lstick', 'L stick'), ('rstick', 'R stick')):
    LETTERS.append(('opt_val_btn_' + _key, _name, 26, 'DejaVuSans.ttf'))
# P51 item 4: the Region at start row of the Gameplay group, shown only in the
# combined build (docs/P51_FEEDBACK.md). Values: Ask (per language), US and EU
# (the same in every language). Label under 290 px, description under 690 px.
REGION_TEXT = {
    'en': {'row': 'Region at start', 'ask': 'Ask',
           'desc': 'Used at the next start. Ask = choose US or EU.'},
    'fr': {'row': 'R\u00e9gion au d\u00e9marrage', 'ask': 'Demander',
           'desc': 'Prochain d\u00e9marrage. Demander = choisir US ou EU.'},
    'de': {'row': 'Region beim Start', 'ask': 'Fragen',
           'desc': 'Beim n\u00e4chsten Start. Fragen = US oder EU w\u00e4hlen.'},
    'es': {'row': 'Regi\u00f3n al iniciar', 'ask': 'Preguntar',
           'desc': 'En el pr\u00f3ximo inicio. Preguntar = elegir US o EU.'},
    'it': {'row': 'Regione all\'avvio', 'ask': 'Chiedere',
           'desc': 'Al prossimo avvio. Chiedere = scegliere US o EU.'},
}
for _lang, _t in REGION_TEXT.items():
    LETTERS.append(('opt_row_rg_' + _lang, _t['row'], 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_val_rg_ask_' + _lang, _t['ask'], 26, 'DejaVuSans.ttf'))
    LETTERS.append(('opt_desc_rg_' + _lang, _t['desc'], 25, 'DejaVuSans.ttf'))
LETTERS.append(('opt_val_rg_us', 'US', 26, 'DejaVuSans.ttf'))
LETTERS.append(('opt_val_rg_eu', 'EU', 26, 'DejaVuSans.ttf'))
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
CHEAT_LETTERS = []
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
    limit = (LIMITS['dlg'] if name.startswith(('opt_dlg_ask_q', 'opt_dlg_ask_warn')) else
             LIMITS['grp'] if name.startswith('opt_grp_') else LIMITS['row'] if name.startswith(('opt_row', 'opt_cht_row')) else
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
