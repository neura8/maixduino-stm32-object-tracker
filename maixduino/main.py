import sensor
import image
import lcd
import time
from machine import UART
from fpioa_manager import fm

# Maixduino physical pin 10 = K210 IO12
fm.register(12, fm.fpioa.UART1_TX, force=True)

uart = UART(
    UART.UART1,
    115200,
    8,
    None,
    1,
    timeout=1000,
    read_buf_len=64
)

#
red_threshold = (20, 100, 30, 127, 15, 127)
min_pixels = 300
min_area = 300

# Variables
center_x = 160
center_y = 120

pan_base = 90
tilt_base = 90

pan_offset = 0
tilt_offset = 0

step = 3

dead_zone_x = 20
dead_zone_y = 20

pan_dir = -1
tilt_dir = 1

max_offset = 90

return_to_base_ms = 2000

# smooth return settings
return_step = 3
return_interval_ms = 5

# cam + LCD
lcd.init(freq=15000000)
lcd.rotation(2)
sensor.reset()
sensor.set_hmirror(1)
sensor.set_vflip(1)
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)

# image color settings
sensor.set_auto_gain(True)
sensor.set_auto_whitebal(True)

sensor.skip_frames(time=3000)

# После настройки фиксируем параметры,
# чтобы цвет не менялся во время распознавания

sensor.set_auto_gain(False)
sensor.set_auto_whitebal(False)

clock = time.clock()

last_seen_time = time.ticks_ms()
returned_to_base = False

last_return_step_time = time.ticks_ms()

# send start position to STM32
uart.write("%d,%d\n" % (pan_base, tilt_base))
print("Start position:", pan_base, tilt_base)

while True:
    clock.tick()

    img = sensor.snapshot()

    #draw dead zone
    img.draw_rectangle(
        (
            center_x - dead_zone_x,
            center_y - dead_zone_y,
            dead_zone_x * 2,
            dead_zone_y * 2
        ),
        color=(0, 255, 0),
        thickness=1)

    blobs = img.find_blobs(
            [red_threshold],
            pixels_threshold = min_pixels,
            area_threshold = min_area,
            merge = True)

    if blobs:
        biggest_blob = None
        biggest_pixels = 0

        # find biggest blob
        for b in blobs:
            if b[4] > biggest_pixels:
                biggest_blob = b
                biggest_pixels = b[4]

        if biggest_blob:
            last_seen_time = time.ticks_ms()
            returned_to_base = False

            last_return_step_time = time.ticks_ms()

            object_x = biggest_blob.cx()
            object_y = biggest_blob.cy()

            # obj frame
            img.draw_rectangle(biggest_blob.rect(), color=(255, 0, 0), thickness=2)

            # object center
            img.draw_cross(object_x, object_y, color=(255, 0, 0))

            # message
            img.draw_string(40, 5, "Red object detected!", color=(255, 0, 0), scale=2)

            # error
            error_x = object_x - center_x
            error_y = object_y - center_y
            old_pan_offset = pan_offset
            old_tilt_offset = tilt_offset

            # Pan
            if abs(error_x) > 100:
                pan_step = 6
            elif abs(error_x) > 50:
                pan_step = 4
            else:
                pan_step = 1

            if error_x > dead_zone_x:
                pan_offset += pan_dir * pan_step

            elif error_x < -dead_zone_x:
                pan_offset -= pan_dir * pan_step

            # Tilt
            if abs(error_y) > 80:
                tilt_step = 6
            elif abs(error_y) > 40:
                tilt_step = 4
            else:
                tilt_step = 1

            if error_y > dead_zone_y:
                tilt_offset += tilt_dir * tilt_step

            elif error_y < -dead_zone_y:
                tilt_offset -= tilt_dir * tilt_step

            # limit pan
            if pan_offset > max_offset:
                pan_offset = max_offset
            elif pan_offset < -max_offset:
                pan_offset = -max_offset

            # limit tilt
            if tilt_offset > max_offset:
                tilt_offset = max_offset
            elif tilt_offset < -max_offset:
                tilt_offset = -max_offset

            # servo angles
            pan_angle = pan_base + pan_offset
            tilt_angle = tilt_base + tilt_offset

            # send UART if position is changed
            if (pan_offset != old_pan_offset or tilt_offset != old_tilt_offset):
                uart.write("%d,%d\n" % (pan_angle, tilt_angle))
                print("Pan:", pan_angle,
                    "Tilt:", tilt_angle,
                    "x:", object_x,
                    "y:", object_y)
    else:
        img.draw_string(40, 5, "No red object", color=(255, 255, 255), scale=2)

        # how long object has been missing
        lost_time = time.ticks_diff(
            time.ticks_ms(),
            last_seen_time)

        if lost_time > return_to_base_ms and not returned_to_base:

            now = time.ticks_ms()

            if time.ticks_diff(now, last_return_step_time) >= return_interval_ms:

                # smooth PAN return
                if pan_offset > 0:
                    pan_offset -= return_step

                    if pan_offset < 0:
                        pan_offset = 0

                elif pan_offset < 0:
                    pan_offset += return_step

                    if pan_offset > 0:
                        pan_offset = 0


                # smooth TILT return
                if tilt_offset > 0:
                    tilt_offset -= return_step

                    if tilt_offset < 0:
                        tilt_offset = 0

                elif tilt_offset < 0:
                    tilt_offset += return_step

                    if tilt_offset > 0:
                        tilt_offset = 0


                # calculate current angles
                pan_angle = pan_base + pan_offset
                tilt_angle = tilt_base + tilt_offset


                # send next small movement to STM32
                uart.write("%d,%d\n" % (pan_angle, tilt_angle))

                last_return_step_time = now

                # base position reached
                if pan_offset == 0 and tilt_offset == 0:
                    returned_to_base = True

    lcd.display(img)

