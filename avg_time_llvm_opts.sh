#!/bin/bash

# List of .b files
FILES=("bench.b" "bottles.b" "deadcodetest.b" "hanoi.b" "hello.b" "long.b" "loopremove.b" "mandel.b" "serptri.b" "twinkle.b")

# Function to run the time command 10 times and calculate the average user time
get_average_user_time() {
  ./llvm_comp.sh
  # TIMEFORMAT="%3R"
  total_user_time=0
  for i in {1..10}; do
    # Capture the user time from the `time` command, extract the value and sum it up
    user_time=$( (time ./program > /dev/null) 2>&1 | grep "user" | awk '{print $2}')
    # Convert user time to seconds if in format mss (e.g., 0m0.018s -> 0.018)
    minutes=$(echo $user_time | cut -d'm' -f1)
    seconds=$(echo $user_time | cut -d'm' -f2 | sed 's/s//')
    user_time_in_seconds=$(echo "$minutes * 60 + $seconds" | bc)
    total_user_time=$(echo "$total_user_time + $user_time_in_seconds" | bc)
  done
  # Calculate the average user time
  average_user_time=$(echo "scale=6; $total_user_time / 10" | bc)
  echo $average_user_time
}

# Iterate over each .b file and run the commands
for FILE in "${FILES[@]}"; do
  echo "======================================="
  echo "Processing file: $FILE"
  echo "======================================="

  # No optimization
  echo -e "\n--- Running: No optimization ---"
  ./bf2llvm "../benches/$FILE" > /dev/null
  echo -e "Average user time (No optimization):"
  get_average_user_time

  # Scan loop optimization
  echo -e "\n--- Running: Optimization with Scan Loops ---"
  ./bf2llvm "../benches/$FILE" --o-scanloops > /dev/null
  echo -e "Average user time (Scan Loops optimization):"
  get_average_user_time

  # Simple loop optimization
  echo -e "\n--- Running: Optimization with Simple Loops ---"
  ./bf2llvm "../benches/$FILE" --o-simpleloops > /dev/null
  echo -e "Average user time (Simple Loops optimization):"
  get_average_user_time

  # Both optimizations
  echo -e "\n--- Running: Optimization with Both Scan and Simple Loops ---"
  ./bf2llvm "../benches/$FILE" -O > /dev/null
  echo -e "Average user time (Both optimizations):"
  get_average_user_time

  echo -e "\nFinished processing $FILE"
  echo "======================================="
done

echo "All files processed."
