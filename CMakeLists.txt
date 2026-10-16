cmake_minimum_required(VERSION 3.2)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "VITASDK is not set. See https://vitasdk.org/ for installation.")
  endif()
endif()

project(Survive60Seconds C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Survive 60 Seconds")
set(VITA_TITLEID  "SURV60001")
set(VITA_VERSION  "01.20")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O2 -std=gnu11 -Wall")

add_executable(${PROJECT_NAME}
  src/main.c
  src/game.c
)

target_link_libraries(${PROJECT_NAME}
  SDL2
  m
  SceDisplay_stub SceCtrl_stub SceAudio_stub SceAudioIn_stub SceSysmodule_stub
  SceGxm_stub SceCommonDialog_stub SceAppMgr_stub SceTouch_stub SceHid_stub SceMotion_stub
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})

vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
  FILE assets assets
)
