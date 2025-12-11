find_package(LV2 CONFIG REQUIRED)

add_library(lv2_host STATIC src/lv2_evbuf.cpp
        src/lv2_symap.cpp)

target_include_directories(lv2_host PUBLIC include)

target_compile_features(lv2_host PRIVATE cxx_std_20)
target_link_libraries(lv2_host PRIVATE lv2::lv2)

if(NOT MSVC)
    target_compile_options(lv2_host PRIVATE -Wall -Wextra -Wno-psabi -fno-rtti -ffast-math)
endif ()
