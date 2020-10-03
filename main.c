// Spacelink
// Copyright (c) Paolo Mazzon 2020
// 
// Original concept for Spacelink as discussed in a chat room:
//     1. launch shit into orbit at a given speed and angle
//     2. they continue to orbit until they crash into earth or other shit u launched
//     3. the longer you take to launch it the less money you make on the launch and money is ur score but a satellite down is hefty cost and 3 down = lose
#define SDL_MAIN_HANDLED
#include "VK2D/VK2D.h"
#include "VK2D/Image.h"
#include <SDL2/SDL.h>
#include <stdio.h>
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
const Dosh MONEY_LOSS_PER_SECOND = 100;
const Dosh MINIMUM_PAYOUT = 500;
const float PLANET_MINIMUM_RADIUS = 50;
const float PLANET_MAXIMUM_RADIUS = 150;
const float PLANET_MINIMUM_GRAVITY = 6;
const float PLANET_MAXIMUM_GRAVITY = 12;
const int GAME_OVER_SATELLITE_COUNT = 3; // How many satellites must crash before game over
const float STANDBY_COOLDOWN = 3; // In seconds
const float MAXIMUM_SATELLITE_VELOCITY = 10;
const float MINIMUM_SATELLITE_VELOCITY = 1;
const float MAXIMUM_SATELLITE_RADIUS = 4;
const float MINIMUM_SATELLITE_RADIUS = 1;
const float MAXIMUM_SATELLITE_DOSH = 2000;
const float MINIMUM_SATELLITE_DOSH = 750;
const float SATELLITE_RADIAL_BONUS = 100; // Bonus dosh per radius of a satellite since bigger == more difficult
const float LAUNCH_DISTANCE = 10; // Distance from the planet satellites are launched from
const float HUD_OFFSET_X = 8;
const float HUD_OFFSET_Y = 8;
const float HUD_BUTTON_WEIGHT = 0.3; // How slow hud buttons move
vec4 BLACK = {0, 0, 0, 1};
vec4 WHITE = {1, 1, 1, 1};
vec4 BLUE = {0, 0, 1, 1};
vec4 RED = {1, 0, 0, 1};
vec4 GREEN = {0, 1, 0, 1};
vec4 CYAN = {0, 1, 1, 1};
vec4 PURPLE = {1, 0, 1, 1};
vec4 YELLOW = {1, 1, 0, 1};
vec4 STAR_COLOUR = {0.4, 0.4, 0.4, 1.0};
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
} Assets;

// Big boy struct holding info for basically everything
typedef struct Game {
	Input input;
	Assets assets;
	PostFX ubo;
	Font font;
	Player player;
	Planet planet;
	bool playing; // False if player has already lost
	Satellite *satellites;
	uint32_t listSize;
	uint32_t numSatellites;
	Satellite standby; // satellite waiting to be launched
	float standbyCooldown; // This is in frames
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
	for (i = 0; i < FONT_RANGE; i++)
		if (font.characters[i] != NULL)
			vk2dTextureFree(font.characters[i]);
	vk2dImageFree(font.sheet);
}

/****************** Functions helpful to the game ******************/
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
	sat.x = (GAME_WIDTH / 2) + planet->radius + LAUNCH_DISTANCE;
	sat.y = (GAME_HEIGHT / 2) + planet->radius + LAUNCH_DISTANCE;
	return sat;
}

void extendSatelliteList(Game *game) {
	game->satellites = realloc(game->satellites, sizeof(Satellite) * (game->listSize + DEFAULT_LIST_EXTENSION));
	game->listSize += DEFAULT_LIST_EXTENSION;
}

// Places the standby satellite into the sat list wherever it can
void loadStandby(Game *game, float velocity, float direction) {
	if (game->numSatellites == game->listSize)
		extendSatelliteList(game);
	game->standby.velocity = velocity;
	game->standby.direction = direction;
	game->satellites[game->numSatellites] = game->standby;
	game->standbyCooldown = STANDBY_COOLDOWN;
	game->numSatellites++;
}

