/*	$Csoft: engine.c,v 1.15 2002/02/18 03:55:20 vedge Exp $	*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <engine/engine.h>
#include <engine/mapedit/mapedit.h>

#ifdef DEBUG
int	engine_debug = 1;
#endif
struct	world *world;

static char *mapdesc = NULL, *mapstr = NULL;
static int mapw = 64, maph = 64;
static int tilew = 32, tileh = 32;
static int mapediting;

static SDL_Joystick *joy = NULL;

static void printusage(char *);

static void
printusage(char *progname)
{
	fprintf(stderr,
	    "Usage: %s [-vxf] [-w width] [-h height] [-d depth] [-j joy#]\n",
	    progname);
	fprintf(stderr,
	    "                 [-e mapname] [-D mapdesc] [-W mapw] [-H maph]\n");
	fprintf(stderr,
	    "                 [-W mapw] [-H maph] [-X tilew] [-Y tileh]\n");
}

int
engine_init(int argc, char *argv[], struct gameinfo *gameinfo, char *path)
{
	int c, w, h, depth, njoy, flags;
	extern int xcf_debug;

	curmapedit = NULL;
	curchar = NULL;

	njoy = 0;
	mapediting = 0;
	w = 640;
	h = 480;
	depth = 32;
	flags = SDL_SWSURFACE;

	/* XXX ridiculous */
	while ((c = getopt(argc, argv, "xvfl:n:w:h:d:j:e:D:W:H:X:Y:")) != -1) {
		switch (c) {
		case 'x':
			xcf_debug++;
			break;
		case 'v':
			printf("AGAR engine v%s\n", ENGINE_VERSION);
			printf("%s v%d.%d\n", gameinfo->name,
			    gameinfo->ver[0], gameinfo->ver[1]);
			printf("%s\n", gameinfo->copyright);
			exit (255);
		case 'f':
			flags |= SDL_FULLSCREEN;
			break;
		case 'w':
			w = atoi(optarg);
			break;
		case 'h':
			h = atoi(optarg);
			break;
		case 'd':
			depth = atoi(optarg);
			break;
		case 'j':
			njoy = atoi(optarg);
			break;
		case 'e':
			mapediting++;
			mapstr = optarg;
			break;
		case 'D':
			mapdesc = optarg;
			break;
		case 'W':
			mapw = atoi(optarg);
			break;
		case 'H':
			maph = atoi(optarg);
			break;
		case 'X':
			tilew = atoi(optarg);
			break;
		case 'Y':
			tileh = atoi(optarg);
			break;
		default:
			printusage(argv[0]);
			exit(255);
		}
	}

	/* Initialize SDL. */
	if (SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_AUDIO|
	    SDL_INIT_EVENTTHREAD|SDL_INIT_NOPARACHUTE) != 0) {
		fatal("SDL_Init: %s\n", SDL_GetError());
		return (-1);
	}
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
		njoy = -1;
	}
#if 0
	if (njoy >= 0) {
		joy = SDL_JoystickOpen(njoy);
		if (joy != NULL) {
			dprintf("using joystick #%d: %s\n",
			    njoy, SDL_JoystickName(njoy));
			SDL_JoystickEventState(SDL_ENABLE);
		} else if (njoy > 0) {
			warning("no joystick at #%d\n", njoy);
		}
	}
#endif
	if (flags & SDL_FULLSCREEN) {
		/* Give the new mode some time to take effect. */
		SDL_Delay(1000);
	}
	
	/*
	 * Create the main viewport. The video mode will be set
	 * as soon as a map is loaded.
	 */
	mainview = view_create(w, h, depth, flags);
	if (mainview == NULL) {
		fatal("view_create\n");
		return (-1);
	}

	/* Initialize the world structure. */
	world = world_create(gameinfo->prog);
	if (world == NULL) {
		fatal("world_create\n");
		return (-1);
	}

	return (0);
}

void
engine_start(void)
{
	if (mapediting) {
		struct mapedit *medit;

		medit = mapedit_create("mapedit");
		if (medit == NULL) {
			return;
		}
		/* Set the map edition arguments. */
		medit->margs.name = strdup(mapstr);
		medit->margs.desc = (mapdesc != NULL) ? strdup(mapdesc) : "";
		medit->margs.mapw = mapw;
		medit->margs.maph = maph;
		medit->margs.tilew = tilew;
		medit->margs.tileh = tileh;

		/* Start map edition. */
		object_link(medit);
	}
	event_loop();
}

void
engine_destroy(void)
{
	object_destroy(world);

	if (joy != NULL) {
		SDL_JoystickClose(joy);
	}

	SDL_Quit();
	exit(0);
}

void *
emalloc(size_t len)
{
	void *p;

	p = malloc(len);
	if (p == NULL) {
		perror("malloc");
		engine_destroy();
	}
	return (p);
}

