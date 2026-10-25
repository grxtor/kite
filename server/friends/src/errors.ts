export type ErrorCode =
  | 'bad_request'
  | 'challenge_invalid'
  | 'unauthorized'
  | 'not_joined'
  | 'not_found'
  | 'already_friends'
  | 'blocked_by_you'
  | 'limit_reached'
  | 'rate_limited'
  | 'mojang_unavailable'
  | 'internal';

export class HttpError extends Error {
  readonly status: number;
  readonly code: ErrorCode;
  readonly retryAfterSeconds: number | null;

  constructor(status: number, code: ErrorCode, message: string, retryAfterSeconds: number | null = null) {
    super(message);
    this.status = status;
    this.code = code;
    this.retryAfterSeconds = retryAfterSeconds;
  }
}

export const notFound = (message: string) => new HttpError(404, 'not_found', message);
export const badRequest = (message: string) => new HttpError(400, 'bad_request', message);
export const rateLimited = (retryAfterSeconds: number) =>
  new HttpError(429, 'rate_limited', 'Too many requests', retryAfterSeconds);
