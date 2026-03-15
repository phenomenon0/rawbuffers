EMCC    = emcc
CFLAGS  = -O2 -Wall -Wextra
SHELL_  = shell.html
EXPORTS = -s EXPORTED_FUNCTIONS='["_main","_rb__web_audio_fill"]' \
          -s EXPORTED_RUNTIME_METHODS='["ccall"]'

all: plasma.html piano.html

plasma.html: plasma.c ../rawbuf.h $(SHELL_)
	$(EMCC) $(CFLAGS) -o $@ $< --shell-file $(SHELL_) $(EXPORTS)

piano.html: piano.c ../rawbuf.h $(SHELL_)
	$(EMCC) $(CFLAGS) -o $@ $< --shell-file $(SHELL_) $(EXPORTS)

serve: all
	python3 -m http.server 8080

clean:
	rm -f plasma.html plasma.js plasma.wasm
	rm -f piano.html piano.js piano.wasm

.PHONY: all clean serve
