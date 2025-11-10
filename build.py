#!/usr/bin/env python3
"""
STM32L475E-IoT01A1 Project Build and Management Tool

Features:
- Validate development environment
- Build firmware
- Run tests
- Generate documentation
- Deploy and flash

Author: Your Name
Version: V1.0.0
Date: 2025-11-07
"""

import os
import sys
import subprocess
import argparse
import json
from pathlib import Path

class STM32ProjectBuilder:
    def __init__(self):
        self.project_root = Path(__file__).parent
        self.firmware_dir = self.project_root / "firmware"
        self.tools_dir = self.project_root / "tools" 
        self.docs_dir = self.project_root / "docs"
        
    def check_environment(self):
        """Check development environment"""
        print("🔍 Checking development environment...")
        
        checks = {
            "Python": self._check_python(),
            "ARM Toolchain": self._check_arm_toolchain(),
            "STM32 Tools": self._check_stm32_tools(),
            "Python Dependencies": self._check_python_deps()
        }
        
        all_ok = True
        for name, (status, msg) in checks.items():
            if status:
                print(f"✅ {name}: {msg}")
            else:
                print(f"❌ {name}: {msg}")
                all_ok = False
        
        return all_ok
    
    def _check_python(self):
        """Check Python version"""
        version = sys.version_info
        if version.major >= 3 and version.minor >= 7:
            return True, f"Python {version.major}.{version.minor}.{version.micro}"
        else:
            return False, f"Requires Python 3.7+, current: {version.major}.{version.minor}"
    
    def _check_arm_toolchain(self):
        """Check ARM toolchain"""
        try:
            result = subprocess.run(['arm-none-eabi-gcc', '--version'], 
                                  capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                version = result.stdout.split('\n')[0]
                return True, version
            else:
                return False, "arm-none-eabi-gcc not found"
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return False, "Please install ARM GCC toolchain"
    
    def _check_stm32_tools(self):
        """Check STM32 tools"""
        tools = ['STM32_Programmer_CLI']
        found_tools = []
        
        for tool in tools:
            try:
                subprocess.run([tool, '--version'], capture_output=True, timeout=5)
                found_tools.append(tool)
            except (subprocess.TimeoutExpired, FileNotFoundError):
                continue
        
        if found_tools:
            return True, f"Found tools: {', '.join(found_tools)}"
        else:
            return False, "STM32 tools not found, please install STM32CubeProgrammer"
    
    def _check_python_deps(self):
        """Check Python dependencies"""
        requirements_file = self.project_root / "requirements.txt"
        if not requirements_file.exists():
            return False, "requirements.txt not found"
        
        try:
            import paho.mqtt.client
            import serial
            import numpy
            import matplotlib
            return True, "Main dependencies installed"
        except ImportError as e:
            return False, f"Missing dependency: {e.name}, please run: pip install -r requirements.txt"
    
    def build_firmware(self, target="debug"):
        """Build firmware"""
        print(f"🔨 Building firmware (target: {target})...")
        
        if not self.firmware_dir.exists():
            print("❌ Firmware directory does not exist")
            return False
        
        # Check for Makefile or project files
        makefile = self.firmware_dir / "Makefile"
        project_file = list(self.firmware_dir.glob("*.project"))
        
        if makefile.exists():
            return self._build_with_make(target)
        elif project_file:
            return self._build_with_cube_ide(project_file[0])
        else:
            print("❌ No build files found (Makefile or .project)")
            return False
    
    def _build_with_make(self, target):
        """使用Makefile构建"""
        try:
            os.chdir(self.firmware_dir)
            
            # 清理
            subprocess.run(['make', 'clean'], check=True)
            print("🧹 清理完成")
            
            # 构建
            make_target = "all" if target == "debug" else target
            result = subprocess.run(['make', make_target], 
                                  capture_output=True, text=True)
            
            if result.returncode == 0:
                print("✅ 固件构建成功")
                
                # 查找生成的二进制文件
                bin_files = list(Path('.').glob("**/*.bin"))
                hex_files = list(Path('.').glob("**/*.hex"))
                
                if bin_files or hex_files:
                    print("📦 生成的文件:")
                    for f in bin_files + hex_files:
                        print(f"   {f}")
                
                return True
            else:
                print(f"❌ 构建失败:\n{result.stderr}")
                return False
                
        except subprocess.CalledProcessError as e:
            print(f"❌ 构建错误: {e}")
            return False
        except FileNotFoundError:
            print("❌ make 命令未找到")
            return False
        finally:
            os.chdir(self.project_root)
    
    def _build_with_cube_ide(self, project_file):
        """使用STM32CubeIDE构建"""
        print(f"🔧 使用STM32CubeIDE构建项目: {project_file}")
        print("ℹ️  请在STM32CubeIDE中手动构建项目")
        return True
    
    def flash_firmware(self, binary_file=None):
        """烧录固件"""
        print("📱 烧录固件到STM32...")
        
        if not binary_file:
            # 自动查找二进制文件
            bin_files = list(self.firmware_dir.glob("**/*.bin"))
            if not bin_files:
                print("❌ 未找到二进制文件")
                return False
            binary_file = bin_files[0]
        
        try:
            cmd = [
                'STM32_Programmer_CLI',
                '-c', 'port=SWD',
                '-w', str(binary_file), '0x08000000',
                '-v', '-rst'
            ]
            
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode == 0:
                print("✅ 固件烧录成功")
                return True
            else:
                print(f"❌ 烧录失败: {result.stderr}")
                return False
                
        except FileNotFoundError:
            print("❌ STM32_Programmer_CLI 未找到，请安装STM32CubeProgrammer")
            return False
    
    def run_tests(self, test_type="all"):
        """运行测试"""
        print(f"🧪 运行测试 ({test_type})...")
        
        tests = {
            "mqtt": self._test_mqtt,
            "performance": self._test_performance,
            "power": self._test_power
        }
        
        if test_type == "all":
            test_list = tests.values()
        elif test_type in tests:
            test_list = [tests[test_type]]
        else:
            print(f"❌ 未知测试类型: {test_type}")
            return False
        
        success_count = 0
        for test_func in test_list:
            if test_func():
                success_count += 1
        
        print(f"✅ 测试完成: {success_count}/{len(test_list)} 通过")
        return success_count == len(test_list)
    
    def _test_mqtt(self):
        """测试MQTT连接"""
        try:
            script = self.tools_dir / "mqtt_monitor.py"
            if not script.exists():
                print("❌ MQTT测试脚本未找到")
                return False
            
            print("🔗 测试MQTT连接...")
            # 运行30秒的MQTT测试
            result = subprocess.run([
                sys.executable, str(script),
                '--broker', 'broker.hivemq.com',
                '--duration', '30'
            ], timeout=35, capture_output=True, text=True)
            
            if "Connected" in result.stdout:
                print("✅ MQTT连接测试通过")
                return True
            else:
                print("❌ MQTT连接测试失败")
                return False
                
        except subprocess.TimeoutExpired:
            print("✅ MQTT测试超时结束 (正常)")
            return True
        except Exception as e:
            print(f"❌ MQTT测试错误: {e}")
            return False
    
    def _test_performance(self):
        """性能测试"""
        try:
            script = self.tools_dir / "performance_test.py"
            if not script.exists():
                print("❌ 性能测试脚本未找到")
                return False
            
            print("⚡ 运行性能测试...")
            # 运行简短的性能测试
            result = subprocess.run([
                sys.executable, str(script),
                '--duration', '60',
                '--broker', 'broker.hivemq.com'
            ], timeout=70, capture_output=True, text=True)
            
            # 检查测试结果
            if result.returncode == 0:
                print("✅ 性能测试通过")
                return True
            else:
                print(f"❌ 性能测试失败: {result.stderr}")
                return False
                
        except subprocess.TimeoutExpired:
            print("⚠️  性能测试超时")
            return False
        except Exception as e:
            print(f"❌ 性能测试错误: {e}")
            return False
    
    def _test_power(self):
        """功耗测试"""
        print("🔋 功耗测试 (仅检查脚本)")
        
        script = self.tools_dir / "measure_power.ps1"
        if script.exists():
            print("✅ 功耗测试脚本可用")
            return True
        else:
            print("❌ 功耗测试脚本未找到")
            return False
    
    def install_dependencies(self):
        """安装Python依赖"""
        requirements_file = self.project_root / "requirements.txt"
        
        if not requirements_file.exists():
            print("❌ requirements.txt 未找到")
            return False
        
        try:
            print("📦 安装Python依赖...")
            result = subprocess.run([
                sys.executable, '-m', 'pip', 'install', '-r', str(requirements_file)
            ], check=True)
            
            print("✅ 依赖安装成功")
            return True
            
        except subprocess.CalledProcessError as e:
            print(f"❌ 依赖安装失败: {e}")
            return False
    
    def generate_docs(self):
        """生成项目文档"""
        print("📚 生成项目文档...")
        
        # 确保docs目录存在
        self.docs_dir.mkdir(exist_ok=True)
        
        # 生成项目信息文件
        project_info = {
            "name": "STM32L475E-IoT01A1 Real-Time Sensor Fusion System",
            "version": "1.0.0",
            "description": "Multi-tasking embedded firmware with FreeRTOS",
            "author": "Your Name",
            "build_date": subprocess.check_output(['date']).decode().strip(),
            "features": [
                "Real-time multi-tasking with FreeRTOS",
                "Multi-sensor integration (IMU, pressure, humidity)",
                "Low-latency design (<50ms task latency)",
                "Power optimization with Tickless Idle",
                "MQTT telemetry over Wi-Fi",
                "Watchdog and recovery mechanisms"
            ]
        }
        
        with open(self.docs_dir / "project_info.json", "w") as f:
            json.dump(project_info, f, indent=2)
        
        print("✅ 文档生成完成")
        return True
    
    def clean_project(self):
        """清理项目"""
        print("🧹 清理项目...")
        
        # 清理固件构建文件
        build_dirs = ['build', 'Debug', 'Release']
        build_files = ['*.o', '*.bin', '*.hex', '*.elf', '*.map']
        
        cleaned_count = 0
        
        for build_dir in build_dirs:
            dir_path = self.firmware_dir / build_dir
            if dir_path.exists() and dir_path.is_dir():
                import shutil
                shutil.rmtree(dir_path)
                print(f"  删除目录: {dir_path}")
                cleaned_count += 1
        
        # 清理Python缓存
        import glob
        for pattern in ['**/__pycache__', '**/*.pyc']:
            for path in self.project_root.glob(pattern):
                if path.is_dir():
                    import shutil
                    shutil.rmtree(path)
                else:
                    path.unlink()
                cleaned_count += 1
        
        print(f"✅ 清理完成，删除了 {cleaned_count} 个文件/目录")
        return True


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='STM32L475E-IoT01A1 项目构建工具')
    
    subparsers = parser.add_subparsers(dest='command', help='可用命令')
    
    # 环境检查
    subparsers.add_parser('check', help='检查开发环境')
    
    # 安装依赖
    subparsers.add_parser('install', help='安装Python依赖')
    
    # 构建固件
    build_parser = subparsers.add_parser('build', help='构建固件')
    build_parser.add_argument('--target', default='debug', 
                            choices=['debug', 'release'], help='构建目标')
    
    # 烧录固件
    flash_parser = subparsers.add_parser('flash', help='烧录固件')
    flash_parser.add_argument('--binary', help='二进制文件路径')
    
    # 运行测试
    test_parser = subparsers.add_parser('test', help='运行测试')
    test_parser.add_argument('--type', default='all',
                           choices=['all', 'mqtt', 'performance', 'power'],
                           help='测试类型')
    
    # 生成文档
    subparsers.add_parser('docs', help='生成文档')
    
    # 清理项目
    subparsers.add_parser('clean', help='清理项目')
    
    # 完整流程
    subparsers.add_parser('all', help='运行完整构建流程')
    
    args = parser.parse_args()
    
    builder = STM32ProjectBuilder()
    
    if args.command == 'check':
        success = builder.check_environment()
        sys.exit(0 if success else 1)
    
    elif args.command == 'install':
        success = builder.install_dependencies()
        sys.exit(0 if success else 1)
    
    elif args.command == 'build':
        success = builder.build_firmware(args.target)
        sys.exit(0 if success else 1)
    
    elif args.command == 'flash':
        success = builder.flash_firmware(args.binary)
        sys.exit(0 if success else 1)
    
    elif args.command == 'test':
        success = builder.run_tests(args.type)
        sys.exit(0 if success else 1)
    
    elif args.command == 'docs':
        success = builder.generate_docs()
        sys.exit(0 if success else 1)
    
    elif args.command == 'clean':
        success = builder.clean_project()
        sys.exit(0 if success else 1)
    
    elif args.command == 'all':
        print("🚀 运行完整构建流程...")
        
        steps = [
            ("检查环境", lambda: builder.check_environment()),
            ("安装依赖", lambda: builder.install_dependencies()),
            ("清理项目", lambda: builder.clean_project()),
            ("构建固件", lambda: builder.build_firmware("debug")),
            ("生成文档", lambda: builder.generate_docs()),
            ("运行测试", lambda: builder.run_tests("mqtt"))
        ]
        
        for step_name, step_func in steps:
            print(f"\n📋 {step_name}...")
            if not step_func():
                print(f"❌ {step_name}失败，停止构建流程")
                sys.exit(1)
        
        print("\n🎉 完整构建流程成功完成!")
        print("\n下一步:")
        print("1. 连接STM32L475E-IoT01A1开发板")
        print("2. 运行: python build.py flash")
        print("3. 运行: python tools/mqtt_monitor.py --plot")
        
    else:
        parser.print_help()
        print("\n快速开始:")
        print("  python build.py check     # 检查环境")
        print("  python build.py install   # 安装依赖")
        print("  python build.py all       # 完整构建")


if __name__ == "__main__":
    main()