# ESP32-C6 C++ Development Setup

A complete VS Code development environment for ESP32-C6 with C++17, FreeRTOS, debugging support, and code quality tools.

## 🚀 Quick Start

### Prerequisites

1. **ESP-IDF 5.5+** installed and configured
2. **VS Code** with the following extensions:
   - C/C++ Extension Pack
   - ESP-IDF Extension
   - CMake Tools

### Getting Started

1. **Setup Environment**
   ```bash
   # Load ESP-IDF environment
   get_idf
   
   # Set target chip
   idf.py set-target esp32c6
   ```

2. **Build Project**
   ```bash
   idf.py build
   ```

3. **Flash and Monitor**
   ```bash
   # Flash firmware
   idf.py flash
   
   # Monitor serial output
   idf.py monitor
   
   # Or both in one command
   idf.py flash monitor
   ```

## 🛠️ VS Code Integration

### Building and Running

Use **Ctrl+Shift+P** and select "Tasks: Run Task", then choose:

- **Build - ESP-IDF** - Compile the project
- **Clean - ESP-IDF** - Clean build files
- **Flash - ESP-IDF** - Flash firmware to device
- **Monitor - ESP-IDF** - Open serial monitor
- **Flash and Monitor - ESP-IDF** - Flash and monitor in one step

### Keyboard Shortcuts

- **Ctrl+Shift+B** - Build project (default build task)
- **F5** - Start debugging
- **Ctrl+Shift+P** - Open command palette for tasks

## 🐛 Debugging

### Hardware Setup

1. Connect your ESP32-C6 development board via USB
2. Ensure the board supports JTAG debugging (built-in USB-JTAG on most ESP32-C6 boards)

### Starting Debug Session

1. **Build for debug**: Use "Build for Debug - ESP-IDF" task
2. **Start debugging**: Press **F5** or use the Debug panel
3. **Select configuration**: Choose "Debug ESP32-C6"

The debugger will:
- Automatically start OpenOCD
- Connect GDB to the ESP32-C6
- Set a breakpoint at `app_main()`
- Allow stepping through code and variable inspection

### Debug Configurations

- **Debug ESP32-C6** - Full debug session with automatic setup
- **Attach to ESP32-C6** - Attach to already running OpenOCD session

## 📝 Code Quality Tools

### Automatic Formatting

- **Format on save** is enabled
- Uses **clang-format** with embedded-friendly Google style
- Manual format: **Ctrl+Shift+I** or **Alt+Shift+F**

### Static Analysis

- **clang-tidy** configured for embedded C++ best practices
- Real-time error detection and suggestions
- Excludes problematic checks for embedded development

### IntelliSense Features

- Smart code completion for ESP-IDF APIs
- Real-time error squiggles
- Go to definition (**F12**)
- Find references (**Shift+F12**)
- Symbol search (**Ctrl+T**)

## 📁 Project Structure

```
.
├── CMakeLists.txt              # Main CMake configuration
├── main/
│   ├── CMakeLists.txt          # Component CMake file
│   └── main.cpp                # Main application code
├── .vscode/
│   ├── c_cpp_properties.json   # IntelliSense configuration
│   ├── tasks.json              # Build and debug tasks
│   ├── launch.json             # Debug configurations
│   └── settings.json           # VS Code settings
├── .clang-format               # Code formatting rules
├── .clang-tidy                 # Static analysis configuration
├── build/                      # Build output (generated)
│   └── compile_commands.json   # Compilation database
└── sdkconfig                   # ESP-IDF configuration
```

## ⚙️ Configuration Files

### ESP-IDF Configuration
- **sdkconfig** - Main ESP-IDF configuration
- Edit with: `idf.py menuconfig`

### VS Code Settings
- **Port**: `/dev/ttyUSB0` (change in `.vscode/settings.json` if needed)
- **Flash baud rate**: 460800
- **Monitor baud rate**: 115200

## 🔧 Customization

### Changing Target Device

```bash
idf.py set-target esp32c3  # For ESP32-C3
idf.py set-target esp32s3  # For ESP32-S3
# Then update .vscode/c_cpp_properties.json compiler paths
```

### Adding Components

1. Create component folder in project root or `components/`
2. Add `CMakeLists.txt` with `idf_component_register()`
3. Update main component's `REQUIRES` list

### Custom Build Flags

Edit `main/CMakeLists.txt`:
```cmake
target_compile_options(${COMPONENT_LIB} PRIVATE 
    -fno-exceptions 
    -fno-rtti 
    -fno-unwind-tables
    -Wall
    -Wextra
)
```

## 📚 Development Tips

### Debugging Tips

1. **Use ESP_LOGI/ESP_LOGD** instead of printf for better performance
2. **Monitor stack usage** with `uxTaskGetStackHighWaterMark()`
3. **Use FreeRTOS task debugging** - variables show in debug window
4. **Memory debugging** with `heap_caps_print_heap_info()`

### Performance Optimization

1. **Profile with ESP-IDF tools**:
   ```bash
   idf.py app-flash monitor
   # Use 'i' command in monitor for heap info
   ```

2. **Optimize build**:
   ```bash
   idf.py -D CMAKE_BUILD_TYPE=Release build
   ```

### Code Organization

```cpp
// Use namespaces for organization
namespace sensors {
    class TemperatureSensor {
        // Implementation
    };
}

// Prefer RAII and smart pointers (when available)
// Use const correctness
// Follow ESP-IDF naming conventions for C APIs
```

## 🛡️ Troubleshooting

### Common Issues

1. **Build Errors**
   ```bash
   # Clean and rebuild
   idf.py fullclean
   idf.py build
   ```

2. **Flash Errors**
   ```bash
   # Check port permissions
   ls -l /dev/ttyUSB*
   sudo usermod -a -G dialout $USER
   # Log out and back in
   ```

3. **Debug Connection Issues**
   ```bash
   # Check OpenOCD is not already running
   pkill openocd
   # Try manual OpenOCD start
   openocd -f interface/esp_usb_jtag.cfg -f target/esp32c6.cfg
   ```

4. **IntelliSense Issues**
   - **Ctrl+Shift+P** → "C/C++: Reload IntelliSense Database"
   - Check include paths in `.vscode/c_cpp_properties.json`

### Port Configuration

If using different USB port, update `.vscode/settings.json`:
```json
{
    "espIdf.port": "/dev/ttyACM0"
}
```

## 📖 Useful Commands

```bash
# ESP-IDF commands
idf.py menuconfig          # Configure project
idf.py size                # Analyze binary size
idf.py monitor             # Serial monitor
idf.py erase-flash         # Erase flash memory
idf.py partition-table     # Show partition layout

# Monitoring and debugging
idf.py monitor --port /dev/ttyUSB0
idf.py gdb                 # Start GDB session
```

## 📋 VS Code Extensions (Optional)

Additional helpful extensions:
- **GitLens** - Enhanced Git capabilities
- **Error Lens** - Inline error messages
- **Bracket Pair Colorizer** - Easier bracket matching
- **Todo Tree** - Track TODO comments

## 🤝 Contributing

1. Follow the code formatting rules (automatic with format-on-save)
2. Run static analysis before committing
3. Test on actual hardware when possible
4. Update documentation for new features

---

## 📞 Support

- **ESP-IDF Documentation**: https://docs.espressif.com/projects/esp-idf/
- **ESP32-C6 Technical Reference**: https://www.espressif.com/sites/default/files/documentation/esp32-c6_technical_reference_manual_en.pdf
- **FreeRTOS Documentation**: https://www.freertos.org/Documentation/RTOS_book.html