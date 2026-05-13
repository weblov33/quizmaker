CXX = c++
CXXFLAGS = -std=c++17 -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lsfml-graphics -lsfml-window -lsfml-system
TARGET = quizmaker_sfml
APP = QuizMaker.app

all:
	$(CXX) $(CXXFLAGS) main.cpp $(LDFLAGS) -o $(TARGET)

run: all
	./$(TARGET)

app: all
	mkdir -p $(APP)/Contents/MacOS
	cp $(TARGET) $(APP)/Contents/MacOS/$(TARGET)
	cp Info.plist $(APP)/Contents/Info.plist

run-app: app
	open $(APP)

clean:
	rm -f $(TARGET)
