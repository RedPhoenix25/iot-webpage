import React from 'react';
import { Activity, Zap } from 'lucide-react';

const AnalogGauge = ({ power, voltage, current, max = 5000 }) => {
  const radius = 80;
  const circumference = Math.PI * radius;
  // Ensure percentage doesn't exceed 1
  const percentage = Math.min(power / max, 1);
  const strokeDashoffset = circumference - percentage * circumference;

  return (
    <div className="glass-panel power-metrics-panel" style={{ gridColumn: '1 / -1' }}>
      <div className="power-gauge-container">
        <h3 style={{ color: 'var(--text-muted)', marginBottom: '1rem', display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ width: '8px', height: '8px', background: 'var(--accent-color)', borderRadius: '50%', boxShadow: '0 0 10px var(--accent-glow)' }}></span>
          Live Wattage
        </h3>
        <div style={{ position: 'relative', width: '200px', height: '110px' }}>
          <svg viewBox="0 0 200 110" width="200" height="110">
            {/* Background Track */}
            <path
              d="M 20 100 A 80 80 0 0 1 180 100"
              fill="none"
              stroke="rgba(255,255,255,0.1)"
              strokeWidth="16"
              strokeLinecap="round"
            />
            {/* Progress Bar */}
            <path
              d="M 20 100 A 80 80 0 0 1 180 100"
              fill="none"
              stroke="url(#gradient)"
              strokeWidth="16"
              strokeLinecap="round"
              strokeDasharray={circumference}
              strokeDashoffset={strokeDashoffset}
              style={{ transition: 'stroke-dashoffset 0.5s ease-in-out' }}
            />
            <defs>
              <linearGradient id="gradient" x1="0%" y1="0%" x2="100%" y2="0%">
                <stop offset="0%" stopColor="#00e5ff" />
                <stop offset="100%" stopColor="#ff4d4f" />
              </linearGradient>
            </defs>
          </svg>
          <div style={{ position: 'absolute', bottom: '-5px', left: '0', right: '0', textAlign: 'center' }}>
            <span style={{ fontSize: '2.5rem', fontWeight: '800', fontFamily: "'Outfit', sans-serif" }}>
              {power.toFixed(0)}
            </span>
            <span style={{ fontSize: '1rem', color: 'var(--text-muted)', marginLeft: '4px' }}>W</span>
          </div>
        </div>
      </div>

      <div className="power-stats-container">
        <div className="power-stat-box">
          <div className="stat-label"><Activity size={16} /> Main Voltage</div>
          <div className="stat-value" style={{ color: 'var(--success-color)' }}>
            {voltage !== null ? voltage.toFixed(1) : '--'} <span className="stat-unit">V</span>
          </div>
        </div>
        <div className="power-stat-box">
          <div className="stat-label"><Zap size={16} /> Total Current</div>
          <div className="stat-value" style={{ color: 'var(--warning-color)' }}>
            {current !== null ? current.toFixed(2) : '--'} <span className="stat-unit">A</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default AnalogGauge;
