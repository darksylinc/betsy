
#include "GL/gl3w.h"

#include "FreeImage.h"
#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>

namespace betsy
{
	extern void initBetsyPlatform();
	extern void shutdownBetsyPlatform();
	extern void pollPlatformWindow();

	extern bool g_hasDebugObjectLabel;
	static SDL_GLContext g_glContext = 0;
	static SDL_Window *g_sdlWindow = 0;

	static void APIENTRY GLDebugCallback( GLenum source, GLenum type, GLuint id, GLenum severity,
										  GLsizei length, const GLchar *message, const void *userParam )
	{
		printf( "%s\n", message );
	}

	void initBetsyPlatform()
	{
		if( SDL_Init( SDL_INIT_VIDEO ) != 0 )
		{
			fprintf( stderr, "Error initializing SDL\n" );
			SDL_Quit();
			abort();
		}

		FreeImage_Initialise( TRUE );

		int width = 1280;
		int height = 720;

		int screen = 0;
		int posX = SDL_WINDOWPOS_CENTERED_DISPLAY( screen );
		int posY = SDL_WINDOWPOS_CENTERED_DISPLAY( screen );

		bool fullscreen = false;
		if( fullscreen )
		{
			posX = SDL_WINDOWPOS_UNDEFINED_DISPLAY( screen );
			posY = SDL_WINDOWPOS_UNDEFINED_DISPLAY( screen );
		}

		g_sdlWindow =
			SDL_CreateWindow( "Betsy Mandatory GL window",  // window title
							  posX,                         // initial x position
							  posY,                         // initial y position
							  width,                        // width, in pixels
							  height,                       // height, in pixels
							  SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL |
								  ( fullscreen ? SDL_WINDOW_FULLSCREEN : 0 ) | SDL_WINDOW_RESIZABLE );

		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
#ifdef DEBUG
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG );
#endif

		// GL 4.3 mandates GL_KHR_debug
		g_hasDebugObjectLabel = true;

		g_glContext = SDL_GL_CreateContext( g_sdlWindow );

		if( !g_glContext )
		{
			fprintf( stderr, "GL Context creation failed.\n" );
			SDL_Quit();
			abort();
		}
		else
		{
			printf( "GL Context creation suceeded.\n" );
		}

		SDL_GL_SetSwapInterval( 1 );

		const bool gl3wFailed = gl3wInit() != 0;
		if( gl3wFailed )
		{
			fprintf( stderr, "Failed to initialize GL3W\n" );
			abort();
		}

#ifdef DEBUG
		glDebugMessageCallback( &GLDebugCallback, NULL );
		glEnable( GL_DEBUG_OUTPUT );
		glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
#endif
	}

	void shutdownBetsyPlatform()
	{
		FreeImage_DeInitialise();

		SDL_GL_DeleteContext( g_glContext );
		g_glContext = 0;
		SDL_Quit();
	}

	void pollPlatformWindow()
	{
		SDL_Event evt;
		while( SDL_PollEvent( &evt ) )
		{
			switch( evt.type )
			{
			case SDL_WINDOWEVENT:
				break;
			case SDL_QUIT:
				break;
			default:
				break;
			}
		}

		SDL_GL_SwapWindow( g_sdlWindow );
	}
}  // namespace betsy
