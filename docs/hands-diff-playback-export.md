# Hands Diff playback export

Schema-v3 reports add `input.playback`: a strictly time-ordered stream of
game-relative integer-millisecond records. The only exported kinds are
`pointer_sample`, `left_click`, `right_click`, `middle_click`, and configured
allowlisted gameplay actions. Coordinates are normalized inclusive `[0, 1]`;
an action without a prior valid in-frame pointer remains coordinate-less.

The collector records pointer samples no faster than 10 Hz for the first 60
game minutes, with at most 36,000 pointer samples, 50,000 total records, and
5 MiB of compact record JSON. It stops pointer samples first at their cap,
then marks the stream truncated and increments `omitted_record_count` when a
record or byte cap prevents a supported marker from being retained. Records
are emitted only from a Live Client game-time anchor no more than 2,000 ms old;
otherwise the omission is recorded and capture resumes only after re-anchoring.

No raw keystrokes, text, chords, local sequence IDs, monotonic timestamps,
window/application/display identifiers, or screen coordinates are serialized
or checkpointed. Legacy v2 checkpoints discard their local gameplay-event
payload during recovery.
