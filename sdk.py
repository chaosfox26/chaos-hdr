
import hashlib
import urllib.request

from project import ROOT
REVISION='374959484e79a640feaba44c93ac8cfb0a03f5b5'
FILES={
    'LICENSE.txt':'d4216e39ebef5f9b50a6712ebb37beeb5379862a67733a9999c651f21592aaf0',
    'nvsdk_ngx_vk.h':'2d364ce7132881eb669e9498fd570d74cba563b1fbebc82c235f8e1ad2dd8b6d',
    'nvsdk_ngx_defs.h':'ea23f33497cd274860d1c25a97644fce807dcb0037c594547203343103fad03e',
    'nvsdk_ngx_params.h':'943bc8cc5cdae03b6303016fbad3183636f2335ae27a2d18776798c3b4efabbc',
    'nvsdk_ngx_defs_vk.h':'0a24d0861ace7d6b9362a67b7f08bea1b33ea123c5ce730b19133de8a891d031',
    'nvsdk_ngx_helpers_vk.h':'c192bff72138f12f770db48e12c1d8f712dfc81ea0e8e055ebb27d8dd1b31623',
    'libnvsdk_ngx.a':'3321c4ca9adf71f345550d10bf2148ed38003151b03ec5f81c99d00949ab8202',
}


def fetch_sdk():
    if not list((ROOT/'.venv/lib').glob('python*/site-packages/nvvfx/libs/libnvidia-ngx-truehdr.so.1.2.3')):
        raise RuntimeError('Install the pinned TrueHDR runtime first using bootstrap.sh.')
    folder=ROOT/'output/compositor/reference/ngx';folder.mkdir(parents=True,exist_ok=True)
    for name,digest in FILES.items():
        path=folder/name
        if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest()==digest:continue
        relative='LICENSE.txt' if name=='LICENSE.txt' else ('lib/Linux_x86_64/' if name.endswith('.a') else 'include/')+name
        url=f'https://raw.githubusercontent.com/NVIDIA/DLSS/{REVISION}/{relative}'
        with urllib.request.urlopen(url,timeout=20) as response:data=response.read()
        if hashlib.sha256(data).hexdigest()!=digest:raise RuntimeError('Official SDK checksum mismatch: '+name)
        temporary=path.with_suffix(path.suffix+'.tmp');temporary.write_bytes(data);temporary.replace(path)
    return folder

