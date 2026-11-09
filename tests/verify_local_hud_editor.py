from pathlib import Path

root = Path(__file__).resolve().parents[1]
views = root / "deploy" / "PrismaUI_F4" / "views" / "Interface" / "FalloutAnomaly"

rad_hud = (root / "src" / "RadHud.cpp").read_text(encoding="utf-8")
rad_hud_header = (root / "src" / "RadHud.h").read_text(encoding="utf-8")
rad_hud_lifecycle = (root / "src" / "RadHudLifecycle.cpp").read_text(encoding="utf-8")
main_source = (root / "src" / "main.cpp").read_text(encoding="utf-8")
rad_debug = (root / "src" / "RadEngine_Debug.cpp").read_text(encoding="utf-8")
rad_layout = (root / "src" / "RadHudLayout.cpp").read_text(encoding="utf-8")
hooks_input = (root / "src" / "Hooks_Input.cpp").read_text(encoding="utf-8")
radiation_engine = (root / "src" / "RadiationEngine.cpp").read_text(encoding="utf-8")
editor_js = (views / "rad_hud_local_editor.js").read_text(encoding="utf-8")
toxic = (views / "hud_toxic.html").read_text(encoding="utf-8")
filter_html = (views / "hud_filter.html").read_text(encoding="utf-8")
main_html = (views / "rad_debug.html").read_text(encoding="utf-8")

assert 'BindGameThreadUIEvent' not in rad_hud
assert 'api->RegisterJSListener(view, "radDebug", OnRadDebugCommand)' in rad_hud
assert '_toxicWidgetRegistered.store(_hudApi != nullptr);' in rad_hud
assert 'const bool fallback = !companion && !_toxicWidgetRegistered.load();' in rad_hud
assert rad_hud.count('SyncLocalHudLayout();') >= 2
assert 'HUD layout editor requires PrismaHUD_F4' not in rad_hud
assert 'void RadHud::OpenHudLayoutEditor()' not in rad_hud

public_api = rad_hud_header.split('public:', 1)[1].split('private:', 1)[0]
assert 'bool StartPrismaViewRetry();' in public_api
views_ready = public_api.split('[[nodiscard]] bool ViewsReady() const', 1)[1]
for token in (
    '_viewsReady.load()',
    '_domReady.load()',
    '_toxicDomReady.load()',
    '_filterDomReady.load()',
):
    assert token in views_ready, f'HUD readiness must include replacement DOM state: {token}'

suppress_callback = rad_hud_lifecycle.split('void SuppressVanillaMetersOnPrismaThread', 1)[1].split(
    'void CreateRadHudViewOnPrismaThread', 1
)[0]
create_callback = rad_hud_lifecycle.split('void CreateRadHudViewOnPrismaThread', 1)[1].split(
    'bool RadHud::StartPrismaViewRetry()', 1
)[0]
for widget in ('HUDPlayerHealthMeter', 'HUDActionPointMeter'):
    restore = f'api11->SuppressHUDWidget("{widget}", false);'
    suppress = f'api11->SuppressHUDWidget("{widget}", true);'
    assert restore in create_callback
    assert suppress in suppress_callback
    assert create_callback.find(restore) < create_callback.find('hud.CreateView();')
assert 'if (!api11 || !RadHud::GetSingleton().ViewsReady()) return;' in suppress_callback
assert 'if (RadHud::GetSingleton().IsCompanionMode()) return;' in suppress_callback
assert 'if (api11 && !hud.IsCompanionMode())' in create_callback
retry_worker = rad_hud_lifecycle.split('bool RadHud::StartPrismaViewRetry()', 1)[1].split('\n}\n', 1)[0] + '\n}'
assert 'if (!_threadApi) return false;' in retry_worker
assert 'if (_viewCreateRetry.exchange(true)) return true;' in retry_worker
assert 'if (nowMs < _viewRetryNotBeforeMs.load()) return true;' not in retry_worker
assert 'if (nowMs < _viewRetryNotBeforeMs.load()) return _viewCreateRetry.load() || ViewsExist();' in retry_worker
assert retry_worker.rstrip().endswith('return true;\n}')
retry_thread = retry_worker.split('auto* retrying', 1)[1]
ready = retry_thread.find('RadHud::GetSingleton().ViewsReady()')
suppress_dispatch = retry_thread.find('DispatchToGameThread(SuppressVanillaMetersOnPrismaThread, api11)')
assert ready != -1 and suppress_dispatch != -1 and ready < suppress_dispatch

push_toxic = rad_hud.split('void RadHud::PushToxic', 1)[1].split('void RadHud::PushMask', 1)[0]
invalid_recovery = push_toxic.split('if (_view == 0 || !api->IsValid(_view)', 1)[1].split('SyncLocalHudLayout();', 1)[0]
assert 'StartPrismaViewRetry();' in invalid_recovery
assert 'if (!_threadApi)' in invalid_recovery
assert 'CreateView();' in invalid_recovery
assert invalid_recovery.find('StartPrismaViewRetry();') < invalid_recovery.find('CreateView();')
assert 'return;' in invalid_recovery

