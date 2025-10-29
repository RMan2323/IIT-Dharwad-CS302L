import subprocess
import os
import sys
from datetime import datetime

# --- Configuration ---
# The name of your compiled C++ program.
SIMULATOR_EXEC = "./simulator.out"

# The path to the memory trace file you want to use for the experiment.
# This path is relative to the `src` directory where the script is run.
TRACE_FILE = "../traces/combined.trace"

# The name of the file where all output will be saved.
OUTPUT_FILE = "full_output.txt"

# --- Parameters for the Experiment ---
# A list of all page sizes to test.
PAGE_SIZES = [64, 128, 512, 1024, 4096, 16384, 32768, 65536]

# A list of all frame counts to test.
NUM_FRAMES = [4, 32, 64, 128, 512, 1024, 4096, 16384, 32768, 65536]

# Replacement policies are identified by numbers 1 through 4.
# 1: Optimal, 2: FIFO, 3: LRU, 4: Random
REPLACEMENT_POLICIES = range(1, 5)

# Allocation policies are identified by numbers 1 and 2.
# 1: Global, 2: Local
ALLOCATION_POLICIES = range(1, 3)


def run_all_combinations():
    """
    Executes the memory simulator for every combination of parameters
    and redirects all output to a single text file.
    """
    # --- Pre-run Checks ---
    # Verify that the simulator executable exists.
    if not os.path.exists(SIMULATOR_EXEC):
        print(f"Error: Simulator executable not found at '{SIMULATOR_EXEC}'")
        print(f"Please compile your C++ code first using: g++ -std=c++11 -o simulator.out *.cpp")
        return

    # Verify that the trace file exists.
    if not os.path.exists(TRACE_FILE):
        print(f"Error: Trace file not found at '{TRACE_FILE}'")
        print("Please ensure the trace file is in the correct directory.")
        return

    # --- Experiment Execution ---
    # Delete the old output file if it exists to start fresh.
    if os.path.exists(OUTPUT_FILE):
        os.remove(OUTPUT_FILE)

    total_runs = len(PAGE_SIZES) * len(NUM_FRAMES) * len(REPLACEMENT_POLICIES) * len(ALLOCATION_POLICIES)
    current_run = 0
    
    print(f"Starting simulation for {total_runs} combinations...")
    print(f"Output will be saved to '{OUTPUT_FILE}'")
    start_time = datetime.now()

    # Open the output file in append mode. The 'with' statement ensures it's properly closed.
    with open(OUTPUT_FILE, 'a') as f:
        # Use nested loops to iterate through every single combination.
        for page_size in PAGE_SIZES:
            for frames in NUM_FRAMES:
                # The simulator requires at least as many frames as processes for local allocation.
                if frames < 4:
                    continue

                for rep_policy in REPLACEMENT_POLICIES:
                    for alloc_policy in ALLOCATION_POLICIES:
                        current_run += 1
                        
                        # Print progress to the console so you know it's working.
                        progress = (current_run / total_runs) * 100
                        sys.stdout.write(f"\r -> Running combination {current_run}/{total_runs} ({progress:.2f}%)")
                        sys.stdout.flush()

                        # --- Write a clear header to the output file for this specific run ---
                        header = f"""
# ===================================================================================
# RUNNING: Page Size = {page_size}, Frames = {frames}, RepPolicy = {rep_policy}, AllocPolicy = {alloc_policy}
# ===================================================================================
"""
                        f.write(header)

                        # --- Construct and run the command ---
                        command = [
                            SIMULATOR_EXEC,
                            str(page_size),
                            str(frames),
                            str(rep_policy),
                            str(alloc_policy),
                            TRACE_FILE
                        ]
                        
                        # Execute the command and redirect both stdout and stderr to the output file.
                        subprocess.run(command, stdout=f, stderr=subprocess.STDOUT)
                        f.write("\n\n") # Add some space between runs for readability.

    end_time = datetime.now()
    print(f"\n\nAll {total_runs} combinations have been executed.")
    print(f"Total execution time: {end_time - start_time}")
    print(f"Results saved in '{OUTPUT_FILE}'")


if __name__ == "__main__":
    run_all_combinations()
