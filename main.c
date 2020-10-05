// Spacelink
// Copyright (c) Paolo Mazzon 2020
// 
// Original concept for Spacelink as discussed in a chat room:
//     1. launch shit into orbit at a given speed and angle
//     2. they continue to orbit until they crash into earth or other shit u launched
//     3. the longer you take to launch it the less money you make on the launch and money is ur score but a satellite down is hefty cost and 3 down = lose
//
// TODO:
//   1. Alien dude that can be shot down and will steal satellites
//   2. Menu music
#define SDL_MAIN_HANDLED
#define CUTE_SOUND_IMPLEMENTATION
#include "VK2D/VK2D.h"
#include "VK2D/Image.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include "cute_sound.h"
#include <SDL2/SDL_syswm.h>
#include <math.h>
#include <malloc.h>

/******************** Types ********************/
typedef double Dosh;
typedef enum {GameState_Menu, GameState_Game} GameState;
typedef enum {Status_Game, Status_Quit, Status_Menu, Status_Restart} Status;

/******************** Constants ********************/
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 800;
const int GAME_WIDTH = 400;
const int GAME_HEIGHT = 400;
const uint32_t FONT_RANGE = 255;
const uint32_t DEFAULT_LIST_EXTENSION = 10;
const Dosh MONEY_LOSS_PER_SECOND = 300;
const Dosh MINIMUM_PAYOUT = 500;
const float PLANET_MINIMUM_RADIUS = 30;
const float PLANET_MAXIMUM_RADIUS = 50;
const float PLANET_MINIMUM_GRAVITY = 0.05;
const float PLANET_MAXIMUM_GRAVITY = 0.10;
const int GAME_OVER_SATELLITE_COUNT = 3; // How many satellites must crash before game over
const float STANDBY_COOLDOWN = 3; // In seconds
const float MAXIMUM_SATELLITE_VELOCITY = 5;
const float MINIMUM_SATELLITE_VELOCITY = 1;
const float MAXIMUM_SATELLITE_RADIUS = 4;
const float MINIMUM_SATELLITE_RADIUS = 1;
const float MAXIMUM_SATELLITE_DOSH = 4000;
const float MINIMUM_SATELLITE_DOSH = 1500;
const float MAXIMUM_SATELLITE_ANGLE = (VK2D_PI * 3) / 2;
const float MINIMUM_SATELLITE_ANGLE = VK2D_PI / 2;
const float SATELLITE_RADIAL_BONUS = 500; // Bonus dosh per radius of a satellite since bigger == more difficult
const float LAUNCH_DISTANCE = 20; // Distance from the planet satellites are launched from
const float HUD_OFFSET_X = 8;
const float HUD_OFFSET_Y = 8;
const float HUD_BUTTON_WEIGHT = 0.02; // How slow hud buttons move
const float VELOCITY_VARIANCE = (MAXIMUM_SATELLITE_VELOCITY - MINIMUM_SATELLITE_VELOCITY) * 0.05;
const float THETA_VARIANCE = (MAXIMUM_SATELLITE_ANGLE - MINIMUM_SATELLITE_ANGLE) * 0.05;
const float SLIDER_W = 160;
const float SLIDER_H = 24;
const float THETA_SLIDER_X = HUD_OFFSET_X;
const float THETA_SLIDER_Y = GAME_HEIGHT - HUD_OFFSET_Y - (SLIDER_H * 2) - 2;
const float VELOCITY_SLIDER_X = HUD_OFFSET_X;
const float VELOCITY_SLIDER_Y = GAME_HEIGHT - HUD_OFFSET_Y - SLIDER_H;
const float LAUNCH_BUTTON_X = (GAME_WIDTH / 2) - 32 + 8;
const float LAUNCH_BUTTON_Y = GAME_HEIGHT - HUD_OFFSET_Y - 64;
const float SHAKE_INTENSITY = 5;
const float SHAKE_DURATION = 0.2;
const float FADE_IN_SECONDS = 1;
const float COMIC_DURATION = 7;
const float SATELLITE_SELECT_RADIUS = 14;
const float SATELLITE_THRUSTER_DURATION = 1.0;
const float SATELLITE_THRUSTER_VELOCITY = 0.80; // percent
vec4 BLACK = {0, 0, 0, 1};
vec4 WHITE = {1, 1, 1, 1};
vec4 BLUE = {0, 0, 1, 1};
vec4 RED = {1, 0, 0, 1};
vec4 GREEN = {0, 1, 0, 1};
vec4 CYAN = {0, 1, 1, 1};
vec4 PURPLE = {1, 0, 1, 1};
vec4 YELLOW = {1, 1, 0, 1};
vec4 STAR_COLOUR = {0.4, 0.4, 0.4, 1.0};
vec4 SATELLITE_COLOUR = {0.7, 0.7, 0.7, 1};
vec4 SPACE_BLUE = {0, 0, 0.02, 1};
vec4 PLANET_COLOUR = {0.05, 0.11, 0.05, 1};
vec4 DEFAULT_COLOUR = {1, 1, 1, 1};

/******************** Assets ********************/
const char *CURSOR_PNG = "assets/cursor.png";
const char *SHADER_FX_VERTEX = "assets/vert.spv";
const char *SHADER_FX_FRAGMENT = "assets/frag.spv";
const char *GAMEOVER_PNG = "assets/gameover.png";
const char *LAUNCH_PNG = "assets/launch.png";
const char *THETA_PNG = "assets/theta.png";
const char *VELOCITY_PNG = "assets/velocity.png";
const char *POINTER_PNG = "assets/pointer.png";
const char *CANNON_PNG = "assets/cannon.png";
const char *SCORE_FILE = "NotTheHighScore.nothighscore";
const char *MENU_PNG = "assets/menu.png";
const char *PLANET_PNG = "assets/planet.png";
const char *INTRO_PNG = "assets/intro.png";
const char *LIFTOFF_WAV = "assets/liftoff.wav";
const char *PLAYING_WAV = "assets/playing.wav";
const char *MENU_WAV = "assets/menu.wav";
const char *CRASH_WAV = "assets/crash.wav";
const char *PEOPLE_PNG = "assets/people.png";
const char *HATS_PNG = "assets/hats.png";
const char *ALIEN_PNG = "assets/alien.png";
const char *TUTORIAL_PNG = "assets/tutorial.png";

