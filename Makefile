CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -Wpedantic
LDFLAGS = -lm -lpthread

DEMOS = plasma bounce life raymarch_sdf audio_visual ringtest piano visualizer sprite player clickgame

all: $(DEMOS)

plasma: demos/plasma.c rawbuf.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

bounce: demos/bounce.c rawbuf.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

life: demos/life.c rawbuf.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

raymarch_sdf: demos/raymarch_sdf.c rawbuf.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

audio_visual: demos/audio_visual.c rawbuf.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

ringtest: demos/ringtest.c rawbuf.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

piano: demos/piano.c rawbuf.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

visualizer: demos/visualizer.c rawbuf.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

sprite: demos/sprite.c rawbuf.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

player: demos/player.c rawbuf.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clickgame: demos/clickgame.c rawbuf.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(DEMOS)

.PHONY: all clean
