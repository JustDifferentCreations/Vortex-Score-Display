#ifndef TM1637_CUSTOM_H
#define TM1637_CUSTOM_H

#include <xc.h>
#include <util/delay.h>
#include "mcc_generated_files/system/system.h" 

// TM1637 Data Configurations
#define TM1637_CMD_DATA      0x40  
#define TM1637_CMD_ADDR      0xC0  
#define TM1637_CMD_CTRL      0x88  

static const uint8_t segment_map[] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

typedef struct {
    uint8_t total_digits;
    uint8_t display_id; 
} tm1637_t;

static inline void tm1637_delay(void) {
    _delay_us(5);
}

// --- TM1637 Low-Level Bit-Banging Signals ---

static void tm1637_start(const tm1637_t *display) {
    // 1. Force target channel pins to outputs and drive high
    switch(display->display_id) {
        case 0: BALL_DIO_SetDigitalOutput();   BALL_DIO_SetHigh();   BALL_CLK_SetHigh();   break;
        case 1: HOLE_DIO_SetDigitalOutput();   HOLE_DIO_SetHigh();   HOLE_CLK_SetHigh();   break;
        case 2: SCORE_DIO_SetDigitalOutput();  SCORE_DIO_SetHigh();  SCORE_CLK_SetHigh();  break;
        case 3: CREDIT_DIO_SetDigitalOutput(); CREDIT_DIO_SetHigh(); CREDIT_CLK_SetHigh(); break;
    }
    tm1637_delay();
    
    // 2. Drop DIO line low while CLK remains high
    switch(display->display_id) {
        case 0: BALL_DIO_SetLow();   break;
        case 1: HOLE_DIO_SetLow();   break;
        case 2: SCORE_DIO_SetLow();  break;
        case 3: CREDIT_DIO_SetLow(); break;
    }
    tm1637_delay();
    
    // 3. Bring CLK line down
    switch(display->display_id) {
        case 0: BALL_CLK_SetLow();   break;
        case 1: HOLE_CLK_SetLow();   break;
        case 2: SCORE_CLK_SetLow();  break;
        case 3: CREDIT_CLK_SetLow(); break;
    }
}

static void tm1637_stop(const tm1637_t *display) {
    // Bring DIO low and CLK high
    switch(display->display_id) {
        case 0: BALL_DIO_SetDigitalOutput();   BALL_DIO_SetLow();   BALL_CLK_SetHigh();   break;
        case 1: HOLE_DIO_SetDigitalOutput();   HOLE_DIO_SetLow();   HOLE_CLK_SetHigh();   break;
        case 2: SCORE_DIO_SetDigitalOutput();  SCORE_DIO_SetLow();  SCORE_CLK_SetHigh();  break;
        case 3: CREDIT_DIO_SetDigitalOutput(); CREDIT_DIO_SetLow(); CREDIT_CLK_SetHigh(); break;
    }
    tm1637_delay();
    
    // Bring DIO line high while CLK is high (Stop sequence)
    switch(display->display_id) {
        case 0: BALL_DIO_SetHigh();   break;
        case 1: HOLE_DIO_SetHigh();   break;
        case 2: SCORE_DIO_SetHigh();  break;
        case 3: CREDIT_DIO_SetHigh(); break;
    }
    tm1637_delay();
}

