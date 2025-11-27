#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <signal.h>

// I2C device file and address
#define I2C_DEV "/dev/i2c-1"
#define IAM20380HT_ADDR 0x69

// Register addresses
#define REG_SELF_TEST_X_GYRO   0x00
#define REG_SELF_TEST_Y_GYRO   0x01
#define REG_SELF_TEST_Z_GYRO   0x02
#define REG_SMPLRT_DIV         0x19
#define REG_CONFIG             0x1A
#define REG_GYRO_CONFIG        0x1B
#define REG_ACCEL_CONFIG       0x1C
#define REG_FIFO_EN            0x23
#define REG_INT_PIN_CFG        0x37
#define REG_INT_ENABLE         0x38
#define REG_TEMP_OUT_H         0x41
#define REG_TEMP_OUT_L         0x42
#define REG_GYRO_XOUT_H        0x43
#define REG_GYRO_XOUT_L        0x44
#define REG_GYRO_YOUT_H        0x45
#define REG_GYRO_YOUT_L        0x46
#define REG_GYRO_ZOUT_H        0x47
#define REG_GYRO_ZOUT_L        0x48
#define REG_SIGNAL_PATH_RESET  0x68
#define REG_USER_CTRL          0x6A
#define REG_PWR_MGMT_1         0x6B
#define REG_PWR_MGMT_2         0x6C
#define REG_WHO_AM_I           0x75

// Expected WHOAMI value
#define EXPECTED_WHOAMI 0xFA

// Gyroscope sensitivity scales
#define GYRO_SCALE_250DPS  131.0f
#define GYRO_SCALE_500DPS  65.5f
#define GYRO_SCALE_1000DPS 32.8f
#define GYRO_SCALE_2000DPS 16.4f

// Global variables
int i2c_fd;
FILE *data_file = NULL;
int running = 1;
float gyro_offset[3] = {0.0f, 0.0f, 0.0f};
float temp_offset = 0.0f;

// Function prototypes
uint8_t i2c_read_byte(uint8_t reg);
void i2c_write_byte(uint8_t reg, uint8_t value);
int16_t i2c_read_word(uint8_t reg);
void initialize_sensor();
void perform_self_test_and_calibration();
void calculate_offsets(int samples);
void signal_handler(int sig);
void cleanup();

int main() {
    char filename[100];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    // Create timestamped filename
    strftime(filename, sizeof(filename), "gyro_data_%Y%m%d_%H%M%S.txt", t);
    
    // Open I2C device
    if ((i2c_fd = open(I2C_DEV, O_RDWR)) < 0) {
        perror("Failed to open I2C device");
        return 1;
    }
    
    // Set I2C slave address
    if (ioctl(i2c_fd, I2C_SLAVE, IAM20380HT_ADDR) < 0) {
        perror("Failed to set I2C slave address");
        close(i2c_fd);
        return 1;
    }
    
    // Check WHOAMI register
    uint8_t whoami = i2c_read_byte(REG_WHO_AM_I);
    printf("WHOAMI: 0x%02X (Expected: 0x%02X)\n", whoami, EXPECTED_WHOAMI);
    
    if (whoami != EXPECTED_WHOAMI) {
        printf("Warning: Unexpected WHOAMI value!\n");
        if (whoami == 0) {
            printf("No response. Check connections and I2C address.\n");
            close(i2c_fd);
            return 1;
        }
    } else {
        printf("Device identified successfully!\n");
    }
    
    // Open data file
    data_file = fopen(filename, "w");
    if (data_file == NULL) {
        perror("Failed to open data file");
        close(i2c_fd);
        return 1;
    }
    
    // Write header to file
    fprintf(data_file, "Timestamp,UnixTime,GyroX,GyroY,GyroZ,Temperature\n");
    
    // Set signal handling for graceful termination
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize the sensor
    initialize_sensor();
    
    // Perform self-test and calibration
    printf("Performing self-test and calibration...\n");
    perform_self_test_and_calibration();
    
    // Calculate offsets from raw readings
    printf("Calculating offsets...\n");
    calculate_offsets(200);
    printf("Gyro offsets: X=%.2f, Y=%.2f, Z=%.2f\n", 
           gyro_offset[0], gyro_offset[1], gyro_offset[2]);
    printf("Temperature offset: %.2f\n", temp_offset);
    
    // Setup for high-speed sampling (1000 Hz)
    i2c_write_byte(REG_SMPLRT_DIV, 0x00);  // No divider for max sample rate
    i2c_write_byte(REG_CONFIG, 0x00);      // Disable DLPF for maximum bandwidth
    
    printf("Starting data collection at 1000 Hz...\n");
    printf("Press Ctrl+C to stop\n");
    
    // Main sampling loop
    struct timespec start, now, elapsed;
    struct timespec sleep_time = {0, 50000};  // 50 microseconds for timing adjustment
    long sample_count = 0;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (running) {
        // Read gyroscope data
        int16_t gyro_x = i2c_read_word(REG_GYRO_XOUT_H);
        int16_t gyro_y = i2c_read_word(REG_GYRO_YOUT_H);
        int16_t gyro_z = i2c_read_word(REG_GYRO_ZOUT_H);
        
        // Read temperature data
        int16_t temp_raw = i2c_read_word(REG_TEMP_OUT_H);
        
        // Get current time
        time_t current_time = time(NULL);
        struct tm *t = localtime(&current_time);
        char time_str[20];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
        
        // Convert raw values to real-world units
        float gyro_x_dps = gyro_x / GYRO_SCALE_2000DPS - gyro_offset[0];
        float gyro_y_dps = gyro_y / GYRO_SCALE_2000DPS - gyro_offset[1];
        float gyro_z_dps = gyro_z / GYRO_SCALE_2000DPS - gyro_offset[2];
        
        // Apply temperature offset and correction (adjust scaling factor as needed)
        float temp_c = ((temp_raw / 340.0f) + 36.53f) - temp_offset;
        
        // Write data to file
        fprintf(data_file, "%s,%ld,%.3f,%.3f,%.3f,%.2f\n",
                time_str, current_time, gyro_x_dps, gyro_y_dps, gyro_z_dps, temp_c);
        
        // Increment sample counter
        sample_count++;
        
        // Calculate timing to maintain 1000 Hz sampling rate
        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed.tv_sec = now.tv_sec - start.tv_sec;
        elapsed.tv_nsec = now.tv_nsec - start.tv_nsec;
        if (elapsed.tv_nsec < 0) {
            elapsed.tv_sec--;
            elapsed.tv_nsec += 1000000000L;
        }
        
        // Adjust sleep time to maintain sampling rate
        long target_time = sample_count * 1000000; // 1ms (1000 Hz) in nanoseconds
        long current_time_ns = elapsed.tv_sec * 1000000000L + elapsed.tv_nsec;
        
        if (current_time_ns < target_time) {
            // Sleep for the remaining time to hit 1000 Hz
            sleep_time.tv_nsec = target_time - current_time_ns;
            nanosleep(&sleep_time, NULL);
        }
        
        // Flush data every 100 samples to ensure it's written to disk
        if (sample_count % 100 == 0) {
            fflush(data_file);
        }
    }
    
    cleanup();
    return 0;
}

