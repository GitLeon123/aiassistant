export type TurnState = 'streaming' | 'done' | 'error' | 'cancelled';

export interface Turn {
  id: string;
  question: string;
  screen: boolean;
  screenLabel: string;
  modelLabel: string;
  providerLabel: string;
  providerId: string;
  limitWarning: string;
  answer: string;
  reasoning: string;
  status: string;
  state: TurnState;
  error: string;
  startedAt: number;
  /** When the first reasoning or answer text arrived. */
  firstTokenAt: number;
  /** When the first answer text arrived (ends the "thinking" phase). */
  answeredAt: number;
  finishedAt: number;
  outputTokens: number;
  /** Whether text arrived token by token (false for providers that send whole messages). */
  streamed: boolean;
}
