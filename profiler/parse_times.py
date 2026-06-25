import re
import sys

def parse_log_file(filepath):
    # More robust regular expressions to handle potential spacing differences
    run_start_pattern = re.compile(r"Starting run:\s*Tasks=(\d+),\s*Length=(\d+)")
    run_end_pattern = re.compile(r"Finished run:")
    seed_pattern = re.compile(r"Starting repetition with seed\s+(\d+)")
    
    exec_pattern = re.compile(r"\[NODE_\d+\].*Total Execution time:\s*([\d.]+)s")
    sync_pattern = re.compile(r"\[NODE_\d+\].*Total Sync time:\s*([\d.]+)s")
    gather_pattern = re.compile(r"\[NODE_\d+\].*Total Gather time:\s*([\d.]+)s")

    results = []
    
    current_tasks = None
    current_length = None
    current_seed = None
    
    current_max_exec = 0.0
    current_max_sync = 0.0
    current_max_gather = 0.0

    def save_current_run():
        """Appends the current run's max values to the results list."""
        if current_tasks is not None and current_seed is not None:
            results.append({
                "Tasks": current_tasks,
                "Length": current_length,
                "Seed": current_seed,
                "Max_Exec": current_max_exec,
                "Max_Sync": current_max_sync,
                "Max_Gather": current_max_gather
            })

    try:
        with open(filepath, 'r') as f:
            for line in f:
                # 1. Check for a new run definition
                run_start_match = run_start_pattern.search(line)
                if run_start_match:
                    # If a run was interrupted without a "Finished run:" line, save it
                    if current_seed is not None:
                        save_current_run()
                        current_seed = None 
                    
                    current_tasks = int(run_start_match.group(1))
                    current_length = int(run_start_match.group(2))
                    continue

                # 2. Check for the end of a run block
                if run_end_pattern.search(line):
                    if current_seed is not None:
                        save_current_run()
                        current_seed = None # Clear active seed so we don't double-save
                    continue

                # 3. Check for a new repetition (Updates Seed and resets maximums)
                seed_match = seed_pattern.search(line)
                if seed_match:
                    # Save previous seed's data before starting the new one
                    if current_seed is not None:
                        save_current_run()
                        
                    current_seed = int(seed_match.group(1))
                    current_max_exec = 0.0
                    current_max_sync = 0.0
                    current_max_gather = 0.0
                    continue

                # 4. Update maximum times across nodes for the current run
                exec_match = exec_pattern.search(line)
                if exec_match:
                    current_max_exec = max(current_max_exec, float(exec_match.group(1)))
                    continue

                sync_match = sync_pattern.search(line)
                if sync_match:
                    current_max_sync = max(current_max_sync, float(sync_match.group(1)))
                    continue

                gather_match = gather_pattern.search(line)
                if gather_match:
                    current_max_gather = max(current_max_gather, float(gather_match.group(1)))
                    continue

        # Save the very last run if the file ended without a "Finished run:" tag
        if current_seed is not None:
            save_current_run()

    except FileNotFoundError:
        print(f"Error: The file '{filepath}' was not found.")
        sys.exit(1)

    return results

if __name__ == "__main__":
    log_file_path = sys.argv[1] 
    parsed_data = parse_log_file(log_file_path)

    print(f"{'Tasks':<6} | {'Length':<8} | {'Seed':<6} | {'Max Exec (s)':<12} | {'Max Sync (s)':<12} | {'Max Gather (s)'}")
    print("-" * 70)
    for data in parsed_data:
        print(f"{data['Tasks']:<6} | {data['Length']:<8} | {data['Seed']:<6} | {data['Max_Exec']:<12.3f} | {data['Max_Sync']:<12.3f} | {data['Max_Gather']:.3f}")
