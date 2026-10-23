# Chaos HDR landing page

The public page is https://chaosfox26.github.io/chaos-hdr/.
Its title and Open Graph/Twitter descriptions are independent of GitHub's generated repository heading. Shared repository URLs retain GitHub's own heading format. Use the landing-page URL for the fully branded preview.

Run `python3 -B site/build.py` from the repository root to copy exactly eleven public files to `output/site`. Preview only that directory with a loopback-bound static server. The Pages workflow deploys that explicit artifact; it never publishes the repository root, installed settings, test evidence or private captures. Fonts and their licenses are copied unchanged from the existing reviewed vendor files.

The generated social artwork has been visually reviewed, resized to 1280 by 640 and stripped of metadata. It is also suitable for the repository Settings → Social preview upload. It references the existing Chaos HDR title and contains no desktop or account capture. The live website uses the same image as its hero and social preview.

The clipboard script runs only after the Copy command button is pressed. The site makes no analytics requests, stores no visitor settings and uses no third-party scripts or fonts. Ordinary hosting access logs are controlled by GitHub. The install command is displayed, never executed by the page.

Keep the installer link on `/releases/latest/download/install.sh`. Keep hardware and desktop limitations beside the command. GPU processing figures must keep their measurement scope and version attribution; they are not end-to-end latency promises.
