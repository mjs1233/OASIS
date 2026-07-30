target_sources(Gateway
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Parser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Server.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ServerConfig.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WorkerRegistry.cpp
)
