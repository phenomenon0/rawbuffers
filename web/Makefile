EMCC    = emcc
CFLAGS  = -O2 -Wall -Wextra
SHELL_  = shell.html
EXPORTS = -s EXPORTED_FUNCTIONS='["_main","_rb__web_audio_fill"]' \
          -s EXPORTED_RUNTIME_METHODS='["ccall"]'

EDITOR_EXPORTS = -s EXPORTED_FUNCTIONS='["_main","_malloc","_free", \
    "_editor_init","_editor_reinit","_editor_get_buffer","_editor_apply_effect","_editor_present", \
    "_editor_width","_editor_height","_editor_draw_text","_editor_draw_rect", \
    "_editor_set_chroma","_editor_set_channel_split", \
    "_editor_set_audio_data","_editor_get_waveform_buffer", \
    "_editor_get_mask_buffer","_editor_set_ml_bg_color", \
    "_editor_set_det_box","_editor_set_det_count", \
    "_editor_set_pose","_editor_set_pose_valid", \
    "_editor_set_face","_editor_set_face_landmarks","_editor_set_face_valid", \
    "_editor_set_face_landmark","_editor_set_face_landmark_count", \
    "_editor_get_depth_buffer", \
    "_editor_get_pip_buffer", \
    "_editor_set_pip_position","_editor_set_pip_opacity", \
    "_editor_get_annotation_buffer","_editor_annotation_clear","_editor_annotation_line", \
    "_editor_set_subtitle", \
    "_editor_set_thermal","_editor_set_edge","_editor_set_nightvision", \
    "_editor_set_vhs","_editor_set_glitch","_editor_set_pixelate", \
    "_editor_set_old_film","_editor_set_fft_gains","_editor_set_waveform", \
    "_editor_set_pose_colors","_editor_set_face_params","_editor_set_fog", \
    "_editor_set_annot_brush","_editor_set_subtitle_scale", \
    "_editor_save_buffer","_editor_blend_buffer","_editor_set_blend_mode", \
    "_editor_set_bg_mode","_editor_set_bg_blur","_editor_get_bg_image_buffer", \
    "_editor_set_hair_color","_editor_set_outline_color","_editor_set_outline_thickness", \
    "_editor_set_tiltshift", \
    "_editor_set_subtitle_decay", \
    "_editor_set_hair_intensity", \
    "_editor_get_face_mask_buffer", \
    "_editor_set_brightness","_editor_set_contrast","_editor_set_saturation", \
    "_editor_set_hue_rotate","_editor_flip_h","_editor_flip_v", \
    "_editor_apply_adjustments"]' \
                 -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","stringToUTF8","HEAPU8","HEAPU32","HEAPF32"]' \
                 -s MODULARIZE=1 -s EXPORT_NAME='RawBuf' -s ALLOW_MEMORY_GROWTH=1

all: plasma.html piano.html editor.js

plasma.html: plasma.c ../rawbuf.h $(SHELL_)
	$(EMCC) $(CFLAGS) -o $@ $< --shell-file $(SHELL_) $(EXPORTS)

piano.html: piano.c ../rawbuf.h $(SHELL_)
	$(EMCC) $(CFLAGS) -o $@ $< --shell-file $(SHELL_) $(EXPORTS)

editor.js: editor.c ../rawbuf.h
	$(EMCC) $(CFLAGS) -o $@ $< $(EDITOR_EXPORTS)

serve: all
	python3 -m http.server 8080

clean:
	rm -f plasma.html plasma.js plasma.wasm
	rm -f piano.html piano.js piano.wasm
	rm -f editor.js editor.wasm

.PHONY: all clean serve
