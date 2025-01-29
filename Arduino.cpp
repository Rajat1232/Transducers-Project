import tkinter as tk
from tkinter import ttk
import serial
import serial.tools.list_ports
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.animation import FuncAnimation
from collections import deque
import numpy as np
import pandas as pd
import time

def find_arduino_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if 'Arduino' in p.description:
            return p.device
    return None

class IrrigationSystem:
    def __init__(self, master):
        self.master = master
        self.master.title("Smart Irrigation System")
        
        self.setup_serial_connection()
        
        self.notebook = ttk.Notebook(self.master)
        self.notebook.pack(fill=tk.BOTH, expand=True)
        
        self.plot_tab = ttk.Frame(self.notebook)
        self.voltage_plot_tab = ttk.Frame(self.notebook)
        self.pump_tab = ttk.Frame(self.notebook)
        self.crop_suggestion_tab = ttk.Frame(self.notebook)
        
        self.notebook.add(self.plot_tab, text="Sensor Data")
        self.notebook.add(self.voltage_plot_tab, text="Voltage Outputs")
        self.notebook.add(self.pump_tab, text="Pump Status")
        self.notebook.add(self.crop_suggestion_tab, text="Crop Suggestions")
        
        self.pump_is_on = False
        
        # Initialize sensor data variables
        self.current_temp = 0
        self.current_humidity = 0
        self.current_moisture = 0
        
        # Load the crop database
        self.crop_db = pd.read_csv('crops.csv')
        
        self.setup_plot()
        self.setup_voltage_plot()
        self.setup_pump_animation()
        self.setup_crop_suggestion()

    def setup_serial_connection(self):
        arduino_port = find_arduino_port()
        if arduino_port is None:
            print("Arduino not found. Please check the connection.")
            arduino_port = input("Enter COM port manually (e.g., COM3): ")

        try:
            self.ser = serial.Serial(arduino_port, 9600, timeout=5)
            print(f"Connected to {arduino_port}")
            time.sleep(2)  # Allow time for the connection to establish
            self.ser.flushInput()  # Clear any residual data
        except Exception as e:
            print(f"Error opening serial port: {e}")
            self.ser = None

    def setup_plot(self):
        fig, ax = plt.subplots(figsize=(8, 4))
        canvas = FigureCanvasTkAgg(fig, master=self.plot_tab)
        canvas_widget = canvas.get_tk_widget()
        canvas_widget.pack(fill=tk.BOTH, expand=True)

        max_points = 100
        self.time_data = deque(maxlen=max_points)
        self.temp_data = deque(maxlen=max_points)
        self.humidity_data = deque(maxlen=max_points)
        self.moisture_data = deque(maxlen=max_points)

        self.line_temp, = ax.plot([], [], 'r-', label='Temperature (°C)')
        self.line_humidity, = ax.plot([], [], 'g-', label='Humidity (%)')
        self.line_moisture, = ax.plot([], [], 'b-', label='Moisture (%)')

        ax.set_xlabel('Time (s)')
        ax.set_ylabel('Value')
        ax.set_title('Sensor Data')
        ax.legend(loc='upper left')
        ax.set_ylim(0, 100)

        self.ani = FuncAnimation(fig, self.update_plot, interval=1000, blit=True, cache_frame_data=False)

    def setup_voltage_plot(self):
        fig, ax = plt.subplots(figsize=(8, 4))
        canvas = FigureCanvasTkAgg(fig, master=self.voltage_plot_tab)
        canvas_widget = canvas.get_tk_widget()
        canvas_widget.pack(fill=tk.BOTH, expand=True)

        max_points = 100
        self.voltage_time_data = deque(maxlen=max_points)
        self.temp_voltage_data = deque(maxlen=max_points)
        self.humidity_voltage_data = deque(maxlen=max_points)
        self.moisture_voltage_data = deque(maxlen=max_points)

        self.voltage_line_temp, = ax.plot([], [], 'r-', label='Temperature Voltage')
        self.voltage_line_humidity, = ax.plot([], [], 'g-', label='Humidity Voltage')
        self.voltage_line_moisture, = ax.plot([], [], 'b-', label='Moisture Voltage')

        ax.set_xlabel('Time (s)')
        ax.set_ylabel('Voltage')
        ax.set_title('Sensor Voltage Outputs')
        ax.legend(loc='upper left')
        ax.set_ylim(0, 5)  # Assuming 0-5V range

        self.voltage_ani = FuncAnimation(fig, self.update_voltage_plot, interval=1000, blit=True, cache_frame_data=False)

    def update_plot(self, frame):
        try:
            if self.ser and self.ser.is_open and self.ser.in_waiting:
                data = self.ser.readline().decode('utf-8').strip().split(',')
                print(f"Raw data: {data}")  # Debug print
                if len(data) == 8:  # time, temp, humidity, moisture, moisture_v, temp_v, humidity_v, pump
                    timestamp, temp, humidity, moisture, _, _, _, pump = map(float, data)
                    
                    self.time_data.append(timestamp / 1000)  # Convert to seconds
                    self.temp_data.append(temp)
                    self.humidity_data.append(humidity)
                    self.moisture_data.append(moisture)

                    self.line_temp.set_data(self.time_data, self.temp_data)
                    self.line_humidity.set_data(self.time_data, self.humidity_data)
                    self.line_moisture.set_data(self.time_data, self.moisture_data)

                    plt.gca().relim()
                    plt.gca().autoscale_view()

                    # Update current readings for crop suggestions
                    self.current_temp = temp
                    self.current_humidity = humidity
                    self.current_moisture = moisture

                    # Update pump status
                    self.update_pump_status(pump)

        except Exception as e:
            print(f"Error in update_plot: {e}")

        return self.line_temp, self.line_humidity, self.line_moisture

    def update_voltage_plot(self, frame):
        try:
            if self.ser and self.ser.is_open and self.ser.in_waiting:
                data = self.ser.readline().decode('utf-8').strip().split(',')
                if len(data) == 8:  # time, temp, humidity, moisture, moisture_v, temp_v, humidity_v, pump
                    timestamp, _, _, _, moisture_v, temp_v, humidity_v, _ = map(float, data)
                    
                    self.voltage_time_data.append(timestamp / 1000)  # Convert to seconds
                    self.temp_voltage_data.append(temp_v)
                    self.humidity_voltage_data.append(humidity_v)
                    self.moisture_voltage_data.append(moisture_v)

                    self.voltage_line_temp.set_data(self.voltage_time_data, self.temp_voltage_data)
                    self.voltage_line_humidity.set_data(self.voltage_time_data, self.humidity_voltage_data)
                    self.voltage_line_moisture.set_data(self.voltage_time_data, self.moisture_voltage_data)

                    plt.gca().relim()
                    plt.gca().autoscale_view()

        except Exception as e:
            print(f"Error in update_voltage_plot: {e}")

        return self.voltage_line_temp, self.voltage_line_humidity, self.voltage_line_moisture

    def setup_pump_animation(self):
        self.pump_canvas = tk.Canvas(self.pump_tab, width=300, height=300, bg='white')
        self.pump_canvas.pack(expand=True)

        self.pump_body = self.pump_canvas.create_rectangle(100, 100, 200, 200, fill='lightgray', outline='black', width=2)
        self.pump_label = self.pump_canvas.create_text(150, 220, text="PUMP", font=("Arial", 16, "bold"))
        self.status_indicator = self.pump_canvas.create_oval(130, 130, 170, 170, fill='red')
        self.status_text = self.pump_canvas.create_text(150, 150, text="OFF", font=("Arial", 14, "bold"), fill='white')

        self.water_flow = []
        for i in range(5):
            x = 200 + i * 20
            y = 150
            drop = self.pump_canvas.create_oval(x-5, y-5, x+5, y+5, fill='blue', state='hidden')
            self.water_flow.append(drop)

        self.update_animation()

    def update_pump_status(self, status):
        if status == 1 and not self.pump_is_on:
            self.pump_is_on = True
            self.pump_canvas.itemconfig(self.status_indicator, fill='green')
            self.pump_canvas.itemconfig(self.status_text, text="ON")
        elif status == 0 and self.pump_is_on:
            self.pump_is_on = False
            self.pump_canvas.itemconfig(self.status_indicator, fill='red')
            self.pump_canvas.itemconfig(self.status_text, text="OFF")

    def update_animation(self):
        if self.pump_is_on:
            self.move_water_drops()
        else:
            for drop in self.water_flow:
                self.pump_canvas.itemconfig(drop, state='hidden')
        
        self.master.after(50, self.update_animation)

    def move_water_drops(self):
        for drop in self.water_flow:
            self.pump_canvas.itemconfig(drop, state='normal')
            self.pump_canvas.move(drop, 10, 0)
            x1, y1, x2, y2 = self.pump_canvas.coords(drop)
            if x2 > 300:
                self.pump_canvas.coords(drop, 195, y1, 205, y2)

    def setup_crop_suggestion(self):
        ttk.Label(self.crop_suggestion_tab, text="Current Readings:").grid(row=0, column=0, columnspan=2, pady=10)
        
        self.temp_label = ttk.Label(self.crop_suggestion_tab, text="Temperature: N/A")
        self.temp_label.grid(row=1, column=0, columnspan=2)
        
        self.humidity_label = ttk.Label(self.crop_suggestion_tab, text="Humidity: N/A")
        self.humidity_label.grid(row=2, column=0, columnspan=2)
        
        self.moisture_label = ttk.Label(self.crop_suggestion_tab, text="Soil Moisture: N/A")
        self.moisture_label.grid(row=3, column=0, columnspan=2)

        ttk.Button(self.crop_suggestion_tab, text="Refresh Suggestions", command=self.suggest_crops).grid(row=4, column=0, columnspan=2, pady=10)

        self.suggestion_text = tk.Text(self.crop_suggestion_tab, height=10, width=50)
        self.suggestion_text.grid(row=5, column=0, columnspan=2, padx=5, pady=5)

    def suggest_crops(self):
        self.temp_label.config(text=f"Temperature: {self.current_temp:.1f}°C")
        self.humidity_label.config(text=f"Humidity: {self.current_humidity:.1f}%")
        self.moisture_label.config(text=f"Soil Moisture: {self.current_moisture:.1f}%")

        suitable_crops = []

        for _, crop in self.crop_db.iterrows():
            if (crop['min_temp'] <= self.current_temp <= crop['max_temp'] and
                crop['min_rainfall'] <= self.current_humidity <= crop['max_rainfall'] and
                crop['min_soil_moisture'] <= self.current_moisture <= crop['max_soil_moisture']):
                suitable_crops.append(crop['crop_name'])

        if suitable_crops:
            self.suggestion_text.delete(1.0, tk.END)
            self.suggestion_text.insert(tk.END, "Suggested crops:\n\n" + "\n".join(suitable_crops))
        else:
            self.suggestion_text.delete(1.0, tk.END)
            self.suggestion_text.insert(tk.END, "No specific crop suggestions for the current conditions. Consider adjusting the environment or consulting with a local agricultural expert.")

def main():
    root = tk.Tk()
    app = IrrigationSystem(root)
    root.mainloop()
    return app

if __name__ == "__main__":
    app = None
    try:
        app = main()
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        if app and hasattr(app, 'ser') and app.ser and app.ser.is_open:
            app.ser.close()
            print("Serial port closed.")
