cmake_minimum_required(VERSION 3.23)

project(f1rc)

add_executable(f1rc
	main.c
)

add_subdirectory(utils)
add_subdirectory(comms)