#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/clib.h>
#include <sstream>
#include <vector>
#include <cstdio>
#include <SDL2/SDL.h>
#include <algorithm>
#include <string>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

//Screen dimension constants
enum {
	SCREEN_WIDTH  = 960,
	SCREEN_HEIGHT = 544
};

SDL_Window    * gWindow   = NULL;
SDL_Renderer  * gRenderer = NULL;

void DrawCircle(SDL_Renderer* renderer, float x, float y, float radius = 50, char r= 255, char g = 255, char b= 255)
{
	std::vector<SDL_Vertex> verts;

	for (size_t i = 0 ; i < 16; i++)
	{
		verts.emplace_back(SDL_Vertex 
		                   { SDL_FPoint { 
		                   x, y },
						   SDL_Color { r, g, b, 255 }, 
						   SDL_FPoint { 0 } });

		verts.emplace_back(SDL_Vertex 
		                   { SDL_FPoint { 
		                   x + radius * sin((2.0f * 3.14159f) / 16.0f * float(i)), 
		                   y + radius * cos((2.0f * 3.14159f) / 16.0f * float(i)), },
						   SDL_Color { r, g, b, 255 }, 
						   SDL_FPoint { 0 } });

		verts.emplace_back(SDL_Vertex 
		                   { SDL_FPoint { 
		                   		x + radius * sin((2.0f * 3.14159f) / 16.0f * float(i + 1)), 
		                   		y + radius * cos((2.0f * 3.14159f) / 16.0f * float(i + 1)), },
							SDL_Color { r, g, b, 255 }, 
							SDL_FPoint { 0 } });
	}

	SDL_RenderGeometry( renderer, nullptr, verts.data(), verts.size(), nullptr, 0 );
}

struct Rect
{
	float x;
	float y;

	float w;
	float h;
};

struct FallingRect
{	
	Rect rect;
	float v;
};

struct Circle
{
	float x;
	float y;
	float r;
};

float Distance(const float x1, const float y1, const float x2, const float y2)
{
	const float dx = x1 - x2;
	const float dy = y1 - y2;

	return sqrt(dx * dx + dy * dy);
}

bool RectangleCircleIntersection(const Rect& rect, const Circle& circle)
{
	float circleDistance_x = abs(circle.x - rect.x);
	float circleDistance_y = abs(circle.y - rect.y);

	if (circleDistance_x > (rect.w/2 + circle.r)) { return false; }
	if (circleDistance_y > (rect.h/2 + circle.r)) { return false; }

	if (circleDistance_x <= (rect.w/2)) { return true; } 
	if (circleDistance_y <= (rect.h/2)) { return true; }

	float cornerDistance_sq = (circleDistance_x - rect.w/2) * (circleDistance_x - rect.w/2) + (circleDistance_y - rect.h/2)*(circleDistance_y - rect.h/2);

	return (cornerDistance_sq <= (circle.r * circle.r));
}

enum GameMode
{
	Menu,
	Game, 
	Victory
};

struct float2
{
	float x;
	float y;
};

struct float3
{
	float x;
	float y;
	float z;
};


struct FontAsset
{
	SDL_Texture* 	textures[256];
	float3			wh[256];
};

struct GameState
{
	GameMode 	mode;
	FontAsset	defaultFont;
};

void MenuState(SDL_GameController* controller1, GameState& state);
void PlayState(SDL_GameController* controller1, GameState& state);
void VictoryState(SDL_GameController* controller1, GameState& state);

