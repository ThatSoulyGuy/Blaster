cmake_minimum_required(VERSION 2.4...4.0)

project(Blaster LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BLASTER_HEADERS_DIR "${CMAKE_SOURCE_DIR}/Blaster/Header")
set(LIBRARY_HEADERS_DIR "${CMAKE_SOURCE_DIR}/Library/Header")

file(GLOB_RECURSE BLASTER_HEADERS   CONFIGURE_DEPENDS "${BLASTER_HEADERS_DIR}/*.hpp" "${BLASTER_HEADERS_DIR}/*.h")
file(GLOB_RECURSE LIBRARY_HEADERS   CONFIGURE_DEPENDS "${LIBRARY_HEADERS_DIR}/*.hpp" "${LIBRARY_HEADERS_DIR}/*.h")

include(FetchContent)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        master
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        FreeImage
        GIT_REPOSITORY https://github.com/danoli3/FreeImage.git
        GIT_TAG        master
)

FetchContent_MakeAvailable(FreeImage)

if(NOT TARGET FreeImage::FreeImage)
    add_library(FreeImage::FreeImage ALIAS FreeImage)
endif()

set(BUILD_BULLET3           ON  CACHE BOOL "" FORCE)
set(BUILD_BULLET2_DEMOS     OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET3_DEMOS     OFF CACHE BOOL "" FORCE)
set(BUILD_EXTRAS            OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS        OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        bullet
        GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
        GIT_TAG        master
)
FetchContent_MakeAvailable(bullet)

if(TARGET BulletDynamics AND NOT TARGET Bullet::BulletDynamics)
    add_library(Bullet::BulletDynamics ALIAS BulletDynamics)
endif()
if(TARGET BulletCollision AND NOT TARGET Bullet::BulletCollision)
    add_library(Bullet::BulletCollision ALIAS BulletCollision)
endif()
if(TARGET LinearMath AND NOT TARGET Bullet::LinearMath)
    add_library(Bullet::LinearMath ALIAS LinearMath)
endif()

set(BOOST_TAG boost-1.88.0)

