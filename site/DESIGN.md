---
name: Chaos HDR website
colors:
  background: '#07070b'
  surface: '#12121a'
  text: '#f7f5fc'
  secondary: '#bdb7cf'
  magenta: '#ff65d0'
  blue: '#71ceff'
  line: '#302c3c'
  key-border: '#554b67'
  command-border: '#40354e'
  action-hover: '#ff97e1'
typography:
  display:
    fontFamily: Audiowide
  body:
    fontFamily: Barlow
  code:
    fontFamily: ui-monospace, SFMono-Regular, Consolas, monospace
---

# Established Chaos HDR identity

The page extends the accepted overlay and social artwork. Large neon lettering and real branching lightning carry the expression; useful text and controls sit on calm black surfaces. White text, magenta actions and blue links preserve the existing palette.

## Landing-page composition
Persuade mode: artwork first, a concise explanation and installation action second, real UI and measured evidence next, system requirements beside the install command. No fabricated game screenshots, testimonials or compatibility claims.

Use a 1160px maximum content width, 32px desktop gutters and 20px mobile gutters. Switch two-column content to a single column at 720px. Barlow is used for readable text and Audiowide for headings. Both are existing unmodified, self-hosted OFL fonts. Surfaces use dividing rules rather than card grids. The install command wraps without horizontal page overflow.

## Interaction and access
The only script copies the visible command after a click and reports success or a manual-copy fallback. Native anchors remain functional without JavaScript. Keyboard focus has a high-contrast blue outline. The artwork has a static glitch treatment, so there is no continuous animation or flashing. Real headings, descriptive links, a skip link and table semantics provide structure.

## Assets and publishing
The new image was generated with OpenAI's built-in image tool using the existing title as the reference. Its JPEG delivery version is 1280 by 640 pixels, with metadata stripped. Only the explicit map in build.py is published. Product/design notes, local captures, private validation, source dependencies and installed settings are excluded from the site artifact.
