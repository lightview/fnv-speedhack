import {
  ButtonItem,
  DropdownItem,
  PanelSection,
  PanelSectionRow,
  SliderField,
  ToggleField,
  Focusable,
  staticClasses
} from "@decky/ui";
import {
  callable,
  definePlugin,
  toaster
} from "@decky/api";
import { useEffect, useRef, useState } from "react";
import { FaTachometerAlt } from "react-icons/fa";

interface Status {
  game_dir: string | null;
  dll_installed: boolean;
  speed: number | null;
  notify: boolean;
  candidates: string[];
}

const getStatus = callable<[], Status>("get_status");
const setSpeed = callable<[v: number], boolean>("set_speed");
const setGameDir = callable<[p: string], boolean>("set_game_dir");
const setNotify = callable<[v: boolean], boolean>("set_notify");
const installDll = callable<[], { ok: boolean; path?: string; error?: string }>("install_dll");

const PRESETS: number[] = [0.5, 1, 2, 3, 5, 10];
const PADDLE_HINT = "L4 1x · L5 3x · R4 5x · R5 10x";

function SpeedButton({ v, active, onClick }: { v: number; active: boolean; onClick: () => void }) {
  return (
    <div style={{ flex: "1 1 30%", minWidth: "90px" }}>
      <ButtonItem onClick={onClick}>
        <span style={active ? { fontWeight: "bold", fontSize: "1.1em" } : undefined}>
          {v}x
        </span>
      </ButtonItem>
    </div>
  );
}

function Content() {
  const [status, setStatus] = useState<Status | null>(null);
  const [selectedCandidate, setSelectedCandidate] = useState<string>("");
  const [busy, setBusy] = useState(false);
  const lastSpeed = useRef<number | null>(null);

  const refresh = async () => {
    try {
      const s = await getStatus();
      setStatus(s);
      if (s.candidates.length > 0 && !selectedCandidate) {
        setSelectedCandidate(s.candidates[0]);
      }
      // toast on speed changes coming from anywhere: paddles, file, DLL itself
      if (s.speed != null) {
        if (lastSpeed.current != null && s.speed !== lastSpeed.current && s.notify) {
          toaster.toast({ title: "FNV Speed", body: `→ ${s.speed}x` });
        }
        lastSpeed.current = s.speed;
      }
    } catch (e) {
      console.error("get_status failed:", e);
    }
  };

  useEffect(() => {
    refresh();
    const t = setInterval(refresh, 3000);
    return () => clearInterval(t);
  }, []);

  const applySpeed = async (v: number) => {
    setBusy(true);
    try {
      const ok = await setSpeed(v);
      if (!ok) {
        toaster.toast({ title: "FNV Speed", body: "Game folder not set!" });
      }
    } finally {
      setBusy(false);
      refresh();
    }
  };

  const doInstall = async () => {
    setBusy(true);
    try {
      const res = await installDll();
      toaster.toast({
        title: "FNV Speed",
        body: res.ok ? "Speedhack installed" : `Install failed: ${res.error}`
      });
    } finally {
      setBusy(false);
      refresh();
    }
  };

  const toggleNotify = async (v: boolean) => {
    try {
      await setNotify(v);
      setStatus((prev) => (prev ? { ...prev, notify: v } : prev));
      if (v) {
        toaster.toast({ title: "FNV Speed", body: "Notifications on" });
      }
    } catch (e) {
      console.error("set_notify failed:", e);
    }
  };

  const saveSelected = async () => {
    if (!selectedCandidate) return;
    setBusy(true);
    try {
      await setGameDir(selectedCandidate);
    } finally {
      setBusy(false);
      refresh();
    }
  };

  const sliderSpeed = (v: number) => {
    // snap to common values so the slider rarely replaces typing exact numbers
    const snapped = [0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 6, 8, 10, 15, 20].reduce((a, b) =>
      Math.abs(b - v) < Math.abs(a - v) ? b : a
    );
    if (snapped !== status?.speed) applySpeed(snapped);
  };

  const active = status?.speed ?? null;

  return (
    <div>
      <PanelSection title="Game Speed">
        <PanelSectionRow>
          <Focusable style={{ display: "flex", justifyContent: "center", gap: "8px" }}>
            <span style={{ fontSize: "2em", fontWeight: "bold", minWidth: "70px", textAlign: "right" }}>
              {active != null ? `${active}x` : "—"}
            </span>
          </Focusable>
        </PanelSectionRow>

        <PanelSectionRow>
          <Focusable style={{ display: "flex", flexWrap: "wrap", gap: "4px" }}>
            {PRESETS.slice(0, 3).map((v) => (
              <SpeedButton key={v} v={v} active={active === v} onClick={() => applySpeed(v)} />
            ))}
          </Focusable>
        </PanelSectionRow>
        <PanelSectionRow>
          <Focusable style={{ display: "flex", flexWrap: "wrap", gap: "4px" }}>
            {PRESETS.slice(3, 6).map((v) => (
              <SpeedButton key={v} v={v} active={active === v} onClick={() => applySpeed(v)} />
            ))}
          </Focusable>
        </PanelSectionRow>

        <PanelSectionRow>
          <SliderField
            label="Fine tune"
            value={active ?? 1}
            min={0.5}
            max={20}
            step={0.5}
            onChange={(v: number) => sliderSpeed(v)}
          />
        </PanelSectionRow>

        <PanelSectionRow>
          <div style={{ fontSize: "0.8em", opacity: 0.65, textAlign: "center", padding: "2px 0 6px" }}>
            {PADDLE_HINT}
          </div>
        </PanelSectionRow>
      </PanelSection>

      <PanelSection title="Setup">
        <PanelSectionRow>
          <div style={{ fontSize: "0.85em", opacity: 0.75, padding: "2px 0" }}>
            {status?.game_dir
              ? status.dll_installed
                ? `✓ installed · ${status.game_dir.split("/").pop()}`
                : "✗ speedhack DLL not installed"
              : "No game folder found"}
          </div>
        </PanelSectionRow>
        <PanelSectionRow>
          <ButtonItem layout="below" onClick={doInstall} disabled={busy || !status?.game_dir}>
            {status?.dll_installed ? "Reinstall / repair" : "Install speedhack"}
          </ButtonItem>
        </PanelSectionRow>

        {status && status.candidates.length > 1 && (
          <PanelSectionRow>
            <DropdownItem
              label="Game copy"
              rgOptions={status.candidates.map((c) => ({
                data: c,
                label: c.split("/").pop() || c,
              }))}
              selectedOption={selectedCandidate || status.candidates[0]}
              onChange={(data) => setSelectedCandidate(String(data.data))}
            />
          </PanelSectionRow>
        )}
        {status && status.candidates.length > 1 && (
          <PanelSectionRow>
            <ButtonItem layout="below" onClick={saveSelected} disabled={busy}>
              Use selected copy
            </ButtonItem>
          </PanelSectionRow>
        )}

        <PanelSectionRow>
          <ToggleField
            label="Toast on speed change"
            checked={status?.notify !== false}
            onChange={(v) => toggleNotify(v)}
          />
        </PanelSectionRow>
      </PanelSection>
    </div>
  );
}

export default definePlugin(() => {
  console.log("FNV Speed plugin initializing");

  return {
    name: "FNV Speed",
    titleView: <div className={staticClasses.Title}>FNV Speed</div>,
    content: <Content />,
    icon: <FaTachometerAlt />,
    onDismount() {
      console.log("FNV Speed plugin unloading");
    },
  };
});