/******************** Structs ********************/
// UBO for the post-fx shader
typedef struct PostFX {
	float trip; // How trippy
	float dir;  // Direction of trippy-ness
} PostFX;

// Describes info for satellites launched
typedef struct Satellite {
	float velocity;
	float direction;
	float radius;
	float x, y;
	Dosh cost;
	vec4 colour;
	float seed;
	bool stolen; // In case an alien is currently stealing it
	float thrusterTimer;
	bool selected;
} Satellite;

// Distant small star
typedef struct Star {
	float radius;
	float x;
	float y;
} Star;

// Info as it pertains to a given planet
typedef struct Planet {
	float gravity;
	float radius;
} Planet;

// Player information for things like score and satellites crashed - will be singleton
typedef struct Player {
	int satellitesCrashed;
	float standbyVelocity;
	float standbyDirection;
	Dosh score;
} Player;

// A simple bitmap font renderer
typedef struct Font {
	VK2DImage sheet; // Image all the characters are on
	VK2DTexture *characters; // 255 long array for ascii characters
	float w;
	float h;
} Font;

// Input things
typedef struct Input {
	const uint8_t *keys;
	const uint8_t *lastKeys;
	bool prm, plm, pmm; // Previous right mouse, previous left mouse ...
	bool rm, lm, mm;
	float mx, my;
} Input;

// Collection of needed assets
typedef struct Assets {
	VK2DTexture texGameOver;
	VK2DTexture texLaunchButton[3]; // normal, hover, pressed
	VK2DTexture texTheta;
	VK2DTexture texVelocity;
	VK2DTexture texPointer;
	VK2DTexture texCannon;
	VK2DTexture texPlanet;
	VK2DTexture texMenu;
	VK2DTexture texIntro;
	VK2DTexture texHats[4];
	VK2DTexture texPeople[4];
	VK2DTexture texAlien;
	cs_loaded_sound_t *sndCrash;
	cs_loaded_sound_t *sndLiftoff;
	cs_loaded_sound_t *sndPlaying;
	cs_loaded_sound_t *sndMenu;
	VK2DTexture texTutorial;
} Assets;

// Big boy struct holding info for basically everything
typedef struct Game {
	// Engine type things
	cs_context_t *cuteSound;
	cs_play_sound_def_t def;
	Input input;
	Assets assets;
	PostFX ubo; // for the post processing shader
	Font font;

	// Important game state
	Player player;
	Planet planet;
	bool playing; // False if player has already lost
	Satellite *satellites;
	uint32_t listSize;
	uint32_t numSatellites;
	Satellite standby; // satellite waiting to be launched

	// Various variables for the game
	float standbyCooldown; // This is in frames
	float time; // current frame
	bool clickTheta, clickVelocity;
	Dosh highscore;
	float shakeDuration;
	uint32_t selectedHat; // For the portrait at the bottom
	uint32_t selectedPerson;
	float tutorialTimer;
	bool selectedSatellite; // to prevent selecting mutliple satellites at once
} Game;

/******************** Helper functions ********************/
static inline float sign(float n) {
	if (n > 0)
		return 1;
	if (n < 0)
		return -1;
	return 0;
}

static inline float pointAngle(float x1, float y1, float x2, float y2) {
	return -atan2f(y1 - y2, x2 - x1);
}

static inline float pointDistance(float x1, float y1, float x2, float y2) {
	return sqrtf(powf(y2 - y1, 2) + powf(x2 - x1, 2));
}

static inline bool pointInRectangle(float x, float y, float x1, float y1, float w, float h) {
	return (x >= x1 && x <= x1 + w && y >= y1 && y <= y1 + h);
}

static inline float absf(float f) {
	return f < 0 ? -f : f;
}

static float clamp(float x, float min, float max) {
	return x < min ? min : (x > max ? max : x);
}

// Loads a font spritesheet into a font assuming each character is width*height and starting index startIndex
Font loadFont(const char *filename, uint32_t width, uint32_t height, uint32_t startIndex, uint32_t endIndex) {
	Font font = {};
	uint32_t i;
	float x, y;
	x = 0;
	y = 0;
	font.w = width;
	font.h = height;
	font.characters = calloc(1, sizeof(VK2DTexture) * FONT_RANGE);
	font.sheet = vk2dImageLoad(vk2dRendererGetDevice(), filename);

	for (i = startIndex; i <= endIndex; i++) {
		font.characters[i] = vk2dTextureLoad(font.sheet, x, y, width, height);
		x += width;
		if (x >= font.sheet->width) {
			x = 0;
			y += height;
		}
	}

	return font;
}

void drawFont(Font font, const char *string, float x, float y) {
	float i = 0;
	while (*string != 0)
		vk2dDrawTexture(font.characters[(uint32_t)(*(string++))], x + (i++ * font.w), y);
}

// Same as above but with numbers at the end of the string
void drawFontNumber(Font font, const char *string, float num, float x, float y) {
	drawFont(font, string, x, y);
	char numbers[30];
	sprintf(numbers, "%.2f", num);
	drawFont(font, numbers, x + ((float)strlen(string) * font.w), y);
}

void destroyFont(Font font) {
	uint32_t i;
	for (i = 0; i <= FONT_RANGE; i++)
		if (font.characters[i] != NULL)
			vk2dTextureFree(font.characters[i]);
	vk2dImageFree(font.sheet);
}

