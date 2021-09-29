
#include <stdio.h>
#include <stdlib.h>

#include "GL/gl3w.h"
#include <GLFW/glfw3.h>

namespace betsy
{
	extern void initBetsyPlatform();
	extern void shutdownBetsyPlatform();
	extern void pollPlatformWindow();

	extern bool g_hasDebugObjectLabel;
	static GLFWwindow *g_window = 0;

	static void APIENTRY GLDebugCallback( GLenum source, GLenum type, GLuint id, GLenum severity,
	                                      GLsizei length, const GLchar *message, const void *userParam )
	{
		printf( "%s\n", message );
	}

	void initBetsyPlatform()
	{
		if (!glfwInit())
		{
			// Initialization failed
			fprintf( stderr, "Error initializing GLFW\n" );
			abort();
		}

		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef DEBUG
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1 );
#endif
		g_window = glfwCreateWindow(640, 480, "", NULL, NULL);
		if( !g_window )
		{
			fprintf( stderr, "GL Context creation failed.\n" );
			glfwTerminate();
			abort();
		}
		else
		{
			printf( "GL Context creation suceeded.\n" );
		}

		glfwMakeContextCurrent(g_window);

		// GL 4.3 mandates GL_KHR_debug
		g_hasDebugObjectLabel = true;

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
		glfwDestroyWindow(g_window);
		glfwTerminate();
	}

	void pollPlatformWindow()
	{
		glfwPollEvents();
		glfwSwapBuffers(g_window);
	}
}  // namespace betsy
