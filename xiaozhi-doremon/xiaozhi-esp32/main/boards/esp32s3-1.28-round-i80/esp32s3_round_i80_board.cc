#include "wifi_board.h"
#include "audio/codecs/no_audio_codec.h"
#include "display/eye_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "assets/lang_config.h"
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_gc9a01.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_io_expander_tca9554.h>
#include <esp_timer.h>
#include <iot_button.h>

#define TAG "Esp32S3RoundI80Board"

class Cst816d : public I2cDevice {
public:
    struct TouchPoint_t {
        int num = 0;
        int x = -1;
        int y = -1;
    };
    Cst816d(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        read_buffer_ = new uint8_t[6];
    }

    ~Cst816d() {
        delete[] read_buffer_;
    }

    void UpdateTouchPoint() {
        esp_err_t ret = TryReadRegs(0x02, read_buffer_, 6);
        if (ret != ESP_OK) {
            tp_.num = 0;
            return;
        }
        tp_.num = read_buffer_[0] & 0x0F;
        tp_.x = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2];
        tp_.y = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4];
    }

    const TouchPoint_t& GetTouchPoint() const {
        return tp_;
    }

private:
    uint8_t* read_buffer_ = nullptr;
    TouchPoint_t tp_;
};

class CustomLcdDisplay : public SpiLcdDisplay {
public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {}

    virtual void SetupUI() override {
        SpiLcdDisplay::SetupUI();
        DisplayLockGuard lock(this);
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.2, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.2, 0);
    }
};

