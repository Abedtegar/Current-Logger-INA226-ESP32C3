import asyncio
import threading
import csv
import os
import queue
import time
from datetime import datetime
from bleak import BleakClient

ADDRESS = "20:6E:F1:6B:C2:AA"
CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
LOG_BASE_DIR = "logs"
OUT_DIR = None
CSV_FILE = None

# Thread-safe queue to pass parsed rows to the plotting loop (main thread)
q = queue.Queue()

def init_output_dir():
    global OUT_DIR, CSV_FILE
    now = datetime.now()
    dir_name = now.strftime("%Y%m%d_%H%M%S")
    OUT_DIR = os.path.join(LOG_BASE_DIR, dir_name)
    os.makedirs(OUT_DIR, exist_ok=True)
    CSV_FILE = os.path.join(OUT_DIR, "data_log.csv")


def save_row_to_csv(row):
    # row is expected: [device_ts, pc_ts, bus_v, shunt, current, power]
    global CSV_FILE
    # Ensure output dir and CSV file are initialized (lazy init)
    if CSV_FILE is None:
        try:
            init_output_dir()
        except Exception as e:
            print("Failed to initialize output dir:", e)
            # fallback: try to create base log dir and set a fallback CSV path
            try:
                os.makedirs(LOG_BASE_DIR, exist_ok=True)
                CSV_FILE = os.path.join(LOG_BASE_DIR, "fallback_data_log.csv")
                print("Using fallback CSV:", CSV_FILE)
            except Exception as e2:
                print("Failed to create fallback CSV path:", e2)
                return
    # Ensure CSV_FILE is a valid string path before using os.path functions
    if not CSV_FILE:
        print("CSV_FILE not set; skipping save")
        return

    try:
        write_header = not os.path.exists(CSV_FILE) or os.path.getsize(CSV_FILE) == 0
    except Exception:
        write_header = True

    try:
        with open(CSV_FILE, "a", newline='') as f:
            writer = csv.writer(f)
            if write_header:
                writer.writerow(["device_timestamp", "pc_timestamp", "bus_voltage", "shunt_voltage_mV", "current_mA", "power_mW"])
            writer.writerow(row)
    except Exception as e:
        print("Error writing CSV:", e, "row:", row)

def parse_and_handle(data_bytes):
    try:
        s = data_bytes.decode().strip()
    except Exception:
        # if data is already str
        s = str(data_bytes).strip()

    parts = [p.strip() for p in s.split(',')]
    if len(parts) < 5:
        print("Unexpected format (need 5 fields):", s)
        return

    try:
        device_ts = parts[0]
        bus_v = float(parts[1])
        shunt = float(parts[2])
        current = float(parts[3])
        power = float(parts[4])
        pc_ts = datetime.now().isoformat(sep=' ')
        row = [device_ts, pc_ts, bus_v, shunt, current, power]
        save_row_to_csv(row)
        q.put(row)
        print("Saved:", row)
    except Exception as e:
        print("Parse error:", e, "data:", s)

async def ble_run(address, char_uuid):
    # Notification handler called on data arrival
    def notif_handler(sender, data):
        parse_and_handle(data)

    RECONNECT_DELAY = 5
    print(f"BLE runner started for {address}, char {char_uuid}")
    while True:
        try:
            async with BleakClient(address) as client:
                try:
                    connected = client.is_connected
                    print("Connected:", connected)
                    if not connected:
                        # sometimes client isn't connected yet; try explicit connect
                        await client.connect()
                        connected = client.is_connected
                        print("Connected after connect():", connected)

                    if not connected:
                        print("Failed to connect to device", address)
                        raise ConnectionError("connect failed")

                    # register notification callback and start notify
                    await client.start_notify(char_uuid, notif_handler)
                    print("Started notify, listening...")

                    # wait while connected; if disconnected, exit this with-block and retry
                    while client.is_connected:
                        await asyncio.sleep(1)

                    print("Device disconnected, will attempt reconnect in", RECONNECT_DELAY, "s")
                except Exception as e:
                    print("BLE session error:", e)
        except Exception as e:
            print("BLE connection failed:", e)

        # wait before reconnecting
        await asyncio.sleep(RECONNECT_DELAY)