static void tm1637_write_byte(const tm1637_t *display, uint8_t data) {
    // Assert line output directions
    switch(display->display_id) {
        case 0: BALL_DIO_SetDigitalOutput();   break;
        case 1: HOLE_DIO_SetDigitalOutput();   break;
        case 2: SCORE_DIO_SetDigitalOutput();  break;
        case 3: CREDIT_DIO_SetDigitalOutput(); break;
    }

    // Shift 8 data bits out (LSB first)
    for (uint8_t i = 0; i < 8; i++) {
        switch(display->display_id) {
            case 0: 
                BALL_CLK_SetLow();   
                if (data & 0x01) BALL_DIO_SetHigh(); else BALL_DIO_SetLow(); 
                break;
            case 1: 
                HOLE_CLK_SetLow();   
                if (data & 0x01) HOLE_DIO_SetHigh(); else HOLE_DIO_SetLow(); 
                break;
            case 2: 
                SCORE_CLK_SetLow();  
                if (data & 0x01) SCORE_DIO_SetHigh(); else SCORE_DIO_SetLow(); 
                break;
            case 3: 
                CREDIT_CLK_SetLow(); 
                if (data & 0x01) CREDIT_DIO_SetHigh(); else CREDIT_DIO_SetLow(); 
                break;
        }
        data >>= 1;
        tm1637_delay();
        
        switch(display->display_id) {
            case 0: BALL_CLK_SetHigh();   break;
            case 1: HOLE_CLK_SetHigh();   break;
            case 2: SCORE_CLK_SetHigh();  break;
            case 3: CREDIT_CLK_SetHigh(); break;
        }
        tm1637_delay();
    }
    
    // Wire Handshake (ACK Frame simulation)
    switch(display->display_id) {
        case 0: BALL_CLK_SetLow();   BALL_DIO_SetDigitalInput();   break;
        case 1: HOLE_CLK_SetLow();   HOLE_DIO_SetDigitalInput();   break;
        case 2: SCORE_CLK_SetLow();  SCORE_DIO_SetDigitalInput();  break;
        case 3: CREDIT_CLK_SetLow(); CREDIT_DIO_SetDigitalInput(); break;
    }
    tm1637_delay();
    
    switch(display->display_id) {
        case 0: BALL_CLK_SetHigh();   break;
        case 1: HOLE_CLK_SetHigh();   break;
        case 2: SCORE_CLK_SetHigh();  break;
        case 3: CREDIT_CLK_SetHigh(); break;
    }
    tm1637_delay();
    
    switch(display->display_id) {
        case 0: BALL_CLK_SetLow();   break;
        case 1: HOLE_CLK_SetLow();   break;
        case 2: SCORE_CLK_SetLow();  break;
        case 3: CREDIT_CLK_SetLow(); break;
    }
}

// --- Main Testing & Driver Utility Functions ---

void tm1637_test_all_segments(const tm1637_t *display) {
    tm1637_start(display);
    tm1637_write_byte(display, TM1637_CMD_DATA);
    tm1637_stop(display);

    tm1637_start(display);
    tm1637_write_byte(display, TM1637_CMD_ADDR);
    for (uint8_t i = 0; i < display->total_digits; i++) {
        tm1637_write_byte(display, 0xFF); 
    }
    tm1637_stop(display);

    tm1637_start(display);
    tm1637_write_byte(display, TM1637_CMD_CTRL | 0x07); 
    tm1637_stop(display);
}

void tm1637_show_number(const tm1637_t *display, uint32_t number) {
    uint8_t buffer[6] = {0};
    uint32_t temp = number;

    if (display->total_digits == 6) {
        // Reversed array filling direction to fix flipped/backwards orientation text layout
        for (int8_t i = 5; i >= 0; i--) {
            buffer[i] = segment_map[temp % 10];
            temp /= 10;
        }
    } else {
        for (int8_t i = display->total_digits - 1; i >= 0; i--) {
            if (temp == 0 && i < (display->total_digits - 1) && number != 0) {
                buffer[i] = 0x00; 
            } else {
                buffer[i] = segment_map[temp % 10];
                temp /= 10;
            }
        }
    }

    tm1637_start(display);
    tm1637_write_byte(display, TM1637_CMD_DATA);
    tm1637_stop(display);

    tm1637_start(display);
    tm1637_write_byte(display, TM1637_CMD_ADDR);
    for (uint8_t i = 0; i < display->total_digits; i++) {
        tm1637_write_byte(display, buffer[i]);
    }
    tm1637_stop(display);

    tm1637_start(display);
    tm1637_write_byte(display, TM1637_CMD_CTRL | 0x03); 
    tm1637_stop(display);
}
/**
 * Pushes raw individual segment bytes directly to a display module.
 * Useful for animations, marquee effects, and custom alphanumeric strings.
 */
void tm1637_show_raw(const tm1637_t *display, const uint8_t *raw_segments) {
    tm1637_start(display);
    tm1637_write_byte(display, TM1637_CMD_DATA);
    tm1637_stop(display);

    tm1637_start(display);
    tm1637_write_byte(display, TM1637_CMD_ADDR);
    for (uint8_t i = 0; i < display->total_digits; i++) {
        tm1637_write_byte(display, raw_segments[i]);
    }
    tm1637_stop(display);

    tm1637_start(display);
    tm1637_write_byte(display, TM1637_CMD_CTRL | 0x03); 
    tm1637_stop(display);
}
#endif // TM1637_CUSTOM_H