/****************** Functions helpful to the game ******************/
void playSound(Game *game, cs_loaded_sound_t *sound, bool looping) {
	game->def = cs_make_def(sound);
	game->def.looped = looping;
	game->def.volume_left = 0.5;
	game->def.volume_right = 0.5;
	cs_play_sound(game->cuteSound, game->def);
}

void logHighScore(Game *game) {
	game->highscore = game->highscore < game->player.score ? game->player.score : game->highscore;
	FILE *hs = fopen(SCORE_FILE, "w");
	fprintf(hs, "%lf", game->highscore);
	fclose(hs);
}

Planet genRandomPlanet() {
	Planet planet = {};
	planet.radius = PLANET_MINIMUM_RADIUS + ((float)rand() / RAND_MAX) * (PLANET_MAXIMUM_RADIUS - PLANET_MINIMUM_RADIUS);
	planet.gravity = PLANET_MINIMUM_GRAVITY + ((float)rand() / RAND_MAX) * (PLANET_MAXIMUM_GRAVITY - PLANET_MINIMUM_GRAVITY);
	return planet;
}

// Satellites launch from the right side
Satellite genRandomSatellite(Planet *planet) {
	Satellite sat = {};
	sat.radius = MINIMUM_SATELLITE_RADIUS + ((float)rand() / RAND_MAX) * (MAXIMUM_SATELLITE_RADIUS - MINIMUM_SATELLITE_RADIUS);
	sat.cost = (MINIMUM_SATELLITE_DOSH + ((float)rand() / RAND_MAX) * (MAXIMUM_SATELLITE_DOSH - MINIMUM_SATELLITE_DOSH)) + (sat.radius * SATELLITE_RADIAL_BONUS);
	sat.seed = rand();
	return sat;
}

void extendSatelliteList(Game *game) {
	game->satellites = realloc(game->satellites, sizeof(Satellite) * (game->listSize + DEFAULT_LIST_EXTENSION));
	game->listSize += DEFAULT_LIST_EXTENSION;
}

// Places the standby satellite into the sat list wherever it can
void loadStandby(Game *game) {
	if (game->numSatellites == game->listSize)
		extendSatelliteList(game);
	game->standby.velocity = game->player.standbyVelocity;
	game->standby.direction = game->player.standbyDirection;
	game->standby.x = (GAME_WIDTH / 2) + game->planet.radius + (cosf(VK2D_PI-game->player.standbyDirection) * LAUNCH_DISTANCE);
	game->standby.y = (GAME_HEIGHT / 2) + (sinf(VK2D_PI-game->player.standbyDirection) * LAUNCH_DISTANCE);
	game->player.score += game->standby.cost;
	game->satellites[game->numSatellites] = game->standby;
	game->standbyCooldown = STANDBY_COOLDOWN;
	playSound(game, game->assets.sndLiftoff, false);
	game->numSatellites++;
}

void drawSatellite(float x, float y, bool drawAtSpecified, Satellite *sat) {
	sat->seed++;
	sat->colour[0] = 0.6 + ((sinf(sat->seed / 30) * 0.4));
	sat->colour[1] = 0.6 + ((sinf(sat->seed / 30) * 0.4));
	sat->colour[2] = 0;
	sat->colour[3] = 1;
	// draw circle if selected
	if (sat->selected) {
		vec4 colour = {0, 0.5, 0.6, 0.5};
		vk2dRendererSetColourMod(colour);
		vk2dRendererDrawCircle(sat->x, sat->y, SATELLITE_SELECT_RADIUS);
		vk2dRendererSetColourMod(DEFAULT_COLOUR);
	}
	vk2dRendererSetColourMod(sat->colour);
	if (drawAtSpecified)
		vk2dRendererDrawCircle(x, y, sat->radius);
	else
		vk2dRendererDrawCircle(sat->x, sat->y, sat->radius);
	vk2dRendererSetColourMod(DEFAULT_COLOUR);
}

void removeSatellite(Game *game, uint32_t index) {
	uint32_t i;
	for (i = index; i < game->numSatellites - 1; i++)
		game->satellites[i] = game->satellites[i + 1];
	game->numSatellites--;
}

void satelliteCrashEffects(Game *game, float x, float y) {
	game->player.satellitesCrashed++;
	game->shakeDuration = SHAKE_DURATION * 60;
}

// This will delete itself and any satellites it hits on a collision as well as update score and all that
void updateSatellite(Game *game, uint32_t index) {
	Satellite *sat = &game->satellites[index];

	// Process thrusters/selecting
	sat->selected = false;
	sat->thrusterTimer -= 1;
	if (!game->selectedSatellite && pointDistance(sat->x, sat->y, game->input.mx, game->input.my) < SATELLITE_SELECT_RADIUS) {
		sat->selected = true;
		if (game->input.lm && !game->input.plm)
			sat->thrusterTimer = SATELLITE_THRUSTER_DURATION * 60;
	}

	// Process movement
	float boost = 1;
	if (sat->thrusterTimer > 0) {
		boost = 1 + (SATELLITE_THRUSTER_VELOCITY * (sat->thrusterTimer / (SATELLITE_THRUSTER_DURATION * 60)));
	}
	sat->x += cosf(sat->direction) * (sat->velocity * boost);
	sat->y += sinf(sat->direction) * (sat->velocity * boost);
	// We have to add its current velocity vector to the vector of of planets gravity / 60 (its per second so must adjust it to be per frame)
	float angle = pointAngle(sat->x, sat->y, GAME_WIDTH / 2, GAME_HEIGHT / 2);
	float v3x = (cosf(sat->direction) * sat->velocity) + (cosf(angle) * (game->planet.gravity));
	float v3y = (sinf(sat->direction) * sat->velocity) + (sinf(angle) * (game->planet.gravity));
	sat->velocity = sqrtf(powf(v3x, 2) + powf(v3y, 2));
	sat->direction = atan2f(v3y, v3x);

	// Check collisions with every other satellite and the planet
	if (absf(pointDistance(sat->x, sat->y, GAME_WIDTH / 2, GAME_HEIGHT / 2)) < game->planet.radius) { // planet collisions are a bit forgiving b/c doesn't factor in sat radius
		satelliteCrashEffects(game, sat->x, sat->y);
		removeSatellite(game, index);
		playSound(game, game->assets.sndCrash, false);
	} else {
		bool dead = false;
		for (uint32_t i = 0; i < game->numSatellites && !dead; i++) {
			if (i != index && absf(pointDistance(sat->x, sat->y, game->satellites[i].x, game->satellites[i].y)) < game->satellites[i].radius + sat->radius) {
				dead = true;
				satelliteCrashEffects(game, sat->x, sat->y);
				removeSatellite(game, index);
				if (i > index) // because removing the first satellite moves everything ahead of it in the list back 1
					removeSatellite(game, i - 1);
				else
					removeSatellite(game, i);
				playSound(game, game->assets.sndCrash, false);
			}
		}
	}
}

