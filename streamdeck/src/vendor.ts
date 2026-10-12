// SPDX-License-Identifier: GPL-2.0-or-later
import { OBSWebSocket, EventSubscription, type OBSRequestTypes } from "obs-websocket-js";
import { VENDOR, type Envelope, type MatchState } from "./protocol";

// obs-websocket-js@5.0.8 does not re-export type-fest's `JsonObject`, so derive it from the
// exported request map: this is exactly the type `call()` expects for `requestData`, whereas
// `Record<string, unknown>` would not satisfy the call signature.
export type JsonObject = NonNullable<OBSRequestTypes["CallVendorRequest"]["requestData"]>;

export interface VendorConfig { host: string; port: number; password?: string; }
export const url = (c: VendorConfig): string => `ws://${c.host}:${c.port}`;

export interface VendorClient {
  request(type: string, data?: JsonObject): Promise<Envelope>;
  on(eventType: string, handler: (data: unknown) => void): void;
  // Fired after every GetFields/GetState sync (initial connect and each reconnect). The feedback
  // hub uses it to seed the keys from the fetched state instead of waiting for the first event.
  onSynced(handler: () => void): void;
  connect(config: VendorConfig): Promise<void>;
  disconnect(): Promise<void>;
  onStatus(cb: (connected: boolean) => void): void;
  getState(): MatchState | undefined;
  getField(key: string): string | undefined;
}

// Injected so tests can drive ObsVendorClient with a fake socket (finding 2).
export type ObsSocketFactory = () => OBSWebSocket;

export const RECONNECT_MIN_MS = 1000;
export const RECONNECT_MAX_MS = 30000;

export class ObsVendorClient implements VendorClient {
  private readonly obs: OBSWebSocket;
  private config?: VendorConfig;
  private stopping = false;
  private retryMs = RECONNECT_MIN_MS;
  private reconnectTimer?: ReturnType<typeof setTimeout>;
  // True while a reconnect attempt is in flight: ConnectionClosed is emitted for every failed
  // connect as well, so this keeps the catch re-entry and the event handler from starting a
  // second loop (finding 1).
  private reconnecting = false;
  // Bumped by connect()/disconnect() so an in-flight or sleeping loop from a previous socket
  // cannot resume against the new one. Each scheduled reconnect remembers its generation.
  private generation = 0;
  private state?: MatchState;
  private fields?: Record<string, string>;
  private readonly statusHandlers: ((connected: boolean) => void)[] = [];
  private readonly syncedHandlers: (() => void)[] = [];

  constructor(obsFactory: ObsSocketFactory = () => new OBSWebSocket()) {
    this.obs = obsFactory();
    this.obs.on("ConnectionClosed", () => {
      this.notify(false);
      this.scheduleReconnect();
    });
  }

  async connect(config: VendorConfig): Promise<void> {
    this.cancelReconnect();
    this.generation += 1;
    this.config = config;
    this.stopping = false;
    this.retryMs = RECONNECT_MIN_MS;
    await this.open();
  }

  private async open(): Promise<void> {
    if (!this.config) return;
    await this.obs.connect(url(this.config), this.config.password || undefined, {
      eventSubscriptions: EventSubscription.Vendors,
    });
    this.retryMs = RECONNECT_MIN_MS;
    this.notify(true);
    await this.syncState(); // initial (or post-reconnect) state
    this.notifySynced();
  }

  // The single reconnect loop: a timer schedules one attempt; the attempt schedules the next on
  // failure. ConnectionClosed only calls this while no attempt is pending/running.
  private scheduleReconnect(): void {
    if (this.stopping || !this.config || this.reconnecting || this.reconnectTimer) return;
    const gen = this.generation;
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = undefined;
      if (gen !== this.generation || this.stopping) return;
      void this.reconnectOnce(gen);
    }, this.retryMs);
  }

  private async reconnectOnce(gen: number): Promise<void> {
    this.reconnecting = true;
    try {
      await this.open();
      return; // connected: no further attempt
    } catch {
      if (gen !== this.generation) return;
      this.retryMs = Math.min(this.retryMs * 2, RECONNECT_MAX_MS);
    } finally {
      this.reconnecting = false;
    }
    if (gen !== this.generation || this.stopping) return;
    this.scheduleReconnect();
  }

  private cancelReconnect(): void {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = undefined;
    }
  }

  async disconnect(): Promise<void> {
    this.stopping = true;
    this.cancelReconnect();
    this.generation += 1;
    try {
      await this.obs.disconnect();
    } finally {
      this.notify(false);
    }
  }

  async syncState(): Promise<void> {
    const fields = await this.request("GetFields");
    if (fields.result === "ok" && fields.fields) this.fields = fields.fields;
    const state = await this.request("GetState");
    if (state.result === "ok" && state.state) this.state = state.state;
  }

  async request(type: string, data: JsonObject = {}): Promise<Envelope> {
    try {
      const response = await this.obs.call("CallVendorRequest", {
        vendorName: VENDOR,
        requestType: type,
        requestData: data,
      });
      // responseData is opaque JSON: go through `unknown` (an index-signature type is not
      // otherwise comparable to the Envelope interface).
      return response.responseData as unknown as Envelope;
    } catch {
      return { result: "error", error: "transport" };
    }
  }

  on(eventType: string, handler: (data: unknown) => void): void {
    this.obs.on("VendorEvent", (event) => {
      if (event.vendorName === VENDOR && event.eventType === eventType) handler(event.eventData);
    });
  }

  onSynced(handler: () => void): void { this.syncedHandlers.push(handler); }

  onStatus(cb: (connected: boolean) => void): void { this.statusHandlers.push(cb); }

  getState(): MatchState | undefined { return this.state; }
  getField(key: string): string | undefined { return this.fields?.[key]; }

  private notify(connected: boolean): void { for (const cb of this.statusHandlers) cb(connected); }
  private notifySynced(): void { for (const cb of this.syncedHandlers) cb(); }
}

// A fake vendor for tests: no obs-websocket, no network.
export class FakeVendor implements VendorClient {
  calls: { type: string; data: JsonObject }[] = [];
  replies: Record<string, Envelope> = {};
  throwOn = new Set<string>();
  private state?: MatchState;
  private fields?: Record<string, string>;
  private readonly handlers = new Map<string, ((data: unknown) => void)[]>();
  private readonly syncedHandlers: (() => void)[] = [];

  async connect(): Promise<void> {
    await this.syncState();
    for (const cb of this.syncedHandlers) cb();
  }
  async disconnect(): Promise<void> {}
  onStatus(): void {}
  onSynced(handler: () => void): void { this.syncedHandlers.push(handler); }
  getState(): MatchState | undefined { return this.state; }
  getField(key: string): string | undefined { return this.fields?.[key]; }

  async syncState(): Promise<void> {
    const fields = await this.request("GetFields");
    if (fields.fields) this.fields = fields.fields;
    const state = await this.request("GetState");
    if (state.state) this.state = state.state;
  }

  async request(type: string, data: JsonObject = {}): Promise<Envelope> {
    this.calls.push({ type, data });
    if (this.throwOn.has(type)) return { result: "error", error: "transport" };
    return this.replies[type] ?? { result: "ok" };
  }

  on(eventType: string, handler: (data: unknown) => void): void {
    const list = this.handlers.get(eventType) ?? [];
    list.push(handler);
    this.handlers.set(eventType, list);
  }

  emit(eventType: string, data: unknown): void {
    for (const handler of this.handlers.get(eventType) ?? []) handler(data);
  }
}
