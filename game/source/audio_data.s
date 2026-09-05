	.section .rodata.ln_audio, "a", %progbits

	.equ LN_AUDIO_WAV_HEADER_BYTES, 44
	.equ LN_AUDIO_SAFE_PADDING_BYTES, 512

	.balign 4, 0x80
	.global ln_audio_ring_subscriber_data
	.type ln_audio_ring_subscriber_data, %object
ln_audio_ring_subscriber_data:
	.incbin "assets/audio/ring_subscriber.wav", LN_AUDIO_WAV_HEADER_BYTES, 13671
	.global ln_audio_ring_subscriber_data_end
ln_audio_ring_subscriber_data_end:
	.size ln_audio_ring_subscriber_data, ln_audio_ring_subscriber_data_end - ln_audio_ring_subscriber_data
	.space LN_AUDIO_SAFE_PADDING_BYTES, 0x80

	.balign 4, 0x80
	.global ln_audio_plug_click_data
	.type ln_audio_plug_click_data, %object
ln_audio_plug_click_data:
	.incbin "assets/audio/plug_click.wav", LN_AUDIO_WAV_HEADER_BYTES, 2315
	.global ln_audio_plug_click_data_end
ln_audio_plug_click_data_end:
	.size ln_audio_plug_click_data, ln_audio_plug_click_data_end - ln_audio_plug_click_data
	.space LN_AUDIO_SAFE_PADDING_BYTES, 0x80

	.balign 4, 0x80
	.global ln_audio_cord_move_data
	.type ln_audio_cord_move_data, %object
ln_audio_cord_move_data:
	.incbin "assets/audio/cord_move.wav", LN_AUDIO_WAV_HEADER_BYTES, 5292
	.global ln_audio_cord_move_data_end
ln_audio_cord_move_data_end:
	.size ln_audio_cord_move_data, ln_audio_cord_move_data_end - ln_audio_cord_move_data
	.space LN_AUDIO_SAFE_PADDING_BYTES, 0x80

	.balign 4, 0x80
	.global ln_audio_crosstalk_data
	.type ln_audio_crosstalk_data, %object
ln_audio_crosstalk_data:
	.incbin "assets/audio/crosstalk.wav", LN_AUDIO_WAV_HEADER_BYTES, 8379
	.global ln_audio_crosstalk_data_end
ln_audio_crosstalk_data_end:
	.size ln_audio_crosstalk_data, ln_audio_crosstalk_data_end - ln_audio_crosstalk_data
	.space LN_AUDIO_SAFE_PADDING_BYTES, 0x80

	.balign 4, 0x80
	.global ln_audio_packet_transfer_data
	.type ln_audio_packet_transfer_data, %object
ln_audio_packet_transfer_data:
	.incbin "assets/audio/packet_transfer.wav", LN_AUDIO_WAV_HEADER_BYTES, 5292
	.global ln_audio_packet_transfer_data_end
ln_audio_packet_transfer_data_end:
	.size ln_audio_packet_transfer_data, ln_audio_packet_transfer_data_end - ln_audio_packet_transfer_data
	.space LN_AUDIO_SAFE_PADDING_BYTES, 0x80

	.balign 4, 0x80
	.global ln_audio_ground_clamp_data
	.type ln_audio_ground_clamp_data, %object
ln_audio_ground_clamp_data:
	.incbin "assets/audio/ground_clamp.wav", LN_AUDIO_WAV_HEADER_BYTES, 6615
	.global ln_audio_ground_clamp_data_end
ln_audio_ground_clamp_data_end:
	.size ln_audio_ground_clamp_data, ln_audio_ground_clamp_data_end - ln_audio_ground_clamp_data
	.space LN_AUDIO_SAFE_PADDING_BYTES, 0x80

	.balign 4, 0x80
	.global ln_audio_verify_data
	.type ln_audio_verify_data, %object
ln_audio_verify_data:
	.incbin "assets/audio/verify.wav", LN_AUDIO_WAV_HEADER_BYTES, 7276
	.global ln_audio_verify_data_end
ln_audio_verify_data_end:
	.size ln_audio_verify_data, ln_audio_verify_data_end - ln_audio_verify_data
	.space LN_AUDIO_SAFE_PADDING_BYTES, 0x80

	.balign 4, 0x80
	.global ln_audio_dawn_chime_data
	.type ln_audio_dawn_chime_data, %object
ln_audio_dawn_chime_data:
	.incbin "assets/audio/dawn_chime.wav", LN_AUDIO_WAV_HEADER_BYTES, 23152
	.global ln_audio_dawn_chime_data_end
ln_audio_dawn_chime_data_end:
	.size ln_audio_dawn_chime_data, ln_audio_dawn_chime_data_end - ln_audio_dawn_chime_data
	.space LN_AUDIO_SAFE_PADDING_BYTES, 0x80

	.balign 4, 0x80
	.section .note.GNU-stack, "", %progbits
