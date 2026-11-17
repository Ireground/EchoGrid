# EchoGrid

Sequencer delay VST3 (port of the EchoGrid JSFX in `jsfx/`), in the HatsOff look.

- **Normal mode**: conventional delay (beat divisions or ms), Normal / Dual Echo / Ping-Pong.
- **Sequencer mode**: every repeat is a handle on the grid. Drag = time/level, double-click empty space = add,
  right-click = delete, wheel = level, Ctrl-drag = ignore snap. Per-repeat level, pan and tone.
- **Types**: Digital, Analog BBD, Studio Tape, Cheap Tape, Space Echo, Dark BBD, Lo-Fi, Ambient (+ Character).
- **FX**: Filter, Phaser, Flanger on Everything / Only Echoes / In Loop. Shifter always in the feedback loop.
- **Freeze**, **Duck**, **Macro** (right-click any slider -> Assign to Macro, drag the range dots).

GitHub Actions builds the Windows VST3 on every push (Actions tab -> latest run -> `EchoGrid-VST3-Windows`).
