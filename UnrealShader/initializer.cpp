/*
 * INITIALIZER.CPP	Shader entry points.
 *
 */
#include <lx_plugin.hpp>

namespace Unreal_Shader
{
	extern void	initialize ();
	extern void	cleanup ();
};


void initialize ()
{
	Unreal_Shader::initialize();
}

void cleanup ()
{
//	Unreal_Shader::cleanup ();
}


