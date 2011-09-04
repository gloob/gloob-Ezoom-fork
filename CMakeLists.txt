find_package (Compiz REQUIRED)

include (CompizPlugin)

compiz_plugin (ezoom PLUGINDEPS composite opengl mousepoll accessibility PKGDEPS atspi-2)
