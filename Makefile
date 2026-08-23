# CrowPanel ESP32 5.79" E-Paper HMI (272x792, SSD1683 x2, ESP32-S3-WROOM-1-N8R8)

FQBN  := esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=huge_app,PSRAM=opi,CPUFreq=240,FlashMode=qio,UploadSpeed=460800
PORT  ?= $(shell arduino-cli board list | awk '/usbserial|usbmodem/ {print $$1; exit}')
SKETCH ?= sketches/hello_epaper
LIBS  := ./libraries

.PHONY: build upload flash monitor port clean material-txt sim sim-fetch

build:
	arduino-cli compile --fqbn "$(FQBN)" --libraries $(LIBS) $(SKETCH)

upload:
	arduino-cli upload --fqbn "$(FQBN)" -p $(PORT) $(SKETCH)

flash: build upload

monitor:
	arduino-cli monitor -p $(PORT) -c baudrate=115200

port:
	@echo "PORT = $(PORT)"
	@arduino-cli board list

clean:
	rm -rf $(SKETCH)/build

# Textversionen der PDFs in material/ — greppbar, mit Seitenmarken.
# Braucht poppler: brew install poppler
material-txt:
	python3 material/pdf2txt.py

# --- Simulator ---------------------------------------------------------------
# Baut den Sketch nativ und schreibt sein Bild als PNG, ohne Geraet. Verwendet
# den echten Zeichencode (EPD.cpp) und die echten Fonts; nur Arduino, WLAN und
# HTTP sind ersetzt (tools/simulator/arduino/). Braucht ImageMagick.
SIM_DIR := tools/simulator
SIM_OUT ?= $(SIM_DIR)/out.png

# Alle .cpp des Sketches ausser denen, die der Simulator selbst ersetzt:
# EPD_Init.cpp und spi.cpp lassen echte Pins wackeln, EPD.cpp zeichnet nur —
# das ist die Datei, auf die es ankommt. Sketches mit eigenen .cpp (ha_wechsel
# teilt sich auf mehrere auf) werden so ohne weiteres Zutun mituebersetzt.
SIM_SRCS = $(filter-out $(SKETCH)/EPD_Init.cpp $(SKETCH)/spi.cpp, \
                        $(wildcard $(SKETCH)/*.cpp))

sim-fetch:
	$(SIM_DIR)/fetch.sh $(SKETCH)

sim:
	@c++ -std=c++17 -O1 -w \
	    -I $(SIM_DIR)/arduino -I $(SKETCH) \
	    -DSKETCH_PATH='"$(abspath $(SKETCH))/$(notdir $(SKETCH)).ino"' \
	    $(SIM_DIR)/sim_main.cpp $(SIM_SRCS) \
	    -o $(SIM_DIR)/sim
	@$(SIM_DIR)/sim $(SIM_DIR)/data > $(SIM_DIR)/out.pbm
	@magick $(SIM_DIR)/out.pbm $(SIM_OUT) && rm -f $(SIM_DIR)/out.pbm
	@echo "-> $(SIM_OUT)"
