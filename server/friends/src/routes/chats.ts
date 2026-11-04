import type { FastifyInstance } from 'fastify';
import {
  defaultPageSize,
  history,
  markRead,
  maxPageSize,
  messageIdPattern,
  reportMessage,
  reportReasons,
  sendMessage,
  type ReportReason,
} from '../chat.ts';
import { currentUser, type Context } from '../context.ts';
import { badRequest } from '../errors.ts';
import { uuidParams } from '../schemas.ts';

const historyQuery = {
  type: 'object',
  additionalProperties: false,
  properties: {
    before: { type: 'string', pattern: messageIdPattern },
    after: { type: 'string', pattern: messageIdPattern },
    limit: { type: 'integer', minimum: 1, maximum: maxPageSize },
  },
} as const;

const messageBody = {
  type: 'object',
  required: ['text'],
  additionalProperties: false,
  properties: { text: { type: 'string' } },
} as const;

const readBody = {
  type: 'object',
  required: ['upTo'],
  additionalProperties: false,
  properties: { upTo: { type: 'string', pattern: messageIdPattern } },
} as const;

const reportBody = {
  type: 'object',
  required: ['reason'],
  additionalProperties: false,
  properties: { reason: { type: 'string', enum: reportReasons } },
} as const;

const reportParams = {
  type: 'object',
  required: ['uuid', 'id'],
  properties: { ...uuidParams.properties, id: { type: 'string', pattern: messageIdPattern } },
} as const;

export function registerChatRoutes(app: FastifyInstance, ctx: Context) {
  app.get<{ Params: { uuid: string }; Querystring: { before?: string; after?: string; limit?: number } }>(
    '/v1/chats/:uuid/messages',
    { schema: { params: uuidParams, querystring: historyQuery } },
    async (request) => {
      const { before, after, limit = defaultPageSize } = request.query;
      if (before !== undefined && after !== undefined) throw badRequest('Use before or after, not both');
      return history(ctx, currentUser(request).uuid, request.params.uuid, { before, after, limit });
    },
  );

  app.post<{ Params: { uuid: string }; Body: { text: string } }>(
    '/v1/chats/:uuid/messages',
    { schema: { params: uuidParams, body: messageBody } },
    async (request, reply) => {
      const message = await sendMessage(ctx, currentUser(request).uuid, request.params.uuid, request.body.text);
      return reply.status(201).send(message);
    },
  );

  app.post<{ Params: { uuid: string }; Body: { upTo: string } }>(
    '/v1/chats/:uuid/read',
    { schema: { params: uuidParams, body: readBody } },
    async (request) => ({ upTo: await markRead(ctx, currentUser(request).uuid, request.params.uuid, request.body.upTo) }),
  );

  app.post<{ Params: { uuid: string; id: string }; Body: { reason: ReportReason } }>(
    '/v1/chats/:uuid/messages/:id/report',
    { schema: { params: reportParams, body: reportBody } },
    async (request, reply) => {
      await reportMessage(ctx, currentUser(request).uuid, request.params.uuid, request.params.id, request.body.reason);
      return reply.status(204).send();
    },
  );
}
