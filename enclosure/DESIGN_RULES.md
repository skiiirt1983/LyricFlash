# Enclosure design rules (Paul)

These rules come from Paul. Read them before changing any geometry. Where they differ from older notes in
CHANGELOG.md, these win.

## Fits
- **Expensive, breakable parts are never press-fit or forced.** That means the board, the AMOLED/touch glass and
  the display stack. Their assembly paths are simulated, with at least 0.2 mm of air everywhere except designed
  contacts.
- **Cheap, durable parts that get handled get the best fit possible**, press-fit included. Examples are the slide
  switch and printed pins. They are cheap to replace, so a firm fit beats a loose one.
- Every part that goes into the case by hand (battery, switch, board) gets an insertion check, not just the
  base swing.

## Securing the electronics
- **The board must be secured so it can't break.** Screwing it into the case through its own M2 standoffs is
  wanted where it can be done.
- Screws and hardware are allowed where they're needed to keep the electronics safe. **Prefer a printed
  solution** where it does the job as well, for example a printed pin or a printed rotating part instead of
  bought hardware. Keep the build cheap, but safe and secure comes first.

## Shell to base
- The top shell and the base must make a **clean, solid connection**: the seam closes evenly and the shell is
  held down firmly, so the case feels like it will keep the device safe. No gasket. A rib, rail, tongue and
  groove or pins are all fine.
- Avoid add-on parts and sliding joints that bind in matte PETG. Build the features into the printed parts.

## Printing
- No internal supports. Supports on the outside are acceptable.
- Top shell: Iridium Gold PLA Metal (filament slot 1). Base: black PETG (slot 2).

## Process
- Ask Paul before any design decision that PLAN.md and these rules don't cover.