hud_wait = main_source.split('if (held >= 5000)', 1)[1].split('if (!createQueued)', 1)[0]
assert 'auto& hud = RadHud::GetSingleton();' in hud_wait
assert 'if (!hud.StartPrismaViewRetry())' in hud_wait
assert 'hud.CreateView();' in hud_wait
assert hud_wait.find('hud.StartPrismaViewRetry()') < hud_wait.find('hud.CreateView();')
assert 'RadHud::GetSingleton().CreateView();' not in hud_wait

session_reset = rad_hud.split('void RadHud::ResetDebugMenuForSession()', 1)[1].split(
    'void RadHud::ToggleDebugMenu()', 1
)[0]
for token in ('g_inWorld.store(false);', '_debugOpen = false;', 'SetLocalHudEditorPreview(false);', 'SetDebugMenu(false);'):
    assert token in session_reset
assert session_reset.find('g_inWorld.store(false);') < session_reset.find('SetDebugMenu(false);')
assert session_reset.find('SetLocalHudEditorPreview(false);') < session_reset.find('SetDebugMenu(false);')

assert 'a.rfind("hudlayout:", 0) == 0' in rad_debug
assert 'ApplyLocalHudLayout(a.substr(std::strlen("hudlayout:")))' in rad_debug
assert 'cmds.swap(_pendingDebugCmds);' in rad_debug
assert 'DispatchDebugCommand(c);' in rad_debug
assert 'constexpr std::size_t kMaxPendingDebugCmds = 32;' in rad_debug
assert '_pendingDebugCmds.erase(_pendingDebugCmds.begin());' in rad_debug
assert 'HudLayoutWidgetKey(queued) == widget' in rad_debug
assert 'Log()->warn("[DIAG] HandleDebugCommand' not in rad_debug
assert 'Log()->warn("[DIAG] DispatchDebugCommand' not in rad_debug
assert 'SEND_INTERVAL_MS = 100;' in editor_js
assert 'function sendNow(id, save)' in editor_js
assert 'sendUpdate(id, true);' in editor_js
drain_debug = rad_debug.split('void RadiationEngine::DrainDebugCommands()', 1)[1].split(
    'void RadiationEngine::DispatchDebugCommand', 1
)[0]
session_gate = drain_debug.find('if (_prevRads < 0.0f)')
mask_reset = drain_debug.find('_maskOverride = -1;')
shelter_reset = drain_debug.find('_debugShelter = false;')
swap = drain_debug.find('cmds.swap(_pendingDebugCmds);')
assert session_gate != -1 and mask_reset != -1 and shelter_reset != -1 and swap != -1
assert '_pendingDebugCmds.clear();' not in drain_debug
assert session_gate < mask_reset < swap
assert session_gate < shelter_reset < swap

assert rad_layout.count('void RadHud::OpenHudLayoutEditor()') == 1
assert 'RegisterJSListener(_debugView, "radHudLayout", OnRadHudLayoutCommand)' in rad_layout
assert 'RadiationEngine::GetSingleton().HandleDebugCommand(command.c_str())' in rad_layout
assert 'if (action == "sync")' in rad_layout
assert 'if (_hudApi)' in rad_layout
assert '_hudEditorApi = PRISMA_HUD_API::RequestEditorAPI();' in rad_layout
assert 'editor->OpenEditor();' in rad_layout
assert 'api->InteropCall(_debugView, "OpenLocalHudEditor", layout.c_str())' in rad_layout
assert 'api->InteropCall(_toxicView, "setLocalHUDLayout", payload.c_str())' in rad_layout
assert 'api->InteropCall(_filterView, "setLocalHUDLayout", payload.c_str())' in rad_layout
assert 'api->InteropCall(_toxicView, "releaseLocalHUDLayout", "")' in rad_layout
assert 'api->InteropCall(_filterView, "releaseLocalHUDLayout", "")' in rad_layout
assert 'api->InteropCall(_climateView, "setLocalHUDLayout", payload.c_str())' in rad_layout
assert 'api->InteropCall(_climateView, "releaseLocalHUDLayout", "")' in rad_layout
assert 'if (id == "rad_climate") return &g_layout.climate;' in rad_layout
assert 'if (id == "rad_companions") return &g_layout.companions;' in rad_layout
assert 'api->InteropCall(_companionsView, "setLocalHUDLayout", payload.c_str())' in rad_layout
assert 'api->InteropCall(_companionsView, "releaseLocalHUDLayout", "")' in rad_layout
assert 'api->InteropCall(_companionsView, "setEditorPreview", value)' in rad_layout
assert rad_layout.count('"rad_companions"') >= 5, 'FullJson, DefaultsJson, load, select and the companion default must all carry rad_companions'
assert editor_js.count('rad_companions') >= 4, 'the local editor must default, card, save and reset rad_companions'
assert 'if (action == "unit")' in rad_layout
assert 'hud_layout.json' in rad_layout

