### Analysis

There are three log files in this data set. All three jobs only read files; none wrote to any file. Thus there were no conflicts detected.

 - Job 4812679, a 1-node job, read from 397 files.
 - Job 4812702, an 8-node job, also read from 397 files.
 - Job 4812732, an 8-node job, read from 3141 files.

Commands used:
```
darshan-dxt-parser mcawood_icoFoam_id4812679_9-9-52896-10961178173883552247_2414038.darshan > icoFoam.1node.dxt
darshan_dxt_conflicts icoFoam.1node.dxt
darshan-dxt-parser mcawood_icoFoam_id4812702_9-9-53526-15798403544431959609_8101443.darshan > icoFoam.8nodes.job4812702.dxt
darshan_dxt_conflicts icoFoam.8nodes.job4812702.dxt
darshan-dxt-parser mcawood_icoFoam_id4812732_9-9-54853-15767485957206509511_8103751.darshan > icoFoam.8nodes.job4812732.dxt
darshan_dxt_conflicts icoFoam.8nodes.job4812732.dxt
```

 
