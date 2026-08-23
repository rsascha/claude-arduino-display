# CrowPanel ESP32 5.79" E-Paper HMI (272x792, SSD1683 x2, ESP32-S3-WROOM-1-N8R8)

FQBN  := esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=huge_app,PSRAM=opi,CPUFreq=240,FlashMode=qio,UploadSpeed=460800
PORT  ?= $(shell arduino-cli board list | awk '/usbserial|usbmodem/ {print $$1; exit}')
SKETCH ?= sketches/hello_epaper
LIBS  := ./libraries

.PHONY: build upload flash monitor port clean material-txt

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
