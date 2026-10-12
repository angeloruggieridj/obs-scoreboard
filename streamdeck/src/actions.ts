// SPDX-License-Identifier: GPL-2.0-or-later
import {
	action,
	SingletonAction,
	type Action,
	type KeyDownEvent,
	type WillAppearEvent,
	type WillDisappearEvent,
} from "@elgato/streamdeck";

import type { FeedbackHub, FeedbackTarget } from "./feedback";
import { command, type ActionSettings } from "./settings";
import type { VendorClient } from "./vendor";

// `ActionSettings` is an interface, so it lacks the implicit index signature the SDK's
// `SingletonAction<T extends JsonObject>` needs; a mapped type keeps the same fields and adds it.
type Settings = { [K in keyof ActionSettings]: ActionSettings[K] };

// One {@link SingletonAction} per protocol command. A shared client and feedback hub are created in
// plugin.ts and passed to every constructor; a key that shows state passes its {@link FeedbackTarget}
// so its title follows the scoreboard.
export class CommandAction extends SingletonAction<Settings> {
	private readonly titles = new Map<string, string>();

	constructor(
		protected readonly vendor: VendorClient,
		protected readonly hub: FeedbackHub,
		private readonly id: string,
		private readonly feedback?: FeedbackTarget,
	) {
		super();
	}

	override async onKeyDown(ev: KeyDownEvent<Settings>): Promise<void> {
		const call = command(this.id, ev.payload.settings);
		if (!call) return;
		const result = await this.vendor.request(call.type, call.data);
		if (result.result !== "ok") await ev.action.showAlert();
	}

	override async onWillAppear(ev: WillAppearEvent<Settings>): Promise<void> {
		await this.paint(ev.action);
	}

	override onWillDisappear(ev: WillDisappearEvent<Settings>): void {
		this.titles.delete(ev.action.id);
	}

	// Called by plugin.ts on every vendor event, so the visible keys follow the scoreboard.
	async refreshTitles(): Promise<void> {
		if (!this.feedback) return;
		for (const visible of this.actions) await this.paint(visible);
	}

	private async paint(target: Action<Settings>): Promise<void> {
		if (!this.feedback || !target.isKey()) return;
		const title = this.hub.title(this.feedback);
		if (this.titles.get(target.id) === title) return;
		this.titles.set(target.id, title);
		await target.setTitle(title);
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.clock-toggle" })
export class ClockToggleAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "clock-toggle", "clock");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.clock-start" })
export class ClockStartAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "clock-start", "clock");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.clock-stop" })
export class ClockStopAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "clock-stop", "clock");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.clock-reset" })
export class ClockResetAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "clock-reset", "clock");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.clock-adjust" })
export class ClockAdjustAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "clock-adjust", "clock");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.clock-set" })
export class ClockSetAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "clock-set", "clock");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.period-next" })
export class PeriodNextAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "period-next", "period");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.period-prev" })
export class PeriodPrevAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "period-prev", "period");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.score" })
export class ScoreAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "score", "homeScore");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.stat" })
export class StatAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "stat");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.set-team-name" })
export class SetTeamNameAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "set-team-name");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.penalty-add" })
export class PenaltyAddAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "penalty-add");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.penalty-edit" })
export class PenaltyEditAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "penalty-edit");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.penalty-cancel" })
export class PenaltyCancelAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "penalty-cancel");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.penalty-cancel-active" })
export class PenaltyCancelActiveAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "penalty-cancel-active");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.stoppage-set" })
export class StoppageSetAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "stoppage-set");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.stoppage-add" })
export class StoppageAddAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "stoppage-add");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.end-match" })
export class EndMatchAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "end-match");
	}
}

@action({ UUID: "com.angeloruggieri.obs-scoreboard.new-match" })
export class NewMatchAction extends CommandAction {
	constructor(vendor: VendorClient, hub: FeedbackHub) {
		super(vendor, hub, "new-match");
	}
}
