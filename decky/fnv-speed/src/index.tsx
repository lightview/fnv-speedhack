import {
  ButtonItem,
  Dropdown,
  PanelSection,
  PanelSectionRow,
  TextField,
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

const PRESETS: number[] = [0.5, 1, 1.5, 2, 3, 5, 10];

function Content() {
  const [status, setStatus] = useState<Status | null>(null);
  const [pathInput, setPathInput] = useState("");
  const [selectedCandidate, setSelectedCandidate] = useState<string>("");
  const [busy, setBusy] = useState(false);
  const lastSpeed = useRef<number | null>(null);

  const refresh = async () => {
    try {
      const s = await getStatus();
      setStatus(s);
      if (s.game_dir) setPathInput(s.game_dir);
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

  const saveDir = async () => {
    setBusy(true);
    try {
      const ok = await setGameDir(pathInput);
      toaster.toast({
        title: "FNV Speed",
        body: ok ? "Game folder saved" : "FalloutNV.exe not found there!"
      });
    } finally {
      setBusy(false);
      refresh();
    }
  };

  const doDetect = async () => {
    setBusy(true);
    try {
      const s = await getStatus();
      if (s.candidates.length > 0) {
        await setGameDir(s.candidates[0]);
        setSelectedCandidate(s.candidates[0]);
        toaster.toast({
          title: "FNV Speed",
          body: s.candidates.length > 1
            ? `Found ${s.candidates.length} copies - pick the right one below`
            : `Found: ${s.candidates[0]}`
        });
      } else {
        toaster.toast({ title: "FNV Speed", body: "No FNV installation found" });
      }
    } finally {
      setBusy(false);
      refresh();
    }
  };

  const saveSelected = async () => {
    if (!selectedCandidate) return;
    setBusy(true);
    try {
      const ok = await setGameDir(selectedCandidate);
      toaster.toast({
        title: "FNV Speed",
        body: ok ? "Game folder saved" : "Save failed"
      });
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
        body: res.ok ? "DLL installed" : `Install failed: ${res.error}`
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

  return (
    <PanelSection title="FNV Speed">
      <PanelSectionRow>
        <div style={{ textAlign: "center", fontSize: "1.4em", fontWeight: "bold" }}>
          {status?.speed != null ? `${status.speed}x` : "—"}
        </div>
      </PanelSectionRow>

      <PanelSectionRow>
        <Focusable style={{ display: "flex", flexWrap: "wrap", gap: "4px" }}>
          {PRESETS.map((v) => (
            <div key={v} style={{ flex: "1 1 30%", minWidth: "80px" }}>
              <ButtonItem onClick={() => applySpeed(v)} disabled={busy}>
                {v}x
              </ButtonItem>
            </div>
          ))}
        </Focusable>
      </PanelSectionRow>

      <PanelSectionRow>
        <div style={{ fontSize: "0.85em", opacity: 0.7, padding: "4px" }}>
          {status?.game_dir
            ? status.dll_installed
              ? "NVSE plugin installed ✓"
              : "DLL not installed in game folder"
            : "Game folder is not set"}
        </div>
      </PanelSectionRow>

      <PanelSectionRow>
        <ButtonItem layout="below" onClick={doInstall} disabled={busy || !status?.game_dir}>
          Install NVSE plugin DLL
        </ButtonItem>
      </PanelSectionRow>

      <PanelSectionRow>
        <ToggleField
          label="Toast on speed change"
          description="Shows a notification whenever the speed changes (paddles, menu, file)"
          checked={status?.notify !== false}
          onChange={(v) => toggleNotify(v)}
        />
      </PanelSectionRow>

      {status && status.candidates.length > 1 && (
        <PanelSectionRow>
          <Dropdown
            label="Found copies"
            options={status.candidates.map((c) => ({ value: c, label: c.split("/").pop() || c }))}
            selected={selectedCandidate || status.candidates[0]}
            onChange={({ value }) => setSelectedCandidate(value as string)}
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
        <TextField
          label="Game folder (path to FalloutNV.exe)"
          value={pathInput}
          onChange={(e) => setPathInput(e.target.value)}
        />
      </PanelSectionRow>
      <PanelSectionRow>
        <Focusable style={{ display: "flex", gap: "4px" }}>
          <div style={{ flex: 1 }}>
            <ButtonItem onClick={saveDir} disabled={busy}>
              Save
            </ButtonItem>
          </div>
          <div style={{ flex: 1 }}>
            <ButtonItem onClick={doDetect} disabled={busy}>
              Auto-detect
            </ButtonItem>
          </div>
        </Focusable>
      </PanelSectionRow>
    </PanelSection>
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
