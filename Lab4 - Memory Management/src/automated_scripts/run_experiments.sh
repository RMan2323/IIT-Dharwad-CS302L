#!/bin/bash

# --- Configuration ---
# The name of your compiled C++ program.
SIMULATOR_EXEC="./simulator.out"

# The path to the memory trace file you want to use for the experiment.
# This path is relative to the `src` directory where the script is run.
TRACE_FILE="../traces/combined.trace"

# The name of the file where all output will be saved.
OUTPUT_FILE="full_output.txt"

# --- Parameters for the Experiment ---
# An array of all page sizes to test.
PAGE_SIZES=(64 128 512 1024 4096 16384 32768 65536)

# An array of all frame counts to test.
NUM_FRAMES=(4 32 64 128 512 1024 4096 16384 32768 65536)

# --- Pre-run Checks ---
# Verify that the simulator executable exists and is executable.
if [ ! -x "$SIMULATOR_EXEC" ]; then
    echo "Error: Simulator executable not found or not executable at '$SIMULATOR_EXEC'"
    echo "Please compile your C++ code first using: g++ -std=c++11 -o simulator.out *.cpp"
    exit 1
fi

# Verify that the trace file exists and is readable.
if [ ! -r "$TRACE_FILE" ]; then
    echo "Error: Trace file not found or not readable at '$TRACE_FILE'"
    echo "Please ensure the trace file is in the correct directory."
    exit 1
fi

# --- Experiment Execution ---
# Delete the old output file if it exists to start fresh.
rm -f "$OUTPUT_FILE"

# Calculate the total number of combinations for the progress indicator.
total_runs=$(( ${#PAGE_SIZES[@]} * ${#NUM_FRAMES[@]} * 4 * 2 ))
current_run=0

echo "Starting simulation for $total_runs combinations..."
echo "Output will be saved to '$OUTPUT_FILE'"
start_time=$(date +%s)

# Use nested loops to iterate through every single combination.
for page_size in "${PAGE_SIZES[@]}"; do
    for frames in "${NUM_FRAMES[@]}"; do
        # The simulator requires at least as many frames as processes for local allocation.
        if [ "$frames" -lt 4 ]; then
            continue
        fi

        # Replacement policies (1-4)
        for rep_policy in {1..4}; do
            # Allocation policies (1-2)
            for alloc_policy in {1..2}; do
                # Increment the run counter
                current_run=$((current_run + 1))

                # Calculate and print progress to the console. 'bc' is used for floating point math.
                progress=$(echo "scale=2; ($current_run / $total_runs) * 100" | bc)
                printf "\r -> Running combination %d/%d (%.2f%%)" "$current_run" "$total_runs" "$progress"

                # --- Write a clear header to the output file for this specific run ---
                {
                    echo ""
                    echo "# ==================================================================================="
                    echo "# RUNNING: Page Size = $page_size, Frames = $frames, RepPolicy = $rep_policy, AllocPolicy = $alloc_policy"
                    echo "# ==================================================================================="
                } >> "$OUTPUT_FILE"

                # --- Construct and run the command ---
                # Execute the command and redirect both stdout and stderr to the output file.
                "$SIMULATOR_EXEC" "$page_size" "$frames" "$rep_policy" "$alloc_policy" "$TRACE_FILE" >> "$OUTPUT_FILE" 2>&1
                
                # Add some space between runs for readability.
                echo -e "\n" >> "$OUTPUT_FILE"
            done
        done
    done
done

end_time=$(date +%s)
duration=$((end_time - start_time))

# Move to a new line after the progress indicator is finished.
echo
echo
echo "All $total_runs combinations have been executed."
echo "Total execution time: $duration seconds"
echo "Results saved in '$OUTPUT_FILE'"