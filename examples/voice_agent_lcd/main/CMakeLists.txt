idf_component_register(SRCS "main.c" "example.c" "board.c" "media.c" "ui.c"
                       INCLUDE_DIRS ".")

# Images are converted manually using LVGLImage.py script form LVGL repo:
# python LVGLImage.py --ofmt C --cf AUTO --name img_<name> <name>.png

lvgl_port_add_images(${COMPONENT_LIB} "images/")