# CMAKE generated file: DO NOT EDIT!
# Generated by "MinGW Makefiles" Generator, CMake Version 4.0

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

SHELL = cmd.exe

# The CMake executable.
CMAKE_COMMAND = "C:\Program Files\CMake\bin\cmake.exe"

# The command to remove a file.
RM = "C:\Program Files\CMake\bin\cmake.exe" -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = C:\Users\berna\Downloads\Sapphire

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = C:\Users\berna\Downloads\Sapphire\build

# Include any dependencies generated for this target.
include CMakeFiles/sapphire.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/sapphire.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/sapphire.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/sapphire.dir/flags.make

CMakeFiles/sapphire.dir/codegen:
.PHONY : CMakeFiles/sapphire.dir/codegen

CMakeFiles/sapphire.dir/src/main.cpp.obj: CMakeFiles/sapphire.dir/flags.make
CMakeFiles/sapphire.dir/src/main.cpp.obj: CMakeFiles/sapphire.dir/includes_CXX.rsp
CMakeFiles/sapphire.dir/src/main.cpp.obj: C:/Users/berna/Downloads/Sapphire/src/main.cpp
CMakeFiles/sapphire.dir/src/main.cpp.obj: CMakeFiles/sapphire.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=C:\Users\berna\Downloads\Sapphire\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/sapphire.dir/src/main.cpp.obj"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/sapphire.dir/src/main.cpp.obj -MF CMakeFiles\sapphire.dir\src\main.cpp.obj.d -o CMakeFiles\sapphire.dir\src\main.cpp.obj -c C:\Users\berna\Downloads\Sapphire\src\main.cpp

CMakeFiles/sapphire.dir/src/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/sapphire.dir/src/main.cpp.i"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E C:\Users\berna\Downloads\Sapphire\src\main.cpp > CMakeFiles\sapphire.dir\src\main.cpp.i

CMakeFiles/sapphire.dir/src/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/sapphire.dir/src/main.cpp.s"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S C:\Users\berna\Downloads\Sapphire\src\main.cpp -o CMakeFiles\sapphire.dir\src\main.cpp.s

CMakeFiles/sapphire.dir/src/lexer.cpp.obj: CMakeFiles/sapphire.dir/flags.make
CMakeFiles/sapphire.dir/src/lexer.cpp.obj: CMakeFiles/sapphire.dir/includes_CXX.rsp
CMakeFiles/sapphire.dir/src/lexer.cpp.obj: C:/Users/berna/Downloads/Sapphire/src/lexer.cpp
CMakeFiles/sapphire.dir/src/lexer.cpp.obj: CMakeFiles/sapphire.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=C:\Users\berna\Downloads\Sapphire\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/sapphire.dir/src/lexer.cpp.obj"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/sapphire.dir/src/lexer.cpp.obj -MF CMakeFiles\sapphire.dir\src\lexer.cpp.obj.d -o CMakeFiles\sapphire.dir\src\lexer.cpp.obj -c C:\Users\berna\Downloads\Sapphire\src\lexer.cpp

CMakeFiles/sapphire.dir/src/lexer.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/sapphire.dir/src/lexer.cpp.i"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E C:\Users\berna\Downloads\Sapphire\src\lexer.cpp > CMakeFiles\sapphire.dir\src\lexer.cpp.i

CMakeFiles/sapphire.dir/src/lexer.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/sapphire.dir/src/lexer.cpp.s"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S C:\Users\berna\Downloads\Sapphire\src\lexer.cpp -o CMakeFiles\sapphire.dir\src\lexer.cpp.s

CMakeFiles/sapphire.dir/src/compiler.cpp.obj: CMakeFiles/sapphire.dir/flags.make
CMakeFiles/sapphire.dir/src/compiler.cpp.obj: CMakeFiles/sapphire.dir/includes_CXX.rsp
CMakeFiles/sapphire.dir/src/compiler.cpp.obj: C:/Users/berna/Downloads/Sapphire/src/compiler.cpp
CMakeFiles/sapphire.dir/src/compiler.cpp.obj: CMakeFiles/sapphire.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=C:\Users\berna\Downloads\Sapphire\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/sapphire.dir/src/compiler.cpp.obj"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/sapphire.dir/src/compiler.cpp.obj -MF CMakeFiles\sapphire.dir\src\compiler.cpp.obj.d -o CMakeFiles\sapphire.dir\src\compiler.cpp.obj -c C:\Users\berna\Downloads\Sapphire\src\compiler.cpp

CMakeFiles/sapphire.dir/src/compiler.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/sapphire.dir/src/compiler.cpp.i"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E C:\Users\berna\Downloads\Sapphire\src\compiler.cpp > CMakeFiles\sapphire.dir\src\compiler.cpp.i

CMakeFiles/sapphire.dir/src/compiler.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/sapphire.dir/src/compiler.cpp.s"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S C:\Users\berna\Downloads\Sapphire\src\compiler.cpp -o CMakeFiles\sapphire.dir\src\compiler.cpp.s

