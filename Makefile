CC=gcc
CFLAGS=-Wall -O2
TARGET=stfu

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)

# Установка с SUID битом (рекомендуется)
install-suid: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chown root:root /usr/local/bin/$(TARGET)
	sudo chmod 4755 /usr/local/bin/$(TARGET)

# Проверка SUID
check-suid:
	ls -la /usr/local/bin/$(TARGET)

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all install install-suid check-suid uninstall clean
