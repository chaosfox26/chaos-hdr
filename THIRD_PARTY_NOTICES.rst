Chaos HDR dependency notices
============================

Third-party components
======================

This project integrates NVIDIA TrueHDR; it does not own or relicense NVIDIA's
engine. The project-wide license for original integration code remains to be
selected after the KWin/NGX compatibility review. Publication has not occurred.

Dear ImGui is used for the compositor controls. Its MIT license and upstream
attribution are retained in vendor/imgui/LICENSE.txt and UPSTREAM.txt.

KWin's effect interfaces and library carry GPL terms. Qt and KDE Frameworks
retain their respective upstream terms. They are installed through the system
package manager rather than copied into this source archive.

NVIDIA NGX headers and loader are obtained from the pinned official DLSS SDK;
the accompanying agreement must be retained with those downloaded files. The
NVIDIA VFX wheel contains separately licensed runtime components and notices.
These are dependencies obtained from NVIDIA, not relicensed source in this
archive. Source publication and distribution of the combined linked binary
require separate compatibility review. An MIT sample repository does not grant
MIT rights to the proprietary runtime or static loader.

Primary terms:
https://github.com/NVIDIA/DLSS/blob/374959484e79a640feaba44c93ac8cfb0a03f5b5/LICENSE.txt
https://www.nvidia.com/en-us/agreements/enterprise-software/nvidia-software-license-agreement/
https://www.nvidia.com/en-us/agreements/enterprise-software/product-specific-terms-for-ai-products/
