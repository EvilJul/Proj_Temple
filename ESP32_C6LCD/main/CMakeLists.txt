# file(GLOB_RECURSE SRC_UI ${CMAKE_SOURCE_DIR} "ui/*.c")
idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "." "ui")
