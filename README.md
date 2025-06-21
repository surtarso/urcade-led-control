# URCade LED Controller
URCade's emulator based arcade button led controller with Arduino.

Check [URCade](https://urcade-files.ddns.net) project if you want to use our own pre-configured daemon.

## How it works
- You need a daemon installed on you emulator machine that will detect the emulator being launched and send the corresponding number of active buttons for that emulator to the arduino, as exemplified in [urcade-leds-daemon.sh](urcade-leds-daemon.sh). The arduino will receive the number of buttons related to the launched emulator and light up only the active buttons;
- The mode button will cycle between differente light effects (all up, random effects, all off);
- The hitbox toggle will let you use a hitbox on the fly.

## Possible configurations
You can use any of the 6 configurations:
- only hitbuttons;
- only hitbuttons with mode toggle button;
- hitbuttons + directionals (hitbox) with mode button;
- hitbuttons + directionals (hitbox) with mode button and hitbox toggle;
- hitbuttons + directionals (hitbox) with mode button and light sensor (for cabinets with lids);
- hitbuttons + directionals (hitbox) with mode button, hitbox toggle and light sensor (for cabinets with lids).

![image](https://github.com/user-attachments/assets/e11a3b43-b71d-43fc-ad84-7a18ad0edf9d)

Use any of the provided configurations for your setup, changing the indicated variable in the [urcade-leds-arduino.ino](urcade-leds-arduino.ino) file.