def ble_thread():
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    try:
        loop.run_until_complete(ble_run(ADDRESS, CHAR_UUID))
    except Exception as e:
        print("BLE thread exception:", e)
    finally:
        try:
            loop.run_until_complete(loop.shutdown_asyncgens())
        except Exception:
            pass
        loop.close()

def plot_main():
    try:
        import matplotlib.pyplot as plt
        import matplotlib.dates as mdates
    except Exception:
        print("matplotlib not installed. Running CSV-only logging.")
        print("Install with: python -m pip install -r requirements.txt")
        try:
            while True:
                while not q.empty():
                    row = q.get()
                    print("Logged (CSV):", row)
                time.sleep(1)
        except KeyboardInterrupt:
            print("CSV-only logging stopped by user")
        return

    plt.ion()
    fig, ax = plt.subplots()

    times = []  # list of datetime objects (PC timestamps)
    bus_v = []
    shunt = []
    current = []
    power = []

    ln1, = ax.plot_date([], [], '-', label='Bus V')
    ln2, = ax.plot_date([], [], '-', label='Shunt mV')
    ln3, = ax.plot_date([], [], '-', label='Current mA')
    ln4, = ax.plot_date([], [], '-', label='Power mW')
    ax.legend()
    ax.set_xlabel('Time (PC)')
    ax.set_ylabel('Value')
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))

    # Text box to show running averages and sample count
    avg_text = ax.text(
        0.01,
        0.95,
        '',
        transform=ax.transAxes,
        fontsize=9,
        verticalalignment='top',
        bbox=dict(facecolor='white', alpha=0.7, edgecolor='none')
    )

    maxlen = 200
    try:
        while True:
            updated = False
            while not q.empty():
                row = q.get()
                # row: [device_ts, pc_ts, bus_v, shunt, current, power]
                try:
                    pc_dt = datetime.fromisoformat(row[1])
                except Exception:
                    # fallback: parse simple space-separated
                    pc_dt = datetime.now()
                times.append(pc_dt)
                bus_v.append(row[2])
                shunt.append(row[3])
                current.append(row[4])
                power.append(row[5])
                updated = True

            if len(bus_v) > maxlen:
                times = times[-maxlen:]
                bus_v = bus_v[-maxlen:]
                shunt = shunt[-maxlen:]
                current = current[-maxlen:]
                power = power[-maxlen:]

            if updated:
                x_nums = mdates.date2num(times)
                ln1.set_data(x_nums, bus_v)
                ln2.set_data(x_nums, shunt)
                ln3.set_data(x_nums, current)
                ln4.set_data(x_nums, power)
                # compute averages for visible samples
                try:
                    n = len(bus_v)
                    if n > 0:
                        avg_bus = sum(bus_v) / n
                        avg_shunt = sum(shunt) / n
                        avg_current = sum(current) / n
                        avg_power = sum(power) / n
                        avg_lines = (
                            f"Samples: {n}\n"
                            f"Avg BusV: {avg_bus:.3f}\n"
                            f"Avg Shunt mV: {avg_shunt:.3f}\n"
                            f"Avg Current mA: {avg_current:.3f}\n"
                            f"Avg Power mW: {avg_power:.3f}"
                        )
                    else:
                        avg_lines = "No samples"
                except Exception:
                    avg_lines = "Avg calc error"
                avg_text.set_text(avg_lines)
                ax.relim()
                ax.autoscale_view()
                fig.autofmt_xdate()
                fig.canvas.draw()
                fig.canvas.flush_events()

            plt.pause(0.1)
    except KeyboardInterrupt:
        print("Plotting stopped by user")

if __name__ == "__main__":
    # create timestamped output folder and CSV file before any saves
    init_output_dir()

    t = threading.Thread(target=ble_thread, daemon=True)
    t.start()
    plot_main()
