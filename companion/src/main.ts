// SPDX-License-Identifier: MIT
import {
  InstanceBase,
  InstanceStatus,
  type CompanionActionDefinitions,
  type CompanionActionSchemaWithoutResult,
  type CompanionFeedbackSchema,
  type CompanionOptionValues,
  type CompanionVariableValues,
  type SomeCompanionConfigField,
} from "@companion-module/base";
import { buildActionDefinitions } from "./actions";
import { buildFeedbackDefinitions, feedbackIds } from "./feedbacks";
import { buildVariableDefinitions, fieldKeys } from "./variables";
import { ModuleState } from "./state";
import { ObsVendorClient, type VendorClient } from "./vendor";
import type { PeriodEndedEvent, ScoreChangedEvent, StateChangedEvent } from "./protocol";

export type ModuleConfig = { host: string; port: number };
export type ModuleSecrets = { password?: string };

// The action/feedback schemas the SDK resolves the definitions against. The record form keeps one
// type for all 20 requests without duplicating the id list that `actions.ts`/`feedbacks.ts` own.
export type ActionSchemas = Record<string, CompanionActionSchemaWithoutResult<CompanionOptionValues>>;
export type FeedbackSchemas = Record<string, CompanionFeedbackSchema<CompanionOptionValues>>;
export type ModuleSchema = {
  config: ModuleConfig;
  secrets: ModuleSecrets | undefined;
  actions: ActionSchemas;
  feedbacks: FeedbackSchemas;
  variables: CompanionVariableValues;
};

export default class ScoreboardInstance extends InstanceBase<ModuleSchema> {
  private vendor?: VendorClient;
  private readonly state = new ModuleState();

  async init(
    config: ModuleConfig,
    _isFirstInit: boolean,
    secrets: ModuleSecrets | undefined,
  ): Promise<void> {
    this.updateVariableDefinitions();
    await this.connect(config, secrets);
  }

  async configUpdated(config: ModuleConfig, secrets: ModuleSecrets | undefined): Promise<void> {
    await this.connect(config, secrets);
  }

  async destroy(): Promise<void> {
    await this.vendor?.disconnect();
    this.vendor = undefined;
  }

  getConfigFields(): SomeCompanionConfigField[] {
    return [];
  }

  // (Re)creates the vendor client, wires the events and connects. Called from init and configUpdated.
  private async connect(config: ModuleConfig, secrets: ModuleSecrets | undefined): Promise<void> {
    await this.vendor?.disconnect();

    const vendor = this.createVendor();
    this.vendor = vendor;

    vendor.on("StateChanged", (data) => {
      this.state.applyStateChanged(data as StateChangedEvent);
      this.publish();
    });
    vendor.on("ScoreChanged", (data) => {
      this.state.applyScoreChanged(data as ScoreChangedEvent);
      this.publish();
    });
    vendor.on("PeriodEnded", (data) => {
      this.state.applyPeriodEnded(data as PeriodEndedEvent);
      this.publish();
    });
    vendor.on("MatchEnded", () => this.publish());
    vendor.onStatus((connected) =>
      this.updateStatus(connected ? InstanceStatus.Ok : InstanceStatus.ConnectionFailure),
    );

    this.updateActions();
    this.updateFeedbacks();
    this.updateStatus(InstanceStatus.Connecting);

    try {
      await vendor.connect({
        host: config?.host ?? "127.0.0.1",
        port: config?.port ?? 4455,
        password: secrets?.password,
      });
    } catch (err) {
      this.log("error", `obs-websocket connection failed: ${String(err)}`);
      this.updateStatus(InstanceStatus.ConnectionFailure);
      return;
    }

    // The vendor fetched GetFields/GetState on connect: seed the state so the variables are live
    // before the first StateChanged event arrives.
    this.seed(vendor);
  }

  private createVendor(): VendorClient {
    return new ObsVendorClient();
  }

  private seed(vendor: VendorClient): void {
    const changed: Record<string, string> = {};
    for (const key of fieldKeys) {
      const value = vendor.getField(key);
      if (value !== undefined) changed[key] = value;
    }
    const match = vendor.getState();
    if (Object.keys(changed).length > 0 || match) {
      this.state.applyStateChanged({
        changed,
        clockRunning: match?.clockRunning ?? false,
        matchEnded: match?.matchEnded ?? false,
      });
    }
    this.publish();
  }

  private publish(): void {
    this.setVariableValues(this.state.variables());
    const [first, ...rest] = feedbackIds;
    this.checkFeedbacks(first, ...rest);
  }

  private updateActions(): void {
    const definitions = buildActionDefinitions(this.vendor!, (level, message) =>
      this.log(level, message),
    );
    // `buildActionDefinitions` returns `Record<string, CompanionActionDefinition>`; the SDK wants the
    // schema-mapped type. They are the same definitions at runtime.
    this.setActionDefinitions(definitions as unknown as CompanionActionDefinitions<ActionSchemas>);
  }

  private updateFeedbacks(): void {
    this.setFeedbackDefinitions(buildFeedbackDefinitions(this.state));
  }

  private updateVariableDefinitions(): void {
    this.setVariableDefinitions(buildVariableDefinitions());
  }
}
