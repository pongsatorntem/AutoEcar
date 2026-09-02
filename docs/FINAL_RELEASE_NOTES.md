# Traffic Light — Final Release Notes

## Final display rule
- GREEN: full-screen green, no normal text.
- YELLOW: full-screen yellow with black `CAUTION`.
- RED: full-screen red with white `STOP`.
- RETURN: same as YELLOW.
- Sensor/link faults remain small corner/bottom warnings and never replace the traffic state.
- All displays in the junction show the same state simultaneously.

## Logic locked for Friday test
- TF-Mini Plus x4.
- Forward direction only: S1 -> S2 confirms Yellow, S3 -> S4 confirms Red.
- Timestamp window prevents raw simultaneous-AND dependency.
- Debounce + gap-hold merges cab/body/operator/dolly gaps into one convoy.
- Red overwrites Idle/Yellow immediately.
- RED fixed 3 s -> RETURN yellow fixed 5 s.
- If the yellow convoy is still active after RETURN, continue Yellow without a green flash.
- Sensor failure does not force the entire junction Red; it is displayed as a fault badge and logged.
- Special-junction priority logic is intentionally deferred.

## Friday rule
Do not change multiple timing parameters simultaneously. Verify sensor distance/strength first, then tune `gap_hold_s` only if the cab/dolly convoy is being split. Change `pair_window_s` only if a correct S1->S2 or S3->S4 passage is missed.
