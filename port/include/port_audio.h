/* P6.17 music / SFX volume, shared by the PC and Switch game hosts.
 * Design and limits: docs/P6.17_VOLUME.md. */
#ifndef DW3_PORT_AUDIO_H
#define DW3_PORT_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* The Port options rows hold 0..10 steps; 10 is the original mix. */
#define DW3_AUDIO_VOLUME_MAX 10

/* Q15 gain (0x8000 = 1.0) of a volume step. gain = step^2 / 100, a power
 * curve that is close to equal loudness per step; 10 maps to exactly 0x8000
 * (the mix is left bit-identical) and 0 to silence. Out-of-range steps are
 * clamped. Pure, in port/audio/volume.c. */
int dw3_port_audio_gain_q15(int step);

/* The game's SEP track numbers run past the 16 tracks of one SEQ: SoundState
 * opens the system bank's two SEP files as access numbers 0 and 1 and plays the
 * second file's tracks as (0, 16..31) (the win jingle is 30). libsnd stores
 * the table in rows of 16, so (seq, sep) with sep >= 16 is the entry
 * (seq + sep / 16, sep % 16), and that is the pair the ticker plays and the
 * pair a voice carries in seq_sep_no. Pure, in port/audio/volume.c. */
#define DW3_AUDIO_TRACKS_PER_SEQ 16
void dw3_port_audio_track_slot(int seq_access_num, int sep, int *slot_seq,
                               int *slot_sep);

/* Apply the two steps to the SPU mixer (port/audio/audio_bus.c): sound
 * effects on bus 0, music on bus 1, and the CD/XA input (movie audio)
 * follows the music step. */
void dw3_port_audio_apply(int music_step, int sfx_step);

/* From playSound (patch 0060): SEQ access number and track (SEP) that are
 * about to play, and whether they are music (the exclusive flag of the sound
 * id). Routes the track's voices to the music bus or the SFX bus. */
void dw3_port_audio_note_sequence(int seq_access_num, int sep, int music);

/* Once per frame (patch 0060): the audio health counters and their line
 * every 60 s, and the optional bus statistics log (DW3_AUDIO_STATS=<frames>). */
void dw3_port_audio_poll(void);

/* From playSound (patch 0178): the result of SsUtKeyOn (a voice number, -1 =
 * no free voice). Counts the key-ons for the audio health line and warns when
 * voices ran out (#188). */
void dw3_port_audio_note_keyon(int voice);

/* The run-total audio health line; also runs at exit. Safe to call twice. */
void dw3_port_audio_health_exit(void);

/* P6.36 (docs/P6.36_TURBO_MUSIC.md, port/audio/turbo_music.c): music at normal
 * speed while turbo is on. vsyncCallback (patch 0191) calls the sequencer tick
 * this many times in a game frame: always 1 with the Turbo music row on Fast
 * or with turbo off (the original, bit for bit), otherwise 0, 1 or 2 so that
 * the music gets the ticks of a 1x game. Game thread only. */
int dw3_port_audio_ticks_due(void);

/* fadeOutSound (patch 0191): the tick count of a music fade. Unchanged unless
 * the gate is on, then scaled by the game rate so the fade spans the same game
 * time as at 1x (60 ticks -> 15 at 4x). */
int dw3_port_audio_fade_ticks(int ticks);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_AUDIO_H */
