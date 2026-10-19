````test
main.c
  ├── hal.h
  ├── ports/temp_sensor.h
  ├── ports/heater.h
  ├── ports/status_sink.h
  └── adapters/*.h

domain/thermostat.c
  └── domain/thermostat.h
        ├── ports/temp_sensor.h
        ├── ports/heater.h
        └── ports/status_sink.h

adapters/lm75_i2c.c
  ├── ports/temp_sensor.h
  └── hal.h

test/test_thermostat.c
  └── domain/thermostat.h
```

```
smart_temp ──> libdomain.a
           ──> libadapters.a ──> libhal.a
           ──> libhal.a

test_thermostat ──> libdomain.a
                    （故意不链接 libadapters、libhal）

```
```
LM75 --(i2c)--> lm75_i2c --(temp_sensor_read)--> thermostat
thermostat --(heater_set)--> gpio_heater --(gpio_set)--> 继电器
thermostat --(status_publish)--> uart_status --(uart_print)--> 串口
```