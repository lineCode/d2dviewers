// main.cpp
//{{{  includes
#include <stdlib.h>

#include <SDL.h>
#include <SDL_main.h>
#include <SDL_opengl.h>

#pragma comment (lib,"SDL2.lib")
#pragma comment (lib,"SDL2main.lib")
#pragma comment (lib,"OpenGL32.lib")
//}}}

static SDL_Window *window = NULL;
static SDL_GLContext gl_context;
static bool quitting = false;
static float r = 0.0f;

//{{{
void render() {

  SDL_GL_MakeCurrent(window, gl_context);

  r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
  glClearColor (r, 0.4f, 0.1f, 1.0f );
  glClear (GL_COLOR_BUFFER_BIT );

  SDL_GL_SwapWindow (window);
  }
//}}}
//{{{
int SDLCALL watch (void *userdata, SDL_Event* event) {

  if (event->type == SDL_APP_WILLENTERBACKGROUND) 
    quitting = true;

  return 1;
  }
//}}}

//{{{
int SDL_main (int argc, char* argv[]) {

  if (SDL_Init (SDL_INIT_VIDEO|SDL_INIT_EVENTS) != 0) {
    SDL_Log ("Failed to initialize SDL: %s", SDL_GetError());
    return 1;
    }

  window = SDL_CreateWindow ("title", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 512, 512, SDL_WINDOW_OPENGL);
  gl_context = SDL_GL_CreateContext (window);
  SDL_AddEventWatch (watch, NULL);

  while (!quitting) {
    SDL_Event event;
    while( SDL_PollEvent(&event) ) {
      if (event.type == SDL_QUIT) {
        quitting = true;
        }
      }

    render();
    SDL_Delay (2);
    }

  SDL_DelEventWatch (watch, NULL);
  SDL_GL_DeleteContext (gl_context);
  SDL_DestroyWindow (window);
  SDL_Quit();

  exit(0);
  }
//}}}
