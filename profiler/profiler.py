import re
import sys
from collections import defaultdict

def parse_log_file(filepath):
    # [Same robust parsing logic from the previous step]
    run_start_pattern = re.compile(r"Starting run:\s*Tasks=(\d+),\s*Length=(\d+)")
    run_end_pattern = re.compile(r"Finished run:")
    seed_pattern = re.compile(r"Starting repetition with seed\s+(\d+)")
    
    exec_pattern = re.compile(r"\[NODE_\d+\].*Total Execution time:\s*([\d.]+)s")

    results = []
    
    current_tasks = None
    current_length = None
    current_seed = None
    current_max_exec = 0.0

    def save_current_run():
        if current_tasks is not None and current_seed is not None:
            results.append({
                "Tasks": current_tasks,
                "Length": current_length,
                "Seed": current_seed,
                "Max_Exec": current_max_exec,
            })

    try:
        with open(filepath, 'r') as f:
            for line in f:
                run_start_match = run_start_pattern.search(line)
                if run_start_match:
                    if current_seed is not None:
                        save_current_run()
                        current_seed = None 
                    current_tasks = int(run_start_match.group(1))
                    current_length = int(run_start_match.group(2))
                    continue

                if run_end_pattern.search(line):
                    if current_seed is not None:
                        save_current_run()
                        current_seed = None 
                    continue

                seed_match = seed_pattern.search(line)
                if seed_match:
                    if current_seed is not None:
                        save_current_run()
                    current_seed = int(seed_match.group(1))
                    current_max_exec = 0.0
                    continue

                exec_match = exec_pattern.search(line)
                if exec_match:
                    current_max_exec = max(current_max_exec, float(exec_match.group(1)))
                    continue

        if current_seed is not None:
            save_current_run()

    except FileNotFoundError:
        print(f"Error: The file '{filepath}' was not found.")
        sys.exit(1)

    return results

def compute_scaling(parsed_data):
    # 1. Aggregate and average the data by (Tasks, Length)
    # Dictionary structure: dict[(Tasks, Length)] = list of exec_times
    aggregated_data = defaultdict(list)
    for row in parsed_data:
        # Only include runs that actually executed (time > 0)
        if row["Max_Exec"] > 0:
            aggregated_data[(row["Tasks"], row["Length"])].append(row["Max_Exec"])

    # Compute averages
    # avg_times map: (Tasks, Length) -> average_exec_time
    avg_times = {}
    for key, times in aggregated_data.items():
        avg_times[key] = sum(times) / len(times)

    print("\n--- AVERAGED EXECUTION TIMES ---")
    print(f"{'Tasks':<6} | {'Length':<8} | {'Avg Exec (s)'}")
    print("-" * 35)
    for (tasks, length) in sorted(avg_times.keys()):
        print(f"{tasks:<6} | {length:<8} | {avg_times[(tasks, length)]:.3f}")

    # 2. Compute Strong Scaling (Fixed Length, Increasing Tasks)
    print("\n--- STRONG SCALING ---")
    print(f"{'Length':<8} | {'Tasks':<6} | {'Time (s)':<10} | {'Speedup':<8} | {'Efficiency'}")
    print("-" * 60)
    
    # Group by Length
    lengths = sorted(list(set(k[1] for k in avg_times.keys())))
    
    for length in lengths:
        # Get all tasks for this length
        tasks_for_length = sorted([k[0] for k in avg_times.keys() if k[1] == length])
        
        if not tasks_for_length:
            continue
            
        base_tasks = tasks_for_length[0]
        base_time = avg_times[(base_tasks, length)]
        
        for tasks in tasks_for_length:
            time_n = avg_times[(tasks, length)]
            speedup = base_time / time_n
            # Efficiency scales based on the ratio of tasks compared to the baseline
            task_ratio = tasks / base_tasks 
            efficiency = speedup / task_ratio
            
            print(f"{length:<8} | {tasks:<6} | {time_n:<10.3f} | {speedup:<8.2f} | {efficiency * 100:.2f}%")

if __name__ == "__main__":
    log_file_path = sys.argv[1] 
    
    try:
        parsed_data = parse_log_file(log_file_path)
        if parsed_data:
            compute_scaling(parsed_data)
        else:
            print("No valid data parsed from log file.")
            
    except BrokenPipeError:
        # Python flushes standard streams on exit; redirect remaining output to devnull to silently close
        devnull = os.open(os.devnull, os.O_WRONLY)
        os.dup2(devnull, sys.stdout.fileno())
        sys.exit(1)
