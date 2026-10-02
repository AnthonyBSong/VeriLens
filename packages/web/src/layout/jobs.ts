// Versioned async layout jobs. Each scope key has a monotonically increasing
// version; a result is applied only if it is still the latest for its key, so a
// slow stale job can never overwrite newer state.
import type { ModuleLayout } from './types';

export interface JobResult { key: string; version: number; layout: ModuleLayout }

export class LayoutJobs {
  private latest = new Map<string, number>();

  /** Bump and return the version that a new job for `key` must carry. */
  next(key: string): number {
    const v = (this.latest.get(key) ?? 0) + 1;
    this.latest.set(key, v);
    return v;
  }

  /** True when `version` is still the newest job for `key`. */
  isCurrent(key: string, version: number): boolean {
    return this.latest.get(key) === version;
  }

  /** Run `compute` asynchronously; resolves to null when the result is stale. */
  run(key: string, compute: () => ModuleLayout, schedule: (fn: () => void) => void = queueMicrotask): Promise<JobResult | null> {
    const version = this.next(key);
    return new Promise((resolve) => {
      schedule(() => {
        const layout = compute();
        resolve(this.isCurrent(key, version) ? { key, version, layout } : null);
      });
    });
  }

  /** Invalidate every key (e.g. new layout file or design). */
  reset() { this.latest.clear(); }
}
