CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
TARGET   = chess_server
SRCS     = engine/chess_engine.cpp engine/server.cpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)
	@echo ""
	@echo "✅  Build successful!"
	@echo "   Run: ./chess_server"
	@echo "   Then open: http://localhost:8080"
	@echo ""

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