FetchContent_Declare(
        boost_system
        GIT_REPOSITORY https://github.com/boostorg/system.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_system)

if(NOT TARGET Boost::system)
    add_library(Boost::system ALIAS boost_system)
endif()

FetchContent_Declare(
        boost_assert
        GIT_REPOSITORY https://github.com/boostorg/assert.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_assert)

if(NOT TARGET Boost::assert)
    add_library(Boost::assert ALIAS boost_assert)
endif()

FetchContent_Declare(
        boost_config
        GIT_REPOSITORY https://github.com/boostorg/config.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_config)

if(NOT TARGET Boost::config)
    add_library(Boost::config ALIAS boost_config)
endif()

FetchContent_Declare(
        boost_throw_exception
        GIT_REPOSITORY https://github.com/boostorg/throw_exception.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_throw_exception)

if(NOT TARGET Boost::throw_exception)
    add_library(Boost::throw_exception ALIAS boost_throw_exception)
endif()

FetchContent_Declare(
        boost_variant2
        GIT_REPOSITORY https://github.com/boostorg/variant2.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_variant2)

if(NOT TARGET Boost::variant2)
    add_library(Boost::variant2 ALIAS boost_variant2)
endif()

FetchContent_Declare(
        boost_winapi
        GIT_REPOSITORY https://github.com/boostorg/winapi.git
        GIT_TAG        ${BOOST_TAG}
)
FetchContent_MakeAvailable(boost_winapi)

if(NOT TARGET Boost::winapi)
    add_library(Boost::winapi ALIAS boost_winapi)
endif()

FetchContent_Declare(
        boost_mp11
        GIT_REPOSITORY https://github.com/boostorg/mp11.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_mp11)

if(NOT TARGET Boost::mp11)
    add_library(Boost::mp11 ALIAS boost_mp11)
endif()

FetchContent_Declare(
        boost_predef
        GIT_REPOSITORY https://github.com/boostorg/predef.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_predef)

if(NOT TARGET Boost::predef)
    add_library(Boost::predef ALIAS boost_predef)
endif()

FetchContent_Declare(
        boost_align
        GIT_REPOSITORY https://github.com/boostorg/align.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_align)

if(NOT TARGET Boost::align)
    add_library(Boost::align ALIAS boost_align)
endif()

FetchContent_Declare(
        boost_core
        GIT_REPOSITORY https://github.com/boostorg/core.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_core)

if(NOT TARGET Boost::core)
    add_library(Boost::core ALIAS boost_core)
endif()

FetchContent_Declare(
        boost_static_assert
        GIT_REPOSITORY https://github.com/boostorg/static_assert.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_static_assert)

if(NOT TARGET Boost::static_assert)
    add_library(Boost::static_assert ALIAS boost_static_assert)
endif()

FetchContent_Declare(
        boost_context
        GIT_REPOSITORY https://github.com/boostorg/context.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_context)

if(NOT TARGET Boost::context)
    add_library(Boost::context ALIAS boost_context)
endif()

FetchContent_Declare(
        boost_pool
        GIT_REPOSITORY https://github.com/boostorg/pool.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_pool)

if(NOT TARGET Boost::pool)
    add_library(Boost::pool ALIAS boost_pool)
endif()

FetchContent_Declare(
        boost_smart_ptr
        GIT_REPOSITORY https://github.com/boostorg/smart_ptr.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_smart_ptr)

if(NOT TARGET Boost::smart_ptr)
    add_library(Boost::smart_ptr ALIAS boost_smart_ptr)
endif()

FetchContent_Declare(
        boost_integer
        GIT_REPOSITORY https://github.com/boostorg/integer.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_integer)

if(NOT TARGET Boost::integer)
    add_library(Boost::integer ALIAS boost_integer)
endif()

FetchContent_Declare(
        boost_type_traits
        GIT_REPOSITORY https://github.com/boostorg/type_traits.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_type_traits)

if(NOT TARGET Boost::type_traits)
    add_library(Boost::type_traits ALIAS boost_type_traits)
endif()

FetchContent_Declare(
        boost_date_time
        GIT_REPOSITORY https://github.com/boostorg/date_time.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_date_time)

if(NOT TARGET Boost::date_time)
    add_library(Boost::date_time ALIAS boost_date_time)
endif()

FetchContent_Declare(
        boost_algorithm
        GIT_REPOSITORY https://github.com/boostorg/algorithm.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_algorithm)

if(NOT TARGET Boost::algorithm)
    add_library(Boost::algorithm ALIAS boost_algorithm)
endif()

FetchContent_Declare(
        boost_io
        GIT_REPOSITORY https://github.com/boostorg/io.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_io)

if(NOT TARGET Boost::io)
    add_library(Boost::io ALIAS boost_io)
endif()

FetchContent_Declare(
        boost_lexical_cast
        GIT_REPOSITORY https://github.com/boostorg/lexical_cast.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_lexical_cast)

if(NOT TARGET Boost::lexical_cast)
    add_library(Boost::lexical_cast ALIAS boost_lexical_cast)
endif()

FetchContent_Declare(
        boost_numeric_conversion
        GIT_REPOSITORY https://github.com/boostorg/numeric_conversion.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_numeric_conversion)

if(NOT TARGET Boost::numeric_conversion)
    add_library(Boost::numeric_conversion ALIAS boost_numeric_conversion)
endif()

FetchContent_Declare(
        boost_range
        GIT_REPOSITORY https://github.com/boostorg/range.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_range)

if(NOT TARGET Boost::range)
    add_library(Boost::range ALIAS boost_range)
endif()

FetchContent_Declare(
        boost_tokenizer
        GIT_REPOSITORY https://github.com/boostorg/tokenizer.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_tokenizer)

if(NOT TARGET Boost::tokenizer)
    add_library(Boost::tokenizer ALIAS boost_tokenizer)
endif()

FetchContent_Declare(
        boost_utility
        GIT_REPOSITORY https://github.com/boostorg/utility.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_utility)

if(NOT TARGET Boost::utility)
    add_library(Boost::utility ALIAS boost_utility)
endif()

FetchContent_Declare(
        boost_array
        GIT_REPOSITORY https://github.com/boostorg/array.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_array)

if(NOT TARGET Boost::array)
    add_library(Boost::array ALIAS boost_array)
endif()

FetchContent_Declare(
        boost_bind
        GIT_REPOSITORY https://github.com/boostorg/bind.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_bind)

if(NOT TARGET Boost::bind)
    add_library(Boost::bind ALIAS boost_bind)
endif()

FetchContent_Declare(
        boost_concept_check
        GIT_REPOSITORY https://github.com/boostorg/concept_check.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_concept_check)

if(NOT TARGET Boost::concept_check)
    add_library(Boost::concept_check ALIAS boost_concept_check)
endif()

FetchContent_Declare(
        boost_exception
        GIT_REPOSITORY https://github.com/boostorg/exception.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_exception)

if(NOT TARGET Boost::exception)
    add_library(Boost::exception ALIAS boost_exception)
endif()

FetchContent_Declare(
        boost_function
        GIT_REPOSITORY https://github.com/boostorg/function.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_function)

if(NOT TARGET Boost::function)
    add_library(Boost::function ALIAS boost_function)
endif()

FetchContent_Declare(
        boost_iterator
        GIT_REPOSITORY https://github.com/boostorg/iterator.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_iterator)

if(NOT TARGET Boost::iterator)
    add_library(Boost::iterator ALIAS boost_iterator)
endif()

FetchContent_Declare(
        boost_mpl
        GIT_REPOSITORY https://github.com/boostorg/mpl.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_mpl)

if(NOT TARGET Boost::mpl)
    add_library(Boost::mpl ALIAS boost_mpl)
endif()

FetchContent_Declare(
        boost_regex
        GIT_REPOSITORY https://github.com/boostorg/regex.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_regex)

if(NOT TARGET Boost::regex)
    add_library(Boost::regex ALIAS boost_regex)
endif()

FetchContent_Declare(
        boost_tuple
        GIT_REPOSITORY https://github.com/boostorg/tuple.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_tuple)

if(NOT TARGET Boost::tuple)
    add_library(Boost::tuple ALIAS boost_tuple)
endif()

FetchContent_Declare(
        boost_unordered
        GIT_REPOSITORY https://github.com/boostorg/unordered.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_unordered)

if(NOT TARGET Boost::unordered)
    add_library(Boost::unordered ALIAS boost_unordered)
endif()

FetchContent_Declare(
        boost_container
        GIT_REPOSITORY https://github.com/boostorg/container.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_container)

if(NOT TARGET Boost::container)
    add_library(Boost::container ALIAS boost_container)
endif()

FetchContent_Declare(
        boost_conversion
        GIT_REPOSITORY https://github.com/boostorg/conversion.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_conversion)

if(NOT TARGET Boost::conversion)
    add_library(Boost::conversion ALIAS boost_conversion)
endif()

FetchContent_Declare(
        boost_preprocessor
        GIT_REPOSITORY https://github.com/boostorg/preprocessor.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_preprocessor)

if(NOT TARGET Boost::preprocessor)
    add_library(Boost::preprocessor ALIAS boost_preprocessor)
endif()

FetchContent_Declare(
        boost_container_hash
        GIT_REPOSITORY https://github.com/boostorg/container_hash.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_container_hash)

if(NOT TARGET Boost::container_hash)
    add_library(Boost::container_hash ALIAS boost_container_hash)
endif()

FetchContent_Declare(
        boost_detail
        GIT_REPOSITORY https://github.com/boostorg/detail.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_detail)

if(NOT TARGET Boost::detail)
    add_library(Boost::detail ALIAS boost_detail)
endif()

FetchContent_Declare(
        boost_optional
        GIT_REPOSITORY https://github.com/boostorg/optional.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_optional)

if(NOT TARGET Boost::optional)
    add_library(Boost::optional ALIAS boost_optional)
endif()

FetchContent_Declare(
        boost_fusion
        GIT_REPOSITORY https://github.com/boostorg/fusion.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_fusion)

if(NOT TARGET Boost::fusion)
    add_library(Boost::fusion ALIAS boost_fusion)
endif()

FetchContent_Declare(
        boost_intrusive
        GIT_REPOSITORY https://github.com/boostorg/intrusive.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_intrusive)

if(NOT TARGET Boost::intrusive)
    add_library(Boost::intrusive ALIAS boost_intrusive)
endif()

FetchContent_Declare(
        boost_move
        GIT_REPOSITORY https://github.com/boostorg/move.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_move)

if(NOT TARGET Boost::move)
    add_library(Boost::move ALIAS boost_move)
endif()

FetchContent_Declare(
        boost_describe
        GIT_REPOSITORY https://github.com/boostorg/describe.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_describe)

if(NOT TARGET Boost::describe)
    add_library(Boost::describe ALIAS boost_describe)
endif()

FetchContent_Declare(
        boost_function_types
        GIT_REPOSITORY https://github.com/boostorg/function_types.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_function_types)

if(NOT TARGET Boost::function_types)
    add_library(Boost::function_types ALIAS boost_function_types)
endif()

FetchContent_Declare(
        boost_typeof
        GIT_REPOSITORY https://github.com/boostorg/typeof.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_typeof)

if(NOT TARGET Boost::typeof)
    add_library(Boost::typeof ALIAS boost_typeof)
endif()

FetchContent_Declare(
        boost_functional
        GIT_REPOSITORY https://github.com/boostorg/functional.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_functional)

if(NOT TARGET Boost::functional)
    add_library(Boost::functional ALIAS boost_functional)
endif()

FetchContent_Declare(
        boost_serialization
        GIT_REPOSITORY https://github.com/boostorg/serialization.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_serialization)

if(NOT TARGET Boost::serialization)
    add_library(Boost::serialization ALIAS boost_serialization)
    target_include_directories(Boost::serialization INTERFACE "${boost_serialization_SOURCE_DIR}/include")

endif()

FetchContent_Declare(
        boost_spirit
        GIT_REPOSITORY https://github.com/boostorg/spirit.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_spirit)

if(NOT TARGET Boost::spirit)
    add_library(Boost::spirit ALIAS boost_spirit)
endif()

FetchContent_Declare(
        boost_variant
        GIT_REPOSITORY https://github.com/boostorg/variant.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_variant)

if(NOT TARGET Boost::variant)
    add_library(Boost::variant ALIAS boost_variant)
endif()

FetchContent_Declare(
        boost_endian
        GIT_REPOSITORY https://github.com/boostorg/endian.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_endian)

if(NOT TARGET Boost::endian)
    add_library(Boost::endian ALIAS boost_endian)
endif()

FetchContent_Declare(
        boost_phoenix
        GIT_REPOSITORY https://github.com/boostorg/phoenix.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_phoenix)

if(NOT TARGET Boost::phoenix)
    add_library(Boost::phoenix ALIAS boost_phoenix)
endif()

FetchContent_Declare(
        boost_proto
        GIT_REPOSITORY https://github.com/boostorg/proto.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_proto)

if(NOT TARGET Boost::proto)
    add_library(Boost::proto ALIAS boost_proto)
endif()

FetchContent_Declare(
        boost_thread
        GIT_REPOSITORY https://github.com/boostorg/thread.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_thread)

if(NOT TARGET Boost::thread)
    add_library(Boost::thread ALIAS boost_thread)
endif()

FetchContent_Declare(
        boost_type_index
        GIT_REPOSITORY https://github.com/boostorg/type_index.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_type_index)

if(NOT TARGET Boost::type_index)
    add_library(Boost::type_index ALIAS boost_type_index)
endif()

FetchContent_Declare(
        boost_atomic
        GIT_REPOSITORY https://github.com/boostorg/atomic.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_atomic)

if(NOT TARGET Boost::atomic)
    add_library(Boost::atomic ALIAS boost_atomic)
endif()

FetchContent_Declare(
        boost_chrono
        GIT_REPOSITORY https://github.com/boostorg/chrono.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_chrono)

if(NOT TARGET Boost::chrono)
    add_library(Boost::chrono ALIAS boost_chrono)
endif()

FetchContent_Declare(
        boost_ratio
        GIT_REPOSITORY https://github.com/boostorg/ratio.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_ratio)

if(NOT TARGET Boost::ratio)
    add_library(Boost::ratio ALIAS boost_ratio)
endif()

FetchContent_Declare(
        boost_asio
        GIT_REPOSITORY https://github.com/boostorg/asio.git
        GIT_TAG        ${BOOST_TAG})
FetchContent_MakeAvailable(boost_asio)

if(NOT TARGET Boost::asio)
    add_library(Boost::asio INTERFACE
            Blaster/Source/Independent/ComponentExportList.cpp
            Blaster/Source/Independent/ComponentExportList.hpp)
    target_include_directories(Boost::asio INTERFACE
            ${boost_asio_SOURCE_DIR}/include)

    target_link_libraries(Boost::asio INTERFACE Boost::system)
endif()

set(EXTRA_PLATFORM_LIBS "")
set(APPLE_FRAMEWORKS    "")

if (WIN32)
    list(APPEND EXTRA_PLATFORM_LIBS
            opengl32
            ws2_32
            mswsock)
elseif (APPLE)
    list(APPEND APPLE_FRAMEWORKS
            "-framework Cocoa"
            "-framework QuartzCore"
            "-framework OpenGL"
            "-framework IOKit"
            "-framework CoreVideo"
            "-framework CoreFoundation"
            "-framework Metal"
            "-framework MetalKit"
            "-framework AVFoundation")
elseif (UNIX)
    find_package(OpenGL REQUIRED)
    list(APPEND EXTRA_PLATFORM_LIBS OpenGL::GL)
    add_compile_definitions(_GLFW_X11)
endif()

function(blaster_add_executable target source_glob)
    file(GLOB_RECURSE SRC CONFIGURE_DEPENDS ${source_glob})

    add_executable(${target}
            ${BLASTER_HEADERS}
            ${LIBRARY_HEADERS}
            ${SRC}
    )

    target_include_directories(${target}
            PRIVATE
            "${boost_serialization_SOURCE_DIR}/include"
            ${BLASTER_HEADERS_DIR}
            ${LIBRARY_HEADERS_DIR}
    )

    target_link_libraries(${target}
            PRIVATE
            glfw
            FreeImage::FreeImage
            Bullet::BulletDynamics
            Bullet::BulletCollision
            Bullet::LinearMath
            Boost::serialization
            Boost::asio
            ${EXTRA_PLATFORM_LIBS}
    )

    if (APPLE)
        target_link_libraries(${target} PRIVATE ${APPLE_FRAMEWORKS})
        target_compile_options(${target} PRIVATE -Wno-deprecated-declarations -mmacosx-version-min=13.7)
    endif()
endfunction()

blaster_add_executable(Client "${CMAKE_SOURCE_DIR}/Blaster/Source/Client/*.cpp" "${CMAKE_SOURCE_DIR}/Blaster/Source/Independent/*.cpp")
blaster_add_executable(Server "${CMAKE_SOURCE_DIR}/Blaster/Source/Server/*.cpp" "${CMAKE_SOURCE_DIR}/Blaster/Source/Independent/*.cpp")

set(SOURCE_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
set(BUILD_ASSETS_DIR  "${CMAKE_CURRENT_BINARY_DIR}/Assets")

add_custom_command(
        TARGET   Client POST_BUILD
        COMMAND  ${CMAKE_COMMAND} -E copy_directory ${SOURCE_ASSETS_DIR} ${BUILD_ASSETS_DIR}
        COMMENT  "Copying assets to build folder..."
)