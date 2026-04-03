# Boot Fonts

Liberation now ships three PSF2 boot fonts in `Image/LIBERATION/FONTS/`:

- `Boot.psf` — default alias, currently the same as `Boot-16x32.psf`
- `Boot-16x28.psf` — slightly denser alternative
- `Boot-16x32.psf` — default recommended boot font
- `Boot-24x32.psf` — larger alternative

These files are included in the source tree and installation media payload so the kernel screen path can load a chosen PSF font later. The current kernel still uses its built-in fallback font until PSF parsing and runtime selection are wired in.

PSF boot font handoff is now wired: the monitor loads Boot.psf and passes it to the kernel, and the kernel console uses the supplied PSF2 font with built-in fallback.
