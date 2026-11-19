# The Ogdoad engine. Plain C++17, no JUCE, no plugin code.
# Later modulation plugins (tágh, cèis, cnōc ...) can reuse this folder, CcStream in particular.

add_library(ogdoad_engine STATIC
    src/Engine.cpp
    src/CcStream.cpp
)

target_include_directories(ogdoad_engine PUBLIC include)
target_compile_features(ogdoad_engine PUBLIC cxx_std_17)

if(OGDOAD_BUILD_TESTS)
    add_executable(ogdoad_engine_test tests/engine_test.cpp)
    target_link_libraries(ogdoad_engine_test PRIVATE ogdoad_engine)
    add_test(NAME engine COMMAND ogdoad_engine_test)

    # Writes the engine's real CC stream to a file; the mapper test (Node.js) feeds it through the Scripter mapper.
    add_executable(ogdoad_ccdump tests/ccdump.cpp)
    target_link_libraries(ogdoad_ccdump PRIVATE ogdoad_engine)

    find_program(NODE_EXE node)
    if(NODE_EXE)
        add_test(NAME mapper
                 COMMAND ${CMAKE_COMMAND} -E env
                         OGDOAD_CCDUMP=$<TARGET_FILE:ogdoad_ccdump>
                         ${NODE_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/mapper_test.js
                         ${CMAKE_SOURCE_DIR}/tools/mapper/Ogdoad-Mapper.js)
    endif()
endif()
