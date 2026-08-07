CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME qrcodescanner-demo
    TITLE "QR Code Scanner"
    DESCRIPTION "Scans QR codes from a live camera feed."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    MACOS_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in
)
