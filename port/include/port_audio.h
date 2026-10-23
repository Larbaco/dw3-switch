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

/* Once per frame (patch 0060): the optional bus statistics log
 * (DW3_AUDIO_STATS=<frames>); does nothing otherwise. */
void dw3_port_audio_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_AUDIO_H */
