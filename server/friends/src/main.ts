import { buildApp } from './app.ts';
import { loadConfig } from './config.ts';
import { createPool, migrate } from './db.ts';

const config = loadConfig(process.env);
const pool = createPool(config.databaseUrl);
await migrate(pool);
const app = await buildApp({ config, pool });

for (const signal of ['SIGINT', 'SIGTERM'] as const) {
  process.once(signal, () => {
    app.log.info(`${signal} received, shutting down`);
    void app
      .close()
      .then(() => pool.end())
      .then(() => process.exit(0));
  });
}

await app.listen({ port: config.port, host: config.host });
