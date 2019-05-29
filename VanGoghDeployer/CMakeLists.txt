# CMakeList.txt : CMake project for VanGoghDeployer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

INCLUDE_DIRECTORIES(include)
LINK_DIRECTORIES(libs)

# Add source to this project's executable.
add_executable (VanGoghDeployer "VanGoghDeployer.cpp" "VanGoghDeployer.h")
TARGET_LINK_LIBRARIES(VanGoghDeployer ssh)
