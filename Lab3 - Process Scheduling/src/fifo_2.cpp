#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <iomanip> // For std::fixed and std::setprecision
#include <numeric> // For std::accumulate
#include <algorithm> // For std::max_element

#include "algos.h"

using namespace std;

// Defines the types of events that drive the simulation.
enum EventType {
    ARRIVAL,      // A process arrives in the system.
    CPU_DONE,     // A CPU burst has finished on one of the CPUs.
    IO_DONE       // An I/O burst has finished.
};

// Represents an event in the simulation timeline.
struct Event {
    int time;
    int process_id;
    EventType type;
    int cpu_id; // Which CPU this event is related to (-1 for non-CPU events).

    // Custom comparator for the priority queue to make it a min-heap (earliest time first).
    bool operator>(const Event& other) const {
        // If times are equal, prioritize arrivals to ensure they enter the ready queue first.
        if (time == other.time) {
            return type > other.type;
        }
        return time > other.time;
    }
};

/**
 * @brief Simulates a First-In, First-Out (FIFO) scheduling algorithm on two CPUs.
 * * @param processes A vector of Process objects to be scheduled. The state of these
 * objects will be modified to record completion times.
 */
void fifo_2(vector<Process>& processes) {
    // --- 1. Initialization ---
    
    // Open the output file for the schedule, overwriting any previous content.
    ofstream schedule_file;
    schedule_file.open("./schedule_2.txt");

    const int NUM_CPUS = 2;
    int simulation_time = 0;
    int completed_processes = 0;

    // `cpu_free_time` tracks when each CPU will become available.
    vector<int> cpu_free_time(NUM_CPUS, 0);

    // Event queue (min-priority queue) stores all future events, ordered by time.
    priority_queue<Event, vector<Event>, greater<Event>> event_queue;

    // Ready queue (a standard FIFO queue) holds processes waiting for a CPU.
    queue<int> ready_queue;

    // Populate the event queue with the initial ARRIVAL events for all processes.
    for (const auto& p : processes) {
        event_queue.push({p.arrival, p.p_no - 1, ARRIVAL, -1});
    }

    cout << "\n--- FIFO 2-CPU Simulation Starting ---" << endl;

    // --- 2. Main Simulation Loop ---
    // The loop continues until all processes have been completed.
    while (completed_processes < processes.size()) {
        if (event_queue.empty()) {
            cerr << "Error: Event queue is empty but simulation is not finished. Exiting." << endl;
            return;
        }

        // Get the next chronological event.
        Event current_event = event_queue.top();
        event_queue.pop();

        // Advance simulation time to the current event's time.
        simulation_time = current_event.time;
        Process& current_process = processes[current_event.process_id];

        // --- 3. Event Handling ---
        switch (current_event.type) {
            case ARRIVAL: {
                cout << "Time " << simulation_time << ": Process " << current_process.p_no << " arrived." << endl;
                ready_queue.push(current_process.p_no - 1);
                break;
            }

            case CPU_DONE: {
                cout << "Time " << simulation_time << ": CPU-" << current_event.cpu_id + 1 << " finished burst for Process " << current_process.p_no << "." << endl;
                
                // Move to the next burst (which will be an I/O burst).
                current_process.current++;

                // Check if the process has more bursts to run.
                if (current_process.current < current_process.bursts.size()) {
                    int io_duration = current_process.bursts[current_process.current];
                    int io_finish_time = simulation_time + io_duration;
                    event_queue.push({io_finish_time, current_process.p_no - 1, IO_DONE, -1});
                } else {
                    // This was the last burst; the process is complete.
                    current_process.completed = simulation_time;
                    completed_processes++;
                    cout << "Time " << simulation_time << ": Process " << current_process.p_no << " COMPLETED." << endl;
                }
                break;
            }

            case IO_DONE: {
                cout << "Time " << simulation_time << ": Process " << current_process.p_no << " finished I/O." << endl;
                // Move to the next burst (which will be a CPU burst).
                current_process.current++;
                ready_queue.push(current_process.p_no - 1);
                break;
            }
        }

        // --- 4. Scheduler Logic ---
        // Assign processes from the ready queue to any available CPUs.
        for (int i = 0; i < NUM_CPUS; ++i) {
            if (cpu_free_time[i] <= simulation_time && !ready_queue.empty()) {
                int process_id_to_run = ready_queue.front();
                ready_queue.pop();
                Process& p_to_run = processes[process_id_to_run];

                int start_time = simulation_time;
                int cpu_duration = p_to_run.bursts[p_to_run.current];
                int finish_time = start_time + cpu_duration;

                // Mark the CPU as busy until the finish time.
                cpu_free_time[i] = finish_time;

                // Create a CPU_DONE event for when this burst finishes.
                event_queue.push({finish_time, p_to_run.p_no - 1, CPU_DONE, i});

                // Write the schedule to the file in the required format.
                int burst_number = (p_to_run.current - 1) / 2;
                schedule_file << "CPU" << i + 1 << "\n" << "P" << p_to_run.p_no << "," << burst_number
                              << "\t" << start_time << "\t" << finish_time << "\n";
                
                cout << "Time " << start_time << ": CPU-" << i + 1 << " starts burst for Process " << p_to_run.p_no
                     << " (duration " << cpu_duration << ")" << endl;
            }
        }
    }

    schedule_file.close();
    cout << "\n--- FIFO 2-CPU Simulation Finished ---" << endl;

    // --- 5. Calculate and Print Metrics ---
    double total_turnaround_time = 0;
    int max_turnaround_time = 0;

    for (const auto& p : processes) {
        int turnaround = p.completed - p.arrival;
        total_turnaround_time += turnaround;
        if (turnaround > max_turnaround_time) {
            max_turnaround_time = turnaround;
        }
    }

    cout << "\n--- Performance Metrics ---" << endl;
    cout << fixed << setprecision(2);
    cout << "Average Turnaround Time: " << total_turnaround_time / processes.size() << endl;
    cout << "Maximum Turnaround Time: " << max_turnaround_time << endl;
    cout << "---------------------------" << endl;
}