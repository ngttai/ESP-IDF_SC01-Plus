file(GLOB_RECURSE IMAGE_SOURCES lvgl_demo_ui/images/*.c)
file(GLOB_RECURSE DEMO_UI_SOURCES lvgl_demo_ui/*.c)

idf_component_register(SRCS "main.c" ${DEMO_UI_SOURCES} ${IMAGE_SOURCES}
                       INCLUDE_DIRS ".")