/****************** Game functions ******************/
void unloadGame(Game *game) {
	free(game->satellites);
	game->satellites = NULL;
	game->numSatellites = 0;
	game->listSize = 0;
	game->ubo.trip = 0;
}

void setupGame(Game *game) {
	unloadGame(game);
	game->numSatellites = 0;
	game->planet = genRandomPlanet();
	game->standby = genRandomSatellite(&game->planet);
	game->player.satellitesCrashed = 0;
	game->player.score = 0;
	game->player.standbyDirection = VK2D_PI;
	game->player.standbyVelocity = MINIMUM_SATELLITE_VELOCITY;
	game->player.standbyDirection = MINIMUM_SATELLITE_ANGLE + (MAXIMUM_SATELLITE_ANGLE - MINIMUM_SATELLITE_ANGLE) / 2;
	game->playing = true;
	game->standbyCooldown = STANDBY_COOLDOWN * 2 * 60;
}

Status updateGame(Game *game) {
	// Process satellites
	for (uint32_t i = 0; i < game->numSatellites; i++)
		updateSatellite(game, i);
	if (game->playing && game->player.satellitesCrashed >= GAME_OVER_SATELLITE_COUNT) {
		game->playing = false;
		logHighScore(game);
	}
	bool launchButtonPressed = pointInRectangle(game->input.mx, game->input.my, LAUNCH_BUTTON_X, LAUNCH_BUTTON_Y, 64, 64) && game->input.lm;
	game->selectedSatellite = false;

	// Sway sliders back and forth
	float velX = VELOCITY_SLIDER_X;
	float velY = VELOCITY_SLIDER_Y;
	float thetaX = THETA_SLIDER_X;
	float thetaY = THETA_SLIDER_Y;
	float time = game->time / 60;
	game->player.standbyVelocity += sinf(time) * (VELOCITY_VARIANCE / 60);
	game->player.standbyDirection += cosf(time) * (THETA_VARIANCE / 60);
	game->player.standbyDirection = clamp(game->player.standbyDirection, MINIMUM_SATELLITE_ANGLE, MAXIMUM_SATELLITE_ANGLE);
	game->player.standbyVelocity = clamp(game->player.standbyVelocity, MINIMUM_SATELLITE_VELOCITY, MAXIMUM_SATELLITE_VELOCITY);

	// Allow the player to drag the sliders
	if ((pointInRectangle(game->input.mx, game->input.my, velX, velY, SLIDER_W, SLIDER_H) && game->input.lm && !game->clickTheta) || (game->clickVelocity && !game->clickTheta)) {
		float relative = clamp((game->input.mx - velX) / SLIDER_W, 0, 1);
		float difference = (MINIMUM_SATELLITE_VELOCITY + (relative * (MAXIMUM_SATELLITE_VELOCITY - MINIMUM_SATELLITE_VELOCITY))) - game->player.standbyVelocity;
		game->player.standbyVelocity += difference * HUD_BUTTON_WEIGHT;
		game->clickVelocity = true;
	}
	if ((pointInRectangle(game->input.mx, game->input.my, thetaX, thetaY, SLIDER_W, SLIDER_H) && game->input.lm && !game->clickVelocity) || (game->clickTheta && !game->clickVelocity)) {
		float relative = clamp((game->input.mx - thetaX) / SLIDER_W, 0, 1);
		float difference = (MINIMUM_SATELLITE_ANGLE + (relative * (MAXIMUM_SATELLITE_ANGLE - MINIMUM_SATELLITE_ANGLE))) - game->player.standbyDirection;
		game->player.standbyDirection += difference * HUD_BUTTON_WEIGHT;
		game->clickTheta = true;
	}
	if (!game->input.lm) { // they are not still clicking if they let go of the lmb
		game->clickTheta = false;
		game->clickVelocity = false;
	}

	// Launching satellites/cooldown/payout
	if (game->standbyCooldown != 0) {
		game->standbyCooldown -= 1;
	} else if (launchButtonPressed && !game->clickVelocity && !game->clickTheta && game->playing) {
		loadStandby(game);
		game->standbyCooldown = STANDBY_COOLDOWN * 60;
		game->standby = genRandomSatellite(&game->planet);
		game->selectedPerson = rand() % 4;
		game->selectedHat = rand() % 4;
	} else {
		game->standby.cost -= MONEY_LOSS_PER_SECOND / 60;
		game->standby.cost = game->standby.cost < MINIMUM_PAYOUT ? MINIMUM_PAYOUT : game->standby.cost;
	}

	if (game->playing)
		game->ubo.trip = game->player.satellitesCrashed;
	else
		game->ubo.trip = 0;

	if (game->input.keys[SDL_SCANCODE_RETURN])
		return Status_Menu;
	else if (game->input.keys[SDL_SCANCODE_R] && !game->input.lastKeys[SDL_SCANCODE_R])
		return Status_Restart;
	return Status_Game;
}