poll_input = radiation_engine.split("void RadiationEngine::PollInput()", 1)[1].split(
    "void RadiationEngine::EnsureEslLoaded()", 1
)[0]
assert poll_input.count("RadHud::GetSingleton().ToggleDebugMenu();") == 1
assert "RadHud::GetSingleton().ToggleDebugMenu();" not in hooks_input
assert "const_cast<RE::ButtonEvent*>(e)->handled = RE::InputEvent::HANDLED_RESULT::kStop;" in hooks_input

assert 'Fallback vitals: Health + Rads / Stamina (AP) / Tissue' in editor_js
assert 'Without PrismaHUD, Rad supplies Health with radiation loss, Stamina/AP, Tissue' in editor_js
assert 'window.radHudLayout(JSON.stringify(command))' in editor_js
assert 'window.radDebug("hudlayout:"' not in editor_js
assert 'localStorage' not in editor_js

for token in ('aria-label="HEALTH"', 'id="stamina"', 'id="rads"', 'hud_status.css'):
    assert token in toxic
assert 'window.setLocalHUDLayout=function(json)' in toxic
assert 'window.releaseLocalHUDLayout=function()' in toxic
assert 'fallback=!!d.fallback' in toxic

for html in (toxic, filter_html):
    assert 'window.setHUDPosition=function(json)' in html
    assert 'window.setEditorPreview=function(json)' in html
    assert 'window.setLocalHUDLayout=function(json)' in html
    assert 'window.releaseLocalHUDLayout=function()' in html
    assert 'rad_hud_local_widget.js' not in html

assert not (views / 'rad_hud_local_widget.js').exists()
assert '<script src="rad_hud_local_editor.js"></script>' in main_html
assert "cmd('hud:open')" in main_html

import re

debug_js = (views / "rad_debug.js").read_text(encoding="utf-8")

assert 'document.body.appendChild(editor)' in editor_js, 'editor must not live inside the console tab, widgets drew over its sliders'
assert 'getElementById("tab-hud").appendChild' not in editor_js
dock = re.search(r"#radLocalHudEditor\{([^}]*)\}", editor_js).group(1)
for token in ('position:fixed', 'top:0', 'right:0', 'bottom:0', 'overflow-y:auto'):
    assert token in dock, f'docked editor panel must declare {token}'
assert not re.search(r"(?<![-\w])left:", dock)
width = re.search(r"width:min\((\d+)px,(\d+)vw\)", dock)
assert width and int(width.group(1)) <= 400 and int(width.group(2)) <= 40, 'editor panel must stay a narrow side strip'
assert 'body.rad-hud-editing #debugPanel,body.rad-hud-editing #debugBackdrop{display:none!important}' in editor_js, 'console and dim backdrop must hide while editing'
assert 'document.body.classList.add("rad-hud-editing")' in editor_js
assert 'document.body.classList.remove("rad-hud-editing")' in editor_js
assert 'id="radLocalExit"' in editor_js and 'window.closeDebugMenu()' in editor_js
assert 'if (!show && window.CloseLocalHudEditor) window.CloseLocalHudEditor();' in debug_js, 'closing the menu must end the docked editor'
assert 'e.key === "Escape"' in debug_js and 'panel.classList.contains("on")' in debug_js, 'Esc must still close while the console is hidden'

exit_handler = editor_js.split('getElementById("radLocalExit").onclick', 1)[1].split('};', 1)[0]
assert 'setPreview(false);' in exit_handler and exit_handler.find('setPreview(false);') < exit_handler.find('closeDebugMenu'), 'CLOSE MENU must turn the widget preview off before closing'
destroy_body = rad_hud.split('void RadHud::DestroyDebugView()', 1)[1].split('\n}\n', 1)[0]
assert 'SetLocalHudEditorPreview(false);' in destroy_body and destroy_body.find('SetLocalHudEditorPreview(false);') < destroy_body.find('if (_debugView == 0) return;'), 'every close route must clear the widget preview, even with no debug view left'

assert 'openJson["defaults"] = DefaultsJson();' in rad_layout, 'Reset must use the same defaults as the widgets, including companion slots'
assert 'ApplyCompanionDefaults(g_layout);' in rad_layout and 'ApplyCompanionDefaults(base);' in rad_layout
assert 'applyDefaults(parsed && parsed.defaults);' in editor_js

for widget in ('hud_needs', 'hud_climate', 'hud_toxic', 'hud_filter', 'hud_companions'):
    body = (views / f'{widget}.html').read_text(encoding='utf-8')
    assert '(_want.xf*W)' in body and '(_want.yf*H)' in body, f'{widget}: position must be a fraction of the viewport'
    assert 'PhosCore.fit(W,H)' in body, f'{widget}: size must be scaled by PhosCore.fit'
assert 'x: cfg.x,' in editor_js and 'y: cfg.y,' in editor_js and 's: cfg.s,' in editor_js, 'editor must send viewport fractions, fit scales size only'
assert 'innerWidth' not in editor_js and 'PhosCore' not in editor_js

print("Rad native fallback HUD ownership, session reset, in-world gate, recovery routing, load-safe debug state, and single debug-hotkey guard passed")