void LoadFont(GameState& state)
{
	long size;
	unsigned char* fontBuffer;
	stbtt_fontinfo font;

	FILE* fontFile = fopen("font.ttf", "rb");

	fseek(fontFile, 0, SEEK_END);
	size = ftell(fontFile); /* how long is the file ? */
	fseek(fontFile, 0, SEEK_SET); /* reset */
    
	fontBuffer = (unsigned char*)malloc(size);

	fread(fontBuffer, size, 1, fontFile);
	fclose(fontFile);

	if (!stbtt_InitFont(&font, fontBuffer, 0))
	{
		printf("failed\n");
	}

	int b_w = 128; /* bitmap width */
	int b_h = 32; /* bitmap height */
	int l_h = 64; /* line height */
	int baseline = 64; 

	struct RGBA
	{
		unsigned char R, G, B, A;
	};

	const char characters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_ ";

	for(size_t I = 0; I < sizeof(characters); I++)
	{
		char c = characters[I];

		auto fontBitmap = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, 64), c, &b_w, &b_h, 0, &baseline);
		RGBA* bitmap = (RGBA*)malloc(sizeof(RGBA) * b_w * b_h);

		for (size_t I = 0; I < b_w * b_h; I++)
			bitmap[I] = RGBA{ fontBitmap[I], fontBitmap[I], fontBitmap[I], fontBitmap[I]};

		SDL_Texture* texture = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, b_w, b_h);
		SDL_UpdateTexture(texture, nullptr, bitmap, b_w * 4);
		SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

		state.defaultFont.textures[c] 	= texture;
		state.defaultFont.wh[c] 		= { b_w, b_h, baseline };

		free(fontBitmap);
		free(bitmap);
	}	

}

void PlayState(SDL_GameController* controller1, GameState& state)
{
	const size_t paddleHeight	= 50;
	const size_t paddleWidth 	= 100;

	float t = 0.0f;
	float paddleX = 0.5f * SCREEN_WIDTH - paddleWidth / 2;
	float moveRate = 500.0f;

	float ballX = float(SCREEN_WIDTH) / 2.0f;
	float ballY	= float(SCREEN_HEIGHT) / 2.0f;

	float ballX_V = 50.0f;
	float ballY_V = 50.0f;

	const float blockStepX = SCREEN_WIDTH / 12;

	std::vector<Rect> blocks;
	std::vector<FallingRect> fallingBlocks;

	for(size_t I = 0; I < 10; I++)
		blocks.push_back(Rect{ blockStepX + blockStepX * I, 50, blockStepX - 10, 50 });
	
	for(size_t I = 0; I < 9; I++)
		blocks.push_back(Rect{ blockStepX * 1.5f + blockStepX * I, 110, blockStepX - 10, 50 });

	for(size_t I = 0; I < 10; I++)
		blocks.push_back(Rect{ blockStepX + blockStepX * I, 170, blockStepX - 10, 50 });

	while (blocks.size())
	{
		for(SDL_Event event; SDL_PollEvent(&event);){}
		
		const Sint16 x_axis = SDL_GameControllerGetAxis(controller1, SDL_CONTROLLER_AXIS_LEFTX);

		const float x_relative = float(x_axis) / float(32768);
		paddleX += x_relative / 16.0f * moveRate;

		paddleX = std::max(0.0f, paddleX);
		paddleX = std::min(float(SCREEN_WIDTH - paddleWidth), paddleX);

		SDL_SetRenderDrawColor(gRenderer, 0xF1, 0xD3, 0xB3, 0xff);
		SDL_RenderClear(gRenderer);

		const int x = (int)(100 * cos(t / 10.0f));
		const int y = (int)(100 * sin(t / 10.0f));

		const SDL_Rect paddleRect = 
		{ 	(int)paddleX,  
			(int)SCREEN_HEIGHT - 100,
			(int)paddleWidth, 
			(int)paddleHeight
		};

		ballX += 1.0f / 16.0f * ballX_V;
		ballY += 1.0f / 16.0f * ballY_V;

		if(ballY + 25 > SCREEN_HEIGHT)
			break; // Player Lost!

		if (ballX > SCREEN_WIDTH - 25.0f || ballX < 25.0f)
		{
			ballX = std::max(25.0f, ballX);
			ballX = std::min(float(SCREEN_WIDTH) - 25.0f, ballX);

			ballX_V = -ballX_V;
		}

		if (ballY > SCREEN_HEIGHT - 25.0f || ballY < 25.0f)
		{
			ballY = std::max(25.0f, ballY);
			ballY = std::min(float(SCREEN_HEIGHT) - 25.0f, ballY);

			ballY_V = -ballY_V;
		}


		for (auto& block : fallingBlocks)
		{
			block.v += 9.8 * 1.0f / 60.0f;
			block.rect.y += block.v;
		}

		if (RectangleCircleIntersection(
				Rect	{ paddleX + paddleWidth / 2.0f, SCREEN_HEIGHT - 100 + paddleHeight / 2.0f,  paddleWidth, paddleHeight }, 
				Circle	{ ballX, ballY, 50.0f }))
		{
			if(0.0f < ballY_V)
				ballY_V = -ballY_V;
		}
		
		for (auto& block : blocks)
		{
			if (RectangleCircleIntersection(block, Circle { ballX, ballY, 50.0f }))
			{
				FallingRect fallingBlock;
				fallingBlock.rect 	= block;
				fallingBlock.v 		= 0.0f;
				fallingBlocks.push_back(fallingBlock);
			}
		}

		fallingBlocks.erase(
			std::remove_if(fallingBlocks.begin(), fallingBlocks.end(), 
			               [&](FallingRect& block) -> bool
			               { 
			               		return block.rect.y > SCREEN_HEIGHT + block.rect.h / 2.0f;  
						   }),
			fallingBlocks.end());

		
		auto intersection_begin = std::remove_if(blocks.begin(), blocks.end(), 
									[&](Rect& block) -> bool
									{ 
											return RectangleCircleIntersection(block, Circle{ ballX, ballY, 50.0f });  
									});

		
		for (auto I = intersection_begin; I < blocks.end(); I++)
		{
			ballX_V *= 1.05f;
			ballY_V *= 1.05f;
		}

		if(std::distance(intersection_begin, blocks.end()) > 0)
		{
			auto I = intersection_begin;
			float d = 0;
			for(; I < blocks.end(); I++)
			{
				d = std::max(Distance(I->x, I->y, ballX, ballY), d);
			}

			const float diffX = abs( ballX - I->x ) - I->w / 2.0f;
			const float diffY = abs( ballY - I->y ) - I->h / 2.0f;

			if(diffX > diffY)
				ballX_V *= -1.0f;
			else
				ballY_V *= -1.0f;
		}
		blocks.erase(intersection_begin, blocks.end());

		SDL_SetRenderDrawColor(gRenderer, 0x8B, 0X7E, 0X74, 255);

		for (auto& block : blocks)
		{
			const SDL_Rect blockRect = 
			{ 	(int)block.x - block.w / 2,  
				(int)block.y - block.h / 2,
				(int)block.w, 
				(int)block.h
			};

			SDL_RenderFillRect(gRenderer, &blockRect);
		}

		SDL_SetRenderDrawColor(gRenderer, 0xC7, 0XBC, 0XA1, 255);

		for (auto& block : fallingBlocks)
		{
			const SDL_Rect blockRect = 
			{ 	(int)block.rect.x - block.rect.w / 2,  
				(int)block.rect.y - block.rect.h / 2,
				(int)block.rect.w, 
				(int)block.rect.h
			};

			SDL_RenderFillRect(gRenderer, &blockRect);
		}

		printf("Ball Y Velocity: %i\n", (int)ballY_V);

		DrawCircle(gRenderer, ballX, ballY, 50.0f, 0x65, 0x64, 0x7c);

		SDL_SetRenderDrawColor(gRenderer, 0x61, 0x76, 0x4b, 255);
		SDL_RenderFillRect(gRenderer, &paddleRect);

		SDL_RenderPresent(gRenderer);
		SDL_Delay(16);

		t += 1.0f / 3.0f;
	}

	if(blocks.size())
	{
		state.mode = GameMode::Menu;
		return;
	}
	else
		VictoryState(controller1, state);
}

