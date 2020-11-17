

find_library(LIBCXX_LIB c++)
find_package(Threads)
find_library(LIBCXXABI_LIB c++abi)

link_libraries(
    ${LIBCXX_LIB}
    ${LIBCXXABI_LIB}
)

add_library(entropy
    range_encoder.cpp
    range_decoder.cpp
)

target_compile_features(entropy PUBLIC cxx_std_20)

target_link_libraries(entropy ${CMAKE_THREAD_LIBS_INIT} io)

target_include_directories(entropy
    PUBLIC
        $<BUILD_INTERFACE:${_entropy_include_dir}>
        $<INSTALL_INTERFACE:include/entropy>
)

