#!/usr/bin/env python3
"""
STM32L475E-IoT01A1 MQTT Data Capture and Monitoring Tool

Features:
- Connect to HiveMQ broker and subscribe to sensor telemetry data
- Real-time display of sensor data and system status
- Log data to CSV files
- Performance metrics monitoring and reporting
- Data visualization

Author: Your Name
Version: V1.0.0
Date: 2025-11-07
"""

import json
import csv
import time
import threading
import argparse
from datetime import datetime
from collections import deque
import paho.mqtt.client as mqtt
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.dates import DateFormatter
import numpy as np

class STM32SensorMonitor:
    def __init__(self, broker_host="broker.hivemq.com", broker_port=1883, 
                 topic="stm32/sensor/telemetry"):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.topic = topic
        
        # Data storage
        self.sensor_data = deque(maxlen=1000)  # Recent 1000 data points
        self.timestamps = deque(maxlen=1000)
        self.accel_data = {'x': deque(maxlen=1000), 'y': deque(maxlen=1000), 'z': deque(maxlen=1000)}
        self.gyro_data = {'x': deque(maxlen=1000), 'y': deque(maxlen=1000), 'z': deque(maxlen=1000)}
        self.env_data = {'pressure': deque(maxlen=1000), 'temperature': deque(maxlen=1000), 'humidity': deque(maxlen=1000)}
        
        # Statistics
        self.stats = {
            'total_messages': 0,
            'message_rate': 0.0,
            'last_message_time': None,
            'connection_status': 'Disconnected',
            'data_loss_count': 0,
            'avg_latency': 0.0
        }
        
        # Control flags
        self.running = False
        self.save_to_csv = False
        self.csv_filename = None
        self.csv_writer = None
        self.csv_file = None
        
        # MQTT client
        self.mqtt_client = mqtt.Client()
        self.mqtt_client.on_connect = self.on_connect
        self.mqtt_client.on_message = self.on_message
        self.mqtt_client.on_disconnect = self.on_disconnect
        
        # Visualization
        self.enable_plot = False
        self.fig = None
        self.axes = None
        
    def on_connect(self, client, userdata, flags, rc):
        """MQTT connection callback"""
        if rc == 0:
            print(f"✓ Successfully connected to MQTT broker: {self.broker_host}:{self.broker_port}")
            self.stats['connection_status'] = 'Connected'
            client.subscribe(self.topic)
            print(f"✓ Subscribed to topic: {self.topic}")
        else:
            print(f"✗ Connection failed, error code: {rc}")
            self.stats['connection_status'] = 'Failed'
    
    def on_disconnect(self, client, userdata, rc):
        """MQTT disconnection callback"""
        print(f"✗ Disconnected from MQTT broker, error code: {rc}")
        self.stats['connection_status'] = 'Disconnected'
    
    def on_message(self, client, userdata, msg):
        """MQTT message receive callback"""
        try:
            # Parse JSON data
            payload = json.loads(msg.payload.decode('utf-8'))
            current_time = time.time()
            
            # Update statistics
            self.stats['total_messages'] += 1
            self.stats['last_message_time'] = current_time
            
            # Calculate latency (if data contains timestamp)
            if 'timestamp' in payload:
                device_time = payload['timestamp'] / 1000.0  # Assume device timestamp is in milliseconds
                latency = (current_time - device_time) if current_time > device_time else 0
                self.stats['avg_latency'] = (self.stats['avg_latency'] * 0.9 + latency * 0.1)
            
            # Store data
            self.store_sensor_data(payload, current_time)
            
            # Display real-time data
            self.display_data(payload)
            
            # Save to CSV
            if self.save_to_csv and self.csv_writer:
                self.write_to_csv(payload, current_time)
                
        except json.JSONDecodeError as e:
            print(f"✗ JSON parsing error: {e}")
            self.stats['data_loss_count'] += 1
        except Exception as e:
            print(f"✗ Data processing error: {e}")
            self.stats['data_loss_count'] += 1
    
    def store_sensor_data(self, data, timestamp):
        """Store sensor data for visualization"""
        self.timestamps.append(datetime.fromtimestamp(timestamp))
        
        # 存储加速度数据
        if 'sensor_data' in data:
            sensor = data['sensor_data']
            self.accel_data['x'].append(sensor.get('accel_x', 0))
            self.accel_data['y'].append(sensor.get('accel_y', 0))
            self.accel_data['z'].append(sensor.get('accel_z', 0))
            
            self.gyro_data['x'].append(sensor.get('gyro_x', 0))
            self.gyro_data['y'].append(sensor.get('gyro_y', 0))
            self.gyro_data['z'].append(sensor.get('gyro_z', 0))
            
            self.env_data['pressure'].append(sensor.get('pressure', 0))
            self.env_data['temperature'].append(sensor.get('temperature', 0))
            self.env_data['humidity'].append(sensor.get('humidity', 0))
    
    def display_data(self, data):
        """显示实时数据"""
        print(f"\n=== 传感器数据 (序号: {data.get('sequence', 'N/A')}) ===")
        print(f"时间戳: {data.get('timestamp', 'N/A')} ms")
        print(f"系统状态: {data.get('system_status', 'N/A')}")
        print(f"CPU使用率: {data.get('cpu_usage', 'N/A'):.1f}%")
        print(f"空闲堆内存: {data.get('free_heap', 'N/A')} bytes")
        
        if 'sensor_data' in data:
            sensor = data['sensor_data']
            print(f"\n--- IMU数据 ---")
            print(f"加速度: X={sensor.get('accel_x', 0):.3f}, Y={sensor.get('accel_y', 0):.3f}, Z={sensor.get('accel_z', 0):.3f} m/s²")
            print(f"陀螺仪: X={sensor.get('gyro_x', 0):.3f}, Y={sensor.get('gyro_y', 0):.3f}, Z={sensor.get('gyro_z', 0):.3f} rad/s")
            
            print(f"\n--- 环境数据 ---")
            print(f"气压: {sensor.get('pressure', 0):.2f} hPa")
            print(f"温度: {sensor.get('temperature', 0):.1f} °C")
            print(f"湿度: {sensor.get('humidity', 0):.1f} %RH")
        
        # 显示统计信息
        rate = self.calculate_message_rate()
        print(f"\n--- 统计信息 ---")
        print(f"总消息数: {self.stats['total_messages']}")
        print(f"消息速率: {rate:.1f} msg/s")
        print(f"平均延迟: {self.stats['avg_latency']*1000:.1f} ms")
        print(f"数据丢失: {self.stats['data_loss_count']}")
        print("-" * 50)
    
    def calculate_message_rate(self):
        """计算消息接收速率"""
        current_time = time.time()
        if len(self.timestamps) >= 2:
            time_diff = (self.timestamps[-1] - self.timestamps[-10 if len(self.timestamps) >= 10 else 0]).total_seconds()
            if time_diff > 0:
                msg_count = min(10, len(self.timestamps))
                return msg_count / time_diff
        return 0.0
    
    def setup_csv_logging(self, filename):
        """设置CSV日志记录"""
        self.csv_filename = filename
        self.csv_file = open(filename, 'w', newline='', encoding='utf-8')
        
        fieldnames = [
            'timestamp', 'sequence', 'system_status', 'cpu_usage', 'free_heap',
            'accel_x', 'accel_y', 'accel_z',
            'gyro_x', 'gyro_y', 'gyro_z',
            'pressure', 'temperature', 'humidity',
            'data_valid'
        ]
        
        self.csv_writer = csv.DictWriter(self.csv_file, fieldnames=fieldnames)
        self.csv_writer.writeheader()
        self.save_to_csv = True
        print(f"✓ CSV日志已启用: {filename}")
    
    def write_to_csv(self, data, timestamp):
        """写入数据到CSV文件"""
        try:
            sensor = data.get('sensor_data', {})
            row = {
                'timestamp': datetime.fromtimestamp(timestamp).isoformat(),
                'sequence': data.get('sequence', ''),
                'system_status': data.get('system_status', ''),
                'cpu_usage': data.get('cpu_usage', ''),
                'free_heap': data.get('free_heap', ''),
                'accel_x': sensor.get('accel_x', ''),
                'accel_y': sensor.get('accel_y', ''),
                'accel_z': sensor.get('accel_z', ''),
                'gyro_x': sensor.get('gyro_x', ''),
                'gyro_y': sensor.get('gyro_y', ''),
                'gyro_z': sensor.get('gyro_z', ''),
                'pressure': sensor.get('pressure', ''),
                'temperature': sensor.get('temperature', ''),
                'humidity': sensor.get('humidity', ''),
                'data_valid': sensor.get('data_valid', '')
            }
            self.csv_writer.writerow(row)
            self.csv_file.flush()
        except Exception as e:
            print(f"✗ CSV写入错误: {e}")
    
    def setup_plot(self):
        """设置实时数据可视化"""
        self.enable_plot = True
        plt.ion()
        
        self.fig, self.axes = plt.subplots(2, 2, figsize=(12, 8))
        self.fig.suptitle('STM32L475E 实时传感器数据监控')
        
        # 配置子图
        self.axes[0, 0].set_title('加速度计 (m/s²)')
        self.axes[0, 0].set_ylabel('加速度')
        self.axes[0, 0].legend(['X', 'Y', 'Z'])
        
        self.axes[0, 1].set_title('陀螺仪 (rad/s)')
        self.axes[0, 1].set_ylabel('角速度')
        self.axes[0, 1].legend(['X', 'Y', 'Z'])
        
        self.axes[1, 0].set_title('环境传感器')
        self.axes[1, 0].set_ylabel('压力 (hPa)')
        
        self.axes[1, 1].set_title('温湿度')
        self.axes[1, 1].set_ylabel('温度 (°C) / 湿度 (%RH)')
        
        plt.tight_layout()
    
    def update_plot(self):
        """更新可视化图表"""
        if not self.enable_plot or len(self.timestamps) < 2:
            return
            
        try:
            # 清除之前的图
            for ax in self.axes.flat:
                ax.clear()
            
            times = list(self.timestamps)
            
            # 加速度计数据
            self.axes[0, 0].plot(times, list(self.accel_data['x']), 'r-', label='X')
            self.axes[0, 0].plot(times, list(self.accel_data['y']), 'g-', label='Y')
            self.axes[0, 0].plot(times, list(self.accel_data['z']), 'b-', label='Z')
            self.axes[0, 0].set_title('加速度计 (m/s²)')
            self.axes[0, 0].legend()
            self.axes[0, 0].grid(True)
            
            # 陀螺仪数据
            self.axes[0, 1].plot(times, list(self.gyro_data['x']), 'r-', label='X')
            self.axes[0, 1].plot(times, list(self.gyro_data['y']), 'g-', label='Y')
            self.axes[0, 1].plot(times, list(self.gyro_data['z']), 'b-', label='Z')
            self.axes[0, 1].set_title('陀螺仪 (rad/s)')
            self.axes[0, 1].legend()
            self.axes[0, 1].grid(True)
            
            # 压力数据
            self.axes[1, 0].plot(times, list(self.env_data['pressure']), 'purple', label='压力')
            self.axes[1, 0].set_title('气压 (hPa)')
            self.axes[1, 0].grid(True)
            
            # 温湿度数据
            ax_temp = self.axes[1, 1]
            ax_humi = ax_temp.twinx()
            
            line1 = ax_temp.plot(times, list(self.env_data['temperature']), 'red', label='温度')
            line2 = ax_humi.plot(times, list(self.env_data['humidity']), 'blue', label='湿度')
            
            ax_temp.set_ylabel('温度 (°C)', color='red')
            ax_humi.set_ylabel('湿度 (%RH)', color='blue')
            ax_temp.set_title('温湿度')
            
            # 组合图例
            lines = line1 + line2
            labels = [l.get_label() for l in lines]
            ax_temp.legend(lines, labels, loc='upper left')
            
            ax_temp.grid(True)
            
            plt.tight_layout()
            plt.draw()
            plt.pause(0.01)
            
        except Exception as e:
            print(f"✗ 图表更新错误: {e}")
    
    def start(self):
        """启动监控"""
        self.running = True
        print(f"启动STM32传感器监控...")
        print(f"连接到: {self.broker_host}:{self.broker_port}")
        print(f"订阅主题: {self.topic}")
        
        # 连接MQTT代理
        try:
            self.mqtt_client.connect(self.broker_host, self.broker_port, 60)
            self.mqtt_client.loop_start()
        except Exception as e:
            print(f"✗ MQTT连接错误: {e}")
            return
        
        # 主循环
        try:
            while self.running:
                if self.enable_plot:
                    self.update_plot()
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\n收到中断信号，正在关闭...")
        finally:
            self.stop()
    
    def stop(self):
        """停止监控"""
        print("正在停止监控...")
        self.running = False
        
        # 断开MQTT连接
        self.mqtt_client.loop_stop()
        self.mqtt_client.disconnect()
        
        # 关闭CSV文件
        if self.csv_file:
            self.csv_file.close()
            print(f"✓ CSV文件已保存: {self.csv_filename}")
        
        # 关闭图表
        if self.enable_plot:
            plt.close('all')
        
        # 打印最终统计
        self.print_final_stats()
    
    def print_final_stats(self):
        """打印最终统计信息"""
        print("\n=== 监控会话统计 ===")
        print(f"总接收消息: {self.stats['total_messages']}")
        print(f"数据丢失次数: {self.stats['data_loss_count']}")
        print(f"平均延迟: {self.stats['avg_latency']*1000:.1f} ms")
        if self.stats['total_messages'] > 0:
            success_rate = (1 - self.stats['data_loss_count'] / self.stats['total_messages']) * 100
            print(f"成功率: {success_rate:.1f}%")
        print("=" * 25)


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='STM32L475E-IoT01A1 MQTT传感器监控工具')
    parser.add_argument('--broker', default='broker.hivemq.com', 
                       help='MQTT代理地址 (默认: broker.hivemq.com)')
    parser.add_argument('--port', type=int, default=1883, 
                       help='MQTT代理端口 (默认: 1883)')
    parser.add_argument('--topic', default='stm32/sensor/telemetry', 
                       help='MQTT主题 (默认: stm32/sensor/telemetry)')
    parser.add_argument('--csv', type=str, 
                       help='保存数据到CSV文件')
    parser.add_argument('--plot', action='store_true', 
                       help='启用实时数据可视化')
    parser.add_argument('--verbose', '-v', action='store_true', 
                       help='详细输出模式')
    
    args = parser.parse_args()
    
    # 创建监控实例
    monitor = STM32SensorMonitor(args.broker, args.port, args.topic)
    
    # 配置CSV日志
    if args.csv:
        monitor.setup_csv_logging(args.csv)
    
    # 配置可视化
    if args.plot:
        monitor.setup_plot()
    
    print("STM32L475E-IoT01A1 实时传感器监控工具 v1.0.0")
    print("按 Ctrl+C 退出")
    print("-" * 50)
    
    # 启动监控
    monitor.start()


if __name__ == "__main__":
    main()