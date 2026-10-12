// SPDX-License-Identifier: GPL-2.0-or-later
import streamDeck from "@elgato/streamdeck";

import {
	ClockAdjustAction,
	ClockResetAction,
	ClockSetAction,
	ClockStartAction,
	ClockStopAction,
	ClockToggleAction,
	type CommandAction,
	EndMatchAction,
	NewMatchAction,
	PenaltyAddAction,
	PenaltyCancelAction,
	PenaltyCancelActiveAction,
	PenaltyEditAction,
	PeriodNextAction,
	PeriodPrevAction,
	ScoreAction,
	SetTeamNameAction,
	StatAction,
	StoppageAddAction,
	StoppageSetAction,
} from "./actions";
import { FeedbackHub } from "./feedback";
import { ObsVendorClient, type VendorConfig } from "./vendor";

// Host/port/password are written to the global settings by the property inspector (Task 7.2.6).
type ConnectionSettings = { host?: string; port?: string | number; password?: string };

const vendor = new ObsVendorClient();
const hub = new FeedbackHub(vendor);
hub.start();

// One instance per action id; the @action decorator on each class supplies its manifest UUID.
const actions: CommandAction[] = [
	new ClockToggleAction(vendor, hub),
	new ClockStartAction(vendor, hub),
	new ClockStopAction(vendor, hub),
	new ClockResetAction(vendor, hub),
	new ClockAdjustAction(vendor, hub),
	new ClockSetAction(vendor, hub),
	new PeriodNextAction(vendor, hub),
	new PeriodPrevAction(vendor, hub),
	new ScoreAction(vendor, hub),
	new StatAction(vendor, hub),
	new SetTeamNameAction(vendor, hub),
	new PenaltyAddAction(vendor, hub),
	new PenaltyEditAction(vendor, hub),
	new PenaltyCancelAction(vendor, hub),
	new PenaltyCancelActiveAction(vendor, hub),
	new StoppageSetAction(vendor, hub),
	new StoppageAddAction(vendor, hub),
	new EndMatchAction(vendor, hub),
	new NewMatchAction(vendor, hub),
];

for (const action of actions) streamDeck.actions.registerAction(action);

// Every vendor event repaints the visible keys that show state.
const refreshTitles = (): void => {
	for (const action of actions) void action.refreshTitles();
};
vendor.on("StateChanged", refreshTitles);
vendor.on("ScoreChanged", refreshTitles);

// The property inspector writes host/port/password to the global settings (Task 7.2.6); reconnect
// whenever they change, so the user does not have to restart Stream Deck.
streamDeck.settings.onDidReceiveGlobalSettings<ConnectionSettings>((ev) => {
	void reconnect(ev.settings);
});

void start();

async function start(): Promise<void> {
	try {
		await streamDeck.connect();
	} catch (err) {
		streamDeck.logger.error(`obs-scoreboard: Stream Deck connection failed: ${String(err)}`);
		return;
	}
	await reconnect(await streamDeck.settings.getGlobalSettings<ConnectionSettings>());
}

// Close any previous socket, then connect with the new settings. A failure is logged and swallowed:
// a missing or misconfigured obs-websocket must never crash the plugin.
async function reconnect(settings: ConnectionSettings): Promise<void> {
	try {
		await vendor.disconnect();
	} catch {
		// Nothing to close before the first connection.
	}
	try {
		await vendor.connect(toConfig(settings));
	} catch (err) {
		streamDeck.logger.error(`obs-scoreboard: obs-websocket connection failed: ${String(err)}`);
	}
}

function toConfig(settings: ConnectionSettings): VendorConfig {
	const port = Number(settings.port ?? 4455);
	return {
		host: settings.host || "127.0.0.1",
		port: Number.isFinite(port) ? port : 4455,
		password: settings.password || undefined,
	};
}
