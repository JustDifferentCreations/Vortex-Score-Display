#include "mcc_generated_files/system/system.h"
#include "tm1637_custom.h"
#include <avr/interrupt.h> 

const tm1637_t display_ball   = { .total_digits = 2, .display_id = 0 };
const tm1637_t display_hole   = { .total_digits = 3, .display_id = 1 };
const tm1637_t display_score  = { .total_digits = 6, .display_id = 2 };
const tm1637_t display_credit = { .total_digits = 2, .display_id = 3 };

volatile uint8_t  global_balls      = 0;   
volatile uint16_t global_hole_value = 0;   
volatile uint32_t global_score      = 0;   
volatile uint8_t  global_credits    = 0;   

volatile bool game_active             = false;
volatile bool refresh_displays_needed = false;

// --- Debounce Lockout Counters ---
volatile uint8_t credit_lockout_timer = 0; 
volatile uint8_t start_lockout_timer  = 0;
volatile uint8_t hole_lockout_timer   = 0; 
volatile uint8_t drain_lockout_timer  = 0; 
volatile uint8_t tilt_lockout_timer   = 0; // Debounce tracking for the tilt pendulum sensor

uint16_t hole_next_base_value = 100;
uint16_t hole_decay_cycle_counter = 0; 

// --- Attract Mode Animation Tracking Structures ---
uint16_t attract_frame_counter = 0;
uint8_t  anim_index = 0;

// --- Added Feature Tracking Counters ---
uint16_t ball_added_light_timer = 0; 

// Chasing light sequence: lights up outer segments sequentially
const uint8_t chase_sequence[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20};

void MC1_Custom_ISR(void) {
    if (credit_lockout_timer == 0) {
        global_credits++;
        if (global_credits > 99) global_credits = 0; 
        if (global_credits > 0)  MC8_SetHigh();
        refresh_displays_needed = true;
        credit_lockout_timer = 20; 
    }
}

void MC2_Custom_ISR(void) {
    if (start_lockout_timer == 0) {
        if (!game_active && global_credits > 0) {
            global_credits--;      
            global_balls = 3;      
            hole_next_base_value = 100; 
            global_hole_value = hole_next_base_value; 
            global_score = 0;      
            
            if (global_credits == 0) MC8_SetLow();
            
            GM_OVR_LT_SetHigh(); // Turn off Game Over lamp when game starts
            TILT_LT_SetHigh();   // Turn off the Tilt lamp when a new game starts
            
            hole_decay_cycle_counter = 0; 
            game_active = true;    
            refresh_displays_needed = true;
        }
        start_lockout_timer = 30; 
    }
}

void MC3_Custom_ISR(void) {
    if (hole_lockout_timer == 0) {
        if (game_active && global_balls > 0) {
            global_score += global_hole_value;
            
            if (global_hole_value > 700) {
                global_balls++;
                if (global_balls > 99) global_balls = 99;
                
                B_ADD_LT_SetLow(); // Pull low to turn lamp ON
                ball_added_light_timer = 1000; 
            }
            
            hole_next_base_value += 100;
            if (hole_next_base_value > 999) hole_next_base_value = 999;
            global_hole_value = hole_next_base_value;
            hole_decay_cycle_counter = 0; 
            refresh_displays_needed = true;
        }
        hole_lockout_timer = 40; 
    }
}

void MC4_Custom_ISR(void) {
    if (drain_lockout_timer == 0) {
        if (game_active && global_balls > 0) {
            global_balls--;
            if (global_balls == 0) {
                game_active = false;
                global_hole_value = 0; 
                GM_OVR_LT_SetLow(); // Turn Game Over lamp ON
            } else {
                hole_decay_cycle_counter = 0;
            }
            refresh_displays_needed = true;
        }
        drain_lockout_timer = 40; 
    }
}

/**
 * Custom ISR Callback for the Tilt Pin (MC5)
 */