// Read a single byte from the sensor
uint8_t i2c_read_byte(uint8_t reg) {
    uint8_t value;
    if (write(i2c_fd, &reg, 1) != 1) {
        perror("Failed to write to I2C bus");
        cleanup();
        exit(1);
    }
    if (read(i2c_fd, &value, 1) != 1) {
        perror("Failed to read from I2C bus");
        cleanup();
        exit(1);
    }
    return value;
}

// Write a single byte to the sensor
void i2c_write_byte(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    if (write(i2c_fd, buf, 2) != 2) {
        perror("Failed to write to I2C bus");
        cleanup();
        exit(1);
    }
    // Small delay to ensure write completes
    usleep(10);
}

// Read a 16-bit word from the sensor (big-endian)
int16_t i2c_read_word(uint8_t reg) {
    uint8_t buf[2];
    buf[0] = i2c_read_byte(reg);
    buf[1] = i2c_read_byte(reg + 1);
    return (buf[0] << 8) | buf[1];
}

// Initialize the sensor with appropriate settings
void initialize_sensor() {
    // Reset device
    i2c_write_byte(REG_PWR_MGMT_1, 0x80);
    usleep(100000);  // 100ms delay
    
    // Wake up the device
    i2c_write_byte(REG_PWR_MGMT_1, 0x01);  // Use PLL with X gyro reference
    usleep(10000);  // 10ms delay
    
    // Set gyro full scale range to ±2000 dps
    i2c_write_byte(REG_GYRO_CONFIG, 0x18);
    
    // No DLPF for maximum bandwidth
    i2c_write_byte(REG_CONFIG, 0x00);
    
    // Set sample rate divider to 0 for maximum rate
    i2c_write_byte(REG_SMPLRT_DIV, 0x00);
    
    printf("Sensor initialized for maximum performance\n");
}

