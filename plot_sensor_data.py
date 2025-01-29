import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque
import numpy as np

# Configure the serial port
ser = serial.Serial('COM3', 9600, timeout=1) # Replace 'COM3' with your Arduino's port

# Create figure and subplot
fig, ax = plt.subplots(figsize=(10,6))

# Create empty deques to store data
max_points = 100
time_data = deque(maxlen=max_points)
temp_data = deque(maxlen=max_points)
humidity_data = deque(maxlen=max_points)
moisture_data = deque(maxlen=max_points)

# Initialize lines for plotting
line_temp, = ax.plot([], [], 'r-', label='Temperature Voltage')
line_humidity, = ax.plot([], [], 'g-', label='Humidity Voltage')
line_moisture, = ax.plot([], [], 'b-', label='Moisture Voltage')

# Set up plot labels and title
ax.set_xlabel('Time (s)')
ax.set_ylabel('Voltage')
ax.set_title('Temperature Voltage vs Humidity Voltage vs Moisture Voltage')
ax.legend(loc='upper left')

def init():
    ax.set_ylim(0,5) # Adjust y-axis for voltage range [0V-5V]
    return line_temp,line_humidity,line_moisture

def calculate_slope(x,y):
    if len(x) >1 :
        slope = np.polyfit(x,y ,1)[0]
        return slope 
    else:
        return None 

def animate(i):
    try:
        if ser.in_waiting:
            data = ser.readline().decode().strip().split(',')
            
            if len(data)==5: # timestamp,temp_voltage ,humidity_voltage ,moisture_voltage,pump_status 
                timestamp,temp_voltage ,humidity_voltage ,moisture_voltage ,_=map(float,data)

                time_data.append(timestamp/1000 ) # convert ms->seconds 
                temp_data.append(temp_voltage )
                humidity_data.append(humidity_voltage )
                moisture_data.append(moisture_voltage )

                ax.set_xlim(min(time_data),max(time_data))

                line_temp.set_data(time_data,temp_data)
                line_humidity.set_data(time_data ,humidity_data )
                line_moisture.set_data(time_data ,moisture_data)

                #calculate slopes :
                temp_slope=calculate_slope(time_data,temp_data )
                humidity_slope=calculate_slope(time_data ,humidity_data )
                moisture_slope=calculate_slope(time_data ,moisture_data )

                print(f"Slopes -> Temp:{temp_slope} Humidity:{humidity_slope} Moisture:{moisture_slope}")

    except Exception as e:
        print(f"Error:{e}")

    return line_temp,line_humidity,line_moisture 

ani=FuncAnimation(fig ,animate ,init_func=init ,interval=100 ,blit=True)

plt.show()

ser.close()
