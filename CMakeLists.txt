cmake_minimum_required(VERSION 3.15)
project(FaceRecognition)

set(CMAKE_CXX_STANDARD 17)
#find_path(OPENCV_FACE "C:/Users/Vlad/Desktop/Projects/C++/vcpkg/vcpkg/installed/x64-windows/include/opencv4/")
set(CMAKE_PREFIX_PATH "C:/Users/Vlad/Desktop/Projects/C++/vcpkg/vcpkg/installed/x64-windows")

# Hint for HDF5 to use vcpkg
set(HDF5_NO_FIND_PACKAGE_CONFIG_FILE ON CACHE BOOL "Disable HDF5 package config")
set(HDF5_PREFER_PARALLEL OFF CACHE BOOL "Disable parallel HDF5")
#set(Tesseract_DIR "C:/Users/Vlad/Desktop/Projects/C++/vcpkg/vcpkg/installed/x64-windows/share/tesseract")
#set(Tesseract_LIBRARIES "C:/Users/Vlad/Desktop/Projects/C++/vcpkg/vcpkg/installed/x64-windows/lib")
#set(OPENCV_FACE "C:/Users/Vlad/Desktop/Projects/C++/vcpkg/vcpkg/installed/x64-windows/include/opencv4/")
# With this
find_package(Tesseract CONFIG REQUIRED)


find_package(HDF5 REQUIRED)
find_package(Tesseract REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)


# Find OpenCV with required components
find_package(OpenCV REQUIRED COMPONENTS core imgproc objdetect highgui face)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV not found. Ensure it is installed via vcpkg with contrib module: vcpkg install opencv4[contrib]")
endif()

# Find CURL


# Manually find opencv_face
find_library(OPENCV_FACE_LIBRARY NAMES opencv_face4 opencv_face411
    PATHS "C:/Users/Vlad/Desktop/Projects/C++/vcpkg/vcpkg/installed/x64-windows/lib"
)


find_library(TESSERACT_LIB
    NAMES tesseract41 tesseract42 tesseract50 tesseract tesseract55
    PATHS "C:/Users/Vlad/Desktop/Projects/C++/vcpkg/vcpkg/installed/x64-windows/lib"
    REQUIRED
    NO_DEFAULT_PATH
)

# Debug information
message(STATUS "OpenCV found: ${OpenCV_FOUND}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

message(STATUS "HDF5 include dirs: ${HDF5_INCLUDE_DIRS}")
message(STATUS "HDF5 libraries: ${HDF5_LIBRARIES}")

message(STATUS "Tesseract found: ${Tesseract_FOUND}")
message(STATUS "Tesseract include dirs: ${Tesseract_INCLUDE_DIRS}")
message(STATUS "Tesseract libraries: ${Tesseract_LIBRARIES}")

# Add executable
add_executable(FaceRecognition FaceRecognition.cpp)

# Link libraries and include directories
target_link_libraries(FaceRecognition PRIVATE 
${OpenCV_LIBS} 
${CURL_LIBRARIES}
 ${HDF5_LIBRARIES}
 ${Tesseract_LIB}
 ${ZLIB_LIBRARIES})
 
target_include_directories(FaceRecognition PRIVATE 
    ${OpenCV_INCLUDE_DIRS}
    ${HDF5_INCLUDE_DIRS}
    ${Tesseract_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    
    
)





# Optional: Tesseract (uncomment if needed)
# find_package(Tesseract REQUIRED)
# target_link_libraries(FaceRecognition PRIVATE ${Tesseract_LIBRARIES})