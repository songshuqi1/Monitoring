Monitoring Runtime
==================

This folder is the runtime package. It does not contain frontend/src, backend/src, headers, node_modules, or build scripts.

Run:
  start_runtime.bat

Public internet access without hardware:
  1. Download cloudflared.exe for Windows from Cloudflare.
  2. Put cloudflared.exe in this folder.
  3. Run start_public_runtime.bat.
  4. Share the printed https://*.trycloudflare.com URL with other computers.

Communication resources:
  The public URL only exposes this web/API service on port 8081.
  OPC UA and UDP data sources are still connected by this computer's backend.
  Software-defined communication (SDC) is configured in Communication Resources.
  Enter the Monitor Agent address (for example http://DEVICE-IP:9100); the runtime
  directly polls /api/runtime/stream and expects {"vars":{"name": value}}.
  Enabled SDC resources are saved under data and automatically restart with the runtime.
  For OPC UA, the endpoint must be reachable from this computer, for example:
    opc.tcp://DEVICE-IP:4840
  Cloudflare Quick Tunnel does not expose OPC UA/UDP ports automatically.

Local access:
  http://127.0.0.1:8081

LAN access:
  http://202.118.21.28:8081

Optional ports:
  HTTP_SERVER_PORT controls the web/API port. Default: 8081.
  OPCUA_SERVER_PORT controls the local OPC UA server port. Default: 4841.

Open Windows Firewall port 8081 if another computer cannot access it.