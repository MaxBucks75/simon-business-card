#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "src/T3A33BRG.h"
#include "src/platform.h"
/*
Software Requirements

You will create a game with the following requirements:

Startup Behavior
- When powered on, the system shall enter an idle mode that clearly indicates the device is ready to begin a game.
- The idle mode shall include a repeating LED animation that utilizes all four LEDs and gives the appearance of motion.

Starting a Game
- A method shall be provided to intentionally start a new game while minimizing accidental activation.
- The start action shall require simultaneous interaction with multiple touch inputs and shall include a minimum hold time.

Game Play
- The game shall generate a random sequence of events involving the four available inputs.
- Ensure that each time the game is played, a new random sequence is generated.
- The sequence shall be presented to the user using the LEDs.
- After the sequence is presented, the user shall be given an opportunity to reproduce it using the touch inputs.
- Visual feedback shall be provided whenever the user interacts with the game.

Progression
- Successful completion of a round shall result in extending the current turn.
- The challenge shall increase by extending the sequence that the player must remember and reproduce.
- The game shall continue until the player makes an error.

*/

/* Button configuration on board:
 * Button 0: Top Left
 * Button 1: Top Right
 * Button 2: Bottom Right
 * Button 3: Bottom Left
 *
 * LED i is assumed to sit under button i (so 0->1->2->3 is clockwise).
 */

#define NUM_BUTTONS     4
#define MAX_ROUNDS      256
#define START_HOLD_MS   2000  // both top buttons must be held this long to start
#define DEBOUNCE_MS     20    // a touch must be held this long to count

static Rgb pixels[4];

static const uint8_t    off[] = {0,   0,    0 };
static const uint8_t    red[] = {64,  0,    0 };
static const uint8_t  green[] = {0,   64,   0 };
static const uint8_t   blue[] = {0,   0,    64};
static const uint8_t yellow[] = {64,  64,   0 };
static const uint8_t purple[] = {64,  0,    64};
static const uint8_t   cyan[] = {0,   64,   64};
static const uint8_t  white[] = {64,  64,   64};

typedef enum {IDLE, PLAYING} State;
static State state;

// btn_ms_ti: time (ms since boot) the button was first seen down, or -1 if up
// btn_ms_th: how long (ms) the button has currently been held, 0 if up
static int32_t btn_ms_ti[NUM_BUTTONS] = {-1, -1, -1, -1};
static int32_t btn_ms_th[NUM_BUTTONS] = {0, 0, 0, 0};

static int rounds;
static int sequence[MAX_ROUNDS];

// Updates the each pixel on the board to the color argument passed
void update_lights(const uint8_t* led_0, const uint8_t* led_1, const uint8_t* led_2, const uint8_t* led_3) {
  const uint8_t* leds[4] = {led_0, led_1, led_2, led_3};
  for (int i = 0; i < 4; i++) {
    pixels[i].r = leds[i][0];
    pixels[i].g = leds[i][1];
    pixels[i].b = leds[i][2];
  }
  T3A33BRG_write(pixels, 4);
}

// Creates a spiral pattern with the LEDs on the board, starting from the top left
// lighting up one tile at a time clock-wise
void spiral_animation(const uint8_t* color, int delay) {
  update_lights(color, off, off, off);
  platform_delay_ms(delay);
  update_lights(off, color, off, off);
  platform_delay_ms(delay);
  update_lights(off, off, color, off);
  platform_delay_ms(delay);
  update_lights(off, off, off, color);
  platform_delay_ms(delay);
}

// Creates a spiral pattern with the LEDs on the board, starting from the top left
// turning off one tile at a time clock-wise
void inverted_spiral_animation(const uint8_t* color, int delay) {
  update_lights(off, color, color, color);
  platform_delay_ms(delay);
  update_lights(color, off, color, color);
  platform_delay_ms(delay);
  update_lights(color, color, off, color);
  platform_delay_ms(delay);
  update_lights(color, color, color, off);
  platform_delay_ms(delay);
}

