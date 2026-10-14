// Typed message channel to the native host. Outside WebView2 (plain browser, `npm run dev`)
// a mock host is loaded instead so the UI can be developed without the app.

import type { HostMessage, HostMessageOf, UiMessage } from './types';

interface WebViewChannel {
  postMessage(message: unknown): void;
  addEventListener(type: 'message', listener: (event: { data: unknown }) => void): void;
}

declare global {
  interface Window {
    chrome?: { webview?: WebViewChannel };
  }
}

type Listener = (msg: HostMessage) => void;

export interface Transport {
  post(message: UiMessage): void;
  start(deliver: (msg: HostMessage) => void): void;
}

const listeners = new Map<string, Set<Listener>>();
let transport: Transport | null = null;
let outbox: UiMessage[] = [];

function deliver(msg: HostMessage) {
  if (!msg || typeof msg !== 'object' || typeof msg.type !== 'string') return;
  for (const key of [msg.type, '*']) {
    const set = listeners.get(key);
    if (!set) continue;
    for (const fn of [...set]) {
      try {
        fn(msg);
      } catch (err) {
        console.error(`bridge listener for "${msg.type}" failed`, err);
      }
    }
  }
}

export const isNative = typeof window !== 'undefined' && !!window.chrome?.webview;

export function send(message: UiMessage) {
  if (transport) transport.post(message);
  else outbox.push(message);
}

export function on<T extends HostMessage['type']>(type: T, fn: (msg: HostMessageOf<T>) => void): () => void {
  let set = listeners.get(type);
  if (!set) listeners.set(type, (set = new Set()));
  set.add(fn as Listener);
  return () => set.delete(fn as Listener);
}

export function onAny(fn: Listener): () => void {
  return on('*' as HostMessage['type'], fn as never);
}

/** Connects to the host and announces the page. Call once, after listeners are registered. */
export async function connect(page: 'bar' | 'settings') {
  if (isNative) {
    const channel = window.chrome!.webview!;
    transport = {
      post: (m) => channel.postMessage(m),
      start: (d) => channel.addEventListener('message', (e) => d(e.data as HostMessage)),
    };
  } else {
    const { createMockTransport } = await import('./mock');
    transport = createMockTransport(page);
  }
  transport.start(deliver);
  const pending = outbox;
  outbox = [];
  transport.post({ type: 'ready' });
  for (const m of pending) transport.post(m);
}
