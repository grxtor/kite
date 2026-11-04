import type { FastifyInstance, FastifyRequest } from 'fastify';
import { hour, type Context } from '../context.ts';
import { parseCrashReport } from '../crashReports.ts';
import { recordCrashReport } from '../crashStore.ts';

// 500 mods with long ids and versions, and 24 lines of 300 characters, stay under this.
export const crashReportBodyLimitBytes = 64 * 1024;

/*
 * POST /v1/crash-reports: a crash the player chose to send. No sign-in, and no token is read even
 * when one is sent, so a report never leads back to an account. The address is used only for the
 * rate limit, in memory.
 */
export function registerCrashReportRoutes(
  app: FastifyInstance,
  ctx: Context,
  clientIp: (request: FastifyRequest) => string,
) {
  app.post('/v1/crash-reports', { bodyLimit: crashReportBodyLimitBytes }, async (request, reply) => {
    ctx.limit(`crash-reports:${clientIp(request)}`, ctx.config.rateLimits.crashReportsPerHour, hour);
    const report = parseCrashReport(request.body);
    await recordCrashReport(ctx.pool, report, ctx.now());
    // Nothing to say back: no id, so the launcher has nothing that could tie it to the report.
    return reply.status(202).send();
  });
}
