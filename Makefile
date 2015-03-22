CFLAGS = `pkg-config --cflags gtk+-3.0 vte-2.91`
LDFLAGS = `pkg-config --libs gtk+-3.0 vte-2.91`
TARGET = bterm

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) -Wall $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	$(RM) $(TARGET)
