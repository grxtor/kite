export type ErrorCode =
  | 'bad_request'
  | 'challenge_invalid'
  | 'unauthorized'
  | 'not_joined'
  | 'not_found'
  | 'already_friends'
  | 'blocked_by_you'
  | 'limit_reached'
  | 'address_not_shared'
  | 'invites_off'
  | 'rate_limited'
  | 'mojang_unavailable'
  | 'muted'
  | 'banned'
  | 'internal';

/** Extra fields an error body carries next to `error` and `message`, e.g. `until` for `muted`. */
export type ErrorDetails = Readonly<Record<string, string | null>>;

export class HttpError extends Error {
  readonly status: number;
  readonly code: ErrorCode;
  readonly retryAfterSeconds: number | null;
  readonly details: ErrorDetails;

  constructor(
    status: number,
    code: ErrorCode,
    message: string,
    retryAfterSeconds: number | null = null,
    details: ErrorDetails = {},
  ) {
    super(message);
    this.status = status;
    this.code = code;
    this.retryAfterSeconds = retryAfterSeconds;
    this.details = details;
  }
}

export const notFound = (message: string) => new HttpError(404, 'not_found', message);
export const badRequest = (message: string) => new HttpError(400, 'bad_request', message);
export const rateLimited = (retryAfterSeconds: number) =>
  new HttpError(429, 'rate_limited', 'Too many requests', retryAfterSeconds);
