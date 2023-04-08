# file(GLOB_RECURSE IMAGE_SOURCES lvgl_demo_ui/images/*.c)
# file(GLOB_RECURSE DEMO_UI_SOURCES lvgl_demo_ui/*.c)

# idf_component_register(SRCS ${DEMO_UI_SOURCES} ${IMAGE_SOURCES}
#                        INCLUDE_DIRS ".")

set(LV_DEMO_DIR ../managed_components/lvgl__lvgl/demos)
file(GLOB_RECURSE LV_DEMOS_SOURCES ${LV_DEMO_DIR}/*.c)

idf_component_register(
    SRCS ${LV_DEMOS_SOURCES}
    SRCS "main.c" ${LV_DEMOS_SOURCES}
    INCLUDE_DIRS "." ${LV_DEMO_DIR})

set_source_files_properties(
    ${LV_DEMOS_SOURCES}
    PROPERTIES COMPILE_OPTIONS
    -DLV_LVGL_H_INCLUDE_SIMPLE)
