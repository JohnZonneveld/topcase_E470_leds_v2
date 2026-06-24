# topcase_E470_leds_v2
Made some additional changes. Added a FAT NINJA (compressed 5x3) scrolling animation as startup sequence with an additional double flash at the end.
At first was trying to recalculate which LEDs had to be activated to display the message. But ran into problems that I could only get it to work when I mirrored the right side. So left you could see the text scrolling normally and right side mirrored.
Now treating the 6 LED-strips as one 3x18 display with a translation table, during the animation it will look the number in the table if it sees a '-1' value it discards the number and moves on.
This resulted that I now have a text scrolling from Right to Left over the complete width of the topcase.
Removed some obsolete code and added some additional comments to clarify what is happening.
As I am receiving a pulsing input for the turn signals and not the state of the turn signal switch I am using a hold routine to keep the turn status active. As also the turnsignal can be inconsistent I am actually measuring the on/off time to adjust the hold timer when needed so the turn signal is synced to the bike's flash pattern.
State as of now
- Running lights when inputs are no active (at about 80 of 255 brightness).
- Turn Signals synced with bike, turn signal side will show a comet pattern from inside to outside and other side will show running light state ( turn signal at 200 of 255 brightness). 
- Brake Signal, both sides will be fully lit (at about 200 of 255 brightness).
- Brake and Turn active, turning side will show the comet sweep other side will be fully lit (both turn and brake are at 200 of 255 brightness).
- Hazards (both sides will show comet sweep at 200 of 255 brightness).
- Hazards plus brake active, hazards will override brake, only bike's brakelight will show brake active. LEDs in topcase will continue with comet sweep at 200 of 255 brightness.

```
Startup animation
[0.00s] ──────────────────────────► Key Turned ON
│
├── 0,000ms to 3,240ms: "FAT NINJA" scrolls smoothly across the display (54 frames)
│
├── 3,240ms to 3,360ms: Flash 1 (120ms burst)
│
├── 3,360ms to 3,480ms: Flash 2 (120ms burst)
│
[3.48s] ──────────────────────────► Ready to Ride
```

