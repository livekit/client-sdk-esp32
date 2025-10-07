file(GLOB_RECURSE ALL_SOURCES ./*.c)

idf_component_register(SRC_DIRS "." "images")

set_source_files_properties(
    ${ALL_SOURCES}
    PROPERTIES COMPILE_OPTIONS
    -DLV_LVGL_H_INCLUDE_SIMPLE
)

# Images are converted manually using LVGLImage.py script from the LVGL repo:
# python LVGLImage.py --ofmt C --cf AUTO --name img_<name> <name>.png