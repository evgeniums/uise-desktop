ADD_EXECUTABLE(${PROJECT_NAME} ${HEADERS} ${SOURCES})

OPTION(BOOST_STATIC "Use static Boost libraries" OFF)
IF (BOOST_STATIC)
    SET (Boost_USE_STATIC_LIBS ON CACHE BOOL "Boost static libs")
ELSE (BOOST_STATIC)
    SET (Boost_USE_STATIC_LIBS OFF CACHE BOOL "Boost static libs")
    SET(Boost_USE_STATIC_RUNTIME OFF)
    IF(WIN32)
        ADD_DEFINITIONS(-DBOOST_ALL_NO_LIB)
    ENDIF(WIN32)
    ADD_DEFINITIONS(-DBOOST_ALL_DYN_LINK)
ENDIF (BOOST_STATIC)

FIND_PACKAGE(Boost 1.65 COMPONENTS unit_test_framework REQUIRED)
FIND_PACKAGE(Qt6 COMPONENTS Test REQUIRED)

SET (HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/uise/test/uise-test.hpp
    ${CMAKE_CURRENT_LIST_DIR}/uise/test/uise-testwrapper.hpp
    ${CMAKE_CURRENT_LIST_DIR}/uise/test/uise-testthread.hpp
)

SET (SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/uise-testthread.cpp
)

TARGET_SOURCES(${PROJECT_NAME} PRIVATE ${HEADERS} ${SOURCES})
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE ${CMAKE_PROJECT_NAME} ${Boost_LIBRARIES} Qt6::Test)

ADD_TEST(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})