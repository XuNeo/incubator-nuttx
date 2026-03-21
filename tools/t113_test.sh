#!/bin/bash
set -e

NUTTX_DIR="$(cd "$(dirname "$0")/.." && pwd)"
APPS_DIR="$NUTTX_DIR/../apps"
JLINK_SN=941000024
JLINK_DEVICE=R528S3-Core0
DDR_ADDR=0x40000000
ENTRY_ADDR=0x40000040
WDT_SOFT_RST=0x020500A8

CONFIGS="nsh nsh_smp adb usbnsh composite spinand"

die() { echo "FAIL: $*" >&2; exit 1; }

jlink_cmd() {
    printf '%s\n' "$@" "exit" | timeout 15 JLinkExe \
        -device $JLINK_DEVICE -if JTAG -speed 20000 -nogui 1 \
        -autoconnect 1 -SelectEmuBySN $JLINK_SN 2>&1
}

reset_to_fel() {
    for i in 1 2 3; do
        jlink_cmd "connect" "halt" "go" "w4 $WDT_SOFT_RST, 0x16AA0001" "sleep 2000" >/dev/null
        sleep 5
        if xfel version 2>&1 | grep -q "AWUSBFEX"; then return 0; fi
        echo "  FEL retry $i..."
    done
    die "Cannot enter FEL mode"
}

deploy() {
    local bin="$1"
    xfel ddr t113-s3 >/dev/null 2>&1
    sleep 1
    xfel write $DDR_ADDR "$bin" 2>&1 | tail -1
    jlink_cmd "halt" "SetPC $ENTRY_ADDR" "go" "sleep 100" >/dev/null
}

build_config() {
    local cfg="$1"
    echo -n "  Build $cfg... "
    rm -rf "$NUTTX_DIR/build_$cfg"
    cmake -B "$NUTTX_DIR/build_$cfg" -GNinja \
        -DBOARD_CONFIG=t113-evb:$cfg \
        -DNUTTX_APPS_DIR="$APPS_DIR" >/dev/null 2>&1
    if ninja -C "$NUTTX_DIR/build_$cfg" 2>&1 | grep -q "Generating System.map"; then
        echo "OK"
    else
        echo "FAILED"
        return 1
    fi
}

verify_serial_cmd() {
    local cmd="$1" expect="$2" timeout_s="${3:-5}"
    sleep 1
    tmux send-keys -t serial "$cmd" Enter
    sleep "$timeout_s"
    tmux capture-pane -p -J -t serial -S -20 2>/dev/null
}

verify_usbnsh() {
    sleep 8
    local dev
    dev=$(ls -t /dev/ttyACM* 2>/dev/null | head -1)
    [ -z "$dev" ] && die "usbnsh: no ttyACM device"
    timeout 15 python3 -uc "
import serial, time
s = serial.Serial('$dev', 115200, timeout=5)
s.dtr = True
time.sleep(2)
for _ in range(3):
    s.write(b'\r')
    time.sleep(0.3)
time.sleep(2)
data = s.read(8192)
assert b'nsh>' in data, f'No nsh prompt: {data[:100]}'
s.write(b'hello\r\n')
time.sleep(2)
data = s.read(4096)
assert b'Hello, World!!' in data, f'No hello: {data[:100]}'
print('PASS')
s.close()
" 2>&1
}

verify_adb() {
    sleep 5
    tmux send-keys -t serial 'adbd &' Enter
    sleep 5
    adb kill-server >/dev/null 2>&1
    sleep 1
    adb start-server >/dev/null 2>&1
    sleep 2
    adb devices 2>&1 | grep -q "device$" || die "adb: device not online"
    local out
    out=$(timeout 10 adb -s 1234 shell "hello" 2>&1)
    echo "$out" | grep -q "Hello, World!!" || die "adb shell hello failed: $out"
    echo "PASS"
}

verify_composite() {
    sleep 3
    tmux send-keys -t serial 'conn' Enter
    sleep 3
    lsusb -t 2>/dev/null | grep -q "cdc_acm.*480M" || die "composite: no CDC-ACM 480M"
    echo "PASS"
}

# --- Main ---

case "${1:-all}" in
    build)
        echo "=== Building all configs ==="
        for cfg in $CONFIGS; do
            build_config "$cfg" || die "Build failed: $cfg"
        done
        echo "All builds OK"
        ;;

    test)
        echo "=== Hardware verification ==="
        for cfg in nsh nsh_smp adb usbnsh composite spinand; do
            echo "--- $cfg ---"
            reset_to_fel
            deploy "$NUTTX_DIR/build_$cfg/nuttx.bin"

            case $cfg in
                nsh)
                    out=$(verify_serial_cmd "hello" "Hello" 3)
                    echo "$out" | grep -q "Hello, World!!" && echo "  hello: PASS" || die "$cfg: hello failed"
                    ;;
                nsh_smp)
                    out=$(verify_serial_cmd "hello" "Hello" 3)
                    echo "$out" | grep -q "Hello, World!!" && echo "  hello: PASS" || die "$cfg: hello failed"
                    ;;
                adb)
                    echo -n "  adb shell: "
                    verify_adb
                    ;;
                usbnsh)
                    echo -n "  usb console: "
                    verify_usbnsh
                    ;;
                composite)
                    echo -n "  conn: "
                    verify_composite
                    ;;
                spinand)
                    out=$(verify_serial_cmd "hello" "Hello" 3)
                    echo "$out" | grep -q "Hello, World!!" && echo "  hello: PASS" || die "$cfg: hello failed"
                    ;;
            esac
        done
        echo "=== All tests PASSED ==="
        ;;

    *)
        echo "Usage: $0 {build|test}"
        echo "  build  - Build all 6 configs"
        echo "  test   - Deploy and verify each config on hardware"
        ;;
esac
