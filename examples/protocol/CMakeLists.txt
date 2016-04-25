cmake_minimum_required(VERSION 2.8)
find_package(Boost 1.43 COMPONENTS system thread program_options)
find_package(FFTW3 COMPONENTS single)

if (NOT Boost_FOUND)
    message("Boost not found. Not building protocol demo")
    return()
endif (NOT Boost_FOUND)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(${CMAKE_BINARY_DIR}/include ${CMAKE_SOURCE_DIR}/include)

add_executable(ismrmrd_protocol_demo main.cpp)
target_link_libraries(ismrmrd_protocol_demo ismrmrd ${Boost_LIBRARIES})
install(TARGETS ismrmrd_protocol_demo DESTINATION bin)

add_library(icpInputManager SHARED icpInputManager.cpp)
target_link_libraries(icpInputManager ismrmrd ${Boost_LIBRARIES})
install(TARGETS icpInputManager DESTINATION lib)

add_library(icpOutputManager SHARED icpOutputManager.cpp)
target_link_libraries(icpOutputManager ismrmrd ${Boost_LIBRARIES})
install(TARGETS icpOutputManager DESTINATION lib)

add_executable(icpClient icpClient.cpp)
target_link_libraries(icpClient ismrmrd ${Boost_LIBRARIES} )
install(TARGETS icpClient DESTINATION bin)

add_library(icpServer SHARED icpServer.cpp)
target_link_libraries(icpServer ismrmrd ${Boost_LIBRARIES} ${FFTW3_LIBRARIES} icpInputManager icpOutputManager )
install(TARGETS icpServer DESTINATION lib)

add_executable(icpProcessor icpProcessor.cpp)
target_link_libraries(icpProcessor ismrmrd ${Boost_LIBRARIES} ${FFTW3_LIBRARIES} icpInputManager icpOutputManager icpServer)
install(TARGETS icpProcessor DESTINATION bin)

