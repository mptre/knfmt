/*
 * Long expression, honor optional line.
 */

int
main(void)
{
	if (nest) {
		synth_chan_pc(syn, i,
		    (i == MIDI_DRUMCHAN || i == MIDI_DRUMCHAN + 1) ?
		    PATCH_ISDRUM : 0);
	}
}
