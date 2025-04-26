# System-Monitoring-Tool
Linux System Monitoring Tool
<div style="display: flex; justify-content: space-around; flex-wrap: wrap;">
  <img src="https://github.com/user-attachments/assets/551dce3f-2168-46d7-944b-53c347d68751" alt="Image 1" style="width: 45%; height: auto; margin: 10px;">
  <img src="https://github.com/user-attachments/assets/a59cd567-be28-4c21-96e5-c118b26d1389" alt="Image 2" style="width: 45%; height: auto; margin: 10px;">
</div>


## Features
- showFDtables: Shows system's pid, numeric file descriptors, file_name and inode number. Includes a summary table and filter option.
- myMonitoringTool: Shows system cpu and memory usage. Also finds max core frequency.

## How to run
./myMonitoringTool [samples = N] [tdelay = T] [--memory] [--cpu] [--cores] 
- Note: Samples and tdelay arguments can be used as positional arguments in first 2 positions

./showFDtables [--per-process] [--systemWide] [--Vnodes] [--composite] [--summary] [--threshold=]
- Note: To show only a specific PID, add PID to the argument ./showFDtables --composite 442

