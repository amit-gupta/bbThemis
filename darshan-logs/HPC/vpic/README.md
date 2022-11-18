### Analysis

Detailed conflict analysis not available for this job, becuase the log file `iwang_test_224.ft_clx_intel-18.0.2_impi-18.0.2_-xCORE-AVX512_id4801201_9-6-39817-15716882828044732868_7830073.darshan` did not contain DXT per-call data.

However, at a higher level it is possible to see which ranks accessed each file, just not the actual byte offset ranges.

The job accessed 901 files, and each file was written to by a single rank. Most ranks wrote to four files, and rank 0 wrote to nine. Thus there were no conflicts in this job.

Commands used:
```
../../analysis/darshan_file_access_summary.py < vpic.log
```
