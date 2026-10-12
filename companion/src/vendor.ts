// SPDX-License-Identifier: MIT
import { OBSWebSocket, EventSubscription, type OBSRequestTypes } from "obs-websocket-js";
import { VENDOR, type Envelope, type MatchState } from "./protocol";

// obs-websocket-js does not re-export the `JsonObject` type from `type-fest` (it only uses it in
// the generated request/response types), so derive it from `CallVendorRequest`'s requestData.
export type JsonObject = NonNullable<OBSRequestTypes["CallVendorRequest"]["requestData"]>;

export interface VendorConfig { host: string; port: number; password?: string; }
export const url = (c: VendorConfig): string => `ws://${c.host}:${c.port}`;

// The payload of an obs-websocket `VendorEvent` (only the fields the client reads).
export interface VendorEventPayload {
  vendorName: string;
  eventType: string;
  eventData: unknown;
}

// The slice of obs-websocket-js the client uses. It is a constructor parameter so tests can inject
// a fake socket: reconnect/backoff and the transport mapping have no other coverage (finding 2).
export interface VendorSocket {
  connect(
    url: string,
    password?: string,
    options?: { eventSubscriptions?: number },
  ): Promise<unknown>;
  disconnect(): Promise<unknown>;
  call(requestType: string, data?: JsonObject): Promise<{ responseData?: unknown }>;
  on(event: "ConnectionClosed", listener: () => void): void;
  on(event: "VendorEvent", listener: (event: VendorEventPayload) => void): void;
}

export interface VendorClient {
  request(type: string, data?: JsonObject): Promise<Envelope>;
  on(eventType: string, handler: (data: unknown) => void): void;
  connect(config: VendorConfig): Promise<void>;
  disconnect(): Promise<void>;
  onStatus(cb: (connected: boolean) => void): void;
  // Fires after every successful syncState() (the initial connect and every reconnect), so the
  // host seeds its state instead of racing the connect promise (finding 4).
  onSynced(cb: () => void): void;
  getState(): MatchState | undefined;
  getField(key: string): string | undefined;
}

export class ObsVendorClient implements VendorClient {
  private readonly obs: VendorSocket;
  private config?: VendorConfig;
  private stopping = false;
  private retryMs = 1000;
  private reconnectTimer?: ReturnType<typeof setTimeout>;
  private state?: MatchState;
  private fields?: Record<string, string>;
  private readonly statusHandlers: ((connected: boolean) => void)[] = [];
  private readonly syncedHandlers: (() => void)[] = [];

  constructor(socket?: VendorSocket) {
    this.obs = socket ?? (new OBSWebSocket() as unknown as VendorSocket);
    // A single loop: a failed connect also reaches scheduleReconnect() from open()'s catch, and
    // obs-websocket-js emits ConnectionClosed for every failed attempt, so this handler alone
    // would start a second loop (finding 1).
    this.obs.on("ConnectionClosed", () => {
      this.notify(false);
      this.scheduleReconnect();
    });
  }

  async connect(config: VendorConfig): Promise<void> {
    this.config = config;
    this.stopping = false;
    await this.open();
  }

  private async open(): Promise<void> {
    if (!this.config) return;
    await this.obs.connect(url(this.config), this.config.password || undefined, {
      eventSubscriptions: EventSubscription.Vendors,
    });
    this.retryMs = 1000;
    this.notify(true);
    await this.syncState(); // initial (or post-reconnect) state
    this.notifySynced();
  }

  // One reconnect timer at a time: both a failed open() and a ConnectionClosed land here, and the
  // pending-timer guard keeps a single attempt per backoff step (finding 1).
  private scheduleReconnect(): void {
    if (this.stopping || this.reconnectTimer !== undefined) return;
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = undefined;
      if (this.stopping) return;
      void this.open().catch(() => this.scheduleReconnect());
    }, this.retryMs);
    this.retryMs = Math.min(this.retryMs * 2, 30000);
  }

  async disconnect(): Promise<void> {
    this.stopping = true;
    if (this.reconnectTimer !== undefined) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = undefined;
    }
    await this.obs.disconnect();
    this.notify(false);
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

  onStatus(cb: (connected: boolean) => void): void { this.statusHandlers.push(cb); }
  onSynced(cb: () => void): void { this.syncedHandlers.push(cb); }

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

  async connect(): Promise<void> { await this.syncState(); }
  async disconnect(): Promise<void> {}
  onStatus(): void {}
  onSynced(cb: () => void): void { this.syncedHandlers.push(cb); }
  getState(): MatchState | undefined { return this.state; }
  getField(key: string): string | undefined { return this.fields?.[key]; }

  async syncState(): Promise<void> {
    const fields = await this.request("GetFields");
    if (fields.fields) this.fields = fields.fields;
    const state = await this.request("GetState");
    if (state.state) this.state = state.state;
    for (const cb of this.syncedHandlers) cb();
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
