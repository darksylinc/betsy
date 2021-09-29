macro( add_recursive dir retVal )
	file( GLOB_RECURSE ${retVal} FOLLOW_SYMLINKS ${dir}/*.h ${dir}/*.cpp ${dir}/*.c ${dir}/*.cc ${dir}/*.inl )
endmacro()
