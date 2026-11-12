import sys, time, serial

PORT = "COM9"
BAUD = 115200
OUT = "mic_raw.log"

ser = serial.Serial(PORT, BAUD, timeout=1)
print("logging to", OUT, "- reset the board now if it does not restart by itself")
with open(OUT, "w", encoding="ascii", errors="replace", newline="\n") as f:
    t0 = time.time()
    while True:
        line = ser.readline().decode("ascii", errors="replace").rstrip("\r\n")
        if not line:
            if time.time() - t0 > 120:
                print("timeout, no raw end seen")
                break
            continue
        f.write(line + "\n")
        if "[mic] raw" not in line:
            print(line)
        if line.startswith("[mic] raw end"):
            print("got raw end, done")
            break
ser.close()