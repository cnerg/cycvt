### DO NOT DELETE THIS COMMENT: INSERT_ARCHETYPES_HERE ###
USE_CYCLUS("cycvt" "special_enrich")

INSTALL_CYCLUS_MODULE("cycvt" "./cycvt")

# install header files
FILE(GLOB h_files "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
