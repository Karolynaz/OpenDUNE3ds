/* clang -std=gnu11 -fsanitize=address,undefined -I scripts/test_stubs \
 *   -iquote include -iquote src scripts/test_3ds_midi.c -o /tmp/dune-midi-test */
#include <assert.h>
#include <stdio.h>
#include "../src/audio/midi_3ds.c"
int main(void)
{
	for (int fail=0;fail<AUDIO_BUFFER_COUNT;fail++) {
		test_alloc_fail_after=fail;
		assert(!midi_init());
		assert(test_alloc_count==0 && test_ndsp_refs==0);
	}
	test_alloc_fail_after=-1;
	assert(midi_init());
	/* The organ's doubled phase used to index past the 512-entry sine table. */
	midi_send(0xC0 | (16u<<8));
	midi_send(0x90 | (69u<<8) | (100u<<16));
	int16 audio[BUFFER_SAMPLES*2];
	for (int n=0;n<100;n++) midi_3ds_render_buffer(audio,BUFFER_SAMPLES);
	assert(s_voices[0].active);
	midi_send(0x80 | (69u<<8));
	for (int n=0;n<100;n++) midi_3ds_render_buffer(audio,BUFFER_SAMPLES);
	assert(!s_voices[0].active);
	/* NDSP refill must reuse completed buffers without adding queued ones again. */
	for (int b=0;b<AUDIO_BUFFER_COUNT;b++) assert(s_waveBuf[b].status==NDSP_WBUF_QUEUED);
	s_waveBuf[1].status=NDSP_WBUF_DONE;
	midi_3ds_pump(); assert(s_waveBuf[1].status==NDSP_WBUF_QUEUED);
	assert(BUFFER_SAMPLES*AUDIO_BUFFER_COUNT*1000/SAMPLE_RATE<50);
	/* Pedal release must release a note stopped during its attack/decay,
	 * while leaving a key still held by the sequencer sounding. */
	midi_send(0xC0 | (48u << 8));
	midi_send(0xB0 | (64u << 8) | (127u << 16));
	midi_send(0x90 | (60u << 8) | (100u << 16));
	midi_send(0x90 | (64u << 8) | (100u << 16));
	midi_send(0x80 | (60u << 8));
	assert(s_voices[0].env_stage == ENV_ATTACK);
	midi_send(0xB0 | (64u << 8));
	assert(s_voices[0].env_stage == ENV_RELEASE);
	assert(s_voices[1].keyHeld && s_voices[1].env_stage == ENV_ATTACK);
	midi_send(0x90 | (64u << 8)); /* velocity zero is also note off */
	assert(!s_voices[1].keyHeld && s_voices[1].env_stage == ENV_RELEASE);
	midi_uninit();
	assert(test_alloc_count==0 && test_ndsp_refs==0);
	puts("3DS MIDI synthesis checks passed");
	return 0;
}
