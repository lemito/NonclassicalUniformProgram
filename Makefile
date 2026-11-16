BUILD := build
BIN := bin/fuzzy-server
ADDR ?= :8080
DESKTOP_BUILD := build-desktop
DESKTOP_BIN := $(DESKTOP_BUILD)/Release/fuzzy-desktop
JOBS ?= 2

.PHONY: all lib server test run desktop run-desktop report docx clean

all: server

lib:
	cmake -S fuzzy -B $(BUILD) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD) -j

test: lib
	cd $(BUILD) && ctest --output-on-failure

server: lib
	mkdir -p bin
	cd server && CGO_ENABLED=1 go build -a -o ../$(BIN) .

run: server
	./$(BIN) -addr $(ADDR) -log fuzzy.log

desktop: server
	cmake -S desktop -B $(DESKTOP_BUILD) -DCMAKE_BUILD_TYPE=Release -DFUZZY_SERVER_BIN=$(abspath $(BIN))
	cmake --build $(DESKTOP_BUILD) -j $(JOBS)

run-desktop: desktop
	./$(DESKTOP_BIN)

clean:
	rm -rf $(BUILD) $(DESKTOP_BUILD) bin fuzzy.log

report:
	cd report_tex && xelatex -interaction=nonstopmode -halt-on-error main.tex && biber main && xelatex -interaction=nonstopmode -halt-on-error main.tex && xelatex -interaction=nonstopmode -halt-on-error main.tex

docx:
	python3 scripts/make_report_docx.py
