Please record the CMD, number of nodes, and configuration for each run along with the Darshan log.

# Analysis

This job accessed 1,338,353 files, but had zero conflicts. The vast majority of files were accessed read-only. No files were read and written. Here are the files that were only written:
```
/dev/shm/sem.2WeMlD
/dev/shm/sem.9nNWHR
/dev/shm/sem.DHbcER
/dev/shm/sem.ErBORO
/dev/shm/sem.MunDSO
/dev/shm/sem.SiWkCN
/dev/shm/sem.xWXXER
/dev/shm/sem.zWiRQO
/proc/234262/task/234341/comm
/proc/234263/task/234334/comm
/proc/234264/task/234330/comm
/proc/234265/task/234328/comm
/proc/234266/task/234340/comm
/proc/234267/task/234329/comm
/proc/234268/task/234342/comm
/proc/234269/task/234339/comm
/tmp/vu555l32
```

Commands used:
```
strace_to_dxt.py < ResNet50-io.strace > ResNet50-io.dxt
darshan_dxt_conflicts.exe ResNet50-io.dxt > ResNet50-io.txt
darshan_dxt_conflicts.exe -summary -file=/tmp/vu555l32 ResNet50-io.dxt
```