void drawGame(Game *game) {
	// Draw planet
	vk2dRendererDrawTexture(game->assets.texPlanet, (GAME_WIDTH / 2) - game->planet.radius, (GAME_HEIGHT / 2) - game->planet.radius, (game->planet.radius / 100) * 2, (game->planet.radius / 100) * 2, -(game->time / 180), 50, 50);
	vk2dRendererDrawTexture(game->assets.texCannon, (GAME_WIDTH / 2) + game->planet.radius - 4, (GAME_HEIGHT / 2) - 6, 1, 1, game->player.standbyDirection + VK2D_PI, 4, 6);

	// Draw all satellites
	for (uint32_t i = 0; i < game->numSatellites; i++)
		drawSatellite(0, 0, false, &game->satellites[i]);

	if (game->playing) {
		/******************** Draw the HUD ********************/
		// Standby
		if (game->standbyCooldown == 0) {
			drawFontNumber(game->font, "JOB PAYOUT: ", game->standby.cost, HUD_OFFSET_X, HUD_OFFSET_Y);
		} else {
			drawFontNumber(game->font, "NEXT JOB: ", (game->standbyCooldown / 60), HUD_OFFSET_X, HUD_OFFSET_Y);
		}

		// Buttons
		vk2dDrawTexture(game->assets.texVelocity, VELOCITY_SLIDER_X, VELOCITY_SLIDER_Y);
		vk2dDrawTexture(game->assets.texTheta, THETA_SLIDER_X, THETA_SLIDER_Y);
		vk2dDrawTexture(game->assets.texPointer, (VELOCITY_SLIDER_X + ((game->player.standbyVelocity - MINIMUM_SATELLITE_VELOCITY) / (MAXIMUM_SATELLITE_VELOCITY - MINIMUM_SATELLITE_VELOCITY)) * game->assets.texVelocity->img->width) - 7, VELOCITY_SLIDER_Y);
		vk2dDrawTexture(game->assets.texPointer, (THETA_SLIDER_X + ((game->player.standbyDirection - MINIMUM_SATELLITE_ANGLE) / (MAXIMUM_SATELLITE_ANGLE - MINIMUM_SATELLITE_ANGLE)) * game->assets.texVelocity->img->width) - 7, THETA_SLIDER_Y);
		uint32_t index = 0;
		if (pointInRectangle(game->input.mx, game->input.my, LAUNCH_BUTTON_X, LAUNCH_BUTTON_Y, 64, 64)) {
			if (game->input.lm && !game->clickTheta && !game->clickVelocity)
				index = 2;
			else
				index = 1;
		}
		vk2dDrawTexture(game->assets.texLaunchButton[index], LAUNCH_BUTTON_X, LAUNCH_BUTTON_Y);

		// Various metrics
		drawFont(game->font, "PUBLIC DISTRUST", GAME_WIDTH - HUD_OFFSET_X - (15 * 8), HUD_OFFSET_Y);
		vk2dDrawRectangle(GAME_WIDTH - HUD_OFFSET_X - (15 * 8), HUD_OFFSET_Y + 17, 15 * 8, 4);
		vk2dRendererSetColourMod(RED);
		vk2dDrawRectangle(GAME_WIDTH - HUD_OFFSET_X - (15 * 8), HUD_OFFSET_Y + 17, clamp(((float)game->player.satellitesCrashed / GAME_OVER_SATELLITE_COUNT), 0, 1) * (15 * 8), 4);
		vk2dRendererSetColourMod(DEFAULT_COLOUR);

		drawFont(game->font, "PLANET GRAVITY", GAME_WIDTH - HUD_OFFSET_X - (14 * 8), HUD_OFFSET_Y + 23);
		vk2dDrawRectangle(GAME_WIDTH - HUD_OFFSET_X - (14 * 8), HUD_OFFSET_Y + 23 + 17, 14 * 8, 4);
		vk2dRendererSetColourMod(RED);
		vk2dDrawRectangle(GAME_WIDTH - HUD_OFFSET_X - (14 * 8), HUD_OFFSET_Y + 17 + 23, ((game->planet.gravity - PLANET_MINIMUM_GRAVITY) / (PLANET_MAXIMUM_GRAVITY - PLANET_MINIMUM_GRAVITY)) * (14 * 8), 4);
		vk2dRendererSetColourMod(DEFAULT_COLOUR);

		// Current score
		char score[50];
		sprintf(score, "Dosh: %.2f$", game->player.score);
		float w = 8 * strlen(score);
		drawFont(game->font, score, GAME_WIDTH - HUD_OFFSET_X - w, GAME_HEIGHT - 16 - HUD_OFFSET_Y);

		// Portrait
		if (game->standbyCooldown <= 0) {
			vk2dDrawTexture(game->assets.texPeople[game->selectedPerson], GAME_WIDTH - HUD_OFFSET_X - 64, GAME_HEIGHT - HUD_OFFSET_Y - 96 - (18 * 2));
			vk2dDrawTexture(game->assets.texHats[game->selectedHat], GAME_WIDTH - HUD_OFFSET_X - 64 + 5, GAME_HEIGHT - HUD_OFFSET_Y - 96 - (18 * 2) - 4);
			sprintf(score, "Sat Mass: %ikg", (int)round(game->standby.radius * 1000));
			w = 8 * strlen(score);
			drawFont(game->font, score, GAME_WIDTH - HUD_OFFSET_X - w, GAME_HEIGHT - 18 - 16 - HUD_OFFSET_Y);
		}
	} else {
		vk2dDrawTexture(game->assets.texGameOver, 0, 0);
		char score[50];
		sprintf(score, "Lost in bankruptcy: %.2f$", game->highscore);
		float w = 8 * strlen(score);
		drawFont(game->font, score, (GAME_WIDTH / 2) - (w / 2), (GAME_HEIGHT / 2) + game->planet.radius + 20);
	}
}

