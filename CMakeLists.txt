###################################################################################################
##
##      Информация о проекте
##
###################################################################################################

cmake_minimum_required(VERSION 3.14)
project(Textum VERSION 1.1.0 LANGUAGES CXX)

get_directory_property(IS_SUBPROJECT PARENT_DIRECTORY)

###################################################################################################
##
##      Опции проекта
##
###################################################################################################

if (IS_SUBPROJECT AND NOT DEFINED TEXTUM_TESTING)
     message(STATUS "Textum не тестируется в режиме подмодуля")
     set(TEXTUM_TESTING OFF)
 endif()

 if (IS_SUBPROJECT AND NOT DEFINED TEXTUM_DOC)
     message(STATUS "Автодокументация для Textum выключена в режиме подмодуля")
     set(TEXTUM_DOC OFF)
 endif()

 if (IS_SUBPROJECT AND NOT DEFINED TEXTUM_WANDBOX)
     message(STATUS "Онлайн-песочница для Textum выключена в режиме подмодуля")
     set(TEXTUM_WANDBOX OFF)
 endif()

 option(TEXTUM_TESTING "Включить модульное тестирование" ON)
 option(TEXTUM_COVERAGE "Включить измерение покрытия кода тестами" OFF)
 option(TEXTUM_DOC "Включить документирование" ON)
 option(TEXTUM_WANDBOX "Включить онлайн-песочницу" ON)

###################################################################################################
##
##      Опции компиляции
##
###################################################################################################

add_compile_options(
    -Werror

    -Wall
    -Wextra
    -Wpedantic

    -Wcast-align
    -Wcast-qual
    -Wconversion
    -Wctor-dtor-privacy
    -Wenum-compare
    -Wfloat-equal
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Woverloaded-virtual
    -Wredundant-decls
    -Wsign-conversion
    -Wsign-promo
)

if(NOT CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

###################################################################################################
##
##      Заголовочная библиотека
##
###################################################################################################

add_library(textum_headers INTERFACE)
 target_include_directories(textum_headers INTERFACE
     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
     $<INSTALL_INTERFACE:include>
 )
 target_compile_features(textum_headers INTERFACE cxx_std_17)
 set_target_properties(textum_headers PROPERTIES EXPORT_NAME textum)

 add_library(Textum::textum ALIAS textum_headers)

###################################################################################################
##
##      Установка
##
###################################################################################################

install(DIRECTORY include/textum DESTINATION include)

install(TARGETS textum_headers EXPORT TextumConfig)
install(EXPORT TextumConfig NAMESPACE Textum:: DESTINATION share/Textum/cmake)

include(CMakePackageConfigHelpers)
write_basic_package_version_file("${PROJECT_BINARY_DIR}/TextumConfigVersion.cmake"
    VERSION
        ${PROJECT_VERSION}
    COMPATIBILITY
        AnyNewerVersion
)
install(FILES "${PROJECT_BINARY_DIR}/TextumConfigVersion.cmake" DESTINATION share/Textum/cmake)

###################################################################################################
##
##      Тесты
##
###################################################################################################

if(NOT TEXTUM_TESTING)
    message(STATUS "Тестирование проекта Textum выключено")
else()
    add_subdirectory(test)
endif()

###################################################################################################
##
##      Документация
##
###################################################################################################

if(NOT TEXTUM_DOC)
    message(STATUS "Автодокументация проекта Textum выключена")
else()
    add_subdirectory(doc)
endif()

###################################################################################################
##
##      Онлайн-песочница
##
###################################################################################################

if(NOT TEXTUM_WANDBOX)
    message(STATUS "Онлайн-песочница для проекта Textum выключена")
else()
    add_subdirectory(online)
endif()

###################################################################################################
##
##      Инструменты
##
###################################################################################################

add_subdirectory(tools)
