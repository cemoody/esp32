CXX = c++
CXXFLAGS = -std=c++17 -Wall -Wno-format -Wno-unused-function -I./mocks -I..
BUILDDIR = build

TESTS = test_audio_buffer test_groq_sse test_elevenlabs test_pipeline test_conversation test_integration

# Source files to watch for changes
WATCH_SRCS = $(wildcard *.cpp) $(wildcard ../*.cpp) $(wildcard ../*.h) $(wildcard ../config.h) $(wildcard mocks/*.h)

all: $(BUILDDIR) $(addprefix $(BUILDDIR)/, $(TESTS))

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/test_audio_buffer: test_audio_buffer.cpp ../audio_buffer.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILDDIR)/test_groq_sse: test_groq_sse.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILDDIR)/test_elevenlabs: test_elevenlabs.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILDDIR)/test_pipeline: test_pipeline.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILDDIR)/test_conversation: test_conversation.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILDDIR)/test_integration: test_integration.cpp ../audio_buffer.cpp ../groq.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test: all
	@failed=0; total=0; \
	for t in $(TESTS); do \
		if ./$(BUILDDIR)/$$t; then \
			true; \
		else \
			echo "FAILED: $$t"; \
			failed=1; \
		fi; \
	done; \
	if [ $$failed -eq 0 ]; then echo "=== ALL TESTS PASSED ==="; fi; \
	exit $$failed

# Auto-rebuild and run tests on file changes
# Uses fswatch if available, falls back to polling
watch:
	@echo "Watching for changes... (Ctrl+C to stop)"
	@if command -v fswatch >/dev/null 2>&1; then \
		fswatch -o ../*.cpp ../*.h ../*.ino ./*.cpp mocks/*.h | while read; do \
			clear; \
			$(MAKE) test 2>&1; \
		done; \
	else \
		echo "(install fswatch for instant rebuilds, using 2s poll)"; \
		last=""; \
		while true; do \
			cur=$$(stat -f '%m' ../*.cpp ../*.h ./*.cpp mocks/*.h 2>/dev/null | md5); \
			if [ "$$cur" != "$$last" ]; then \
				last="$$cur"; \
				clear; \
				$(MAKE) test 2>&1; \
			fi; \
			sleep 2; \
		done; \
	fi

clean:
	rm -rf $(BUILDDIR)

.PHONY: all test watch clean