// Does a spiral animation followed by an inverted spiral animation for each
// color defined above. Plays once when the device is powered on
void startup_animation(int delay) {
  // red spirals
  spiral_animation(red, delay);
  inverted_spiral_animation(red, delay);
  // green spirals
  spiral_animation(green, delay);
  inverted_spiral_animation(green, delay);
  // blue spirals
  spiral_animation(blue, delay);
  inverted_spiral_animation(blue, delay);
  // yellow spirals
  spiral_animation(yellow, delay);
  inverted_spiral_animation(yellow, delay);
  // purple spirals
  spiral_animation(purple, delay);
  inverted_spiral_animation(purple, delay);
  // cyan spirals
  spiral_animation(cyan, delay);
  inverted_spiral_animation(cyan, delay);
  // hold on dark for a moment
  update_lights(off, off, off, off);
  platform_delay_ms(delay * 3);
}

// Tracks how long each capacitive button has been held. Call this often.
void update_btn_status(void) {
  uint32_t now = platform_millis();
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (platform_is_button_down(i)) {
      if (btn_ms_ti[i] == -1) {
        btn_ms_ti[i] = (int32_t)now;
        btn_ms_th[i] = 0;
      } else {
        btn_ms_th[i] = (int32_t)now - btn_ms_ti[i];
      }
    } else {
      btn_ms_ti[i] = -1;
      btn_ms_th[i] = 0;
    }
  }
}

bool any_button_down(void) {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (platform_is_button_down(i)) return true;
  }
  return false;
}

// Idle screen. Called repeatedly from the IDLE loop, must not block for long.
//  - Nobody touching the top buttons: a single light chases clockwise around
//    all four LEDs, changing color every lap.
//  - Both top buttons held: the top two LEDs fade from white to red over
//    START_HOLD_MS.
//  - Held long enough (armed): the top two LEDs blink red, telling the user
//    to let go. The game starts once every button is released.
//
// Returns true when the game should start.
bool idle_animation(void) {
  static bool armed = false;
  static const uint8_t* const lap_colors[] = {red, green, blue, yellow, purple, cyan};

  uint32_t now = platform_millis();
  int32_t hold = (btn_ms_th[0] < btn_ms_th[1]) ? btn_ms_th[0] : btn_ms_th[1];

  if (hold >= START_HOLD_MS) {
    armed = true;
  }

  if (armed) {
    if (!any_button_down()) {
      armed = false;
      return true;
    }
    // blink red at ~5 Hz
    if ((now / 100) % 2) {
      update_lights(red, red, off, off);
    } else {
      update_lights(off, off, off, off);
    }
  } else if (hold > 0) {
    // drift from white -> red while held (integer math: multiply before divide)
    uint8_t dimming_factor = (uint8_t)((64 * hold) / START_HOLD_MS);
    uint8_t led[3] = {64, (uint8_t)(64 - dimming_factor), (uint8_t)(64 - dimming_factor)};
    update_lights(led, led, off, off);
  } else {
    // chase animation
    uint32_t step = now / 150;
    int pos = step % 4;
    const uint8_t* c = lap_colors[(step / 4) % 6];
    update_lights(pos == 0 ? c : off,
                  pos == 1 ? c : off,
                  pos == 2 ? c : off,
                  pos == 3 ? c : off);
  }
  return false;
}

// Create a sequence of random numbers 0-3
void generate_sequence(void) {
  for (int i = 0; i < MAX_ROUNDS; i++) {
    sequence[i] = rand() % 4;
  }
}

// Goes through the sequence with varying delays for the lights to be on or off
void show_sequence(void) {

  // short pause so the sequence doesn't run into the previous animation
  update_lights(off, off, off, off);
  platform_delay_ms(500);

  for (int i = 0; i < rounds; i++) {

    switch (sequence[i]) {
      case 0: update_lights(green, off, off, off);  break;
      case 1: update_lights(off, red, off, off);    break;
      case 2: update_lights(off, off, blue, off);   break;
      case 3: update_lights(off, off, off, yellow); break;
      default: break; // should never execute
    }
    platform_delay_ms(400 + rand() % 100);

    // turn off between steps so repeated colors are distinguishable,
    // slightly varying the gap
    update_lights(off, off, off, off);
    platform_delay_ms(200 + rand() % 100);
  }

  // flash white to all LEDs to indicate that the sequence has finished playing
  update_lights(white, white, white, white);
  platform_delay_ms(500);
  update_lights(off, off, off, off);

}