Status updateMenu(Game *game) {
	static bool beganTutorial = false;
	if (!beganTutorial) {
		beganTutorial = true;
		game->tutorialTimer = COMIC_DURATION * 60;
	}
	if (game->input.keys[SDL_SCANCODE_ESCAPE])
		return Status_Quit;
	else if (game->input.keys[SDL_SCANCODE_SPACE] && !game->input.lastKeys[SDL_SCANCODE_SPACE] && game->tutorialTimer <= 0)
		return Status_Game;
	else if (game->input.keys[SDL_SCANCODE_SPACE] && game->tutorialTimer > 0)
		game->tutorialTimer = 0;

	if (game->tutorialTimer > 0)
		game->tutorialTimer--;

	return Status_Menu;
}

void drawMenu(Game *game) {
	if (game->input.keys[SDL_SCANCODE_T]) { // tutorial image
		vk2dDrawTexture(game->assets.texTutorial, 0, 0);
	} else if (game->tutorialTimer > 0) { // for the opening comic
		vec4 fade = {1, 1, 1, 0};
		if (game->tutorialTimer - (COMIC_DURATION * 60) <= FADE_IN_SECONDS * 60 && absf(game->tutorialTimer - (COMIC_DURATION * 60)) < FADE_IN_SECONDS * 60) {
			fade[3] = clamp((absf(game->tutorialTimer - (COMIC_DURATION * 60))) / (FADE_IN_SECONDS * 60), 0, 1);
			fade[2] = fade[3];
			fade[1] = fade[3];
			fade[0] = fade[3];
		} else {
			fade[3] = clamp(game->tutorialTimer / (FADE_IN_SECONDS * 60), 0, 1);
			fade[2] = fade[3];
			fade[1] = fade[3];
			fade[0] = fade[3];
		}
		vk2dRendererSetColourMod(BLACK);
		vk2dRendererClear();
		vk2dRendererSetColourMod(fade);
		vk2dDrawTexture(game->assets.texIntro, 0, 0);
		vk2dRendererSetColourMod(DEFAULT_COLOUR);
	} else { // usual menu stuff
		vk2dDrawTexture(game->assets.texMenu, 0, 0);
		char score[50];
		sprintf(score, "%.2f$", game->highscore);
		float w = 8 * strlen(score);
		drawFont(game->font, score, (GAME_WIDTH / 2) - (w / 2), 202);
	}
}