template<typename TY>
TY Clamp(const TY min, const TY x, const TY max) noexcept
{
	return std::max(std::min(x, max), min);
}

void DrawButton(const int x, const int y, const int w, const int h, const std::string& text, const SDL_Color& buttonColor, FontAsset& font)
{
	const SDL_Rect btnRect = { x, y, w, h };

	SDL_SetRenderDrawColor(gRenderer, buttonColor.r, buttonColor.g, buttonColor.b, buttonColor.a);
	SDL_RenderFillRect(gRenderer, &btnRect);

	const float textCharacterWidth = float(w * 0.9f) / float(text.size());
	
	float characterX = x + w / 20.0f;

	for(auto c : text)
	{
		const float3 wh = font.wh[c];
		const float scale = wh.x / wh.y;
		const SDL_Rect characterRect = { characterX, y + w / 10.0f + scale * wh.z + scale * wh.y, textCharacterWidth, textCharacterWidth * scale };
		characterX += textCharacterWidth;

		SDL_RenderCopy(gRenderer, font.textures[c], nullptr, &characterRect);
	}
}

void MenuState(SDL_GameController* controller1, GameState& state)
{
	const static SDL_Color palette[] = {
		{ 0x55, 0x49, 0x94 },
		{ 0xF6, 0x75, 0xA8 },
		{ 0xF2, 0x93, 0x93 },
		{ 0xFF, 0xCC, 0xB3 },
	};

	int menuSelection = 0;

	bool up_Button_prev		= false;
	bool down_Button_prev	= false;

	while (true)
	{
		for (SDL_Event event; SDL_PollEvent(&event););

		SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
		SDL_RenderClear(gRenderer);

		const int buttonWidth 	= SCREEN_WIDTH / 5;
		const int buttonHeight 	= 100;
		
		DrawButton(
			SCREEN_WIDTH / 2 - buttonWidth / 2, SCREEN_HEIGHT / 5 * 1, buttonWidth, buttonHeight, "Play", 
			palette[menuSelection == 0 ? 0 : 3], state.defaultFont);
		DrawButton(SCREEN_WIDTH / 2 - buttonWidth / 2, SCREEN_HEIGHT / 5 * 2, buttonWidth, buttonHeight, "Quit", 
			palette[menuSelection == 1 ? 0 : 3], state.defaultFont);

		SDL_RenderPresent(gRenderer);

		const bool up_Button	= SDL_GameControllerGetButton(controller1, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
		const bool down_Button	= SDL_GameControllerGetButton(controller1, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
		const bool x_Button 	= SDL_GameControllerGetButton(controller1, SDL_CONTROLLER_BUTTON_A) != 0;

		if(up_Button && !up_Button_prev)
		{
			printf("Up_Button down \n");
			menuSelection = Clamp(0, menuSelection - 1, 1);
		}
		if(down_Button && !down_Button_prev)
		{
			printf("Down_Button down \n");
			menuSelection = Clamp(0, menuSelection + 1, 1);
		}

		up_Button_prev = up_Button;
		down_Button_prev = down_Button;

		if(x_Button)
		{
			switch(menuSelection)
			{
				case 0:
					state.mode = GameMode::Game;
					return;
					break;
				case 1:
					SDL_Quit();
					sceKernelExitProcess(0);
					break;
			}
		}

		SDL_Delay(16);
	}
}

void VictoryState(SDL_GameController* controller1, GameState& state)
{
	while (true)
	{
		SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
		SDL_RenderClear(gRenderer);

		for (SDL_Event event; SDL_PollEvent(&event););

		const int buttonWidth 	= SCREEN_WIDTH / 5;
		const int buttonHeight 	= 100;
		
		DrawButton(
			SCREEN_WIDTH / 2 - 150 / 2, SCREEN_HEIGHT / 5 * 1, buttonWidth, buttonHeight, "Player Wins", 
			{ 0x00, 0x00, 0x00, 0x00 }, state.defaultFont);

		SDL_RenderPresent(gRenderer);

		const bool x_Button	= SDL_GameControllerGetButton(controller1, SDL_CONTROLLER_BUTTON_A) != 0;
		if(x_Button)
		{
			state.mode = GameMode::Menu;
			return;
		}
	}
}


int main(int argc, char *argv[]) 
{
	if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER ) < 0 )
		return -1;

	if ((gWindow = SDL_CreateWindow( "RedRectangle", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN)) == NULL)
		return -1;

	if ((gRenderer = SDL_CreateRenderer( gWindow, -1, 0)) == NULL)
		return -1;

	SDL_GameController* controller1 = SDL_GameControllerOpen(0);
	printf("Hello: %i\n", (int)controller1 );

	GameState state;
	LoadFont(state);

	state.mode = GameMode::Menu;

	while(true)
		switch (state.mode)
		{
			case GameMode::Menu:
				MenuState(controller1, state);
				break;
			case GameMode::Game:
				PlayState(controller1, state);
				break;
		}

	SDL_DestroyRenderer( gRenderer );
	SDL_DestroyWindow( gWindow );

	gWindow = NULL;
	gRenderer = NULL;

	SDL_Quit();
	sceKernelExitProcess(0);

	return 0;
}
