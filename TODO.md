# Log of work on
## 2021-04-23
- Got `TEST_CASE("run", "[scanner_set]")` mostly working.
- After it runs, the output directory looks like this:
```
(base) simsong@nimi be13_api % ls -l /var/folders/09/8v4pdnys627fqqh3vjbvsnq40000gn/T/ISmG9qlC/
total 4
-rw-r--r--  1 simsong  staff    0 Apr 23 21:20 alerts.txt
-rw-r--r--  1 simsong  staff  172 Apr 23 21:20 sha1_bufs.txt
-rw-------  1 simsong  staff    0 Apr 23 21:20 sha1_bufs_Az??
(base) simsong@nimi be13_api % cat  /var/folders/09/8v4pdnys627fqqh3vjbvsnq40000gn/T/ISmG9qlC/sha1_bufs.txt
# BANNER FILE NOT PROVIDED (-b option)
# BE13_API-Version: 1.0.0
# Feature-Recorder: sha1_bufs
# Feature-File-Version: 1.1
hello-0	d3486ae9136e7856bc42212385ea797094475802
```

- [ ] Histogram is created with the wrong filename
- [ ] Histogram file is empty

# Outstanding things to do

- [ ] move histograms out of feature_recorder and feature_recorder_set.
- [ ] Instead, histograms are made by the scanner set after the scanners have run, in the shutdown mode.
    - The feature recorders just need a way of reading the contents.
    - The feature_recorder can have any number of readers. It's just an open iostream.
- [ ] Make histogram in-memory and throw them out if you run out of memory, going into low-memory mode for the second pass.
- [ ] Merge of all outstanding histograms can be done single-threaded
      or multi-threaded.
- [ ] Make sure there is an alert recorder if dup_data_alerts is set
