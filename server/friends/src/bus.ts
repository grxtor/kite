import type { MessageJson } from './chat.ts';
import type { InviteJson } from './routes/invites.ts';
import type { StatusView } from './status.ts';
import type { UserRef } from './users.ts';

/** What the server pushes to a player's open sockets. */
export type ServerEvent =
  | { type: 'friend.status'; uuid: string; status: StatusView }
  | { type: 'friend.request'; user: UserRef; createdAt: string }
  | { type: 'friend.accepted'; user: UserRef }
  | { type: 'friend.removed'; uuid: string }
  | { type: 'invite'; invite: InviteJson }
  | { type: 'invite.cancelled'; uuid: string }
  | { type: 'chat.message'; message: MessageJson }
  | { type: 'chat.read'; with: string; upTo: string }
  | { type: 'chat.deleted'; uuid: string };

export type BusMessage =
  /** `exceptConnection` keeps an event off the socket that caused it, which got its own answer. */
  | { kind: 'event'; event: ServerEvent; exceptConnection?: string }
  /** Closes the player's sockets: the ones signed in with `tokenHash` (hex), or all of them. */
  | { kind: 'end'; tokenHash?: string };

/**
 * Delivers messages to every socket a player has open. Messages carry only JSON, so an
 * implementation over Postgres LISTEN/NOTIFY can replace this one when there is more than
 * one server instance.
 */
export interface EventBus {
  publish(user: string, message: BusMessage): void;
  /** Returns the function that unsubscribes. */
  subscribe(user: string, listener: (message: BusMessage) => void): () => void;
}

/** The bus for a single server instance. */
export class MemoryEventBus implements EventBus {
  private readonly listeners = new Map<string, Set<(message: BusMessage) => void>>();

  publish(user: string, message: BusMessage): void {
    for (const listener of this.listeners.get(user) ?? []) listener(message);
  }

  subscribe(user: string, listener: (message: BusMessage) => void): () => void {
    let set = this.listeners.get(user);
    if (!set) {
      set = new Set();
      this.listeners.set(user, set);
    }
    set.add(listener);
    return () => {
      set.delete(listener);
      if (set.size === 0 && this.listeners.get(user) === set) this.listeners.delete(user);
    };
  }
}

export function publishEvent(bus: EventBus, user: string, event: ServerEvent, exceptConnection?: string): void {
  bus.publish(user, exceptConnection === undefined ? { kind: 'event', event } : { kind: 'event', event, exceptConnection });
}
