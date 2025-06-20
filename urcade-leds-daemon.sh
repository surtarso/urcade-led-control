#!/bin/bash

# Configuration
ARDUINO_PORT="/dev/ttyUSB0"  # Update this to your Arduino's port
BAUD_RATE=9600               # Baud rate for communication
CHECK_INTERVAL=2             # Check interval in seconds
RETROARCH_LOG="/tmp/retroarch.log"  # Path to Retroarch log file

# Mapping of emulator names to LED states
declare -A emulator_to_state=(
  ["stella"]=1
  ["zsnes"]=6
  ["snes9x"]=6
  ["nestopia"]=2
  ["fceux"]=2
  ["gens"]=3
  ["fusion"]=6
  ["yabause"]=6
  ["pcsx"]=6
  ["epsxe"]=6
  ["mame"]=6
  ["vbam"]=2
  ["mgba"]=4
  ["mupen64plus"]=6
  ["dolphin-emu"]=8
  ["pcsx2"]=12
  ["rpcs3"]=12
  ["cemu"]=10
  ["yuzu"]=10
  ["ryujinx"]=10
)

# Function to extract the core from Retroarch log file
get_retroarch_core_from_log() {
  if [[ -f "$RETROARCH_LOG" ]]; then
    # Look for lines indicating the loaded core
    local core
    core=$(grep -oP '(?<=Using core: )[^\s]+' "$RETROARCH_LOG" | tail -n 1)

    if [[ -n "$core" ]]; then
      # Remove "_libretro" suffix if present
      echo "${core%_libretro}"
      return
    fi
  fi

  echo ""  # No core found
}

# Function to find the running emulator and return the corresponding state
get_running_emulator() {
  # Check for Retroarch and extract its core
  local retroarch_core
  retroarch_core=$(get_retroarch_core_from_log)

  if [[ -n "$retroarch_core" && -n "${emulator_to_state[$retroarch_core]}" ]]; then
    echo "${emulator_to_state[$retroarch_core]}"
    return
  fi

  # Get the list of running processes
  local processes
  processes=$(ps -e)

  # Loop through the emulator-to-state map
  for emulator in "${!emulator_to_state[@]}"; do
    if echo "$processes" | grep -q "$emulator"; then
      echo "${emulator_to_state[$emulator]}"
      return
    fi
  done

  echo 0  # Default to 0 (no LEDs) if no emulator is running
}

# Open the serial port once and keep it open
exec 3>"$ARDUINO_PORT"  # File descriptor 3 for writing to the port
stty -F "$ARDUINO_PORT" "$BAUD_RATE"  # Configure the port
sleep 2  # Wait for Arduino initialization

# Main loop
current_state=-1

while true; do
  # Get the current state based on the running emulator
  new_state=$(get_running_emulator)

  # Validate new_state is an integer
  if [[ "$new_state" =~ ^[0-9]+$ ]]; then
    # Only send the state if it has changed
    if [ "$new_state" -ne "$current_state" ]; then
      echo "$new_state" >&3  # Send the state to the Arduino
      echo "Sent state $new_state to Arduino."
      current_state="$new_state"
    fi
  else
    echo "Invalid state: $new_state (skipped)"
  fi

  # Wait for the check interval
  sleep "$CHECK_INTERVAL"
done

# Close the serial port when the script exits
exec 3>&-
