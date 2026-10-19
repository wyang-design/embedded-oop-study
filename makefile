CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Werror -std=c11 -I.

all: smart_temp test_thermostat

smart_temp: main.o libhal.a libdomain.a libadapters.a
	$(CC) $(CFLAGS) -o $@ $^

libhal.a:hal/hal_pc.o
	ar rcs $@ $^

libdomain.a:domain/thermostat.o
	ar rcs $@ $^

libadapters.a:adapters/uart_status.o adapters/gpio_heater.o adapters/lm75_i2c.o
	ar rcs $@ $^


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


run: smart_temp
	./smart_temp

test: test_thermostat
	./test_thermostat

clean:
	rm -f *.o domain/*.o adapters/*.o hal/*.o test/*.o
	rm -f libdomain.a libadapters.a libhal.a
	rm -f smart_temp test_thermostat