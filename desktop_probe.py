
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
    if '--audit-displays' in sys.argv:fixture_profile['sharpen_intensity']=50
    profile.write_text(json.dumps(fixture_profile))
    for name in ('config','data','cache'):(FOLDER/name).mkdir(exist_ok=True)
    (FOLDER/'config/kwinoutputconfig.json').unlink(missing_ok=True)
    env=os.environ.copy();env.pop('QT_PLUGIN_PATH',None);env.update(
        XDG_CONFIG_HOME=str(FOLDER/'config'),XDG_DATA_HOME=str(FOLDER/'data'),XDG_CACHE_HOME=str(FOLDER/'cache'),
        TRUEHDR_PROFILE=str(profile),TRUEHDR_DESKTOP_TEST_ROOT=str(FOLDER),TRUEHDR_DESKTOP_TEST_CLASS='truehdrtest',
        KWIN_COMPOSE='O2ES',QT_QPA_PLATFORM='offscreen',WAYLAND_DISPLAY='truehdr-desktop-test')
    selection='--selection' in sys.argv
    strict='--strict-direct' in sys.argv
    if selection:env['TRUEHDR_DESKTOP_TEST_CLASS']='unmatched-legacy-selector'
    if '--gpu-input' not in sys.argv and not strict:env['TRUEHDR_DESKTOP_TEST_CAPTURE']='1'
    (FOLDER/'window-targets.json').unlink(missing_ok=True)
    (FOLDER/'profiles.json').unlink(missing_ok=True)
    Path(str(profile)+'.pending').unlink(missing_ok=True)
    if '--capture' in sys.argv:
        capture=FOLDER/('capture-gl' if '--capture-gl' in sys.argv else 'capture-direct')
        capture.mkdir(exist_ok=True);env['TRUEHDR_NATIVE_VERIFY']=str(capture)
    processes=[];logs=[]
    def start(command,env,name):
        log=(FOLDER/name).open('w');logs.append(log)
        p=subprocess.Popen(command,env=env,stdout=log,stderr=log,start_new_session=True,cwd=ROOT);processes.append(p);return p
    def call(path,method,*args,service='org.kde.KWin'):
        return subprocess.run(['qdbus6',service,path,method,*args],env=env,check=True,text=True,capture_output=True,timeout=5).stdout.strip()
    def unload():
        from desktop import prepare_unload
        prepare_unload(call)
        call('/Effects','org.kde.kwin.Effects.unloadEffect',effect_id)
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
        data=until(lambda d:d.get('overlay_visible') and d.get('input',{}).get('panel_progress',1)>.999 and any(r['key']==key for r in d.get('controls',[])),key)
        rect=next(r for r in data['controls'] if r['key']==key)
        if 'panel_progress' in data.get('input',{}):
            for _ in range(12):
                if 200<rect['y']<int(env.get('CHAOS_HDR_TEST_HEIGHT','900'))-140:break
                call('/TrueHDR','org.truehdr.Desktop.testPointer','200','450','-1')
                call('/TrueHDR','org.truehdr.Desktop.testScroll','-3' if rect['y']>450 else '3')
                time.sleep(.12);data=json.loads(status.read_text());rect=next(r for r in data['controls'] if r['key']==key)
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
        compositor=start(['kwin_wayland','--virtual','--socket','truehdr-desktop-test','--width',os.environ.get('CHAOS_HDR_TEST_WIDTH','800'),'--height',os.environ.get('CHAOS_HDR_TEST_HEIGHT','900' if selection else '600'),'--output-count',os.environ.get('CHAOS_HDR_TEST_OUTPUTS','1')],env,'kwin.log')
        deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            try:
                call('/Effects','org.freedesktop.DBus.Properties.Get','org.kde.kwin.Effects','loadedEffects');break
            except subprocess.SubprocessError:
                if compositor.poll() is not None:raise RuntimeError('Isolated compositor startup failed')
                time.sleep(.1)
        client=env.copy();client.update(QT_QPA_PLATFORM='wayland',WAYLAND_DISPLAY='truehdr-desktop-test')
        if '--regional-input' in sys.argv:
            subprocess.run(['dbus-update-activation-environment','WAYLAND_DISPLAY=truehdr-desktop-test','QT_QPA_PLATFORM=wayland'],env=env,check=True)
        if '--sdr-pixels' in sys.argv:
            for name,strength,grain,pattern in [('zero','0','0','normal'),('sharp','50','30','grain')]:
                measured=subprocess.run([str(ROOT/'build-desktop/truehdr-desktop-gpu-test'),str(FOLDER/('sdr-'+name)),strength,grain,pattern,'sdr'],env=client,cwd=ROOT,text=True,capture_output=True,timeout=60)
                (FOLDER/('sdr-'+name+'.log')).write_text(measured.stdout+measured.stderr)
                measured.check_returncode()
            result['sdr_pixels_match_reference']=True
        if selection:client['TRUEHDR_TEST_WINDOW_CLASS']='truehdr-universal-fixture'
        if '--gpu-input' in sys.argv:client['TRUEHDR_TEST_GPU_WINDOW']='1'
        if '--alpha' in sys.argv:client['TRUEHDR_TEST_ALPHA']='1'
        if '--regional-input' in sys.argv:client['TRUEHDR_TEST_INPUT_LOG']=str(FOLDER/'fixture-input.json')
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
        if '--regional-input' in sys.argv or '--audit-displays' in sys.argv:
            call('/TrueHDR','org.truehdr.Desktop.testInputDevice')
            time.sleep(.3)
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
        if strict or '--alpha' in sys.argv:
            blocked=until(lambda d:d.get('stage')=='unsupported-window','incompatible buffer bypass')
            assert blocked['frames']==0 and not blocked['direct_input'] and blocked['fast_path_required']
            assert not blocked['error']
            if '--alpha' in sys.argv:assert 'alpha' in blocked['direct_fallback']
            result.update(passed=True,unsupported_buffer_unchanged=True,no_capture_fallback=True,status=blocked)
            unload()
            (FOLDER/'strict-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
            return
        if '--capture-gl' in sys.argv:call('/TrueHDR','org.truehdr.Desktop.setDirect','false')
        first=until(lambda d:d.get('frames',0)>10,'HDR frames')
        if '--audit-gpu-retirement' in sys.argv:
            def gpu_state():return json.loads(call('/TrueHDR','org.truehdr.Desktop.testGpuState'))
            def gpu_until(predicate,label,timeout=5):
                deadline=time.monotonic()+timeout
                while time.monotonic()<deadline:
                    state=gpu_state()
                    if predicate(state):return state
                    time.sleep(.02)
                raise RuntimeError('GPU recovery check timed out: '+label+' '+str(state))
            def fault(mode,retire=False):
                if mode:
                    frames=lambda:json.loads(call('/TrueHDR','org.truehdr.Desktop.testRecoveryState'))['frames']
                    before=frames();gpu_until(lambda s:s['active'] and frames()>before,'processed frame before fault')
                call('/TrueHDR','org.truehdr.Desktop.testGpuRetirement',str(mode),'true' if retire else 'false')
            def reload_effect():
                assert call('/Effects','org.kde.kwin.Effects.loadEffect',effect_id)=='true'
                assert call('/TrueHDR','org.truehdr.Desktop.ping')=='true'
            for _ in range(3):
                unload();reload_effect();gpu_until(lambda s:s['active'] and not s['blocked'],'healthy reload')
            result['healthy_unload_reload']=True
            before=gpu_state();fault(1)
            gpu_until(lambda s:s['queue_timer'],'pending queue watchdog')
            time.sleep(.12);assert gpu_state()['creations']==before['creations']
            fault(0);gpu_until(lambda s:s['active'] and not s['queue_timer'],'temporary queue recovery')
            assert gpu_state()['creations']==before['creations']
            result['temporary_queue_delay_keeps_context']=True
            fault(1,True)
            held=gpu_until(lambda s:s['retiring'],'deferred cleanup')
            assert held['buffer_retained'] and not held['active'] and held['retire_timer']
            time.sleep(.12);assert gpu_state()['creations']==before['creations']
            fault(0);gpu_until(lambda s:s['active'] and not s['retiring'],'deferred cleanup completes')
            assert gpu_state()['creations']==before['creations']+1
            result['pending_cleanup_retains_source_and_prevents_rebuild']=True
            fault(1,True);fixture.terminate();fixture.wait(timeout=3);time.sleep(.12)
            assert gpu_state()['buffer_retained']
            fault(0);gpu_until(lambda s:not s['retiring'],'closed window retirement')
            fixture=start([str(ROOT/'build-desktop/truehdr-desktop-window-test')],client,'fixture-gpu-reopened.log')
            gpu_until(lambda s:s['active'],'closed window reopens')
            result['window_close_preserves_pending_buffer']=True
            before=gpu_state();fault(3)
            gpu_until(lambda s:s['creations']>before['creations'] and s['active'],'failed recording cleanup')
            assert not gpu_state()['blocked']
            result['reset_but_unsubmitted_fence_does_not_stall_cleanup']=True
            before=gpu_state();fault(4)
            held=gpu_until(lambda s:s['retiring'],'post-submit error cleanup')
            assert held['buffer_retained'] and not held['active']
            fault(0);gpu_until(lambda s:s['creations']>before['creations'] and s['active'],'post-submit error recovery')
            result['post_submit_failure_retains_source']=True
            before=gpu_state();start_fault=time.monotonic()
            if '--gpu-fault-unload' in sys.argv:
                fault(1)
                started=time.monotonic();call('/Effects','org.kde.kwin.Effects.unloadEffect',effect_id)
                result['unexpected_unload_seconds']=time.monotonic()-started
                assert result['unexpected_unload_seconds']<.5
                reload_effect()
            elif '--gpu-fault-idle' in sys.argv:
                frames=lambda:json.loads(call('/TrueHDR','org.truehdr.Desktop.testRecoveryState'))['frames']
                before_frames=frames();gpu_until(lambda s:not s['queue_timer'] and frames()>before_frames,'settled queue before quiet fault')
                os.kill(fixture.pid,signal.SIGSTOP);time.sleep(.15)
                assert not gpu_state()['queue_timer']
                call('/TrueHDR','org.truehdr.Desktop.testGpuRetirement','1','false')
            else:
                fault(2 if '--gpu-fault-device' in sys.argv else 1,True)
            blocked=gpu_until(lambda s:s['blocked'],'bounded persistent-failure quarantine')
            if '--gpu-fault-idle' in sys.argv:os.kill(fixture.pid,signal.SIGCONT)
            result['quarantine_seconds']=time.monotonic()-start_fault
            assert not blocked['active'] and not blocked['retiring'] and not blocked['retire_timer'] and not blocked['queue_timer']
            for _ in range(3):
                call('/TrueHDR','org.truehdr.Desktop.setEnabled','false')
                call('/TrueHDR','org.truehdr.Desktop.setEnabled','true')
                call('/TrueHDR','org.truehdr.Desktop.selectFocusedWindow')
                call('/TrueHDR','org.truehdr.Desktop.toggleControls')
            time.sleep(.4);assert gpu_state()['creations']==blocked['creations']
            started=time.monotonic();assert call('/TrueHDR','org.truehdr.Desktop.ping')=='true'
            result['quarantined_ping_seconds']=time.monotonic()-started
            assert result['quarantined_ping_seconds']<.5
            if json.loads(status.read_text())['overlay_visible']:call('/TrueHDR','org.truehdr.Desktop.toggleControls')
            time.sleep(.5);stamp=status.stat().st_mtime_ns;time.sleep(1.2)
            assert status.stat().st_mtime_ns==stamp,'Quarantined effect still writes an idle heartbeat'
            start([str(ROOT/'build-desktop/truehdr-desktop-session'),str(ROOT)],env,'quarantined-controller.log')
            controller_until(lambda d:d.get('ready') and not d.get('watchdog_active'),'quarantine stops watchdog')
            result['quarantine_stops_idle_polling']=True
            supervisor=processes[-1];supervisor.terminate();supervisor.wait(timeout=3)
            unload();reload_effect();time.sleep(.25)
            state=gpu_state();assert state['blocked'] and not state['active'] and state['creations']==0
            result.update(passed=True,persistent_failure_stops_rebuilds=True,reload_preserves_quarantine=True,fixture_still_running=fixture.poll() is None)
            unload()
            (FOLDER/'gpu-recovery-before-exit.log').write_bytes((FOLDER/'kwin.log').read_bytes())
            (FOLDER/'gpu-recovery-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));return
        if '--audit-activation' in sys.argv:
            from deployment import Deployment
            from project import atomic_write
            from desktop import start as attach
            transaction=Deployment(ROOT);transaction.begin(['build-desktop/install-manifest.json']);transaction.activation()
            installed=json.loads((ROOT/'build-desktop/install-manifest.json').read_text())
            invalid=installed.copy();invalid['effect_id']='truehdr_desktop_missing_fixture'
            try:
                atomic_write(ROOT/'build-desktop/install-manifest.json',json.dumps(invalid))
                try:attach()
                except RuntimeError:pass
                else:raise AssertionError('Missing candidate unexpectedly loaded')
                loaded=call('/Effects','org.freedesktop.DBus.Properties.Get','org.kde.kwin.Effects','loadedEffects')
                assert installed['effect_id'] in loaded,'Previous plugin was not restored after load failure'
            finally:transaction.rollback(lambda command:attach())
            assert json.loads((ROOT/'build-desktop/install-manifest.json').read_text())==installed
            assert call('/TrueHDR','org.truehdr.Desktop.ping')=='true'
            result['actual_effect_load_failure_rolls_back']=True
            unload()
            (FOLDER/'activation-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));return
        if '--benchmark' in sys.argv:
            measurements={}
            modes=[('combined',True,True)]
            if 'hdr_enabled' in first and os.environ.get('CHAOS_HDR_BENCH_FULL')=='1':
                modes += [('hdr-only',True,False),('sharpen-only',False,True),('bypass',False,False)]
            for label,hdr,sharp in modes:
                settings=fixture_profile.copy();settings.update(enabled=hdr or sharp,hdr_enabled=hdr,sharpen_enabled=sharp,sharpen_intensity=50,ignore_film_grain=0)
                pending=profile.with_suffix('.pending');pending.write_text(json.dumps(settings));pending.replace(profile)
                time.sleep(1)
                for opened in (False,True):
                    state=json.loads(status.read_text())
                    if bool(state.get('overlay_visible'))!=opened:call('/TrueHDR','org.truehdr.Desktop.toggleControls')
                    time.sleep(1)
                    call('/TrueHDR','org.truehdr.Desktop.setFrameTrace','true')
                    deadline=time.monotonic()+20
                    while True:
                        time.sleep(1)
                        trace=json.loads(call('/TrueHDR','org.truehdr.Desktop.frameTrace'))
                        if len(trace.get('presentation',[]))>240:break
                        if time.monotonic()>deadline:raise RuntimeError('Benchmark collection timed out')
                    measurements[label+('-open' if opened else '-closed')]=trace
                    call('/TrueHDR','org.truehdr.Desktop.setFrameTrace','false')
            if os.environ.get('CHAOS_HDR_BENCH_TRANSITIONS')=='1':
                settings=fixture_profile.copy();settings.update(enabled=True,hdr_enabled=True,sharpen_enabled=True,sharpen_intensity=50,ignore_film_grain=0)
                pending=profile.with_suffix('.pending');pending.write_text(json.dumps(settings));pending.replace(profile);time.sleep(1)
                if json.loads(status.read_text()).get('overlay_visible'):call('/TrueHDR','org.truehdr.Desktop.toggleControls')
                time.sleep(.5)
                for action in ('motion','toggle','initialization'):
                    call('/TrueHDR','org.truehdr.Desktop.setFrameTrace','true')
                    events=[]
                    for index in range(12):
                        events.append(time.monotonic_ns())
                        if action=='motion':call('/TrueHDR','org.truehdr.Desktop.toggleControls')
                        elif action=='toggle':call('/TrueHDR','org.truehdr.Desktop.setEnabled','false' if index%2==0 else 'true')
                        else:call('/TrueHDR','org.truehdr.Desktop.setDirect','true')
                        time.sleep(.3)
                    trace=json.loads(call('/TrueHDR','org.truehdr.Desktop.frameTrace'));trace['transition_monotonic_ns']=[str(value) for value in events]
                    measurements['transition-'+action]=trace
                    call('/TrueHDR','org.truehdr.Desktop.setFrameTrace','false')
            output=FOLDER/'benchmark.json'
            output.write_text(json.dumps({'width':int(env.get('CHAOS_HDR_TEST_WIDTH','800')),'height':int(env.get('CHAOS_HDR_TEST_HEIGHT','600')),'sharpness':50,'measurements':measurements},indent=2)+'\n')
            unload()
            print('Matched benchmark complete:',', '.join(measurements))
            return
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
        ui=until(lambda d:d.get('overlay_visible') and d.get('input',{}).get('panel_progress',0)>.999 and any(r['key']=='sharpen_intensity' for r in d.get('controls',[])),'overlay fully open with current control geometry')
        if '--audit-recovery' in sys.argv:
            def recovery():return json.loads(call('/TrueHDR','org.truehdr.Desktop.testRecoveryState'))
            call('/TrueHDR','org.truehdr.Desktop.testImportFailures','2')
            time.sleep(2)
            state=recovery();assert state['remaining']==0 and not state['disabled'] and not state['pending'],state
            call('/TrueHDR','org.truehdr.Desktop.testImportFailures','4')
            time.sleep(8)
            first=recovery();time.sleep(.5);second=recovery()
            assert first['remaining']==0 and first['disabled'] and first['retries']==3 and not first['pending'],first
            assert second['frames']==first['frames'],'Failed import retried without a backoff or user action'
            click('retry_processing');time.sleep(.5)
            state=recovery();assert not state['disabled'] and state['frames']>second['frames'],state
            result['transient_and_bounded_import_recovery']=True
            if '--audit-recovery-only' in sys.argv:
                unload()
                (FOLDER/'recovery-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));return
        if '--audit-displays' in sys.argv:
            call('/TrueHDR','org.truehdr.Desktop.testDisplay','0','1500','true','true')
            until(lambda d:d.get('display_peak')==1500 and d.get('display_hdr'),'first output peak')
            call('/TrueHDR','org.truehdr.Desktop.testDisplay','1','650','true','true')
            second=until(lambda d:d.get('display_peak')==650 and d.get('color_pipeline',{}).get('hdr_source_peak')==650,'second output peak follows window')
            assert second['display']!='Virtual-0'
            call('/TrueHDR','org.truehdr.Desktop.testDisplay','1','650','false','false')
            until(lambda d:not d.get('display_hdr') and not d.get('color_pipeline',{}).get('hdr_processing',True),'SDR output disables HDR conversion')
            call('/TrueHDR','org.truehdr.Desktop.testDisplay','1','250','true','false')
            until(lambda d:d.get('display_peak')==0 and not d.get('display_hdr'),'unsupported output peak bypass')
            call('/TrueHDR','org.truehdr.Desktop.testDisplay','0','1500','true','true')
            until(lambda d:d.get('display_peak')==1500 and d.get('display_hdr'),'return to HDR output')
            config=json.loads(profile.read_text());config['reduced_motion']=True
            pending=profile.with_suffix('.pending');pending.write_text(json.dumps(config));pending.replace(profile)
            time.sleep(.4)
            call('/TrueHDR','org.truehdr.Desktop.toggleControls');time.sleep(.2)
            call('/TrueHDR','org.truehdr.Desktop.toggleControls')
            until(lambda d:d.get('overlay_visible') and d.get('input',{}).get('panel_progress')==1,'reduced-motion immediate open')
            display_info=json.loads(subprocess.check_output(['kscreen-doctor','-j'],env=client))
            outputs=display_info['outputs'];assert len(outputs)==2 and all(o['name'].startswith('Virtual-') for o in outputs)
            active=json.loads(status.read_text())['input']['screen']
            subprocess.run(['kscreen-doctor','output.'+active+'.disable'],env=client,check=True,capture_output=True)
            time.sleep(1)
            state=json.loads(call('/TrueHDR','org.truehdr.Desktop.testInputState'))
            assert not state['input_installed'] and not state['overlay_cursor'] and not state['native_cursor_hidden'],state
            result.update(display_metadata_tracks_window=True,sdr_and_invalid_peak_bypass=True,reduced_motion=True,output_removal_releases_input=True)
            unload()
            (FOLDER/'display-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));return
        if '--audit-input' in sys.argv:
            click('profile_section');click('save_profile')
            until(lambda d:any(r['key']=='profile_name' for r in d.get('controls',[])),'profile name editor')
            text='Night / 夜 Ω Ж'
            writer=subprocess.Popen([str(ROOT/'build/truehdr-panel'),str(profile),'--clipboard-write'],env=client,stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,cwd=ROOT,start_new_session=True)
            processes.append(writer);writer.stdin.write(text.encode());writer.stdin.close();time.sleep(.4)
            contents=subprocess.check_output([str(ROOT/'build/truehdr-panel'),str(profile),'--clipboard-read'],env=client,cwd=ROOT,timeout=5).decode()
            assert contents==text,'Native clipboard helper did not publish text'
            def chord(scan):
                for key,down in ((29,True),(scan,True),(scan,False),(29,False)):
                    call('/TrueHDR','org.truehdr.Desktop.testSystemKey',str(key),'true' if down else 'false')
            def text_state():return json.loads(call('/TrueHDR','org.truehdr.Desktop.testTextState'))
            chord(47)
            end=time.monotonic()+5
            while text_state().get('text')!=text and time.monotonic()<end:time.sleep(.05)
            assert text_state()['text']==text,('System clipboard paste did not reach the name editor',text_state())
            time.sleep(.3);assert text_state()['missing']==[],'Missing glyphs in the name editor'
            chord(30);chord(46);time.sleep(.3)
            copied=subprocess.check_output([str(ROOT/'build/truehdr-panel'),str(profile),'--clipboard-read'],env=client,cwd=ROOT,timeout=5).decode()
            assert copied==text,'Copy did not reach the system clipboard'
            chord(45);time.sleep(.2);assert text_state()['text']=='','Cut did not clear selection'
            chord(44);time.sleep(.2);assert text_state()['text']==text,'Undo cut failed'
            chord(30);chord(45);time.sleep(.2)
            call('/TrueHDR','org.truehdr.Desktop.testSystemKey','29','true');call('/TrueHDR','org.truehdr.Desktop.testSystemKey','47','true')
            time.sleep(.5);assert text_state()['text']==text,'Paste while Ctrl remains held failed'
            call('/TrueHDR','org.truehdr.Desktop.testSystemKey','47','false');call('/TrueHDR','org.truehdr.Desktop.testSystemKey','29','false')
            call('/TrueHDR','org.truehdr.Desktop.testSystemKey','28','true');call('/TrueHDR','org.truehdr.Desktop.testSystemKey','28','false')
            until(lambda d:not any(r['key']=='profile_name' for r in d.get('controls',[])),'profile editor closed')
            assert text in json.loads((FOLDER/'profiles.json').read_text())['profiles']
            result['clipboard_copy_cut_paste_and_unicode']=True
            if '--audit-input-only' in sys.argv:
                unload()
                (FOLDER/'clipboard-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));return
            click('profile_section')
        if '--regional-input' in sys.argv:
            time.sleep(.4)
            assert not ui['input']['fullscreen_input_owned']
            def events():return json.loads((FOLDER/'fixture-input.json').read_text())
            def system_pointer(x,y,state=-1):call('/TrueHDR','org.truehdr.Desktop.testSystemPointer',str(x),str(y),str(state))
            def system_key(code):
                call('/TrueHDR','org.truehdr.Desktop.testSystemKey',str(code),'true')
                call('/TrueHDR','org.truehdr.Desktop.testSystemKey',str(code),'false')
                time.sleep(.15)
            before=events()
            system_pointer(720,500,1);system_pointer(720,500,0);time.sleep(.2)
            assert events()['presses']==before['presses']+1 and events()['releases']==before['releases']+1
            system_key(30)
            assert events()['keys']==before['keys']+1
            result['desktop_clicks_and_typing_pass_through']=True
            rect=next(r for r in json.loads(status.read_text())['controls'] if r['key']=='sharpen_intensity')
            sx,sy=rect['x']+rect['width']*.55,rect['y']+rect['height']*.5
            before=events();old=json.loads(profile.read_text())['sharpen_intensity']
            system_pointer(sx,sy,1);time.sleep(.15);system_pointer(720,sy,0);time.sleep(.2)
            assert events()['presses']==before['presses'] and events()['releases']==before['releases']
            assert json.loads(profile.read_text())['sharpen_intensity']!=old
            result['sidebar_drag_stays_owned_outside_panel']=True
            system_pointer(720,500,1);system_pointer(100,500,0);time.sleep(.2)
            assert events()['presses']==before['presses']+1 and events()['releases']==before['releases']+1
            result['desktop_drag_release_passes_through_sidebar']=True
            system_pointer(sx,sy,1);system_pointer(sx,sy,0);time.sleep(.2)
            dock_env=client.copy();dock_env.update(TRUEHDR_TEST_DOCK='1',TRUEHDR_TEST_WINDOW_CLASS='truehdr-dock-fixture',TRUEHDR_TEST_INPUT_LOG=str(FOLDER/'dock-input.json'))
            dock=start([str(ROOT/'build-desktop/truehdr-desktop-window-test')],dock_env,'dock.log');time.sleep(.8)
            dock_y=int(os.environ.get('CHAOS_HDR_TEST_HEIGHT','600'))-24
            for dock_x in (100,720):system_pointer(dock_x,dock_y,1);system_pointer(dock_x,dock_y,0)
            time.sleep(.2)
            dock_events=json.loads((FOLDER/'dock-input.json').read_text())
            assert dock_events['presses']==1 and dock_events['releases']==1,'Exposed taskbar clicks did not reach the dock'
            result['exposed_taskbar_clicks_pass_beside_full_height_sidebar']=True
            capture=FOLDER/'sidebar-screenshot.png';capture.unlink(missing_ok=True)
            shot=subprocess.run(['spectacle','--background','--nonotify','--fullscreen','--output',str(capture)],env=client,cwd=ROOT,text=True,capture_output=True,timeout=25)
            (FOLDER/'spectacle.log').write_text(shot.stdout+shot.stderr);shot.check_returncode()
            assert capture.is_file() and capture.stat().st_size>1000
            for dock_x in (780,):
                color=subprocess.check_output(['magick',str(capture),'-crop',f'1x1+{dock_x}+{dock_y}','-depth','8','RGB:-'],timeout=5)
                assert max(abs(a-b) for a,b in zip(color,(240,80,20)))<=3,('Taskbar pixels changed',color)
            color=subprocess.check_output(['magick',str(capture),'-crop',f'1x1+20+{dock_y}','-depth','8','RGB:-'],timeout=5)
            assert color[0]<160,'Sidebar did not extend over the taskbar to the bottom edge'
            result['full_height_sidebar_and_exposed_taskbar_pixels']=True
            result['spectacle_captures_while_sidebar_open']=True
            dock.terminate();dock.wait(timeout=5);time.sleep(.3)
            def screenshot_to(path):
                subprocess.run(['spectacle','--background','--nonotify','--fullscreen','--output',str(path)],env=client,cwd=ROOT,check=True,capture_output=True,timeout=25)
            def cursor_white_pixels(path):
                pixels=subprocess.check_output(['magick',str(path),'-crop','22x32+300+300','-depth','8','RGB:-'],timeout=5)
                return sum(min(pixels[i:i+3])>220 for i in range(0,len(pixels),3))
            system_pointer(720,300);time.sleep(.15)
            hidden=json.loads(call('/TrueHDR','org.truehdr.Desktop.testInputState'))
            assert not hidden['native_cursor_hidden'] and not hidden['overlay_cursor']
            before_cursor=FOLDER/'cursor-outside.png';screenshot_to(before_cursor)
            system_pointer(300,300);time.sleep(.15)
            cursor=json.loads(call('/TrueHDR','org.truehdr.Desktop.testInputState'))
            assert cursor['native_cursor_hidden'] and cursor['overlay_cursor']
            if int(os.environ.get('CHAOS_HDR_TEST_OUTPUTS','1'))>1:
                system_pointer(1100,300);time.sleep(.15)
                cursor=json.loads(call('/TrueHDR','org.truehdr.Desktop.testInputState'))
                assert not cursor['native_cursor_hidden'] and not cursor['overlay_cursor']
                system_pointer(300,300);time.sleep(.15)
                result['cursor_restored_when_crossing_to_another_monitor']=True
            call('/TrueHDR','org.truehdr.Desktop.testOverlayClipping','true')
            os.kill(fixture.pid,signal.SIGSTOP)
            try:
                for index in range(3):
                    time.sleep(.2);captured=FOLDER/f'cursor-clipped-{index}.png';screenshot_to(captured)
                    assert cursor_white_pixels(captured)>cursor_white_pixels(before_cursor)+20,'Cursor was not drawn above the sidebar'
            finally:
                os.kill(fixture.pid,signal.SIGCONT)
                call('/TrueHDR','org.truehdr.Desktop.testOverlayClipping','false')
            result['cursor_visible_above_panel_with_partial_repaint_and_static_background']=True
            capture=FOLDER/'sidebar-region-screenshot.png';capture.unlink(missing_ok=True)
            region=start(['spectacle','--background','--nonotify','--region','--release-capture','--output',str(capture)],client,'spectacle-region.log')
            time.sleep(2)
            system_pointer(10,10,1);time.sleep(.2);system_pointer(780,850,0)
            deadline=time.monotonic()+8
            while time.monotonic()<deadline and not capture.exists():time.sleep(.1)
            if not capture.exists():
                (FOLDER/'input-failure-state.json').write_text(call('/TrueHDR','org.truehdr.Desktop.testInputState'))
            assert capture.is_file() and capture.stat().st_size>1000,'Region screenshot did not complete'
            result['spectacle_region_selection_crosses_sidebar']=True
            region.wait(timeout=5)
            system_key(99);time.sleep(1)
            assert call('/org/freedesktop/DBus','org.freedesktop.DBus.NameHasOwner','org.kde.spectacle',service='org.freedesktop.DBus')=='true'
            result['print_shortcut_launches_spectacle_with_sidebar_open']=True
            pid=int(call('/org/freedesktop/DBus','org.freedesktop.DBus.GetConnectionUnixProcessID','org.kde.spectacle',service='org.freedesktop.DBus'))
            os.kill(pid,signal.SIGTERM);time.sleep(.3)
            system_pointer(sx,sy,1);system_pointer(sx,sy,0)
            for scan in (125,42,31):call('/TrueHDR','org.truehdr.Desktop.testSystemKey',str(scan),'true')
            for scan in (31,42,125):call('/TrueHDR','org.truehdr.Desktop.testSystemKey',str(scan),'false')
            time.sleep(1)
            assert call('/org/freedesktop/DBus','org.freedesktop.DBus.NameHasOwner','org.kde.spectacle',service='org.freedesktop.DBus')=='true'
            result['meta_shift_s_launches_spectacle_with_sidebar_open']=True
            pid=int(call('/org/freedesktop/DBus','org.freedesktop.DBus.GetConnectionUnixProcessID','org.kde.spectacle',service='org.freedesktop.DBus'))
            os.kill(pid,signal.SIGTERM);time.sleep(.3)
            ui=json.loads(status.read_text())
        if 'panel_progress' in ui.get('input',{}):
            time.sleep(1.2)
            ui=json.loads(status.read_text())
            call('/TrueHDR','org.truehdr.Desktop.testCaptureControls')
            for _ in range(12):
                ui=json.loads(status.read_text());rect=next(r for r in ui['controls'] if r['key']=='sharpen_intensity')
                if 200<rect['y']<int(env.get('CHAOS_HDR_TEST_HEIGHT','900'))-140:break
                call('/TrueHDR','org.truehdr.Desktop.testPointer','200','450','-1')
                call('/TrueHDR','org.truehdr.Desktop.testScroll','-3')
                time.sleep(.12)
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
        if 'panel_progress' in ui.get('input',{}):
            result['sidebar_controls_reach_sharpening']=True
            call('/TrueHDR','org.truehdr.Desktop.testPointer','200','450','-1')
            call('/TrueHDR','org.truehdr.Desktop.testScroll','100')
            time.sleep(.2)
        if '--overlay' in sys.argv:
            click('hdr_enabled')
            until(lambda d:d.get('hdr_enabled') is False and d.get('sharpen_enabled') and d.get('direct_input'),'sharpening without HDR')
            click('sharpen_enabled')
            until(lambda d:not d.get('hdr_enabled') and not d.get('sharpen_enabled') and not d.get('watchdog_active'),'both features disabled')
            click('hdr_enabled')
            until(lambda d:d.get('hdr_enabled') and d.get('sharpen_enabled') is False and d.get('direct_input'),'HDR without sharpening')
            click('sharpen_enabled')
            until(lambda d:d.get('hdr_enabled') and d.get('sharpen_enabled') and d.get('direct_input'),'combined processing restored')
            result['independent_hdr_and_sharpening_toggles']=True
            call('/TrueHDR','org.truehdr.Desktop.testPointer','200','300','-1')
            call('/TrueHDR','org.truehdr.Desktop.testScroll','100')
            time.sleep(.25)
            call('/TrueHDR','org.truehdr.Desktop.testCaptureControls')
            call('/TrueHDR','org.truehdr.Desktop.testPreviewCapture','true')
            time.sleep(.3)
            call('/TrueHDR','org.truehdr.Desktop.toggleControls')
            time.sleep(.55)
            call('/TrueHDR','org.truehdr.Desktop.toggleControls')
            time.sleep(.8)
            call('/TrueHDR','org.truehdr.Desktop.toggleControls')
            time.sleep(.06)
            call('/TrueHDR','org.truehdr.Desktop.toggleControls')
            time.sleep(.4)
            call('/TrueHDR','org.truehdr.Desktop.testPreviewCapture','false')
            timeline=json.loads((FOLDER/'motion-preview/timing.json').read_text())['frames']
            assert any(0<r['progress']<1 and r['open'] for r in timeline)
            assert any(0<r['progress']<1 and not r['open'] for r in timeline)
            assert any(r['progress']==0 for r in timeline)
            assert timeline[-1]['progress']>.99
            result['rendered_open_close_and_reversal']=True
        if selection:
            assert call('/TrueHDR','org.truehdr.Desktop.testCaptureControls')=='true'
            other_client=client.copy();other_client['TRUEHDR_TEST_WINDOW_CLASS']='truehdr-another-fixture'
            other_client['TRUEHDR_TEST_WINDOW_TYPE']='dialog'
            other=start([str(ROOT/'build-desktop/truehdr-desktop-window-test')],other_client,'fixture-other.log')
            time.sleep(1)
            click('application')
            options=until(lambda d:any('application_option_' in r['key'] and 'truehdr-another-fixture' in r['key'] and 'submitted-GPU-buffer' in r['key'] for r in d.get('controls',[])),'running application list')
            key=next(r['key'] for r in options['controls'] if r['key'].startswith('application_option_') and 'truehdr-another-fixture' in r['key'] and 'submitted-GPU-buffer' in r['key'])
            click(key)
            pending=until(lambda d:any(r['key']=='allow_application' for r in d.get('controls',[])),'candidate requires explicit permission')
            assert 'truehdr-universal-fixture' in pending['target']
            result['list_selection_does_not_allow_application']=True
            click('allow_application')
            other_status=until(lambda d:d.get('direct_input') and 'truehdr-another-fixture' in d.get('target',''),'second application direct filtering')
            result['second_fixture_window_type']=other_status['target_window_type']
            assert other_status['target_transient'] and other_status['target_modal'],'Fixture did not create a real modal transient dialog'
            result['dialog_fixture_request_honored']=True
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
            unload()
            controller_until(lambda d:d.get('ready') and d.get('watchdog_active'),'event-driven effect recovery')
            until(lambda d:d.get('stage')=='filtering' and d.get('direct_input'),'recovered filtering')
            result['effect_unload_recovers_by_event']=True
            supervisor=next(p for p in processes if p.args[0].endswith('truehdr-desktop-session'))
            supervisor.terminate();supervisor.wait(timeout=3)
        unload()
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
    else:
        code=subprocess.run(['dbus-run-session','--',str(ROOT/'.venv/bin/python'),str(Path(__file__).resolve()),'--inside',*sys.argv[1:]],cwd=ROOT).returncode
        if code==0 and 'VK_LAYER_KHRONOS_validation' in os.environ.get('VK_INSTANCE_LAYERS','') and 'Validation Error' in (FOLDER/'kwin.log').read_text():
            raise SystemExit('Vulkan validation reported errors in the private compositor log.')
        raise SystemExit(code)
