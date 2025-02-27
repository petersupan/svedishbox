#include "en_graevmachin.h"
#include "en_harunge.h"
#include "ett_hus.h"
#include "ett_tag.h"
#include "en_lastbil.h"
#include "en_pankaka.h"
#include "en_banan.h"
#include "en_bil.h"
#include "en_bot.h"
#include "en_katt.h"
#include "en_sol.h"
#include "ett_aepple.h"

#define make_audio(_name_) {_name_, sizeof(_name_)}

#define DAC1 25
#define DAC2 26

struct Button {
  const uint8_t PIN;
  // at which time did the button start to have a "pressed" signal
  unsigned long highStartTime;
  // at which time did the button start to have a "not pressed" signal
  unsigned long lowStartTime;
  bool pressed;
  // pressed for more than 20msec, I can trust it
  bool reallyPressed;
};

struct Audio {
  const byte* data;
  int size;
};

struct Player {
  unsigned long previousSample;
  bool playing;
  unsigned long playingStart;
};
Player player = {0, false, 0};

int nrAudios = 6;

// audios for first page
Audio audios2[] = { make_audio(en_banan),
                   make_audio(en_bil),
                   make_audio(en_bot),
                   make_audio(en_katt),
                   make_audio(en_sol),
                   make_audio(ett_aepple) 
                   };

// audios for second page
Audio audios[] = { make_audio(en_graevmachin),   // rosa
                   make_audio(en_harunge),      // gelb
                   make_audio(ett_hus),         // schwarz
                   make_audio(ett_tag),         // weiss
                   make_audio(en_lastbil),      // blau
                   make_audio(en_pankaka)       // gruen
                   };

Audio* current_page = audios;

Audio* current_audio = &audios[0];

Button button1 = {18, 0, 0, false, false};
Button button2 = {19, 0, 0, false, false};
Button button3 = {21, 0, 0, false, false};
Button button4 = {22, 0, 0, false, false};
Button button5 = {23, 0, 0, false, false};
Button button6 = {5, 0, 0, false, false};

Button modeButton = {4, 0, 0, false, false};

int nrButtons = 6;

// pink, yellow, black, white, blue, green
Button buttons[] = {button1, button2, button3, button4, button5, button6};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(button1.PIN, INPUT_PULLUP);
  pinMode(button2.PIN, INPUT_PULLUP);
  pinMode(button3.PIN, INPUT_PULLUP);
  pinMode(button4.PIN, INPUT_PULLUP);
  pinMode(button5.PIN, INPUT_PULLUP);
  pinMode(button6.PIN, INPUT_PULLUP);
  Serial.print("Rebecca version 2");

  pinMode(modeButton.PIN, INPUT_PULLUP);

  //ledcAttachPin(25, 1);
  //ledcSetup(1, 22050, 8);
}

// hacky hack to handle flickering of the button signal around the threshold flickering during pressing.
// Only a button which has uninterrupted "pressed" signal for more than 20msec
// is considered as pressed (and the same with "released")
void prepareButton(Button* b) {
  unsigned long currentTime = micros();
  // low is high and high is low...
  if (digitalRead(b->PIN) == LOW) {  
    if ( !b->pressed) {
      b->pressed = true;
      b->highStartTime = currentTime;
    } else {
      unsigned long diff = currentTime - b->highStartTime;
      if (diff > 20000)
        b->reallyPressed = true;
    }
  } else {
    if ( b->pressed) {
      b->pressed = false;
      b->lowStartTime = currentTime;
    } else {
      unsigned long diff = currentTime - b->lowStartTime;
      if (diff > 20000)
        b->reallyPressed = false;
    }
  }
}

// fade in to avoid popping when starting audio
// would not be necessary if my base level was zero and not 128...
void fade_in() {
  for(int i = 0; i < 128; i++) {
    dacWrite(DAC1, i);
    delay(4);
    yield();
  }
}

// fade out to avoid popping when stopping audio
// would not be necessary if my base level was zero and not 128...
void fade_out() {
   for(int i = 128; i >= 0; i--) {
    dacWrite(DAC1, i);
    delay(2);
    yield();
  }
}

bool modeBefore = false;

// ugly main loop. no interrupts for audio output, just use the 
// main loop for setting the DAC to the right level at the right time
// if any sound file is playing.
void loop() {
  // if the player is not playing, check all the buttons
  if (!player.playing) {
    for (int i = 0; i < nrButtons; i++) 
      prepareButton(&buttons[i]);

    prepareButton(&modeButton);

    current_page = modeButton.reallyPressed ? audios : audios2;
    if (modeButton.reallyPressed != modeBefore) {
      Serial.print("ModeButton is now ");
      Serial.print(modeButton.reallyPressed, DEC);
      modeBefore = modeButton.reallyPressed;
    }

    bool anyButtonPressed = false;
    for (int i = 0; i < nrButtons && !anyButtonPressed; i++) {
      if (buttons[i].reallyPressed) {
        Serial.print("button");
        Serial.print(i, DEC);
        Serial.print(" really pressed, now playing\n");
        current_audio = &(current_page[i]);
        anyButtonPressed = true;
      }
    }
    
    if (anyButtonPressed) {
       fade_in();
       player.playing = true;
       player.playingStart = micros();
       for (int i = 0; i < nrButtons; i++) {
        buttons[i].reallyPressed = false;
        buttons[i].pressed = false; 
       }
    }
  } else {
	// if a sound file is currently playing, just keep on playing and ignore all input
    unsigned long playingTime = micros() - player.playingStart;
    unsigned long sampleNum = playingTime / 50;
    if (sampleNum > current_audio->size) {
      fade_out();
      Serial.print("finished playing\n");
      player.playing = false;
      player.previousSample = 0;
      
    } else {
	  // only set the DAC if we have arrived at a new sample
      if (sampleNum > player.previousSample) {
        int val = current_audio->data[sampleNum];
        val = val < 0 ? 0 : val;
        val = val > 255 ? 255 : val;
        dacWrite(DAC1, val);
		// looks like HAL from Space Odyssey. Too scary for Klara and Jonas!
        //ledcWrite(1, val);
        player.previousSample++;
      }
    }
  }
}
