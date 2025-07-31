file(GLOB_RECURSE ALL_SOURCES ./*.c ./*.cpp)

idf_component_register(SRC_DIRS "." "fonts" "images")

set_source_files_properties(
    ${ALL_SOURCES}
    PROPERTIES COMPILE_OPTIONS
    -DLV_LVGL_H_INCLUDE_SIMPLE
)

# Image and font generation
# 1. Fonts are generated using the LVGL online font converter
# 2. Images are converted manually using LVGLImage.py script form LVGL repo:
#    python LVGLImage.py --ofmt C --cf AUTO --name img_<name> <name>.png