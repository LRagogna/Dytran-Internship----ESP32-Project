import serial
import serial.tools.list_ports
import time
from pathlib import Path

BAUD_RATE = 115200


def find_esp32():
    for port in serial.tools.list_ports.comports():
        description = (port.description or "").lower()

        if (
            "cp210" in description
            or "silicon labs" in description
            or "ch340" in description
            or "usb serial" in description
        ):
            return port.device

    return None


def connect():
    port = find_esp32()

    if port is None:
        raise RuntimeError("ESP32 not found.")

    print(f"Connecting to {port}...")

    ser = serial.Serial(
        port,
        BAUD_RATE,
        timeout=1
    )

    time.sleep(2)

    ser.reset_input_buffer()

    print("Connected.")

    return ser


def get_file_list(ser):
    ser.reset_input_buffer()

    ser.write(b"LIST\n")
    ser.flush()

    files = []
    started = False

    start_time = time.time()

    while True:

        if time.time() - start_time > 5:
            raise TimeoutError("Timed out waiting for file list.")

        raw = ser.readline()

        if not raw:
            continue

        line = raw.decode(
            "utf-8",
            errors="ignore"
        ).strip()

        if line == "LIST_START":
            started = True
            continue

        if line == "LIST_END":
            break

        if started and line.endswith(".csv"):
            files.append(line)

    return files


def download_file(ser, filename):
    ser.reset_input_buffer()

    clean_name = filename.lstrip("/")

    print(f"\nDownloading {clean_name}...")

    ser.write(
        f"GET {filename}\n".encode("utf-8")
    )

    ser.flush()

    lines = []
    receiving = False

    start_time = time.time()

    while True:

        if time.time() - start_time > 10:
            raise TimeoutError("Download timed out.")

        raw = ser.readline()

        if not raw:
            continue

        line = raw.decode(
            "utf-8",
            errors="ignore"
        ).rstrip("\r\n")

        if line == "ERROR:FILE_NOT_FOUND":
            raise RuntimeError("File not found on ESP32.")

        if line == "FILE_START":
            receiving = True
            continue

        if line == "FILE_END":
            break

        if receiving:
            lines.append(line)

    downloads_folder = Path.home() / "Downloads"

    destination = downloads_folder / clean_name

    with open(
        destination,
        "w",
        encoding="utf-8",
        newline=""
    ) as file:

        for line in lines:
            file.write(line + "\n")

    print(f"Downloaded successfully.")
    print(f"Saved to: {destination}")


def main():
    ser = None

    try:
        ser = connect()

        files = get_file_list(ser)

        if not files:
            print("No CSV files found on ESP32.")
            return

        print("\nCSV files on ESP32:\n")

        for i, filename in enumerate(files, start=1):
            print(f"{i}. {filename.lstrip('/')}")

        print()

        while True:
            choice = input(
                "Which file would you like to download? "
            ).strip()

            try:
                choice_number = int(choice)

                if 1 <= choice_number <= len(files):
                    break

            except ValueError:
                pass

            print("Invalid selection.")

        selected_file = files[choice_number - 1]

        download_file(
            ser,
            selected_file
        )

    except Exception as e:
        print(f"Error: {e}")

    finally:
        if ser is not None and ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()