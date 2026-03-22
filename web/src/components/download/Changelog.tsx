import { CHANGELOG } from '@/content/changelog';

export default function Changelog() {
  return (
    <div className="p-6 rounded-xl bg-bg-card border border-border">
      <h3 className="text-lg font-bold text-text-primary mb-6">Changelog</h3>
      <div className="space-y-6">
        {CHANGELOG.map((entry) => (
          <div key={entry.version} className="relative pl-6 border-l-2 border-border">
            <div className="absolute -left-[7px] top-0 w-3 h-3 rounded-full bg-accent border-2 border-bg" />
            <div className="flex items-center gap-3 mb-2">
              <span className="text-accent font-bold">v{entry.version}</span>
              <span className="text-xs text-text-secondary">{entry.date}</span>
            </div>
            <ul className="space-y-1">
              {entry.changes.map((change, i) => (
                <li key={i} className="text-sm text-text-secondary flex items-start gap-2">
                  <span className="text-accent mt-1">•</span>
                  {change}
                </li>
              ))}
            </ul>
          </div>
        ))}
      </div>
    </div>
  );
}
