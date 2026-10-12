// SPDX-License-Identifier: GPL-2.0-or-later
// The obs-scoreboard vendor protocol, shared shape with docs/remote-control.md.

export const VENDOR = "obs-scoreboard";

export type Team = "home" | "away";
export type StatName = "score" | "shots" | "fouls" | "fouls2";

export interface Envelope {
  result: "ok" | "rejected" | "confirmation" | "error";
  reason?: string;
  error?: string;
  state?: MatchState;
  fields?: Record<string, string>;
}

export interface PenaltyRow {
  id: number;
  player: string;
  time: string;
  phase2: string;
  queued: boolean;
}

export interface MatchState {
  protocol: number;
  sport: string;
  period: number;
  periodLabel: string;
  regulationPeriods: number;
  clock: string;
  clockTenths: number;
  clockRunning: boolean;
  clockDirection: "up" | "down";
  homeName: string;
  awayName: string;
  homeScore: number;
  awayScore: number;
  homeShots: number;
  awayShots: number;
  homeFouls: number;
  awayFouls: number;
  homeFouls2: number;
  awayFouls2: number;
  strength: string;
  stoppage: string;
  stoppageAnnounced: string;
  stoppageAnnouncedMinutes: number;
  playTime: string;
  matchEnded: boolean;
  penalties: { home: PenaltyRow[]; away: PenaltyRow[] };
}

export interface StateChangedEvent {
  changed: Record<string, string>;
  clockRunning: boolean;
  matchEnded: boolean;
}
export interface PeriodEndedEvent { period: number; label: string; }
export interface ScoreChangedEvent { team: Team; score: number; homeScore: number; awayScore: number; }

export interface PenaltyPhaseIn { tenths: number; reducesStrength?: boolean; }

// The requestData for each requestType; one builder per request in the protocol.
export const requests = {
  clockToggle: () => ({}),
  clockStart: () => ({}),
  clockStop: () => ({}),
  clockReset: () => ({}),
  clockAdjust: (deltaTenths: number) => ({ deltaTenths }),
  clockSet: (tenths: number) => ({ tenths }),
  periodNext: (confirmed = false) => ({ confirmed }),
  periodPrev: (confirmed = false) => ({ confirmed }),
  addStat: (team: Team, stat: StatName, delta: number) => ({ team, stat, delta }),
  setTeamName: (team: Team, name: string) => ({ team, name }),
  penaltyAdd: (team: Team, player: string, phases: PenaltyPhaseIn[]) => ({ team, player, phases }),
  penaltyEdit: (id: number, tenths: number) => ({ id, tenths }),
  penaltyCancel: (id: number) => ({ id }),
  penaltyCancelActive: (team: Team, slot: number) => ({ team, slot }),
  stoppageSet: (minutes: number) => ({ minutes }),
  stoppageAdd: (minutes: number) => ({ minutes }),
  endMatch: () => ({}),
  newMatch: (sport: string, confirmed = false) => ({ sport, confirmed }),
  getState: () => ({}),
  getFields: () => ({}),
} as const;

export const requestNames = Object.freeze([
  "ClockToggle", "ClockStart", "ClockStop", "ClockReset", "ClockAdjust", "ClockSet",
  "PeriodNext", "PeriodPrev", "AddStat", "SetTeamName", "PenaltyAdd", "PenaltyEdit",
  "PenaltyCancel", "PenaltyCancelActive", "StoppageSet", "StoppageAdd", "EndMatch", "NewMatch",
  "GetState", "GetFields",
]);
