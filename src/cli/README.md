# src/cli

the ekoc program: `ekoc build <file.eko> [-o <out>]`,
`ekoc check <file.eko>`, `ekoc test <file.eko>`

`ekoc test` collects the test bindings of every module in the
compilation unit -- the entry file and its transitive imports. Each
module that declares tests is a test root: it lowers with its own
module as the test root into its own driver binary under build/
(`ekoc-test-<entry>-<module>`), and its tests run one by one under a
`module <stem>` report header. The final `N/M passed` line and the exit
status cover all roots together. A unit with no test binding anywhere
is refused.
