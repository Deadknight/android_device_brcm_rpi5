#!/system/bin/sh

KMSG=/dev/kmsg
DRIVER=/sys/bus/serial/drivers/hci_uart_bcm
DEVICE=serial0-0

mark() {
    echo "$1" > "$KMSG"
}

mark ARDA_PI5_BT_SELFHEAL_V4_START

if ! /system/bin/toybox dmesg | /system/bin/toybox grep -Fq 'BCM: Reset failed (-110)'; then
    mark ARDA_PI5_BT_SELFHEAL_V4_NOOP_HEALTHY
    exit 0
fi

mark ARDA_PI5_BT_SELFHEAL_V4_FAILURE_SEEN

if [ ! -e "$DRIVER/$DEVICE" ]; then
    mark ARDA_PI5_BT_SELFHEAL_V4_SERIAL_NOT_BOUND
    exit 2
fi

mark ARDA_PI5_BT_SELFHEAL_V4_UNBIND
echo "$DEVICE" > "$DRIVER/unbind" || {
    mark ARDA_PI5_BT_SELFHEAL_V4_UNBIND_FAILED
    exit 3
}

/system/bin/sleep 1

mark ARDA_PI5_BT_SELFHEAL_V4_BIND
echo "$DEVICE" > "$DRIVER/bind" || {
    mark ARDA_PI5_BT_SELFHEAL_V4_BIND_FAILED
    exit 4
}

i=0
while [ "$i" -lt 12 ]; do
    if [ -e /sys/class/bluetooth/hci0 ]; then
        mark ARDA_PI5_BT_SELFHEAL_V4_HCI0_READY
        /system/bin/sleep 2

        attempt=0
        while [ "$attempt" -lt 5 ]; do
            mark ARDA_PI5_BT_SELFHEAL_V4_FRAMEWORK_ENABLE
            /system/bin/cmd bluetooth_manager enable >/dev/null 2>&1 \
                || /system/bin/svc bluetooth enable >/dev/null 2>&1 \
                || true

            /system/bin/sleep 1

            if /system/bin/dumpsys bluetooth_manager 2>/dev/null \
                | /system/bin/toybox grep -Fq 'state: ON'; then
                mark ARDA_PI5_BT_SELFHEAL_V4_DONE
                exit 0
            fi

            attempt=$((attempt + 1))
        done

        mark ARDA_PI5_BT_SELFHEAL_V4_FRAMEWORK_ON_TIMEOUT
        exit 5
    fi

    /system/bin/sleep 1
    i=$((i + 1))
done

mark ARDA_PI5_BT_SELFHEAL_V4_HCI0_TIMEOUT
exit 6
