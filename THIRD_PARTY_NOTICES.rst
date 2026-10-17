Chaos HDR dependency notices
============================

Third-party components
======================

Copyright Chaos Fox. Original Chaos HDR code and documentation are licensed
under the GNU General Public License, version 2 only (GPL-2.0-only), provided
in LICENSE. Files with separate upstream notices retain those terms. This grant
applies only to contributions whose rights are held by the Chaos HDR authors.

This project integrates NVIDIA TrueHDR; it does not own or relicense NVIDIA's
engine. The source installer downloads dependencies from upstream and builds
locally. NVIDIA's terms still govern its components and their use. This source
license does not grant permission to redistribute the combined KWin/NGX binary;
compatibility of that binary remains unresolved by the focused review.

Dear ImGui is used for the compositor controls. Its MIT license and upstream
attribution are retained in vendor/imgui/LICENSE.txt and UPSTREAM.txt.

KWin's effect interfaces and library carry GPL terms. Qt and KDE Frameworks
retain their respective upstream terms. They are installed through the system
package manager rather than copied into this source archive.

NVIDIA NGX headers and loader are obtained from the pinned official DLSS SDK;
the accompanying agreement must be retained with those downloaded files. The
NVIDIA VFX wheel contains separately licensed runtime components and notices.
These are dependencies obtained from NVIDIA, not relicensed source in this
archive. Source publication, local use and distribution of the combined linked
binary are separate licensing questions. An MIT sample repository does not grant
MIT rights to the proprietary runtime or static loader.

Primary terms:
https://github.com/NVIDIA/DLSS/blob/374959484e79a640feaba44c93ac8cfb0a03f5b5/LICENSE.txt
https://www.nvidia.com/en-us/agreements/enterprise-software/nvidia-software-license-agreement/
https://www.nvidia.com/en-us/agreements/enterprise-software/product-specific-terms-for-ai-products/
