#!/usr/bin/env python3
"""0.8.29 mic ADC probe: host-mock execution tests, no ESP board required."""
from pathlib import Path
import subprocess
import tempfile
from textwrap import dedent

root=Path(__file__).resolve().parents[1]
source=(root/"main/mic_probe.c").read_text()
network=(root/"main/network.c").read_text()
web=(root/"main/web_device_html.h").read_text()
cmake=(root/"main/CMakeLists.txt").read_text()
assert "i2c_master_probe(bus, LF_ES7210_I2C_ADDR" in source
assert "bsp_i2c_get_handle()" in source
assert "mic_probe.c" in cmake
assert '"/api/hardware/mic-check"' in network
assert "'/api/hardware/mic-check'" in web
assert "i2s_channel_read" not in source and "esp_http_client" not in source

with tempfile.TemporaryDirectory(prefix="lf_mic_probe_") as tmp:
    base=Path(tmp)
    (base/"bsp").mkdir()
    (base/"driver").mkdir()
    (base/"esp_err.h").write_text(dedent("""
        #pragma once
        typedef int esp_err_t;
        #define ESP_OK 0
        #define ESP_ERR_INVALID_STATE 0x103
        #define ESP_ERR_NOT_FOUND 0x105
        #define ESP_ERR_TIMEOUT 0x107
    """))
    (base/"driver/i2c_master.h").write_text(dedent("""
        #pragma once
        #include <stdint.h>
        #include "esp_err.h"
        typedef void *i2c_master_bus_handle_t;
        esp_err_t i2c_master_probe(i2c_master_bus_handle_t, uint16_t, int);
    """))
    (base/"bsp/esp-bsp.h").write_text(dedent("""
        #pragma once
        #include "driver/i2c_master.h"
        i2c_master_bus_handle_t bsp_i2c_get_handle(void);
    """))
    (base/"test.c").write_text(dedent("""
        #include "mic_probe.h"
        #include "driver/i2c_master.h"
        #include <assert.h>
        #include <string.h>
        static int fake_bus;
        static int bus_ready=1;
        static int calls=0;
        static esp_err_t incoming=ESP_OK;
        i2c_master_bus_handle_t bsp_i2c_get_handle(void)
        {return bus_ready?&fake_bus:NULL;}
        esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus, uint16_t addr, int ms)
        {
            assert(bus==&fake_bus);
            assert(addr==0x40);
            assert(ms==100);
            calls++;
            return incoming;
        }
        int main(void)
        {
            lf_mic_probe_snapshot_t s={0};
            lf_mic_probe_snapshot(&s);
            assert(s.result==LF_MIC_NEVER_TESTED && s.attempts==0);
            assert(!strcmp(lf_mic_probe_result_text(s.result),"not_tested"));

            assert(lf_mic_probe_run()==LF_MIC_PRESENT);
            lf_mic_probe_snapshot(&s);
            assert(s.result==LF_MIC_PRESENT && s.attempts==1 && calls==1);

            incoming=ESP_ERR_NOT_FOUND;
            assert(lf_mic_probe_run()==LF_MIC_NOT_DETECTED);
            lf_mic_probe_snapshot(&s);
            assert(s.result==LF_MIC_NOT_DETECTED && s.attempts==2 && calls==2);

            bus_ready=0;
            assert(lf_mic_probe_run()==LF_MIC_BUS_NOT_READY);
            lf_mic_probe_snapshot(&s);
            assert(s.error_code==ESP_ERR_INVALID_STATE && s.attempts==3);
            assert(calls==2);

            bus_ready=1;
            incoming=ESP_ERR_TIMEOUT;
            assert(lf_mic_probe_run()==LF_MIC_BUS_ERROR);
            lf_mic_probe_snapshot(&s);
            assert(s.result==LF_MIC_BUS_ERROR && s.attempts==4 && calls==3);
            assert(s.error_code==ESP_ERR_TIMEOUT);
            return 0;
        }
    """))
    binary=base/"test"
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-pedantic",
                    "-I",str(base),"-I",str(root/"main"),
                    str(root/"main/mic_probe.c"),str(base/"test.c"),
                    "-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print("PASS: manual ES7210 I2C probe host-mock tests: detected, absent, bus missing, timeout")
