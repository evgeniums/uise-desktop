CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME voicerecorder-demo
    TITLE "Voice Recorder"
    DESCRIPTION "Composer with the microphone button and the voice recorder popup, driven by a synthetic recorder."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    MACOS_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in
)
