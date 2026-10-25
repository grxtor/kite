export const minecraftNamePattern = '^[A-Za-z0-9_]{1,16}$';
const uuidPattern = '^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$';

export const nameBody = {
  type: 'object',
  required: ['name'],
  additionalProperties: false,
  properties: { name: { type: 'string', pattern: minecraftNamePattern } },
} as const;

export const nameParams = {
  type: 'object',
  required: ['name'],
  properties: { name: { type: 'string', pattern: minecraftNamePattern } },
} as const;

export const uuidParams = {
  type: 'object',
  required: ['uuid'],
  properties: { uuid: { type: 'string', pattern: uuidPattern } },
} as const;

export const challengeBody = {
  type: 'object',
  required: ['challengeId'],
  additionalProperties: false,
  properties: { challengeId: { type: 'string', pattern: uuidPattern } },
} as const;

export const settingsBody = {
  type: 'object',
  additionalProperties: false,
  properties: {
    visibility: { type: 'string', enum: ['everyone', 'friends', 'invisible'] },
    hideServer: { type: 'boolean' },
    shareIpServers: { type: 'boolean' },
    allowJoin: { type: 'boolean' },
  },
} as const;

// One flat object rather than oneOf: Ajv's removeAdditional would strip fields
// while trying the wrong branch. The handler checks which fields go together.
export const statusBody = {
  type: 'object',
  required: ['state'],
  additionalProperties: false,
  properties: {
    state: { type: 'string', enum: ['online', 'playing'] },
    instance: { type: 'string', minLength: 1, maxLength: 64 },
    version: { type: 'string', pattern: '^[A-Za-z0-9._+ -]{1,32}$' },
    world: {
      type: ['object', 'null'],
      required: ['type'],
      additionalProperties: false,
      properties: {
        type: { type: 'string', enum: ['server', 'singleplayer'] },
        host: { type: 'string', minLength: 1, maxLength: 255 },
        port: { type: 'integer', minimum: 1, maximum: 65535 },
      },
    },
  },
} as const;
