interface Window {
  count: number;
  resetAt: number;
}

/** Fixed-window counters kept in memory. One server instance, so nothing is shared. */
export class RateLimiter {
  private readonly clock: () => number;
  private readonly windows = new Map<string, Window>();

  constructor(clock: () => number) {
    this.clock = clock;
  }

  hit(key: string, limit: number, windowMs: number): { ok: boolean; retryAfterSeconds: number } {
    const now = this.clock();
    let window = this.windows.get(key);
    if (!window || window.resetAt <= now) {
      window = { count: 0, resetAt: now + windowMs };
      this.windows.set(key, window);
    }
    if (window.count >= limit) {
      return { ok: false, retryAfterSeconds: Math.max(1, Math.ceil((window.resetAt - now) / 1000)) };
    }
    window.count += 1;
    return { ok: true, retryAfterSeconds: 0 };
  }

  sweep(): void {
    const now = this.clock();
    for (const [key, window] of this.windows) {
      if (window.resetAt <= now) this.windows.delete(key);
    }
  }
}
