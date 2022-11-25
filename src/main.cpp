#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <sstream>
#include <vector>
#include <cstdio>
#include <SDL2/SDL.h>
#include <algorithm>

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

		SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 0);
		SDL_RenderClear(gRenderer);

		SDL_SetRenderDrawColor(gRenderer, 255, 0, 0, 255);

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

		SDL_RenderFillRect(gRenderer, &paddleRect);

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
				fallingBlock.rect = block;
				fallingBlock.v = 0.0f;
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

		blocks.erase(
			std::remove_if(blocks.begin(), blocks.end(), 
			               [&](Rect& block) -> bool
			               { 
			               	return RectangleCircleIntersection(block, Circle	{ ballX, ballY, 50.0f });  
						   }),
			blocks.end());

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

		DrawCircle(gRenderer, ballX, ballY, 50, 255, 0, 255);

		SDL_RenderPresent(gRenderer);
		SDL_Delay(16);

		t += 1.0f / 3.0f;
	}

	if(blocks.size())
		printf("Player Lost!\n");
	else
		printf("Player Win!\n");

	SDL_DestroyRenderer( gRenderer );
	SDL_DestroyWindow( gWindow );

	gWindow = NULL;
	gRenderer = NULL;

	SDL_Quit();
	sceKernelExitProcess(0);

	return 0;
}
