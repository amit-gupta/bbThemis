Please record the CMD, number of nodes, and configuration for each run along with the Darshan log.

# Analysis

There are no conflicts in this log. Many files are read, a few internal files are accessed write-only (/dev/shm/sem.xxxxxx and /proc/self/task/xxxxxx/comm), and a few output files written by one process.

Write-only files:
```
/dev/shm/sem.1Ykw0B
/dev/shm/sem.8K6r0z
/dev/shm/sem.DOJIdC
/dev/shm/sem.OI5dFR
/dev/shm/sem.PCTMnc
/dev/shm/sem.TU6SFu
/dev/shm/sem.jmD0NF
/dev/shm/sem.nzGXJQ
/proc/self/task/220034/comm
/proc/self/task/220035/comm
/proc/self/task/220036/comm
/proc/self/task/220037/comm
/proc/self/task/220038/comm
/proc/self/task/220039/comm
/proc/self/task/220040/comm
/proc/self/task/220041/comm
/scratch1/00946/zzhang/DGXA100/DeepLearningExamples/PyTorch/Segmentation/MaskRCNN/pytorch/results/log.txt
/tmp/cu_d5_eo
results/inference/coco_2014_minival/coco_results.pth
results/inference/coco_2014_minival/predictions.pth
results/last_checkpoint
results/model_final.pth
```

Files read and written by just one process:
```
results/inference/coco_2014_minival/bbox.json
results/inference/coco_2014_minival/segm.json
```

To generate the full list:
```
strace_to_dxt.py < MaskRCNN-io.strace > MaskRCNN-io.dxt
darshan_dxt_conflicts MaskRCNN-io.dxt
```
