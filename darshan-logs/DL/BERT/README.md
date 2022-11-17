Please record the CMD, number of nodes, and configuration for each run along with the Darshan log.

# Analysis

There are no conflicts in this log. Many files are read, but only two significant files are written, and both are written by just one process:
```
/scratch1/00946/zzhang/DGXA100/DeepLearningExamples/PyTorch/LanguageModeling/BERT/results_strace/ckpt_10.pt
/scratch1/00946/zzhang/DGXA100/DeepLearningExamples/PyTorch/LanguageModeling/BERT/results_strace/dllogger.json
```

There are many small files that are written by one process. Some have filesnames in the form "/dev/shm/sem.xxxx" and some in the form "/proc/xxxxxx/task/yyyyyy/comm". The shm/sem files are memory-mapped, so tracing their consistency would require looking at individual loads and stores, which is beyond the scope of this project. The xxxxxx/task/yyyyyy/comm have the string "cuda-EvtHandlr" written to them, so they appear to be a CUDA thing. In this run they are written by one process each and never read.

As there are no conflicts in this file, the darshan_dxt_conflicts tool doesn't output much by default. I added the "-summary" flag to see whether each file was being read or written. To see that the shm/sem files were being memory mapped I had to grep through the original BERT-io.strace file. 
