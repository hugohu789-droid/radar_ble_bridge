cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(desktop_radar)

target_sources(app PRIVATE
	src/main.c
	src/servo.c
	src/us100.c
	src/radar.c
	src/protocol.c
	src/buzzer.c
	src/alarm.c
	src/radar_ui.c
)