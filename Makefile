HEADERS = config.h main.h rc522.h rfid.h
SOURCES = config.c main.c rc522.c rfid.c

rc522_reader: $(HEADERS) $(SOURCES)
	gcc $(SOURCES) -o rc522_reader