// Perform self-test and calculate factory trim values
void perform_self_test_and_calibration() {
    // Ensure gyro and accel are enabled
    i2c_write_byte(REG_PWR_MGMT_2, 0x00);
    usleep(200000); // 200ms delay
    
    // Read factory trim values
    uint8_t self_test_x = i2c_read_byte(REG_SELF_TEST_X_GYRO);
    uint8_t self_test_y = i2c_read_byte(REG_SELF_TEST_Y_GYRO);
    uint8_t self_test_z = i2c_read_byte(REG_SELF_TEST_Z_GYRO);
    
    // Calculate factory trim
    float factory_trim_x = (float)((2620.0 / 8.0) * pow(1.01, (float)self_test_x - 1.0));
    float factory_trim_y = (float)((2620.0 / 8.0) * pow(1.01, (float)self_test_y - 1.0));
    float factory_trim_z = (float)((2620.0 / 8.0) * pow(1.01, (float)self_test_z - 1.0));
    
    printf("Factory Trim: X=%.2f, Y=%.2f, Z=%.2f\n", 
           factory_trim_x, factory_trim_y, factory_trim_z);
    
    // Get average values without self-test enabled
    float avg_x = 0, avg_y = 0, avg_z = 0;
    for (int i = 0; i < 200; i++) {
        avg_x += i2c_read_word(REG_GYRO_XOUT_H);
        avg_y += i2c_read_word(REG_GYRO_YOUT_H);
        avg_z += i2c_read_word(REG_GYRO_ZOUT_H);
        usleep(1000); // 1ms
    }
    avg_x /= 200.0;
    avg_y /= 200.0;
    avg_z /= 200.0;
    
    // Enable self-test
    i2c_write_byte(REG_GYRO_CONFIG, 0x18 | 0xE0); // 2000dps + self-test on all axes
    usleep(200000); // 200ms delay
    
    // Get average values with self-test enabled
    float avg_x_st = 0, avg_y_st = 0, avg_z_st = 0;
    for (int i = 0; i < 200; i++) {
        avg_x_st += i2c_read_word(REG_GYRO_XOUT_H);
        avg_y_st += i2c_read_word(REG_GYRO_YOUT_H);
        avg_z_st += i2c_read_word(REG_GYRO_ZOUT_H);
        usleep(1000); // 1ms
    }
    avg_x_st /= 200.0;
    avg_y_st /= 200.0;
    avg_z_st /= 200.0;
    
    // Calculate self-test response
    float str_x = avg_x_st - avg_x;
    float str_y = avg_y_st - avg_y;
    float str_z = avg_z_st - avg_z;
    
    // Calculate self-test ratio (STR / FT)
    float str_ratio_x = fabs(str_x / factory_trim_x);
    float str_ratio_y = fabs(str_y / factory_trim_y);
    float str_ratio_z = fabs(str_z / factory_trim_z);
    
    printf("Self-Test Response: X=%.2f, Y=%.2f, Z=%.2f\n", str_x, str_y, str_z);
    printf("Self-Test Ratio: X=%.2f, Y=%.2f, Z=%.2f\n", 
           str_ratio_x, str_ratio_y, str_ratio_z);
    
    // Check if self-test ratios are within acceptable range (typically 0.5 to 1.5)
    int pass = (str_ratio_x > 0.5 && str_ratio_x < 1.5 &&
                str_ratio_y > 0.5 && str_ratio_y < 1.5 &&
                str_ratio_z > 0.5 && str_ratio_z < 1.5);
    
    if (pass) {
        printf("Self-test PASSED!\n");
    } else {
        printf("Self-test FAILED! Values out of acceptable range.\n");
    }
    
    // Disable self-test and return to normal mode
    i2c_write_byte(REG_GYRO_CONFIG, 0x18); // 2000dps, self-test off
    usleep(100000); // 100ms delay
}

// Calculate offsets from multiple samples
void calculate_offsets(int samples) {
    // Take readings to calculate offsets
    float sum_gyro_x = 0, sum_gyro_y = 0, sum_gyro_z = 0;
    float sum_temp = 0;
    
    printf("Keep the sensor still for offset calculation...\n");
    usleep(1000000); // 1 second delay to prepare
    
    for (int i = 0; i < samples; i++) {
        int16_t gyro_x = i2c_read_word(REG_GYRO_XOUT_H);
        int16_t gyro_y = i2c_read_word(REG_GYRO_YOUT_H);
        int16_t gyro_z = i2c_read_word(REG_GYRO_ZOUT_H);
        int16_t temp_raw = i2c_read_word(REG_TEMP_OUT_H);
        
        sum_gyro_x += gyro_x / GYRO_SCALE_2000DPS;
        sum_gyro_y += gyro_y / GYRO_SCALE_2000DPS;
        sum_gyro_z += gyro_z / GYRO_SCALE_2000DPS;
        
        float temp_c = (temp_raw / 340.0f) + 36.53f;
        sum_temp += temp_c;
        
        usleep(5000); // 5ms delay between readings
    }
    
    // Calculate average offsets
    gyro_offset[0] = sum_gyro_x / samples;
    gyro_offset[1] = sum_gyro_y / samples;
    gyro_offset[2] = sum_gyro_z / samples;
    
    float avg_temp = sum_temp / samples;
    // Set temperature offset to adjust reading to room temperature (set to ~25°C)
    temp_offset = avg_temp - 25.0f;
}

// Signal handler for clean termination
void signal_handler(int sig) {
    printf("\nShutting down...\n");
    running = 0;
}

// Cleanup resources
void cleanup() {
    if (data_file) {
        fclose(data_file);
        printf("Data file closed\n");
    }
    
    if (i2c_fd >= 0) {
        close(i2c_fd);
        printf("I2C device closed\n");
    }
}