CMakeFiles/sapphire.dir/src/parser.cpp.obj: CMakeFiles/sapphire.dir/flags.make
CMakeFiles/sapphire.dir/src/parser.cpp.obj: CMakeFiles/sapphire.dir/includes_CXX.rsp
CMakeFiles/sapphire.dir/src/parser.cpp.obj: C:/Users/berna/Downloads/Sapphire/src/parser.cpp
CMakeFiles/sapphire.dir/src/parser.cpp.obj: CMakeFiles/sapphire.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=C:\Users\berna\Downloads\Sapphire\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object CMakeFiles/sapphire.dir/src/parser.cpp.obj"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/sapphire.dir/src/parser.cpp.obj -MF CMakeFiles\sapphire.dir\src\parser.cpp.obj.d -o CMakeFiles\sapphire.dir\src\parser.cpp.obj -c C:\Users\berna\Downloads\Sapphire\src\parser.cpp

CMakeFiles/sapphire.dir/src/parser.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/sapphire.dir/src/parser.cpp.i"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E C:\Users\berna\Downloads\Sapphire\src\parser.cpp > CMakeFiles\sapphire.dir\src\parser.cpp.i

CMakeFiles/sapphire.dir/src/parser.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/sapphire.dir/src/parser.cpp.s"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S C:\Users\berna\Downloads\Sapphire\src\parser.cpp -o CMakeFiles\sapphire.dir\src\parser.cpp.s

CMakeFiles/sapphire.dir/src/object.cpp.obj: CMakeFiles/sapphire.dir/flags.make
CMakeFiles/sapphire.dir/src/object.cpp.obj: CMakeFiles/sapphire.dir/includes_CXX.rsp
CMakeFiles/sapphire.dir/src/object.cpp.obj: C:/Users/berna/Downloads/Sapphire/src/object.cpp
CMakeFiles/sapphire.dir/src/object.cpp.obj: CMakeFiles/sapphire.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=C:\Users\berna\Downloads\Sapphire\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object CMakeFiles/sapphire.dir/src/object.cpp.obj"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/sapphire.dir/src/object.cpp.obj -MF CMakeFiles\sapphire.dir\src\object.cpp.obj.d -o CMakeFiles\sapphire.dir\src\object.cpp.obj -c C:\Users\berna\Downloads\Sapphire\src\object.cpp

CMakeFiles/sapphire.dir/src/object.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/sapphire.dir/src/object.cpp.i"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E C:\Users\berna\Downloads\Sapphire\src\object.cpp > CMakeFiles\sapphire.dir\src\object.cpp.i

CMakeFiles/sapphire.dir/src/object.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/sapphire.dir/src/object.cpp.s"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S C:\Users\berna\Downloads\Sapphire\src\object.cpp -o CMakeFiles\sapphire.dir\src\object.cpp.s

CMakeFiles/sapphire.dir/src/vm.cpp.obj: CMakeFiles/sapphire.dir/flags.make
CMakeFiles/sapphire.dir/src/vm.cpp.obj: CMakeFiles/sapphire.dir/includes_CXX.rsp
CMakeFiles/sapphire.dir/src/vm.cpp.obj: C:/Users/berna/Downloads/Sapphire/src/vm.cpp
CMakeFiles/sapphire.dir/src/vm.cpp.obj: CMakeFiles/sapphire.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=C:\Users\berna\Downloads\Sapphire\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building CXX object CMakeFiles/sapphire.dir/src/vm.cpp.obj"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/sapphire.dir/src/vm.cpp.obj -MF CMakeFiles\sapphire.dir\src\vm.cpp.obj.d -o CMakeFiles\sapphire.dir\src\vm.cpp.obj -c C:\Users\berna\Downloads\Sapphire\src\vm.cpp

CMakeFiles/sapphire.dir/src/vm.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/sapphire.dir/src/vm.cpp.i"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E C:\Users\berna\Downloads\Sapphire\src\vm.cpp > CMakeFiles\sapphire.dir\src\vm.cpp.i

CMakeFiles/sapphire.dir/src/vm.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/sapphire.dir/src/vm.cpp.s"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S C:\Users\berna\Downloads\Sapphire\src\vm.cpp -o CMakeFiles\sapphire.dir\src\vm.cpp.s

CMakeFiles/sapphire.dir/src/value.cpp.obj: CMakeFiles/sapphire.dir/flags.make
CMakeFiles/sapphire.dir/src/value.cpp.obj: CMakeFiles/sapphire.dir/includes_CXX.rsp
CMakeFiles/sapphire.dir/src/value.cpp.obj: C:/Users/berna/Downloads/Sapphire/src/value.cpp
CMakeFiles/sapphire.dir/src/value.cpp.obj: CMakeFiles/sapphire.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=C:\Users\berna\Downloads\Sapphire\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building CXX object CMakeFiles/sapphire.dir/src/value.cpp.obj"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/sapphire.dir/src/value.cpp.obj -MF CMakeFiles\sapphire.dir\src\value.cpp.obj.d -o CMakeFiles\sapphire.dir\src\value.cpp.obj -c C:\Users\berna\Downloads\Sapphire\src\value.cpp

