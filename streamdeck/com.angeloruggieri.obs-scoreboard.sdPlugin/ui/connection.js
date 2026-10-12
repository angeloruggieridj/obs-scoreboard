// SPDX-License-Identifier: GPL-2.0-or-later
let websocket;
const connectElgatoStreamDeckSocket = (_port, _uuid, _registerEvent, _info, _actionInfo) => {
  websocket = new WebSocket(`ws://127.0.0.1:${_port}`);
  websocket.onopen = () => {
    websocket.send(JSON.stringify({ event: _registerEvent, uuid: _uuid }));
    websocket.send(JSON.stringify({ event: "getGlobalSettings", context: _uuid }));
  };
  websocket.onmessage = (e) => {
    const msg = JSON.parse(e.data);
    if (msg.event === "didReceiveGlobalSettings") {
      const s = msg.payload.settings;
      document.querySelector("sdpi-textfield[setting=host]").value = s.host ?? "127.0.0.1";
      document.querySelector("sdpi-textfield[setting=port]").value = s.port ?? "4455";
      document.querySelector("sdpi-password[setting=password]").value = s.password ?? "";
    }
  };
  const save = () => {
    const read = (sel, def) => document.querySelector(sel).value || def;
    websocket.send(JSON.stringify({
      event: "setGlobalSettings", context: _uuid,
      payload: {
        host: read("sdpi-textfield[setting=host]", "127.0.0.1"),
        port: read("sdpi-textfield[setting=port]", "4455"),
        password: read("sdpi-password[setting=password]", ""),
      },
    }));
  };
  for (const el of document.querySelectorAll("sdpi-textfield, sdpi-password"))
    el.addEventListener("change", save);
};