class Esp32S3RoundI80Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    esp_io_expander_handle_t io_expander_ = nullptr;
    LcdDisplay* display_ = nullptr;
    Cst816d* touch_ = nullptr;
    esp_timer_handle_t touch_timer_ = nullptr;

    // IO expander button handles & drivers
    button_handle_t btn_power_ = nullptr;
    button_handle_t btn_up_ = nullptr;
    button_handle_t btn_down_ = nullptr;
    button_driver_t* btn_power_driver_ = nullptr;
    button_driver_t* btn_up_driver_ = nullptr;
    button_driver_t* btn_down_driver_ = nullptr;

    static Esp32S3RoundI80Board* instance_;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = { .enable_internal_pullup = 1 },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeIoExpander() {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_bus_, 0x20, &io_expander_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize TCA9554 IO expander at 0x20");
            return;
        }
        // Set side buttons as input
        esp_io_expander_set_dir(io_expander_, (1 << XIO_KEY_UP) | (1 << XIO_KEY_DOWN) | (1 << XIO_KEY_POWER), IO_EXPANDER_INPUT);
    }

    void InitializeDisplay() {
        esp_lcd_i80_bus_handle_t i80_bus = NULL;
        esp_lcd_i80_bus_config_t bus_config = {
            .dc_gpio_num = LCD_PIN_DC,
            .wr_gpio_num = LCD_PIN_WR,
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .data_gpio_nums = {
                LCD_PIN_D0, LCD_PIN_D1, LCD_PIN_D2, LCD_PIN_D3,
                LCD_PIN_D4, LCD_PIN_D5, LCD_PIN_D6, LCD_PIN_D7,
            },
            .bus_width = 8,
            .max_transfer_bytes = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t),
            .psram_trans_align = 64,
            .sram_trans_align = 4,
        };
        ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_i80_config_t io_config = {
            .cs_gpio_num = LCD_PIN_CS,
            .pclk_hz = 20 * 1000 * 1000,
            .trans_queue_depth = 10,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .dc_levels = {
                .dc_idle_level = 0,
                .dc_cmd_level = 0,
                .dc_dummy_level = 0,
                .dc_data_level = 1,
            },
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

        esp_lcd_panel_handle_t panel_handle = NULL;
        esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = LCD_PIN_RST,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
            .bits_per_pixel = 16,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
        
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
        
        display_ = new EyeDisplay(io_handle, panel_handle,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouch() {
        gpio_config_t rst_cfg = {
            .pin_bit_mask = (1ULL << TOUCH_RST_PIN),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&rst_cfg);

        gpio_set_level((gpio_num_t)TOUCH_RST_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level((gpio_num_t)TOUCH_RST_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));

        touch_ = new Cst816d(i2c_bus_, 0x15);
        
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                auto self = (Esp32S3RoundI80Board*)arg;
                self->touch_->UpdateTouchPoint();
                auto tp = self->touch_->GetTouchPoint();
                if (tp.num > 0) {
                    auto display = self->GetDisplay();
                    if (display) {
                        display->HandleTouch(tp.x, tp.y);
                    }
                    Application::GetInstance().ToggleChatState();
                }
            },
            .arg = this,
            .name = "touch_timer"
        };
        esp_timer_create(&timer_args, &touch_timer_);
        esp_timer_start_periodic(touch_timer_, 20000);
    }

    uint8_t IoExpanderGetKeyLevel(uint32_t pin_bit) {
        if (!io_expander_) return 0;
        uint32_t pin_val = 0;
        uint32_t pin_mask = (1 << XIO_KEY_UP) | (1 << XIO_KEY_POWER) | (1 << XIO_KEY_DOWN);
        esp_io_expander_get_level(io_expander_, pin_mask, &pin_val);
        return (uint8_t)((pin_val & (1 << pin_bit)) ? 1 : 0);
    }

    void InitializeButtons() {
        instance_ = this;

        // --- POWER button (IO expander pin XIO_KEY_POWER) ---
        button_config_t pwr_cfg = {
            .long_press_time = 2000,
            .short_press_time = 0,
        };
        btn_power_driver_ = (button_driver_t*)calloc(1, sizeof(button_driver_t));
        btn_power_driver_->enable_power_save = false;
        btn_power_driver_->get_key_level = [](button_driver_t* drv) -> uint8_t {
            return !instance_->IoExpanderGetKeyLevel(XIO_KEY_POWER);
        };
        ESP_ERROR_CHECK(iot_button_create(&pwr_cfg, btn_power_driver_, &btn_power_));

        // Single click: WiFi config during startup, otherwise toggle chat
        iot_button_register_cb(btn_power_, BUTTON_SINGLE_CLICK, nullptr,
            [](void* handle, void* usr) {
                auto self = static_cast<Esp32S3RoundI80Board*>(usr);
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateStarting) {
                    self->EnterWifiConfigMode();
                    return;
                }
                app.ToggleChatState();
            }, this);

        // Long press: enter WiFi config from any state
        iot_button_register_cb(btn_power_, BUTTON_LONG_PRESS_START, nullptr,
            [](void* handle, void* usr) {
                auto self = static_cast<Esp32S3RoundI80Board*>(usr);
                ESP_LOGI(TAG, "Power long press -> EnterWifiConfigMode");
                self->EnterWifiConfigMode();
            }, this);

        // --- UP button (IO expander pin XIO_KEY_UP) ---
        button_config_t up_cfg = {
            .long_press_time = 1000,
            .short_press_time = 0,
        };
        btn_up_driver_ = (button_driver_t*)calloc(1, sizeof(button_driver_t));
        btn_up_driver_->enable_power_save = false;
        btn_up_driver_->get_key_level = [](button_driver_t* drv) -> uint8_t {
            return !instance_->IoExpanderGetKeyLevel(XIO_KEY_UP);
        };
        ESP_ERROR_CHECK(iot_button_create(&up_cfg, btn_up_driver_, &btn_up_));

        // Click: volume up +10
        iot_button_register_cb(btn_up_, BUTTON_SINGLE_CLICK, nullptr,
            [](void* handle, void* usr) {
                auto self = static_cast<Esp32S3RoundI80Board*>(usr);
                auto codec = self->GetAudioCodec();
                auto volume = codec->output_volume() + 10;
                if (volume > 100) volume = 100;
                codec->SetOutputVolume(volume);
                self->GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
            }, this);

        // --- DOWN button (IO expander pin XIO_KEY_DOWN) ---
        button_config_t down_cfg = {
            .long_press_time = 1000,
            .short_press_time = 0,
        };
        btn_down_driver_ = (button_driver_t*)calloc(1, sizeof(button_driver_t));
        btn_down_driver_->enable_power_save = false;
        btn_down_driver_->get_key_level = [](button_driver_t* drv) -> uint8_t {
            return !instance_->IoExpanderGetKeyLevel(XIO_KEY_DOWN);
        };
        ESP_ERROR_CHECK(iot_button_create(&down_cfg, btn_down_driver_, &btn_down_));

        // Click: volume down -10
        iot_button_register_cb(btn_down_, BUTTON_SINGLE_CLICK, nullptr,
            [](void* handle, void* usr) {
                auto self = static_cast<Esp32S3RoundI80Board*>(usr);
                auto codec = self->GetAudioCodec();
                auto volume = codec->output_volume() - 10;
                if (volume < 0) volume = 0;
                codec->SetOutputVolume(volume);
                self->GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
            }, this);

        ESP_LOGI(TAG, "Buttons initialized: POWER(WiFi/Chat), UP(Vol+), DOWN(Vol-)");
    }

public:
    Esp32S3RoundI80Board() {
        ESP_LOGI(TAG, "Initializing Esp32S3RoundI80Board (V2 - Fixed I2C & IO Expander)");
        InitializeI2c();
        InitializeIoExpander();
        InitializeButtons();
        InitializeDisplay();
        InitializeTouch();
        GetAudioCodec()->SetInputGain(3.0f);
        GetBacklight()->RestoreBrightness();
}

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecDuplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

Esp32S3RoundI80Board* Esp32S3RoundI80Board::instance_ = nullptr;

DECLARE_BOARD(Esp32S3RoundI80Board);