CMakeFiles/sapphire.dir/src/value.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/sapphire.dir/src/value.cpp.i"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E C:\Users\berna\Downloads\Sapphire\src\value.cpp > CMakeFiles\sapphire.dir\src\value.cpp.i

CMakeFiles/sapphire.dir/src/value.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/sapphire.dir/src/value.cpp.s"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S C:\Users\berna\Downloads\Sapphire\src\value.cpp -o CMakeFiles\sapphire.dir\src\value.cpp.s

CMakeFiles/sapphire.dir/src/debug.cpp.obj: CMakeFiles/sapphire.dir/flags.make
CMakeFiles/sapphire.dir/src/debug.cpp.obj: CMakeFiles/sapphire.dir/includes_CXX.rsp
CMakeFiles/sapphire.dir/src/debug.cpp.obj: C:/Users/berna/Downloads/Sapphire/src/debug.cpp
CMakeFiles/sapphire.dir/src/debug.cpp.obj: CMakeFiles/sapphire.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=C:\Users\berna\Downloads\Sapphire\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Building CXX object CMakeFiles/sapphire.dir/src/debug.cpp.obj"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/sapphire.dir/src/debug.cpp.obj -MF CMakeFiles\sapphire.dir\src\debug.cpp.obj.d -o CMakeFiles\sapphire.dir\src\debug.cpp.obj -c C:\Users\berna\Downloads\Sapphire\src\debug.cpp

CMakeFiles/sapphire.dir/src/debug.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/sapphire.dir/src/debug.cpp.i"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E C:\Users\berna\Downloads\Sapphire\src\debug.cpp > CMakeFiles\sapphire.dir\src\debug.cpp.i

CMakeFiles/sapphire.dir/src/debug.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/sapphire.dir/src/debug.cpp.s"
	C:\msys64\mingw64\bin\g++.exe $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S C:\Users\berna\Downloads\Sapphire\src\debug.cpp -o CMakeFiles\sapphire.dir\src\debug.cpp.s

# Object files for target sapphire
sapphire_OBJECTS = \
"CMakeFiles/sapphire.dir/src/main.cpp.obj" \
"CMakeFiles/sapphire.dir/src/lexer.cpp.obj" \
"CMakeFiles/sapphire.dir/src/compiler.cpp.obj" \
"CMakeFiles/sapphire.dir/src/parser.cpp.obj" \
"CMakeFiles/sapphire.dir/src/object.cpp.obj" \
"CMakeFiles/sapphire.dir/src/vm.cpp.obj" \
"CMakeFiles/sapphire.dir/src/value.cpp.obj" \
"CMakeFiles/sapphire.dir/src/debug.cpp.obj"

# External object files for target sapphire
sapphire_EXTERNAL_OBJECTS =

sapphire.exe: CMakeFiles/sapphire.dir/src/main.cpp.obj
sapphire.exe: CMakeFiles/sapphire.dir/src/lexer.cpp.obj
sapphire.exe: CMakeFiles/sapphire.dir/src/compiler.cpp.obj
sapphire.exe: CMakeFiles/sapphire.dir/src/parser.cpp.obj
sapphire.exe: CMakeFiles/sapphire.dir/src/object.cpp.obj
sapphire.exe: CMakeFiles/sapphire.dir/src/vm.cpp.obj
sapphire.exe: CMakeFiles/sapphire.dir/src/value.cpp.obj
sapphire.exe: CMakeFiles/sapphire.dir/src/debug.cpp.obj
sapphire.exe: CMakeFiles/sapphire.dir/build.make
sapphire.exe: CMakeFiles/sapphire.dir/linkLibs.rsp
sapphire.exe: CMakeFiles/sapphire.dir/objects1.rsp
sapphire.exe: CMakeFiles/sapphire.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=C:\Users\berna\Downloads\Sapphire\build\CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "Linking CXX executable sapphire.exe"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles\sapphire.dir\link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/sapphire.dir/build: sapphire.exe
.PHONY : CMakeFiles/sapphire.dir/build

CMakeFiles/sapphire.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles\sapphire.dir\cmake_clean.cmake
.PHONY : CMakeFiles/sapphire.dir/clean

CMakeFiles/sapphire.dir/depend:
	$(CMAKE_COMMAND) -E cmake_depends "MinGW Makefiles" C:\Users\berna\Downloads\Sapphire C:\Users\berna\Downloads\Sapphire C:\Users\berna\Downloads\Sapphire\build C:\Users\berna\Downloads\Sapphire\build C:\Users\berna\Downloads\Sapphire\build\CMakeFiles\sapphire.dir\DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/sapphire.dir/depend

