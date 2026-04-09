EMCC    = emcc
CFLAGS  = -O2 -Wall -Wextra

VIZVIM_EXPORTS = -s EXPORTED_FUNCTIONS='["_vz_init","_vz_reinit","_vz_get_buffer","_vz_apply_effect","_vz_present", \
    "_vz_width","_vz_height","_vz_draw_text","_vz_draw_rect", \
    "_vz_set_chroma","_vz_set_channel_split", \
    "_vz_set_audio_data","_vz_get_waveform_buffer", \
    "_vz_get_mask_buffer","_vz_set_ml_bg_color", \
    "_vz_set_det_box","_vz_set_det_count", \
    "_vz_set_pose","_vz_set_pose_valid", \
    "_vz_set_face","_vz_set_face_landmarks","_vz_set_face_valid", \
    "_vz_set_face_landmark","_vz_set_face_landmark_count", \
    "_vz_get_depth_buffer", \
    "_vz_get_pip_buffer","_vz_set_pip_position","_vz_set_pip_opacity", \
    "_vz_get_annotation_buffer","_vz_annotation_clear","_vz_annotation_line", \
    "_vz_set_subtitle", \
    "_vz_set_thermal","_vz_set_edge","_vz_set_nightvision", \
    "_vz_set_vhs","_vz_set_glitch","_vz_set_pixelate", \
    "_vz_set_old_film","_vz_set_fft_gains","_vz_set_waveform", \
    "_vz_set_pose_colors","_vz_set_face_params","_vz_set_fog", \
    "_vz_set_annot_brush","_vz_set_subtitle_scale", \
    "_vz_save_buffer","_vz_blend_buffer","_vz_set_blend_mode", \
    "_vz_set_bg_mode","_vz_set_bg_blur","_vz_get_bg_image_buffer", \
    "_vz_set_hair_color","_vz_set_outline_color","_vz_set_outline_thickness", \
    "_vz_set_tiltshift","_vz_set_subtitle_decay","_vz_set_hair_intensity", \
    "_vz_get_face_mask_buffer", \
    "_vz_set_brightness","_vz_set_contrast","_vz_set_saturation", \
    "_vz_set_hue_rotate","_vz_flip_h","_vz_flip_v", \
    "_vz_apply_adjustments", \
    "_malloc","_free"]' \
                 -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","stringToUTF8","HEAPU8","HEAPU32","HEAPF32"]' \
                 -s MODULARIZE=1 -s EXPORT_NAME='Vizvim' -s ALLOW_MEMORY_GROWTH=1

all: vizvim.js

vizvim.js: vizvim.c ../../rawbuf.h
	$(EMCC) $(CFLAGS) -o $@ $< $(VIZVIM_EXPORTS)

serve: all
	python3 -m http.server 8080 --directory ..

clean:
	rm -f vizvim.js vizvim.wasm

.PHONY: all clean serve
