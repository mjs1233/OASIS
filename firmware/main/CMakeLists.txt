include(${CMAKE_CURRENT_LIST_DIR}/source/CmakeLists.txt)

idf_component_register(SRCS "${COMPONENT_SRCS}"
                    INCLUDE_DIRS "include")
