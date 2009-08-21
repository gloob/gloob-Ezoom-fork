find_package (Compiz REQUIRED)

include (CompizPlugin)

compiz_plugin (ezoom PLUGINDEPS composite opengl mousepoll)
