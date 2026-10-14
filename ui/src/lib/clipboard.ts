import { on, send } from './bridge';
import { uid } from './dom';

export function copyToHost(text: string, html?: string): Promise<boolean> {
  const id = uid();
  return new Promise((resolve) => {
    const finish = (ok: boolean) => { clearTimeout(timer); unsubscribe(); resolve(ok); };
    const unsubscribe = on('copyResult', (message) => { if (message.id === id) finish(message.ok); });
    const timer = setTimeout(() => finish(false), 5000);
    send({ type: 'copy', id, text, html });
  });
}
