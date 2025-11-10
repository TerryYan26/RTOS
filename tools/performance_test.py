#!/usr/bin/env python3
"""
STM32L475E-IoT01A1 System Performance Testing and Validation Tool

Features:
- Task latency testing
- CPU utilization testing  
- Power measurement and analysis
- Memory usage monitoring
- Communication reliability testing
- System stability testing

Author: Your Name
Version: V1.0.0
Date: 2025-11-07
"""

import time
import json
import serial
import threading
import statistics
import argparse
from datetime import datetime, timedelta
from collections import defaultdict, deque
import paho.mqtt.client as mqtt
import matplotlib.pyplot as plt
import numpy as np

class STM32PerformanceTest:
    def __init__(self, serial_port=None, mqtt_broker="broker.hivemq.com", mqtt_topic="stm32/sensor/telemetry"):
        self.serial_port = serial_port
        self.mqtt_broker = mqtt_broker
        self.mqtt_topic = mqtt_topic
        
        # 连接对象
        self.serial_conn = None
        self.mqtt_client = None
        
        # 测试数据收集
        self.test_results = {
            'latency': [],
            'cpu_usage': [],
            'memory_usage': [],
            'message_intervals': [],
            'error_count': 0,
            'total_messages': 0,
            'test_duration': 0,
            'start_time': None
        }
        
        # 实时监控数据
        self.realtime_data = {
            'latency': deque(maxlen=100),
            'cpu': deque(maxlen=100),
            'memory': deque(maxlen=100),
            'timestamps': deque(maxlen=100)
        }
        
        # 测试配置
        self.test_running = False
        self.test_duration = 300  # 默认5分钟
        self.verbose = False
        
    def setup_serial(self, port, baudrate=115200):
        """设置串口连接"""
        try:
            self.serial_conn = serial.Serial(port, baudrate, timeout=1)
            print(f"✓ 串口连接成功: {port} @ {baudrate}")
            return True
        except Exception as e:
            print(f"✗ 串口连接失败: {e}")
            return False
    
    def setup_mqtt(self):
        """设置MQTT连接"""
        self.mqtt_client = mqtt.Client()
        self.mqtt_client.on_connect = self.on_mqtt_connect
        self.mqtt_client.on_message = self.on_mqtt_message
        
        try:
            self.mqtt_client.connect(self.mqtt_broker, 1883, 60)
            self.mqtt_client.loop_start()
            print(f"✓ MQTT连接成功: {self.mqtt_broker}")
            return True
        except Exception as e:
            print(f"✗ MQTT连接失败: {e}")
            return False
    
    def on_mqtt_connect(self, client, userdata, flags, rc):
        """MQTT连接回调"""
        if rc == 0:
            client.subscribe(self.mqtt_topic)
            print(f"✓ 订阅MQTT主题: {self.mqtt_topic}")
    
    def on_mqtt_message(self, client, userdata, msg):
        """MQTT消息接收回调"""
        try:
            data = json.loads(msg.payload.decode('utf-8'))
            current_time = time.time()
            
            # 处理性能数据
            self.process_performance_data(data, current_time)
            
        except Exception as e:
            self.test_results['error_count'] += 1
            if self.verbose:
                print(f"✗ MQTT数据处理错误: {e}")
    
    def process_performance_data(self, data, timestamp):
        """处理性能数据"""
        self.test_results['total_messages'] += 1
        
        # 计算延迟
        if 'timestamp' in data:
            device_time = data['timestamp'] / 1000.0
            latency = (timestamp - device_time) * 1000  # ms
            if latency > 0 and latency < 10000:  # 合理范围内
                self.test_results['latency'].append(latency)
                self.realtime_data['latency'].append(latency)
        
        # CPU使用率
        if 'cpu_usage' in data:
            cpu = data['cpu_usage']
            self.test_results['cpu_usage'].append(cpu)
            self.realtime_data['cpu'].append(cpu)
        
        # 内存使用
        if 'free_heap' in data:
            memory = data['free_heap']
            self.test_results['memory_usage'].append(memory)
            self.realtime_data['memory'].append(memory)
        
        # 时间戳
        self.realtime_data['timestamps'].append(datetime.fromtimestamp(timestamp))
        
        # 计算消息间隔
        if hasattr(self, 'last_message_time'):
            interval = (timestamp - self.last_message_time) * 1000  # ms
            if interval > 0 and interval < 5000:  # 合理范围
                self.test_results['message_intervals'].append(interval)
        
        self.last_message_time = timestamp
    
    def read_serial_data(self):
        """读取串口调试数据"""
        if not self.serial_conn:
            return
        
        while self.test_running:
            try:
                if self.serial_conn.in_waiting:
                    line = self.serial_conn.readline().decode('utf-8', errors='ignore').strip()
                    if line and self.verbose:
                        print(f"[UART] {line}")
                        
                    # 解析性能相关的调试信息
                    self.parse_debug_output(line)
                        
            except Exception as e:
                if self.verbose:
                    print(f"✗ 串口读取错误: {e}")
            
            time.sleep(0.01)
    
    def parse_debug_output(self, line):
        """解析调试输出中的性能信息"""
        try:
            # 查找任务延迟信息
            if "Task latency:" in line:
                latency = float(line.split("Task latency:")[1].split("ms")[0].strip())
                self.test_results['latency'].append(latency)
            
            # 查找CPU使用率
            if "CPU usage:" in line:
                cpu = float(line.split("CPU usage:")[1].split("%")[0].strip())
                self.test_results['cpu_usage'].append(cpu)
                
            # 查找内存信息
            if "Free heap:" in line:
                memory = int(line.split("Free heap:")[1].split("bytes")[0].strip())
                self.test_results['memory_usage'].append(memory)
                
        except Exception as e:
            if self.verbose:
                print(f"调试输出解析错误: {e}")
    
    def run_latency_test(self, duration=60):
        """运行延迟测试"""
        print(f"\n=== 延迟测试 (持续 {duration}秒) ===")
        
        start_time = time.time()
        latencies = []
        
        while time.time() - start_time < duration:
            if len(self.realtime_data['latency']) > 0:
                current_latency = list(self.realtime_data['latency'])[-1]
                latencies.append(current_latency)
                
                if self.verbose:
                    print(f"当前延迟: {current_latency:.2f} ms")
            
            time.sleep(0.1)
        
        if latencies:
            avg_latency = statistics.mean(latencies)
            max_latency = max(latencies)
            min_latency = min(latencies)
            p95_latency = np.percentile(latencies, 95)
            
            print(f"延迟统计:")
            print(f"  平均: {avg_latency:.2f} ms")
            print(f"  最大: {max_latency:.2f} ms")
            print(f"  最小: {min_latency:.2f} ms")
            print(f"  P95:  {p95_latency:.2f} ms")
            
            # 评估性能
            if avg_latency < 50:
                print(f"✓ 延迟性能优秀 (< 50ms)")
            elif avg_latency < 75:
                print(f"⚠ 延迟性能良好 (< 75ms)")
            else:
                print(f"✗ 延迟性能需要改进 (> 75ms)")
        
        return latencies
    
    def run_throughput_test(self, duration=60):
        """运行吞吐量测试"""
        print(f"\n=== 吞吐量测试 (持续 {duration}秒) ===")
        
        start_count = self.test_results['total_messages']
        start_time = time.time()
        
        time.sleep(duration)
        
        end_count = self.test_results['total_messages']
        end_time = time.time()
        
        message_count = end_count - start_count
        actual_duration = end_time - start_time
        throughput = message_count / actual_duration
        
        print(f"吞吐量统计:")
        print(f"  消息总数: {message_count}")
        print(f"  测试时间: {actual_duration:.1f} 秒")
        print(f"  吞吐量: {throughput:.2f} msg/s")
        
        # 评估吞吐量
        expected_rate = 10.0  # 期望的消息速率
        if throughput >= expected_rate * 0.9:
            print(f"✓ 吞吐量满足要求 (>= {expected_rate * 0.9:.1f} msg/s)")
        else:
            print(f"✗ 吞吐量低于期望 (< {expected_rate * 0.9:.1f} msg/s)")
        
        return throughput
    
    def run_stability_test(self, duration=1800):
        """运行稳定性测试（30分钟）"""
        print(f"\n=== 稳定性测试 (持续 {duration/60:.0f}分钟) ===")
        
        start_time = time.time()
        error_counts = []
        memory_samples = []
        
        while time.time() - start_time < duration:
            # 记录当前错误计数
            error_counts.append(self.test_results['error_count'])
            
            # 记录当前内存使用
            if len(self.realtime_data['memory']) > 0:
                memory_samples.append(list(self.realtime_data['memory'])[-1])
            
            # 打印进度
            elapsed = time.time() - start_time
            progress = (elapsed / duration) * 100
            if int(elapsed) % 60 == 0 or self.verbose:
                print(f"稳定性测试进度: {progress:.1f}% ({elapsed/60:.1f}/{duration/60:.0f} 分钟)")
            
            time.sleep(10)  # 每10秒检查一次
        
        # 分析稳定性
        total_errors = self.test_results['error_count']
        error_rate = total_errors / (duration / 60)  # 错误/分钟
        
        # 内存泄漏检查
        memory_trend = 0
        if len(memory_samples) > 10:
            x = np.arange(len(memory_samples))
            coeffs = np.polyfit(x, memory_samples, 1)
            memory_trend = coeffs[0]  # 斜率
        
        print(f"稳定性统计:")
        print(f"  总错误数: {total_errors}")
        print(f"  错误率: {error_rate:.2f} 错误/分钟")
        print(f"  内存趋势: {memory_trend:.2f} bytes/sample")
        
        # 评估稳定性
        if error_rate < 1 and abs(memory_trend) < 100:
            print(f"✓ 系统稳定性优秀")
        elif error_rate < 5 and abs(memory_trend) < 500:
            print(f"⚠ 系统稳定性良好")
        else:
            print(f"✗ 系统稳定性需要改进")
    
    def run_power_analysis(self):
        """运行功耗分析（需要外部功耗测量设备）"""
        print(f"\n=== 功耗分析 ===")
        print("注意: 此功能需要外部功耗测量设备")
        print("建议使用示波器或专用功耗分析仪测量:")
        print("1. 正常运行模式功耗")
        print("2. Tickless空闲模式功耗")
        print("3. 深度睡眠模式功耗")
        print("4. 各任务功耗分配")
        
        # 这里可以集成外部功耗测量设备的API
        # 示例: 模拟功耗数据
        power_modes = {
            'active': {'current': 15.2, 'voltage': 3.3},  # mA
            'idle': {'current': 8.7, 'voltage': 3.3},
            'sleep': {'current': 2.1, 'voltage': 3.3}
        }
        
        print(f"\n模拟功耗数据:")
        for mode, data in power_modes.items():
            power = data['current'] * data['voltage']  # mW
            print(f"  {mode.upper()}: {data['current']} mA @ {data['voltage']}V = {power:.1f} mW")
    
    def generate_report(self, output_file="performance_report.txt"):
        """生成测试报告"""
        print(f"\n=== 生成测试报告 ===")
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write("STM32L475E-IoT01A1 性能测试报告\n")
            f.write("=" * 50 + "\n")
            f.write(f"测试时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"测试持续时间: {self.test_results['test_duration']:.1f} 秒\n\n")
            
            # 延迟统计
            if self.test_results['latency']:
                latencies = self.test_results['latency']
                f.write("延迟性能:\n")
                f.write(f"  平均延迟: {statistics.mean(latencies):.2f} ms\n")
                f.write(f"  最大延迟: {max(latencies):.2f} ms\n")
                f.write(f"  最小延迟: {min(latencies):.2f} ms\n")
                f.write(f"  P95延迟: {np.percentile(latencies, 95):.2f} ms\n\n")
            
            # CPU使用率统计
            if self.test_results['cpu_usage']:
                cpu_usage = self.test_results['cpu_usage']
                f.write("CPU性能:\n")
                f.write(f"  平均CPU使用率: {statistics.mean(cpu_usage):.1f}%\n")
                f.write(f"  最大CPU使用率: {max(cpu_usage):.1f}%\n")
                f.write(f"  最小CPU使用率: {min(cpu_usage):.1f}%\n\n")
            
            # 内存使用统计
            if self.test_results['memory_usage']:
                memory_usage = self.test_results['memory_usage']
                f.write("内存性能:\n")
                f.write(f"  平均空闲内存: {statistics.mean(memory_usage):.0f} bytes\n")
                f.write(f"  最小空闲内存: {min(memory_usage):.0f} bytes\n")
                f.write(f"  最大空闲内存: {max(memory_usage):.0f} bytes\n\n")
            
            # 通信统计
            f.write("通信性能:\n")
            f.write(f"  总接收消息: {self.test_results['total_messages']}\n")
            f.write(f"  错误计数: {self.test_results['error_count']}\n")
            if self.test_results['total_messages'] > 0:
                success_rate = (1 - self.test_results['error_count'] / self.test_results['total_messages']) * 100
                f.write(f"  成功率: {success_rate:.2f}%\n")
            f.write("\n")
            
            # 消息间隔统计
            if self.test_results['message_intervals']:
                intervals = self.test_results['message_intervals']
                f.write("消息间隔:\n")
                f.write(f"  平均间隔: {statistics.mean(intervals):.2f} ms\n")
                f.write(f"  标准差: {statistics.stdev(intervals):.2f} ms\n\n")
        
        print(f"✓ 测试报告已保存: {output_file}")
    
    def plot_results(self):
        """绘制测试结果图表"""
        if not self.test_results['latency']:
            print("✗ 没有足够的数据用于绘图")
            return
        
        fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(12, 8))
        fig.suptitle('STM32L475E-IoT01A1 性能测试结果')
        
        # 延迟分布
        ax1.hist(self.test_results['latency'], bins=50, alpha=0.7, color='blue')
        ax1.set_title('延迟分布')
        ax1.set_xlabel('延迟 (ms)')
        ax1.set_ylabel('频次')
        ax1.axvline(50, color='red', linestyle='--', label='目标 < 50ms')
        ax1.legend()
        
        # CPU使用率时间序列
        if self.test_results['cpu_usage']:
            ax2.plot(self.test_results['cpu_usage'], color='green')
            ax2.set_title('CPU使用率')
            ax2.set_xlabel('样本')
            ax2.set_ylabel('CPU使用率 (%)')
            ax2.axhline(80, color='red', linestyle='--', label='警告线 80%')
            ax2.legend()
        
        # 内存使用趋势
        if self.test_results['memory_usage']:
            ax3.plot(self.test_results['memory_usage'], color='purple')
            ax3.set_title('空闲内存')
            ax3.set_xlabel('样本')
            ax3.set_ylabel('空闲内存 (bytes)')
        
        # 消息间隔分布
        if self.test_results['message_intervals']:
            ax4.hist(self.test_results['message_intervals'], bins=30, alpha=0.7, color='orange')
            ax4.set_title('消息间隔分布')
            ax4.set_xlabel('间隔 (ms)')
            ax4.set_ylabel('频次')
            ax4.axvline(100, color='red', linestyle='--', label='期望 100ms')
            ax4.legend()
        
        plt.tight_layout()
        plt.savefig('performance_test_results.png', dpi=150, bbox_inches='tight')
        plt.show()
        print("✓ 性能图表已保存: performance_test_results.png")
    
    def run_comprehensive_test(self, duration=300):
        """运行综合性能测试"""
        print("STM32L475E-IoT01A1 综合性能测试")
        print("=" * 50)
        
        self.test_running = True
        self.test_results['start_time'] = time.time()
        
        # 启动串口数据读取线程
        if self.serial_conn:
            serial_thread = threading.Thread(target=self.read_serial_data)
            serial_thread.daemon = True
            serial_thread.start()
        
        try:
            # 等待系统稳定
            print("等待系统初始化...")
            time.sleep(10)
            
            # 运行各项测试
            self.run_latency_test(60)
            self.run_throughput_test(60)
            
            if duration > 300:  # 如果测试时间足够长，运行稳定性测试
                self.run_stability_test(duration - 120)
            
            self.run_power_analysis()
            
        except KeyboardInterrupt:
            print("\n测试被用户中断")
        finally:
            self.test_running = False
            self.test_results['test_duration'] = time.time() - self.test_results['start_time']
            
            # 生成报告和图表
            self.generate_report()
            self.plot_results()
            
            # 清理连接
            if self.mqtt_client:
                self.mqtt_client.loop_stop()
                self.mqtt_client.disconnect()
            
            if self.serial_conn:
                self.serial_conn.close()


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='STM32L475E-IoT01A1 性能测试工具')
    parser.add_argument('--serial', type=str, help='串口端口 (例: COM3 或 /dev/ttyUSB0)')
    parser.add_argument('--broker', default='broker.hivemq.com', help='MQTT代理地址')
    parser.add_argument('--topic', default='stm32/sensor/telemetry', help='MQTT主题')
    parser.add_argument('--duration', type=int, default=300, help='测试持续时间（秒）')
    parser.add_argument('--verbose', '-v', action='store_true', help='详细输出')
    
    args = parser.parse_args()
    
    # 创建测试实例
    test = STM32PerformanceTest(args.serial, args.broker, args.topic)
    test.verbose = args.verbose
    
    # 设置连接
    if args.serial:
        if not test.setup_serial(args.serial):
            return
    
    if not test.setup_mqtt():
        return
    
    print(f"准备运行 {args.duration} 秒的综合性能测试")
    print("按 Ctrl+C 提前结束测试")
    input("按回车键开始测试...")
    
    # 运行测试
    test.run_comprehensive_test(args.duration)
    
    print("性能测试完成!")


if __name__ == "__main__":
    main()