#include <SDL2/SDL.h>

#define RAYLIB_IMPLEMENTATION
//#define GRAPHICS_API_OPENGL_ES2
#define GRAPHICS_API_OPENGL_33
#define PLATFORM_DESKTOP
#include "miniray.h"




int SCREEN_WIDTH  =1024;
int SCREEN_HEIGHT =880;

SDL_Window* window;
SDL_GLContext gl_context;






void init_SDL()
{

    #if defined(GRAPHICS_API_OPENGL_ES2)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    #else

    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );

    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    #endif // defined


    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow("Lua GL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, window_flags);
    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync



}

void free_sdl()
{
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}



int main(int argc, char *argv[])
{


long fileLoad=0;

if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        SDL_LogError(0,"Error: %s\n", SDL_GetError());
        return -1;
    }

init_SDL();


InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "raylib [core] example - mouse input");
Vector2 ballPosition = { -100.0f, -100.0f };
Color ballColor = DARKBLUE;


Uint32 startTime = SDL_GetTicks();
Uint32 timeDelay=1000;




    // Main loop
    bool done = false;
    while (!done )
    {
      Uint32 start = SDL_GetTicks();




        SDL_Event event;
        while (SDL_PollEvent(&event))
        {

            if (event.type == SDL_QUIT)
                done = true;

            if (event.type == SDL_WINDOWEVENT)
            {
               if ( event.window.event == SDL_WINDOWEVENT_CLOSE)
                  done = true;

                if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                {


                    int w =event.window.data1;
                    int h =event.window.data2;


                }
            }




            if (event.type == SDL_KEYUP)
            {
             if (event.key.keysym.scancode==SDL_SCANCODE_ESCAPE) done=true;
            }



            if (event.type==SDL_DROPFILE)
            {
               SDL_Log("file %s ", event.drop.file);

             }


        }

 ballPosition = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ballColor = MAROON;
        else if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) ballColor = LIME;
        else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) ballColor = DARKBLUE;
        else if (IsMouseButtonPressed(MOUSE_BUTTON_SIDE)) ballColor = PURPLE;
        else if (IsMouseButtonPressed(MOUSE_BUTTON_EXTRA)) ballColor = YELLOW;
        else if (IsMouseButtonPressed(MOUSE_BUTTON_FORWARD)) ballColor = ORANGE;
        else if (IsMouseButtonPressed(MOUSE_BUTTON_BACK)) ballColor = BEIGE;
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawCircleV(ballPosition, 40, ballColor);

            DrawText("move ball with mouse and click mouse button to change color", 10, 10, 20, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------


        SDL_GL_SwapWindow(window);


 }






free_sdl();
SDL_Quit();
}
