
import json,os,signal,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parent
FOLDER=ROOT/'output/desktop/nested-check'

def run():
    if not json.loads((ROOT/'build-desktop/install-manifest.json').read_text()).get('development_checks'):
        raise RuntimeError('Build private fixtures first: prepare_desktop.py --developer-checks. Production diagnostics are disabled.')
    FOLDER.mkdir(parents=True,exist_ok=True)
    profile=FOLDER/'profile.json'
    fixture_profile=json.loads((ROOT/'profile.json').read_text());fixture_profile['enabled']=True
    profile.write_text(json.dumps(fixture_profile))
    for name in ('config','data','cache'):(FOLDER/name).mkdir(exist_ok=True)
    env=os.environ.copy();env.pop('QT_PLUGIN_PATH',None);env.update(
        XDG_CONFIG_HOME=str(FOLDER/'config'),XDG_DATA_HOME=str(FOLDER/'data'),XDG_CACHE_HOME=str(FOLDER/'cache'),
        TRUEHDR_PROFILE=str(profile),TRUEHDR_DESKTOP_TEST_ROOT=str(FOLDER),TRUEHDR_DESKTOP_TEST_CLASS='truehdrtest',
        KWIN_COMPOSE='O2ES',QT_QPA_PLATFORM='offscreen')
    selection='--selection' in sys.argv
    strict='--strict-direct' in sys.argv
    if selection:env['TRUEHDR_DESKTOP_TEST_CLASS']='unmatched-legacy-selector'
    if '--gpu-input' not in sys.argv and not strict:env['TRUEHDR_DESKTOP_TEST_CAPTURE']='1'
    (FOLDER/'window-targets.json').unlink(missing_ok=True)
    if '--capture' in sys.argv:
        capture=FOLDER/('capture-gl' if '--capture-gl' in sys.argv else 'capture-direct')
        capture.mkdir(exist_ok=True);env['TRUEHDR_NATIVE_VERIFY']=str(capture)
    processes=[];logs=[]
    def start(command,env,name):
        log=(FOLDER/name).open('w');logs.append(log)
        p=subprocess.Popen(command,env=env,stdout=log,stderr=log,start_new_session=True,cwd=ROOT);processes.append(p);return p
    def call(path,method,*args):
        return subprocess.run(['qdbus6','org.kde.KWin',path,method,*args],env=env,check=True,text=True,capture_output=True,timeout=5).stdout.strip()
    status=FOLDER/'output/desktop/live-status.json';status.unlink(missing_ok=True)
    def until(predicate,label,timeout=12):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            try:
                data=json.loads(status.read_text())
                if data.get('stage')=='error':raise RuntimeError(data['error'])
                if predicate(data):return data
            except (OSError,ValueError):pass
            if processes[0].poll() is not None:raise RuntimeError('Isolated compositor exited')
            time.sleep(.05)
        raise RuntimeError('Desktop check timed out: '+label)
    def click(key):
        data=until(lambda d:any(r['key']==key for r in d.get('controls',[])),key)
        rect=next(r for r in data['controls'] if r['key']==key)
        x=data['input']['origin_x']+rect['x']+rect['width']*.5
        y=data['input']['origin_y']+rect['y']+rect['height']*.5
        call('/TrueHDR','org.truehdr.Desktop.testPointer',str(x),str(y),'-1')
        call('/TrueHDR','org.truehdr.Desktop.testPointer',str(x),str(y),'1')
        time.sleep(.1)
        call('/TrueHDR','org.truehdr.Desktop.testPointer',str(x),str(y),'0')
        time.sleep(.15)
    result={}
    def controller_until(predicate,label,timeout=12):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            try:
                data=json.loads((FOLDER/'output/desktop/controller-status.json').read_text())
                if predicate(data):return data
            except (OSError,ValueError):pass
            time.sleep(.05)
        raise RuntimeError('Event controller check timed out: '+label)
    try:
        compositor=start(['kwin_wayland','--virtual','--socket','truehdr-desktop-test','--width','800','--height','900' if selection else '600'],env,'kwin.log')
        deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            try:
                call('/Effects','org.kde.kwin.Effects.loadedEffects');break
            except subprocess.SubprocessError:
                if compositor.poll() is not None:raise RuntimeError('Isolated compositor startup failed')
                time.sleep(.1)
        client=env.copy();client.update(QT_QPA_PLATFORM='wayland',WAYLAND_DISPLAY='truehdr-desktop-test')
        if selection:client['TRUEHDR_TEST_WINDOW_CLASS']='truehdr-universal-fixture'
        if '--gpu-input' in sys.argv:client['TRUEHDR_TEST_GPU_WINDOW']='1'
        fixture=start([str(ROOT/'build-desktop/truehdr-desktop-window-test')],client,'fixture.log')
        time.sleep(1)
        effect_id=json.loads((ROOT/'build-desktop/effect.json').read_text())['KPlugin']['Id']
        if '--controller' in sys.argv:
            command=[str(ROOT/'.venv/bin/python'),'-c','from desktop import start; print(start())']
            for _ in range(2):
                loaded=subprocess.check_output(command,env=env,cwd=ROOT,text=True,timeout=10).strip()
                assert loaded==effect_id
            result['controller_idempotent']=True
            result['loaded_after_window_started']=True
        else:
            script=call('/Scripting','org.kde.kwin.Scripting.loadDeclarativeScript',str(ROOT/'native/desktop/loader.qml'),'truehdr-desktop-loader-test')
            call('/Scripting/Script'+script,'org.kde.kwin.Script.run')
            result['loaded_after_window_started']=call('/Effects','org.kde.kwin.Effects.loadEffect',effect_id)=='true'
        if not result['loaded_after_window_started']:raise RuntimeError('Effect did not load')
        if '--watchdog' in sys.argv:
            (FOLDER/'output/desktop/controller-status.json').unlink(missing_ok=True)
            start([str(ROOT/'build-desktop/truehdr-desktop-session'),str(ROOT)],env,'controller.log')
            idle=controller_until(lambda d:d.get('ready') and not d.get('watchdog_active'),'idle startup')
            stamp=(FOLDER/'output/desktop/controller-status.json').stat().st_mtime_ns
            native_stamp=status.stat().st_mtime_ns
            time.sleep(3)
            assert (FOLDER/'output/desktop/controller-status.json').stat().st_mtime_ns==stamp,'Idle controller is still writing periodically'
            assert status.stat().st_mtime_ns==native_stamp,'Idle effect is still writing a heartbeat'
            assert idle['watchdog_checks']==0
            result['idle_has_no_watchdog_or_heartbeat']=True
        if selection:
            until(lambda d:d.get('frames',0)==0 and not d.get('target'),'unknown window initially unchanged')
            assert call('/TrueHDR','org.truehdr.Desktop.selectFocusedWindow')=='false'
            call('/TrueHDR','org.truehdr.Desktop.toggleControls')
            untouched=until(lambda d:any(r['key']=='allow_application' for r in d.get('controls',[])),'unapproved application controls')
            assert not untouched['target'] and untouched['frames']==0
            assert not (FOLDER/'window-targets.json').exists()
            result['opening_controls_does_not_allow_application']=True
            click('allow_application')
            until(lambda d:d.get('direct_input') and d.get('frames',0)>10,'explicit allow starts direct filtering')
            if '--watchdog' in sys.argv:
                controller_until(lambda d:d.get('watchdog_active'),'approved window arms watchdog')
                result['approval_starts_watchdog']=True
            result['explicit_allow_without_executable']=True
            remembered=json.loads((FOLDER/'window-targets.json').read_text())['classes']
            assert any('truehdr-universal-fixture' in value for value in remembered)
            result['window_identity_saved']=True
            call('/TrueHDR','org.truehdr.Desktop.toggleControls')
        if strict:
            blocked=until(lambda d:d.get('stage')=='unsupported-window','incompatible buffer bypass')
            assert blocked['frames']==0 and not blocked['direct_input'] and blocked['fast_path_required']
            assert not blocked['error']
            result.update(passed=True,unsupported_buffer_unchanged=True,no_capture_fallback=True,status=blocked)
            call('/Effects','org.kde.kwin.Effects.unloadEffect',effect_id)
            (FOLDER/'strict-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
            return
        if '--capture-gl' in sys.argv:call('/TrueHDR','org.truehdr.Desktop.setDirect','false')
        first=until(lambda d:d.get('frames',0)>10,'HDR frames')
        if '--gpu-input' in sys.argv and '--capture-gl' not in sys.argv:
            assert first.get('direct_input'), 'Submitted GPU buffer route not active: '+str(first.get('direct_fallback') or first.get('source_buffer'))
            result['direct_submitted_buffer']=True
        if '--frame-trace' in sys.argv:
            call('/TrueHDR','org.truehdr.Desktop.setFrameTrace','true')
            until(lambda d:d.get('presentation_timing',{}).get('trace_mode') and d['presentation_timing'].get('presented_frames',0)>40,'per-frame timing collection')
            trace=json.loads(call('/TrueHDR','org.truehdr.Desktop.frameTrace'))
            assert len(trace['presentation'])>40 and len(trace['processing'])>40
            assert all(row[1]>=row[0] and row[2]>=0 and row[5]>=0 for row in trace['processing'])
            stats=trace['summary']['displayed_frame_interval']
            assert stats['max_ms']>=stats['p99_ms']>=stats['p95_ms']
            assert trace['processing'][-1][6]>0
            (FOLDER/'frame-trace.json').write_text(json.dumps(trace,indent=2)+'\n')
            call('/TrueHDR','org.truehdr.Desktop.setFrameTrace','false')
            stopped=until(lambda d:not d.get('presentation_timing',{}),'per-frame timing stops')
            assert json.loads(call('/TrueHDR','org.truehdr.Desktop.frameTrace'))=={}
            result['bounded_frame_trace_and_gpu_timing']=True
            result['frame_trace_stops_cleanly']=True
        call('/TrueHDR','org.truehdr.Desktop.setPresentationTiming','true')
        presented=until(lambda d:d.get('presentation_timing',{}).get('presented_frames',0)>20,'real presentation callbacks')
        call('/TrueHDR','org.truehdr.Desktop.setPresentationTiming','false')
        result['presentation_feedback']=presented['presentation_timing']
        call('/TrueHDR','org.truehdr.Desktop.setEnabled','false')
        until(lambda d:d.get('stage')=='disabled','disable')
        if '--watchdog' in sys.argv:
            stopped=controller_until(lambda d:not d.get('watchdog_active'),'disable stops watchdog')
            time.sleep(2.2)
            assert json.loads((FOLDER/'output/desktop/controller-status.json').read_text())['watchdog_checks']==stopped['watchdog_checks']
            result['disable_stops_watchdog']=True
        call('/TrueHDR','org.truehdr.Desktop.setEnabled','true')
        until(lambda d:d.get('frames',0)>first['frames']+10,'re-enable')
        if '--watchdog' in sys.argv:
            controller_until(lambda d:d.get('watchdog_active') and d.get('watchdog_checks',0)>0,'active watchdog checks')
            result['re_enable_starts_watchdog']=True

            os.kill(fixture.pid,signal.SIGSTOP)
            try:
                time.sleep(.5)
                frozen=json.loads(status.read_text())
                before=controller_until(lambda d:d.get('watchdog_active'),'paused fixture watchdog')
                time.sleep(4.3)
                alive=json.loads(status.read_text())
                healthy=controller_until(lambda d:d.get('watchdog_checks',0)>before['watchdog_checks'],'paused fixture health checks')


                assert Path(f'/proc/{fixture.pid}/stat').read_text().rsplit(')',1)[1].split()[0]=='T','Fixture did not remain stopped'
                result['paused_application_redraws']=alive['frames']-frozen['frames']
                assert alive['updated']>frozen['updated'],'Paused producer stopped component heartbeat'
                assert healthy['watchdog_active'] and not healthy.get('health_error')
                result['paused_application_remains_healthy']=True
            finally:os.kill(fixture.pid,signal.SIGCONT)

            for enabled in (False,True):
                settings=json.loads(profile.read_text());settings['enabled']=enabled
                pending=profile.with_suffix('.pending');pending.write_text(json.dumps(settings));pending.replace(profile)
                until(lambda d:d.get('enabled') is enabled,'settings notification')
                controller_until(lambda d:d.get('watchdog_active') is enabled,'settings watchdog notification')
            result['atomic_settings_notifications']=True
        call('/TrueHDR','org.truehdr.Desktop.toggleControls')
        until(lambda d:d.get('overlay_visible') is True,'controls open')
        ui=until(lambda d:d.get('input',{}).get('overlay_cursor') and d.get('controls'),'overlay input initialized')
        rect=next(r for r in ui['controls'] if r['key']=='sharpen_intensity')
        x=ui['input']['origin_x']+rect['x']+rect['width']*.75
        y=ui['input']['origin_y']+rect['y']+rect['height']*.5
        call('/TrueHDR','org.truehdr.Desktop.testPointer',str(x),str(y),'-1')
        until(lambda d:d.get('input',{}).get('want_mouse') is True,'cursor reaches overlay')
        before=json.loads(profile.read_text())['sharpen_intensity']
        call('/TrueHDR','org.truehdr.Desktop.testPointer',str(x),str(y),'1')
        time.sleep(.08)
        call('/TrueHDR','org.truehdr.Desktop.testPointer',str(x),str(y),'0')
        end=time.monotonic()+5
        while time.monotonic()<end and json.loads(profile.read_text())['sharpen_intensity']==before:time.sleep(.05)
        assert json.loads(profile.read_text())['sharpen_intensity']!=before,'Overlay click did not change sharpening'
        result['cursor_reaches_control']=True;result['pointer_click_changes_sharpening']=True
        if selection:
            assert call('/TrueHDR','org.truehdr.Desktop.testCaptureControls')=='true'
            other_client=client.copy();other_client['TRUEHDR_TEST_WINDOW_CLASS']='truehdr-another-fixture'
            other_client['TRUEHDR_TEST_WINDOW_TYPE']='dialog'
            other=start([str(ROOT/'build-desktop/truehdr-desktop-window-test')],other_client,'fixture-other.log')
            time.sleep(1)
            click('application')
            options=until(lambda d:any('application_option_' in r['key'] and 'truehdr-another-fixture' in r['key'] for r in d.get('controls',[])),'running application list')
            key=next(r['key'] for r in options['controls'] if r['key'].startswith('application_option_') and 'truehdr-another-fixture' in r['key'])
            click(key)
            pending=until(lambda d:any(r['key']=='allow_application' for r in d.get('controls',[])),'candidate requires explicit permission')
            assert 'truehdr-universal-fixture' in pending['target']
            result['list_selection_does_not_allow_application']=True
            click('allow_application')
            other_status=until(lambda d:d.get('direct_input') and 'truehdr-another-fixture' in d.get('target',''),'second application direct filtering')
            result['second_fixture_window_type']=other_status['target_window_type']
            result['dialog_fixture_request_honored']=other_status['target_window_type']!=0
            result['application_list_switches_direct_target']=True
            click('remove_application')
            until(lambda d:not d.get('target'),'remove stops filtering')
            assert not any('truehdr-another-fixture' in value for value in json.loads((FOLDER/'window-targets.json').read_text())['classes'])
            result['remove_application_stops_filtering']=True
            click('allow_application')
            until(lambda d:d.get('direct_input') and 'truehdr-another-fixture' in d.get('target',''),'re-allow second application')
            fixture.terminate();fixture.wait(timeout=3)
            fixture=other;client=other_client
        call('/TrueHDR','org.truehdr.Desktop.toggleControls')
        until(lambda d:d.get('overlay_visible') is False,'controls close')
        if '--occlusion' in sys.argv:
            popup_client=client.copy();popup_client['TRUEHDR_TEST_WINDOW_CLASS']='truehdr-menu-fixture'
            popup_client['TRUEHDR_TEST_STATIC_MENU']='1'
            popup=start([str(ROOT/'build-desktop/truehdr-desktop-window-test')],popup_client,'fixture-menu.log')
            time.sleep(1)
            call('/TrueHDR','org.truehdr.Desktop.testOcclusion','true')
            overlap=until(lambda d:d.get('test_occlusion',{}).get('frames',0)>60,'static window remains above filtered game')
            result['overlapping_window_audit']=overlap['test_occlusion']
            assert overlap['test_occlusion']['failures']==0,'Filtered game overwrote the static overlapping window: '+str(overlap['test_occlusion'])
            result['overlapping_window_preserved']=True
            call('/TrueHDR','org.truehdr.Desktop.testOcclusion','false')
            popup.terminate();popup.wait(timeout=3)
        final=until(lambda d:d.get('stage')=='filtering','final filtering')
        if selection:
            fixture.terminate();fixture.wait(timeout=3)
            until(lambda d:d.get('stage')=='waiting-for-game' and not d.get('target'),'selected window closed')
            if '--watchdog' in sys.argv:
                stopped=controller_until(lambda d:not d.get('watchdog_active'),'closing window stops watchdog')
                stamp=(FOLDER/'output/desktop/controller-status.json').stat().st_mtime_ns
                time.sleep(3)
                assert (FOLDER/'output/desktop/controller-status.json').stat().st_mtime_ns==stamp
                result['closing_window_stops_watchdog']=True
            fixture=start([str(ROOT/'build-desktop/truehdr-desktop-window-test')],client,'fixture-reopened.log')
            reopened=until(lambda d:d.get('direct_input') and 'truehdr-another-fixture' in d.get('target',''),'saved identity selected on reopen')
            assert reopened['fast_path_required']
            result['reopened_window_auto_selected']=True
            if '--watchdog' in sys.argv:
                controller_until(lambda d:d.get('watchdog_active'),'relaunch re-arms watchdog')
                result['relaunch_starts_watchdog']=True
        if '--watchdog' in sys.argv:
            call('/Effects','org.kde.kwin.Effects.unloadEffect',effect_id)
            controller_until(lambda d:d.get('ready') and d.get('watchdog_active'),'event-driven effect recovery')
            until(lambda d:d.get('stage')=='filtering' and d.get('direct_input'),'recovered filtering')
            result['effect_unload_recovers_by_event']=True
            supervisor=next(p for p in processes if p.args[0].endswith('truehdr-desktop-session'))
            supervisor.terminate();supervisor.wait(timeout=3)
        call('/Effects','org.kde.kwin.Effects.unloadEffect',effect_id)
        until(lambda d:d.get('stage')=='unloaded','unload')
        result.update(passed=True,filtering=final,enable_disable=True,controls_open_close=True,unloaded=True,fixture_still_running=fixture.poll() is None)
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                os.killpg(process.pid,signal.SIGTERM)
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait()
        for log in logs:log.close()
    (FOLDER/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

if __name__=='__main__':
    if '--inside' in sys.argv:run()
    else:raise SystemExit(subprocess.run(['dbus-run-session','--',str(ROOT/'.venv/bin/python'),str(Path(__file__).resolve()),'--inside',*sys.argv[1:]],cwd=ROOT).returncode)
