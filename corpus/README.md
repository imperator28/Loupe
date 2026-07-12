# Private corpus contract

`private/` is ignored by Git, local only, and owned by the user who supplies the CAD files. Do not add source STEP files or their private names to the repository.

Run the corpus harness against a local cases manifest. Evidence reports retain hashes and metadata only; they never copy source geometry, STEP payloads, or private source paths.

Use `cases.example.json` as a metadata-only template and keep a local `private/cases.json` beside private files.