// The game
void spacelink(int windowWidth, int windowHeight) {
	/******************** SDL initialization ********************/
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_Window *window = SDL_CreateWindow("Spacelink", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_VULKAN);
	SDL_Event ev;
	int keyCount;
	bool running = true;
	SDL_ShowCursor(SDL_DISABLE);
	volatile double lastTime = SDL_GetPerformanceCounter();

	/******************** VK2D initialization ********************/
	VK2DRendererConfig config = {msaa_32x, sm_TripleBuffer, ft_Nearest};
	vk2dRendererInit(window, config);
	VK2DTexture backbuffer = vk2dTextureCreate(vk2dRendererGetDevice(), GAME_WIDTH, GAME_HEIGHT);
	vk2dRendererSetTextureCamera(true);

	/******************** Asset loading ********************/
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(window, &wmInfo);
	HWND hwnd = wmInfo.info.win.window;
	cs_context_t *ctx = cs_make_context(hwnd, 41000, 1024 * 1024 * 10, 20, NULL);
	VK2DShader shaderPostFX = vk2dShaderCreate(vk2dRendererGetDevice(), SHADER_FX_VERTEX, SHADER_FX_FRAGMENT, sizeof(PostFX));
	VK2DImage imgCursor = vk2dImageLoad(vk2dRendererGetDevice(), CURSOR_PNG);
	VK2DImage imgGameOver = vk2dImageLoad(vk2dRendererGetDevice(), GAMEOVER_PNG);
	VK2DImage imgTheta = vk2dImageLoad(vk2dRendererGetDevice(), THETA_PNG);
	VK2DImage imgVelocity = vk2dImageLoad(vk2dRendererGetDevice(), VELOCITY_PNG);
	VK2DImage imgPointer = vk2dImageLoad(vk2dRendererGetDevice(), POINTER_PNG);
	VK2DImage imgLaunch = vk2dImageLoad(vk2dRendererGetDevice(), LAUNCH_PNG);
	VK2DImage imgCannon = vk2dImageLoad(vk2dRendererGetDevice(), CANNON_PNG);
	VK2DImage imgIntro = vk2dImageLoad(vk2dRendererGetDevice(), INTRO_PNG);
	VK2DImage imgPlanet = vk2dImageLoad(vk2dRendererGetDevice(), PLANET_PNG);
	VK2DImage imgMenu = vk2dImageLoad(vk2dRendererGetDevice(), MENU_PNG);
	VK2DImage imgHats = vk2dImageLoad(vk2dRendererGetDevice(), HATS_PNG);
	VK2DImage imgPeople = vk2dImageLoad(vk2dRendererGetDevice(), PEOPLE_PNG);
	VK2DImage imgAlien = vk2dImageLoad(vk2dRendererGetDevice(), ALIEN_PNG);
	VK2DTexture texTheta = vk2dTextureLoad(imgTheta, 0, 0, SLIDER_W, SLIDER_H);
	VK2DTexture texVelocity = vk2dTextureLoad(imgVelocity, 0, 0, SLIDER_W, SLIDER_H);
	VK2DTexture texPointer = vk2dTextureLoad(imgPointer, 0, 0, 15, 9);
	VK2DTexture texAlien = vk2dTextureLoad(imgAlien, 0, 0, 32, 16);
	VK2DTexture texPeople[4];
	VK2DTexture texHats[4];
	texPeople[0] = vk2dTextureLoad(imgPeople, 0, 0, 64, 96);
	texPeople[1] = vk2dTextureLoad(imgPeople, 64, 0, 64, 96);
	texPeople[2] = vk2dTextureLoad(imgPeople, 128, 0, 64, 96);
	texPeople[3] = vk2dTextureLoad(imgPeople, 192, 0, 64, 96);
	texHats[0] = vk2dTextureLoad(imgHats, 0, 0, 48, 32);
	texHats[1] = vk2dTextureLoad(imgHats, 48, 0, 48, 32);
	texHats[2] = vk2dTextureLoad(imgHats, 96, 0, 48, 32);
	texHats[3] = vk2dTextureLoad(imgHats, 144, 0, 48, 32);
	VK2DTexture texCannon = vk2dTextureLoad(imgCannon, 0, 0, 23, 13);
	VK2DTexture texIntro = vk2dTextureLoad(imgIntro, 0, 0, 400, 400);
	VK2DTexture texPlanet = vk2dTextureLoad(imgPlanet, 0, 0, 100, 100);
	VK2DTexture texMenu = vk2dTextureLoad(imgMenu, 0, 0, 400, 400);
	VK2DTexture texLaunch[3];
	texLaunch[0] = vk2dTextureLoad(imgLaunch, 0, 0, 64, 64);
	texLaunch[1] = vk2dTextureLoad(imgLaunch, 64, 0, 64, 64);
	texLaunch[2] = vk2dTextureLoad(imgLaunch, 128, 0, 64, 64);
	VK2DTexture texCursor = vk2dTextureLoad(imgCursor, 0, 0, 5, 5);
	VK2DTexture texGameOver = vk2dTextureLoad(imgGameOver, 0, 0, 400, 400);
	Font font = loadFont("assets/font.png", 8, 16, 0, 255);
	cs_loaded_sound_t sndLiftoff = cs_load_wav(LIFTOFF_WAV);
	cs_loaded_sound_t sndPlaying = cs_load_wav(PLAYING_WAV);
	cs_loaded_sound_t sndMenu = cs_load_wav(MENU_WAV);
	cs_loaded_sound_t sndCrash = cs_load_wav(CRASH_WAV);
	VK2DImage imgTutorial = vk2dImageLoad(vk2dRendererGetDevice(), TUTORIAL_PNG);
	VK2DTexture texTutorial = vk2dTextureLoad(imgTutorial, 0, 0, 400, 400);

	/******************** Game variables ********************/
	GameState state = GameState_Menu;
	Game game = {};
	game.input.lastKeys = malloc(keyCount);
	game.font = font;
	game.input.keys = SDL_GetKeyboardState(&keyCount);
	game.assets.texGameOver = texGameOver;
	game.assets.texLaunchButton[0] = texLaunch[0];
	game.assets.texLaunchButton[1] = texLaunch[1];
	game.assets.texLaunchButton[2] = texLaunch[2];
	game.assets.texPeople[0] = texPeople[0];
	game.assets.texPeople[1] = texPeople[1];
	game.assets.texPeople[2] = texPeople[2];
	game.assets.texPeople[3] = texPeople[3];
	game.assets.texHats[0] = texHats[0];
	game.assets.texHats[1] = texHats[1];
	game.assets.texHats[2] = texHats[2];
	game.assets.texHats[3] = texHats[3];
	game.assets.texAlien = texAlien;
	game.assets.texPointer = texPointer;
	game.assets.texTheta = texTheta;
	game.assets.texVelocity = texVelocity;
	game.assets.texCannon = texCannon;
	game.assets.texIntro = texIntro;
	game.assets.texPlanet = texPlanet;
	game.assets.texMenu = texMenu;
	game.cuteSound = ctx;
	game.assets.sndLiftoff = &sndLiftoff;
	game.assets.sndPlaying = &sndPlaying;
	game.assets.sndMenu = &sndMenu;
	game.assets.sndCrash = &sndCrash;
	game.assets.texTutorial = texTutorial;

	// Load highscore
	FILE *hs = fopen(SCORE_FILE, "r");
	fscanf(hs, "%lf", &game.highscore);
	fclose(hs);

	// Start the menu music
	cs_stop_all_sounds(ctx);
	playSound(&game, &sndMenu, true);

	const uint32_t starCount = 50;
	Star stars[starCount];
	for (uint32_t i = 0; i < starCount; i++) {
		stars[i].x = round(((float)rand() / RAND_MAX) * GAME_WIDTH);
		stars[i].y = round(((float)rand() / RAND_MAX) * GAME_HEIGHT);
		stars[i].radius = ceil(((float)rand() / RAND_MAX) * 3);
	}

	while (running) {
		memcpy((void*)game.input.lastKeys, game.input.keys, keyCount);
		while (SDL_PollEvent(&ev))
			if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)
				running = false;
		VK2DCamera cam = vk2dRendererGetCamera();
		cs_mix(ctx);

		/******************** Manage SDL input ********************/
		SDL_PumpEvents();
		int mx, my;
		float xmouse, ymouse;
		uint32_t mState = SDL_GetMouseState(&mx, &my);
		game.input.plm = game.input.lm;
		game.input.pmm = game.input.mm;
		game.input.prm = game.input.rm;
		game.input.lm = mState & SDL_BUTTON(SDL_BUTTON_LEFT);
		game.input.mm = mState & SDL_BUTTON(SDL_BUTTON_MIDDLE);
		game.input.rm = mState & SDL_BUTTON(SDL_BUTTON_RIGHT);
		xmouse = (mx / ((float)windowWidth / GAME_WIDTH)) + cam.x;
		ymouse = (my / ((float)windowHeight / GAME_HEIGHT)) + cam.y;
		game.input.mx = xmouse;
		game.input.my = ymouse;

		/******************** Game logic ********************/
		Status status;
		if (state == GameState_Menu) {
			status = updateMenu(&game);
			if (status == Status_Game) {
				state = GameState_Game;
				setupGame(&game);
				cs_stop_all_sounds(ctx);
				playSound(&game, &sndPlaying, true);
			} else if (status == Status_Quit) {
				running = false;
			}
		} else if (state == GameState_Game) {
			status = updateGame(&game);
			if (status == Status_Menu) {
				state = GameState_Menu;
				unloadGame(&game);
			} else if (status == Status_Quit) {
				running = false;
				unloadGame(&game);
				cs_stop_all_sounds(ctx);
				playSound(&game, &sndMenu, true);
			} else if (status == Status_Restart) {
				setupGame(&game);
			}
		}
		game.time += 1;

		// Handle screen shake
		game.shakeDuration--;
		if (game.shakeDuration <= 0) {
			cam.x = 0;
			cam.y = 0;
		} else {
			cam.x = ((float)rand() / RAND_MAX) * SHAKE_INTENSITY;
			cam.y = ((float)rand() / RAND_MAX) * SHAKE_INTENSITY;
		}
		vk2dRendererSetCamera(cam);

		/******************** Begin drawing ********************/
		vk2dRendererStartFrame(WHITE);
		vk2dRendererSetTarget(backbuffer);
		vk2dRendererSetColourMod(SPACE_BLUE);
		vk2dRendererClear();
		vk2dRendererSetColourMod(DEFAULT_COLOUR);

		// Update PostFX ubo
		game.ubo.dir += (VK2D_PI * 2) / 60;
		vk2dShaderUpdate(shaderPostFX, &game.ubo, sizeof(PostFX));

		/******************** Gameplay drawing ********************/
		// Starry background
		vk2dRendererSetColourMod(STAR_COLOUR);
		for (uint32_t i = 0; i < starCount; i++)
			vk2dDrawCircle(stars[i].x, stars[i].y, stars[i].radius);
		vk2dRendererSetColourMod(DEFAULT_COLOUR);
		if (state == GameState_Menu)
			drawMenu(&game);
		else if (state == GameState_Game)
			drawGame(&game);

		/******************** End of drawing/cursor ********************/
		vk2dDrawTexture(texCursor, roundf(xmouse - 2), roundf(ymouse - 2));
		vk2dRendererSetTarget(VK2D_TARGET_SCREEN);
		vk2dRendererDrawShader(shaderPostFX, backbuffer, cam.x, cam.y, (float)WINDOW_WIDTH / GAME_WIDTH, (float)WINDOW_HEIGHT / GAME_HEIGHT, 0, 0, 0);
		vk2dRendererEndFrame();

		/******************** Lock to 60 ********************/
		if ((double)SDL_GetPerformanceCounter() - lastTime < (double)SDL_GetPerformanceFrequency() / 60) {
			while ((double)SDL_GetPerformanceCounter() - lastTime < (double)SDL_GetPerformanceFrequency() / 60) {
				// do nothing lmao
			}
		}
		lastTime = SDL_GetPerformanceCounter();
	}

	/******************** Cleanup ********************/
	vk2dRendererWait();
	free((void*)game.input.lastKeys);
	vk2dShaderFree(shaderPostFX);
	vk2dTextureFree(backbuffer);
	vk2dImageFree(imgCursor);
	vk2dImageFree(imgGameOver);
	vk2dTextureFree(texCursor);
	vk2dTextureFree(texGameOver);
	vk2dTextureFree(texTheta);
	vk2dTextureFree(texPeople[0]);
	vk2dTextureFree(texPeople[1]);
	vk2dTextureFree(texPeople[2]);
	vk2dTextureFree(texPeople[3]);
	vk2dTextureFree(texHats[0]);
	vk2dTextureFree(texHats[1]);
	vk2dTextureFree(texHats[2]);
	vk2dTextureFree(texHats[3]);
	vk2dImageFree(imgHats);
	vk2dImageFree(imgPeople);
	vk2dTextureFree(texAlien);
	vk2dImageFree(imgAlien);
	vk2dImageFree(imgTutorial);
	vk2dTextureFree(texTutorial);
	vk2dTextureFree(texCannon);
	vk2dTextureFree(texIntro);
	vk2dImageFree(imgIntro);
	vk2dTextureFree(texPlanet);
	vk2dImageFree(imgPlanet);
	vk2dTextureFree(texMenu);
	vk2dImageFree(imgMenu);
	vk2dImageFree(imgCannon);
	vk2dImageFree(imgTheta);
	vk2dTextureFree(texVelocity);
	vk2dImageFree(imgVelocity);
	vk2dTextureFree(texPointer);
	vk2dImageFree(imgPointer);
	vk2dTextureFree(texLaunch[0]);
	vk2dTextureFree(texLaunch[1]);
	vk2dTextureFree(texLaunch[2]);
	vk2dImageFree(imgLaunch);
	destroyFont(font);
	cs_free_sound(&sndLiftoff);
	cs_free_sound(&sndPlaying);
	cs_free_sound(&sndMenu);
	cs_free_sound(&sndCrash);
	cs_release_context(ctx);
	vk2dRendererQuit();
	SDL_DestroyWindow(window);
}

int main(int argc, const char *argv[]) {
	spacelink(WINDOW_WIDTH, WINDOW_HEIGHT);
	return 0;
}