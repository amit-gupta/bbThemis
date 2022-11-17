Please record the CMD, number of nodes, and configuration for each run along with the Darshan log.

# Execution
The logs here are generated from the example of Montage Tutorial [here](http://montage.ipac.caltech.edu/docs/m101tutorial.html).
In particular, below are the commands executed after downloading the [tutorial-initial.tar.gz](http://montage.ipac.caltech.edu/docs/m101Example/tutorial-initial.tar.gz) file.
```
tar -xf tutorial-initial.tar.gz
cd m101/
export STRACE_RUN="strace -ttt -f -y -o trace.txt -A"
eval "$STRACE_RUN" mImgtbl rawdir images-rawdir.tbl
eval "$STRACE_RUN" mProjExec -p rawdir images-rawdir.tbl template.hdr projdir stats.tbl
eval "$STRACE_RUN" mImgtbl projdir images.tbl
eval "$STRACE_RUN" mAdd -p projdir images.tbl template.hdr final/m101_uncorrected.fitseval "$STRACE_RUN" ../Montage-6.0/bin/mJPEG -gray final/m101_uncorrected.fits 20% 99.98% loglog -out final/m101_uncorrected.jpg
eval "$STRACE_RUN" mOverlaps images.tbl diffs.tbl
eval "$STRACE_RUN" mDiffExec -p projdir diffs.tbl template.hdr diffdir
eval "$STRACE_RUN" mFitExec diffs.tbl fits.tbl diffdir
eval "$STRACE_RUN" mBgModel images.tbl fits.tbl corrections.tbl
eval "$STRACE_RUN" mBgExec -p projdir images.tbl corrections.tbl corrdir
eval "$STRACE_RUN" mAdd -p corrdir images.tbl template.hdr final/m101_mosaic.fits
eval "$STRACE_RUN" mJPEG -gray final/m101_mosaic.fits 0s max gaussian-log -out final/m101_mosaic.jpg
```
Note that we use strace to capture the I/O calls and combine the output of all processing steps to the `montage.log` file. 
The `-y` argument is needed so that strace will show the filename instead of the file handle in each individual read or write call.

The default Makefile of Montage does not run the "Exec" commands with parallelism, so we have manually specified the parallel versions of them to be compiled. Also, there is an [issue](https://github.com/Caltech-IPAC/Montage/issues/25) of `tsave` and `trestore` missing in the mtbl library. We fixed it by copying those missing functions from Montage 4.1.

# Analysis
There are many files with conflicts, mostly the files with the suffix ".fits".  Here the is a list of all the files with conflicts:
```
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/CORTBLiORcgN
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/IMGTBLDhlcxK
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1100244.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1100244_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1100256.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1100256_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1110021.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1110021_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1110032.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1110032_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1180244.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1180244_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1180256.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1180256_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1190021.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1190021_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1190032.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1190032_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1200244.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1200244_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1200256.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrdir/hdu0_2mass-atlas-990214n-j1200256_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/corrections.tbl
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000000.000003.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000000.000006.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000000.000009.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000001.000002.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000001.000003.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000001.000005.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000001.000006.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000001.000008.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000002.000003.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000002.000005.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000003.000006.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000003.000009.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000004.000005.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000004.000007.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000005.000007.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000005.000008.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000007.000008.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffs.tbl
/scratch1/06058/iwang/benchmarks/montage/m101/final/m101_mosaic.fits
/scratch1/06058/iwang/benchmarks/montage/m101/final/m101_uncorrected.fits
/scratch1/06058/iwang/benchmarks/montage/m101/fits.tbl
/scratch1/06058/iwang/benchmarks/montage/m101/images-rawdir.tbl
/scratch1/06058/iwang/benchmarks/montage/m101/images.tbl
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1100244.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1100244_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1100256.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1100256_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1110021.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1110021_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1110032.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1110032_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1180244.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1180244_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1180256.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1180256_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1190021.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1190021_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1190032.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1190032_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1200244.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1200244_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1200256.fits
/scratch1/06058/iwang/benchmarks/montage/m101/projdir/hdu0_2mass-atlas-990214n-j1200256_area.fits
```

Here are the remaining files which were accessed without conflicts:
```
/etc/ld.so.cache
/opt/apps/xalt/2.10.2/lib64/libxalt_init.so
/proc/meminfo
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000000.000003_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000000.000006_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000000.000009_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000001.000002_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000001.000003_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000001.000005_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000001.000006_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000001.000008_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000002.000003_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000002.000005_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000003.000006_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000003.000009_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000004.000005_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000004.000007_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000005.000007_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000005.000008_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/diffdir/diff.000007.000008_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/final/m101_mosaic.jpg
/scratch1/06058/iwang/benchmarks/montage/m101/final/m101_mosaic_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/final/m101_uncorrected.jpg
/scratch1/06058/iwang/benchmarks/montage/m101/final/m101_uncorrected_area.fits
/scratch1/06058/iwang/benchmarks/montage/m101/images-rawdir.tbl.tmp
/scratch1/06058/iwang/benchmarks/montage/m101/images.tbl.tmp
/scratch1/06058/iwang/benchmarks/montage/m101/projdir
/scratch1/06058/iwang/benchmarks/montage/m101/rawdir
/scratch1/06058/iwang/benchmarks/montage/m101/rawdir/2mass-atlas-990214n-j1100244.fits
/scratch1/06058/iwang/benchmarks/montage/m101/rawdir/2mass-atlas-990214n-j1100256.fits
/scratch1/06058/iwang/benchmarks/montage/m101/rawdir/2mass-atlas-990214n-j1110021.fits
/scratch1/06058/iwang/benchmarks/montage/m101/rawdir/2mass-atlas-990214n-j1110032.fits
/scratch1/06058/iwang/benchmarks/montage/m101/rawdir/2mass-atlas-990214n-j1180244.fits
/scratch1/06058/iwang/benchmarks/montage/m101/rawdir/2mass-atlas-990214n-j1180256.fits
/scratch1/06058/iwang/benchmarks/montage/m101/rawdir/2mass-atlas-990214n-j1190021.fits
/scratch1/06058/iwang/benchmarks/montage/m101/rawdir/2mass-atlas-990214n-j1190032.fits
/scratch1/06058/iwang/benchmarks/montage/m101/rawdir/2mass-atlas-990214n-j1200244.fits
/scratch1/06058/iwang/benchmarks/montage/m101/rawdir/2mass-atlas-990214n-j1200256.fits
/scratch1/06058/iwang/benchmarks/montage/m101/stats.tbl
/scratch1/06058/iwang/benchmarks/montage/m101/template.hdr
/usr/lib64/libc-2.17.so
/usr/lib64/libdl-2.17.so
/usr/lib64/libm-2.17.so
/usr/lib64/libnsl-2.17.so
/usr/share/zoneinfo/America/Chicago
```

Here are the commands I used to generate those lists from the logfile:
```
strace_to_dxt.py < montage.log | darshan_dxt_conflicts -verbose=0 -
strace_to_dxt.py < montage.log | darshan_dxt_conflicts -verbose=1 - | grep ^no-con | sed "s/no-conflicts //"
```
