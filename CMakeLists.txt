cmake_minimum_required(VERSION 3.15)
project(Engine3D LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Usa o raylib instalado no sistema; se nao houver, baixa e compila automaticamente.
# (Requer raylib 5.0 ou mais novo: usa ToggleBorderlessWindowed.)
find_package(Threads REQUIRED)
find_package(raylib 5.0 QUIET)
if (NOT raylib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW 1
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(${PROJECT_NAME}
    main.cpp
    Application.cpp
    AsyncLoader.cpp
    Assets.cpp
    CameraController.cpp
    Collision.cpp
    Coordinates.cpp
    GameObject.cpp
    LightManager.cpp
    Menu.cpp
    ObjectManager.cpp
    PhysicsWorld.cpp
    PlayerController.cpp
    Renderer.cpp
    RigidBody.cpp
    Scene.cpp
    ShadowMap.cpp
    Transform.cpp
    UiFont.cpp
    Window.cpp
    WorldStreamer.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE raylib Threads::Threads)

# Copia a pasta resources (fonte, modelos) para ao lado do executavel.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
)