void drawSatellite(float x, float y, bool drawAtSpecified, Satellite *sat) {
	vk2dRendererSetColourMod(BLACK);
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

// This will delete itself and any satellites it hits on a collision as well as update score and all that
void updateSatellite(Game *game, uint32_t index) {
	// TODO: This
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
	game->player.satellitesCrashed = 0;
	game->player.score = 0;
	game->player.standbyDirection = VK2D_PI;
	game->player.standbyVelocity = MINIMUM_SATELLITE_VELOCITY;
	game->playing = true;
	game->standbyCooldown = STANDBY_COOLDOWN * 60;
}

Status updateGame(Game *game) {
	/* Game algorithm
	 * A) Game
	 *   1. move satellites around
	 *   2. process satellite deaths
	 *   3. lose money if satellite is ready
	 *   4. launch satellite if player wants/wait on cool down
	 *   5. lose if 3 crashes
	 * B) Lose state
	 *   1. Wait for button to restart/quit
	 */

	if (game->playing) {
		// Process satellites
		for (uint32_t i = 0; i < game->numSatellites; i++)
			updateSatellite(game, i);
		game->playing = game->player.satellitesCrashed < GAME_OVER_SATELLITE_COUNT;
		bool launchButtonPressed = pointInRectangle(game->input.mx, game->input.my, (GAME_WIDTH / 2) - 32, GAME_HEIGHT - HUD_OFFSET_Y - 64, 64, 64) && game->input.lm;

		// TODO: Manage cooldown, decrease payout, and handle buttons
	}

	if (game->input.keys[SDL_SCANCODE_RETURN])
		return Status_Menu;
	else if (game->input.keys[SDL_SCANCODE_R])
		return Status_Restart;
	return Status_Game;
}

void drawGame(Game *game) {
	if (game->playing) {
		// Draw planet
		vk2dRendererSetColourMod(PLANET_COLOUR);
		vk2dDrawCircle(GAME_WIDTH / 2, GAME_HEIGHT / 2, game->planet.radius);
		vk2dRendererSetColourMod(DEFAULT_COLOUR);

		// Draw all satellites
		for (uint32_t i = 0; i < game->numSatellites; i++)
			drawSatellite(0, 0, false, &game->satellites[i]);

		/******************** Draw the HUD ********************/
		// Standby
		if (game->standbyCooldown == 0) {
			drawFontNumber(game->font, "STANDBY DOSH: ", game->standby.cost, HUD_OFFSET_X, HUD_OFFSET_Y);
			drawSatellite(HUD_OFFSET_X + 16, HUD_OFFSET_Y + 20, true, &game->standby);
		} else {
			drawFontNumber(game->font, "COOLDOWN: ", (game->standbyCooldown / 60), HUD_OFFSET_X, HUD_OFFSET_Y);
		}

		// Buttons
		vk2dDrawTexture(game->assets.texVelocity, HUD_OFFSET_X, GAME_HEIGHT - HUD_OFFSET_Y - game->assets.texVelocity->img->height);
		vk2dDrawTexture(game->assets.texTheta, GAME_WIDTH - game->assets.texTheta->img->width - HUD_OFFSET_X, GAME_HEIGHT - HUD_OFFSET_Y - game->assets.texVelocity->img->height);
		vk2dDrawTexture(game->assets.texPointer, (HUD_OFFSET_X + ((game->player.standbyVelocity - MINIMUM_SATELLITE_VELOCITY) / (MAXIMUM_SATELLITE_VELOCITY - MINIMUM_SATELLITE_VELOCITY)) * game->assets.texVelocity->img->width) - 7, GAME_HEIGHT - HUD_OFFSET_Y - game->assets.texVelocity->img->height);
		vk2dDrawTexture(game->assets.texPointer, (GAME_WIDTH - game->assets.texTheta->img->width - HUD_OFFSET_X + (game->player.standbyDirection / (2 * VK2D_PI)) * game->assets.texVelocity->img->width) - 7, GAME_HEIGHT - HUD_OFFSET_Y - game->assets.texTheta->img->height);
		uint32_t index = 0;
		float x = (GAME_WIDTH / 2) - 32;
		float y = GAME_HEIGHT - HUD_OFFSET_Y - 64;
		if (pointInRectangle(game->input.mx, game->input.my, x, y, 64, 64)) {
			if (game->input.lm)
				index = 2;
			else
				index = 1;
		}
		vk2dDrawTexture(game->assets.texLaunchButton[index], x, y);

		// Various metrics
		drawFont(game->font, "CRASHED SATELLITES", GAME_WIDTH - HUD_OFFSET_X - (18 * 8), HUD_OFFSET_Y);
		vk2dDrawRectangle(GAME_WIDTH - HUD_OFFSET_X - (18 * 8), HUD_OFFSET_Y + 17, 18 * 8, 4);
		vk2dRendererSetColourMod(RED);
		vk2dDrawRectangle(GAME_WIDTH - HUD_OFFSET_X - (18 * 8), HUD_OFFSET_Y + 17, (game->player.satellitesCrashed / GAME_OVER_SATELLITE_COUNT) * (18 * 8), 4);
		vk2dRendererSetColourMod(DEFAULT_COLOUR);
	} else {
		vk2dDrawTexture(game->assets.texGameOver, 0, 0);
		// TODO: Show high score and user score
	}
}

Status updateMenu(Game *game) {
	if (game->input.keys[SDL_SCANCODE_ESCAPE])
		return Status_Quit;
	else if (game->input.keys[SDL_SCANCODE_SPACE])
		return Status_Game;
	return Status_Menu;
}

void drawMenu(Game *game) {
	drawFont(game->font, "Press <SPACE> to play", 0, 0);
	drawFont(game->font, "Press <ESC> to quit", 0, 16);
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
	VK2DShader shaderPostFX = vk2dShaderCreate(vk2dRendererGetDevice(), SHADER_FX_VERTEX, SHADER_FX_FRAGMENT, sizeof(PostFX));
	VK2DImage imgCursor = vk2dImageLoad(vk2dRendererGetDevice(), CURSOR_PNG);
	VK2DImage imgGameOver = vk2dImageLoad(vk2dRendererGetDevice(), GAMEOVER_PNG);
	VK2DImage imgTheta = vk2dImageLoad(vk2dRendererGetDevice(), THETA_PNG);
	VK2DImage imgVelocity = vk2dImageLoad(vk2dRendererGetDevice(), VELOCITY_PNG);
	VK2DImage imgPointer = vk2dImageLoad(vk2dRendererGetDevice(), POINTER_PNG);
	VK2DImage imgLaunch = vk2dImageLoad(vk2dRendererGetDevice(), LAUNCH_PNG);
	VK2DTexture texTheta = vk2dTextureLoad(imgTheta, 0, 0, 80, 24);
	VK2DTexture texVelocity = vk2dTextureLoad(imgVelocity, 0, 0, 80, 24);
	VK2DTexture texPointer = vk2dTextureLoad(imgPointer, 0, 0, 15, 9);
	VK2DTexture texLaunch[3];
	texLaunch[0] = vk2dTextureLoad(imgLaunch, 0, 0, 64, 64);
	texLaunch[1] = vk2dTextureLoad(imgLaunch, 64, 0, 64, 64);
	texLaunch[2] = vk2dTextureLoad(imgLaunch, 128, 0, 64, 64);
	VK2DTexture texCursor = vk2dTextureLoad(imgCursor, 0, 0, 5, 5);
	VK2DTexture texGameOver = vk2dTextureLoad(imgGameOver, 0, 0, 400, 400);
	Font font = loadFont("assets/font.png", 8, 16, 0, 255);

	/******************** Game variables ********************/
	GameState state = GameState_Menu;
	Game game = {};
	game.font = font;
	game.input.keys = SDL_GetKeyboardState(&keyCount);
	game.assets.texGameOver = texGameOver;
	game.assets.texLaunchButton[0] = texLaunch[0];
	game.assets.texLaunchButton[1] = texLaunch[1];
	game.assets.texLaunchButton[2] = texLaunch[2];
	game.assets.texPointer = texPointer;
	game.assets.texTheta = texTheta;
	game.assets.texVelocity = texVelocity;
	const uint32_t starCount = 50;
	Star stars[starCount];
	for (uint32_t i = 0; i < starCount; i++) {
		stars[i].x = round(((float)rand() / RAND_MAX) * GAME_WIDTH);
		stars[i].y = round(((float)rand() / RAND_MAX) * GAME_HEIGHT);
		stars[i].radius = ceil(((float)rand() / RAND_MAX) * 3);
	}

	while (running) {
		while (SDL_PollEvent(&ev))
			if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)
				running = false;
		VK2DCamera cam = vk2dRendererGetCamera();

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
			} else if (status == Status_Restart) {
				setupGame(&game);
			}
		}

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
	vk2dShaderFree(shaderPostFX);
	vk2dTextureFree(backbuffer);
	vk2dImageFree(imgCursor);
	vk2dImageFree(imgGameOver);
	vk2dTextureFree(texCursor);
	vk2dTextureFree(texGameOver);
	vk2dTextureFree(texTheta);
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
	vk2dRendererQuit();
	SDL_DestroyWindow(window);
}

int main(int argc, const char *argv[]) {
	spacelink(WINDOW_WIDTH, WINDOW_HEIGHT);
	return 0;
}