/*
    主机请求数据：主机（如单片机）向 DHT11 发送一个起始信号，该信号由至少 18ms 的低电平脉冲和随后 20
- 40μs 的高电平脉冲组成。 低电平脉冲用于唤醒 DHT11 传感器，高电平脉冲用于通知传感器准备传输数据。

    传感器响应：DHT11 传感器检测到起始信号后，会发送一个响应信号。这个响应信号由 80μs
的低电平脉冲和随后 80μs 的高电平脉冲组成。
低电平脉冲表示传感器已接收到主机的请求，高电平脉冲表示传感器准备好传输数据。

    数据传输：响应信号之后，DHT11 开始传输数据。数据以字节为单位进行传输，总共传输 5 个字节的数据，
分别是湿度整数部分、湿度小数部分、温度整数部分、温度小数部分和校验和。每个字节由 8
位组成，数据位的传输是从高位到低位。 在传输过程中，DHT11 通过拉低总线电平来表示数据
“0”，通过拉高总线电平来表示数据 “1”。每个数据位的传输时间约为 50μs。

    校验：主机接收到 5 个字节的数据后，会计算前 4 个字节的校验和，并与接收到的第 5
个字节（校验和）进行比较。
如果两者相等，则说明数据传输正确，主机可以使用这些数据；如果不相等，则说明数据传输过程中出现了错误，主机需要重新请求数据。
*/

#include "dht11.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define DHT11_TAG "DHT11"

#define DHT11_GPIO_INPUT_MODE(gpio_num) gpio_set_direction(gpio_num, GPIO_MODE_INPUT)
#define DHT11_GPIO_OUTPUT_MODE(gpio_num) gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT)
#define DHT11_GPIO_SET_LOW(gpio_num) gpio_set_level(gpio_num, 0)
#define DHT11_GPIO_SET_HIGH(gpio_num) gpio_set_level(gpio_num, 1)
#define DHT11_GPIO_GET_LEVEL(gpio_num) gpio_get_level(gpio_num)

static gpio_num_t dht11_gpio_num;

void dht11_init(gpio_num_t gpio_num)
{
    dht11_gpio_num        = gpio_num;
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode         = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    DHT11_GPIO_SET_HIGH(dht11_gpio_num);
}

static esp_err_t dht11_start_signal()
{
    DHT11_GPIO_OUTPUT_MODE(dht11_gpio_num);
    DHT11_GPIO_SET_LOW(dht11_gpio_num);
    esp_rom_delay_us(18000);
    DHT11_GPIO_SET_HIGH(dht11_gpio_num);
    esp_rom_delay_us(40);
    DHT11_GPIO_INPUT_MODE(dht11_gpio_num);
    return ESP_OK;
}

static esp_err_t dht11_check_response()
{
    uint32_t timeout = 2000;
    while (DHT11_GPIO_GET_LEVEL(dht11_gpio_num) == 1) {
        if (--timeout == 0) {
            ESP_LOGE("DHT11_RESPONSE", "TIMEOUT WAITTING FOR SENSOR TO PULL LOW");
            return ESP_ERR_TIMEOUT;
        }
        esp_rom_delay_us(1);
    }
    timeout = 2000;
    while (DHT11_GPIO_GET_LEVEL(dht11_gpio_num) == 0) {
        if (--timeout == 0) {
            ESP_LOGE("DHT11_RESPONSE", "TIMEOUT WAITTING FOR SENSOR TO PULL HIGH");
            return ESP_ERR_TIMEOUT;
        }
        esp_rom_delay_us(1);
    }
    timeout = 2000;
    while (DHT11_GPIO_GET_LEVEL(dht11_gpio_num) == 1) {
        if (--timeout == 0) {
            ESP_LOGE("DHT11_RESPONSE", "TIMEOUT WAITTING FOR SENSOR TO START DATA TRANSMISSION");
            return ESP_ERR_TIMEOUT;
        }
        esp_rom_delay_us(1);
    }
    return ESP_OK;
}

static uint8_t dht11_read_byte()
{
    uint8_t data = 0;
    for (int i = 0; i < 8; i++) {
        uint32_t timeout = 1000;
        while (DHT11_GPIO_GET_LEVEL(dht11_gpio_num) == 0) {
            if (--timeout == 0) {
                return 0;
            }
            esp_rom_delay_us(1);
        }
        esp_rom_delay_us(40);
        if (DHT11_GPIO_GET_LEVEL(dht11_gpio_num) == 1) {
            data |= (1 << (7 - i));
        }
        timeout = 1000;
        while (DHT11_GPIO_GET_LEVEL(dht11_gpio_num) == 1) {
            if (--timeout == 0) {
                return 0;
            }
            esp_rom_delay_us(1);
        }
    }
    return data;
}

esp_err_t dht11_read(dht11_data_t* data)
{
    uint8_t buffer[5] = {0};

    if (dht11_start_signal() != ESP_OK) {
        ESP_LOGE(DHT11_TAG, "Failed to send start signal");
        return ESP_FAIL;
    }

    if (dht11_check_response() != ESP_OK) {
        ESP_LOGE(DHT11_TAG, "Failed to get response");
        return ESP_FAIL;
    }

    for (int i = 0; i < 5; i++) {
        buffer[i] = dht11_read_byte();
    }

    if ((buffer[0] + buffer[1] + buffer[2] + buffer[3]) != buffer[4]) {
        ESP_LOGE(DHT11_TAG, "Checksum error");
        return ESP_FAIL;
    }

    char humidity_str[10];
    char tempurature_str[10];
    sprintf(humidity_str, "%d.%d", buffer[0], buffer[1]);
    sprintf(tempurature_str, "%d.%d", buffer[2], buffer[3]);

    data->humidity    = atof(humidity_str);
    data->temperature = atof(tempurature_str);
    // data->humidity    = buffer[0];
    // data->temperature = buffer[2];

    return ESP_OK;
}