// Display all of the colors corresponding to each button until the user presses one
// of the buttons. At that point, show only the button pressed
//
// Returns the button that was pressed or -1
int respond_to_input(void) {
  if (btn_ms_th[0] >= DEBOUNCE_MS) {
    update_lights(green, off, off, off);
    return 0;
  } else if (btn_ms_th[1] >= DEBOUNCE_MS) {
    update_lights(off, red, off, off);
    return 1;
  } else if (btn_ms_th[2] >= DEBOUNCE_MS) {
    update_lights(off, off, blue, off);
    return 2;
  } else if (btn_ms_th[3] >= DEBOUNCE_MS) {
    update_lights(off, off, off, yellow);
    return 3;
  } else {
    update_lights(green, red, blue, yellow);
    return -1;
  }
}

// Waits for one full touch (press and release) and returns which button it was.
// The pressed button's LED stays lit while it is held.
int wait_for_input(void) {
  // don't count a finger that is still down from before
  while (any_button_down()) {
    platform_delay_ms(10);
  }
  update_btn_status();

  int guess = -1;
  while (guess == -1) {
    update_btn_status();
    guess = respond_to_input();
    platform_delay_ms(10);
  }

  // keep showing the pressed button until it is let go
  while (platform_is_button_down(guess)) {
    platform_delay_ms(10);
  }
  platform_delay_ms(50); // release debounce
  update_lights(off, off, off, off);

  return guess;
}

// Flash green twice to the user on a correct input
void correct_animation(void) {
  update_lights(green, green, green, green);
  platform_delay_ms(100);
  update_lights(off, off, off, off);
  platform_delay_ms(50);
  update_lights(green, green, green, green);
  platform_delay_ms(100);
  update_lights(off, off, off, off);
  platform_delay_ms(50);
}

// Flash red twice to the user on an incorrect input
void incorrect_animation(void) {
  update_lights(red, red, red, red);
  platform_delay_ms(100);
  update_lights(off, off, off, off);
  platform_delay_ms(100);
  update_lights(red, red, red, red);
  platform_delay_ms(100);
  update_lights(off, off, off, off);
  platform_delay_ms(100);
}

// main gameplay loop: Shows the sequence, waits for the user to guess the sequence, signals
// to the user if their guess was correct or incorrect and if they have finished the sequence.
//
// Returns 1 if the user repeats the sequence correctly, -1 if not
int gameplay(void) {

  show_sequence();

  for (int i = 0; i < rounds; i++) {

    int guess = wait_for_input();

    if (guess == sequence[i]) {
      correct_animation();
    } else {
      incorrect_animation();
      return -1;
    }

  }

  // correct sequence
  spiral_animation(green, 50);
  update_lights(off, off, off, off);
  return 1;

}

// show 6 red spiral animations on a loss
void game_over_animation(void) {
  inverted_spiral_animation(red, 100);
  spiral_animation(red, 100);
  inverted_spiral_animation(red, 50);
  spiral_animation(red, 50);
  inverted_spiral_animation(red, 25);
  spiral_animation(red, 25);
  update_lights(off, off, off, off);
}

void startup(void) {

  /* This function needs to be called to initialize the SPI interface used to write to the T3A33BRG */
  /* It also initializes the GPIO pins for the buttons */
  platform_init();

  /* Delay for a bit to allow the system to settle */
  platform_delay_ms(1000);

  /* Make sure to call this function before using the T3A33BRG */
  T3A33BRG_init(platform_spi_write_bytes);

  startup_animation(100);

  update_btn_status();

  state = IDLE;

}

int main(void) {

  startup(); // stdio is initialized in here, so print afterwards

  printf("/**************/\n\r");
  printf("/* Simon Demo */\n\r");
  printf("/**************/\n\r");

  for (;;) {

    switch (state) {

      case IDLE:

        update_btn_status();
        if (idle_animation()) {
          // transition between IDLE and PLAYING states
          update_lights(off, off, off, off);
          platform_delay_ms(500);
          state = PLAYING;
        } else {
          platform_delay_ms(10);
        }
        break;

      case PLAYING:

        // seed from the microsecond timer at the moment the player let go,
        // so every game (even right after power-up) gets a new sequence
        srand(time_us_32());
        generate_sequence();
        rounds = 1;

        while (gameplay() > 0 && rounds < MAX_ROUNDS) {
          rounds++;
        }

        printf("Game over, reached round %d\n\r", rounds);
        game_over_animation();
        platform_delay_ms(1000);

        state = IDLE;
        break;

      default:
        state = IDLE;
        break;

    }
  }

}
