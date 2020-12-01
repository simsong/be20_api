- [ ] move histograms out of feature_recorder and feature_recorder_set.
- [ ] Instead, histograms are made by the scanner set after the scanners have run, in the shutdown mode.
    - The feature recorders just need a way of reading the contents.
    - The feature_recorder can have any number of readers. It's just an open iostream.
- [ ] Make histogram in-memory and throw them out if you run out of memory, going into low-memory mode for the second pass.
- [ ] Merge of all outstanding histograms can be done single-threaded
      or multi-threaded.
- [ ] Make sure there is an alert recorder if dup_data_alerts is set