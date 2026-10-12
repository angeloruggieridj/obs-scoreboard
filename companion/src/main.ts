// SPDX-License-Identifier: MIT
import { InstanceBase, InstanceStatus, type SomeCompanionConfigField } from "@companion-module/base";

export type ModuleConfig = { host: string; port: number };
export type ModuleSecrets = { password?: string };
export type ModuleSchema = {
  config: ModuleConfig;
  secrets: ModuleSecrets | undefined;
  actions: Record<string, never>;
  feedbacks: Record<string, never>;
  variables: Record<string, string>;
};

export default class ScoreboardInstance extends InstanceBase<ModuleSchema> {
  async init(_config: ModuleConfig, _isFirstInit: boolean, _secrets: ModuleSecrets | undefined): Promise<void> {
    this.updateStatus(InstanceStatus.Ok); // actions/feedbacks/variables are added in 7.3.3-7.3.4
  }
  async configUpdated(_config: ModuleConfig, _secrets: ModuleSecrets | undefined): Promise<void> {}
  async destroy(): Promise<void> {}
  getConfigFields(): SomeCompanionConfigField[] { return []; }
}
