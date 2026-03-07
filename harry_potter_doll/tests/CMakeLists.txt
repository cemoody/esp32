cmake_minimum_required(VERSION 3.16)
project(harry_potter_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mock headers take priority over any system headers
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/mocks)
# Project headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

enable_testing()

# Test 1: AudioBuffer
add_executable(test_audio_buffer
  test_audio_buffer.cpp
  ../audio_buffer.cpp
)
add_test(NAME audio_buffer COMMAND test_audio_buffer)

# Test 2: Groq SSE parsing
add_executable(test_groq_sse
  test_groq_sse.cpp
)
add_test(NAME groq_sse COMMAND test_groq_sse)

# Test 3: ElevenLabs audio extraction
add_executable(test_elevenlabs
  test_elevenlabs.cpp
)
add_test(NAME elevenlabs COMMAND test_elevenlabs)

# Test 4: Pipeline state machine
add_executable(test_pipeline
  test_pipeline.cpp
)
add_test(NAME pipeline COMMAND test_pipeline)

# Test 5: Conversation history
add_executable(test_conversation
  test_conversation.cpp
)
add_test(NAME conversation COMMAND test_conversation)