void MC5_Custom_ISR(void) {
    if (tilt_lockout_timer == 0) {
        // Only register a tilt if a game is actively running
        if (game_active) {
            game_active = false;      // Stop all time-decay and score evaluations
            global_balls = 0;         // Strip remaining attempts
            global_hole_value = 0;    // Clear active point targets
            
            GM_OVR_LT_SetLow();       // Turn Game Over lamp ON (Pull Low)
            TILT_LT_SetLow();         // Turn Tilt lamp ON (Pull Low)
            
            refresh_displays_needed = true;
        }
        tilt_lockout_timer = 100;     // 1-second debounce window to absorb heavy pendulum impacts
    }
}

int main(void) {
    SYSTEM_Initialize();

    MC1_SetInterruptHandler(MC1_Custom_ISR);
    MC2_SetInterruptHandler(MC2_Custom_ISR);
    MC3_SetInterruptHandler(MC3_Custom_ISR); 
    MC4_SetInterruptHandler(MC4_Custom_ISR); 
    MC5_SetInterruptHandler(MC5_Custom_ISR); // Register the Tilt callback

    sei(); 

    // Initial hardware output configurations on bootup
    GM_OVR_LT_SetLow(); 
    TILT_LT_SetHigh();  // Start with the tilt light OFF
    B_ADD_LT_SetHigh(); 
    MC8_SetLow(); 

    tm1637_test_all_segments(&display_ball);
    tm1637_test_all_segments(&display_hole);
    tm1637_test_all_segments(&display_score);
    tm1637_test_all_segments(&display_credit);
    _delay_ms(2000); 

    tm1637_show_number(&display_ball, global_balls);
    tm1637_show_number(&display_hole, global_hole_value);
    tm1637_show_number(&display_score, global_score);
    tm1637_show_number(&display_credit, global_credits);

    bool was_in_attract_mode = false;

    while (1) {
        if (credit_lockout_timer > 0) credit_lockout_timer--;
        if (start_lockout_timer > 0)  start_lockout_timer--;
        if (hole_lockout_timer > 0)   hole_lockout_timer--;
        if (drain_lockout_timer > 0)  drain_lockout_timer--;
        if (tilt_lockout_timer > 0)   tilt_lockout_timer--;

        // --- Ball Added Lamp Window Countdown ---
        if (ball_added_light_timer > 0) {
            ball_added_light_timer--;
            if (ball_added_light_timer == 0) {
                B_ADD_LT_SetHigh(); // Turn lamp OFF
            }
        }

        // --- Attract Mode Logic Tree ---
        if (!game_active && MC6_GetValue() == false) {
            was_in_attract_mode = true; 
            attract_frame_counter++;
            
            if (attract_frame_counter >= 10) {
                attract_frame_counter = 0;
                anim_index++;
                if (anim_index >= 6) anim_index = 0;
                
                uint8_t seg2[] = { chase_sequence[anim_index], chase_sequence[(anim_index + 3) % 6] };
                uint8_t seg3[] = { chase_sequence[anim_index], chase_sequence[(anim_index + 2) % 6], chase_sequence[(anim_index + 4) % 6] };
                uint8_t seg6[] = { chase_sequence[anim_index], chase_sequence[(anim_index + 1) % 6], chase_sequence[(anim_index + 2) % 6],
                                    chase_sequence[(anim_index + 3) % 6], chase_sequence[(anim_index + 4) % 6], chase_sequence[(anim_index + 5) % 6] };
                
                tm1637_show_raw(&display_ball, seg2);
                tm1637_show_raw(&display_hole, seg3);
                tm1637_show_raw(&display_score, seg6);
                tm1637_show_number(&display_credit, global_credits);
            }
        } 
        else {
            if (was_in_attract_mode) {
                was_in_attract_mode = false;
                refresh_displays_needed = true; 
            }

            if (game_active) {
                hole_decay_cycle_counter++;
                if (hole_decay_cycle_counter >= 1000) {
                    hole_decay_cycle_counter = 0; 
                    if (global_hole_value > 50) {
                        global_hole_value -= 10;
                        refresh_displays_needed = true; 
                    }
                }
            }

            if (refresh_displays_needed) {
                refresh_displays_needed = false; 
                tm1637_show_number(&display_ball, global_balls);
                tm1637_show_number(&display_hole, global_hole_value);
                tm1637_show_number(&display_score, global_score);
                tm1637_show_number(&display_credit, global_credits);
            }
        }
        
        _delay_ms(10);
    }
    return 0;
}
