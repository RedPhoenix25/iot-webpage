import React, { useState, useEffect } from 'react';
import { ref, get, set, onValue } from 'firebase/database';
import { db } from '../firebase';
import { Zap, Calendar, TrendingUp, Settings, X, Check } from 'lucide-react';

const RATE_DB_PATH = 'settings/electricity_rate';
const DEFAULT_RATE = 225;

const EnergyStats = () => {
  const [monthKwh, setMonthKwh] = useState(null);
  const [rate, setRate] = useState(DEFAULT_RATE);
  const [showRateEditor, setShowRateEditor] = useState(false);
  const [rateInput, setRateInput] = useState(DEFAULT_RATE.toString());
  const [saving, setSaving] = useState(false);

  const fetchEnergyData = async () => {
    const now = new Date();
    const year = now.getFullYear();
    const month = now.getMonth() + 1;
    const day = now.getDate();

    try {


      // --- This Month's kWh ---
      const monthRef = ref(db, `energy_logs/${year}/${month}`);
      const monthSnap = await get(monthRef);
      if (monthSnap.exists()) {
        const days = monthSnap.val();
        let totalMonth = 0;
        for (const d in days) {
          for (const h in days[d]) {
            const entry = days[d][h];
            totalMonth += entry.total_wh || entry.wh || 0;
          }
        }
        setMonthKwh(totalMonth / 1000);
      } else {
        setMonthKwh(0);
      }
    } catch (err) {
      console.error('EnergyStats fetch error:', err);
    }
  };

  // Real-time listener for the electricity rate from Firebase
  useEffect(() => {
    const rateRef = ref(db, RATE_DB_PATH);
    const unsubscribe = onValue(rateRef, (snapshot) => {
      if (snapshot.exists()) {
        const val = snapshot.val();
        if (typeof val === 'number' && val > 0) {
          setRate(val);
          setRateInput(val.toString());
        }
      }
    });
    return () => unsubscribe();
  }, []);

  useEffect(() => {
    fetchEnergyData();
    // Refresh every 2 minutes
    const interval = setInterval(fetchEnergyData, 120000);
    return () => clearInterval(interval);
  }, []);

  const handleSaveRate = async () => {
    const parsed = parseFloat(rateInput);
    if (!isNaN(parsed) && parsed > 0) {
      setSaving(true);
      try {
        // Write to Firebase — the onValue listener will update all devices instantly
        await set(ref(db, RATE_DB_PATH), parsed);
      } catch (err) {
        console.error('Failed to save rate:', err);
      } finally {
        setSaving(false);
      }
    }
    setShowRateEditor(false);
  };

  const estimatedCost = monthKwh !== null ? (monthKwh * rate) : null;

  return (
    <div className="energy-stats-bar">


      {/* This Month */}
      <div className="energy-stat-chip">
        <div className="energy-stat-icon" style={{ background: 'rgba(16, 185, 129, 0.15)', color: 'var(--success-color)' }}>
          <Calendar size={18} />
        </div>
        <div>
          <p className="energy-stat-label">This Month</p>
          <p className="energy-stat-value">
            {monthKwh === null
              ? <span className="loading-pulse">—</span>
              : <>{monthKwh.toFixed(3)} <span className="energy-stat-unit">kWh</span></>
            }
          </p>
        </div>
      </div>

      {/* Divider */}
      <div className="energy-stat-divider" />

      {/* Estimated Cost */}
      <div className="energy-stat-chip" style={{ flex: 1 }}>
        <div className="energy-stat-icon" style={{ background: 'rgba(251, 191, 36, 0.15)', color: '#fbbf24' }}>
          <TrendingUp size={18} />
        </div>
        <div style={{ flex: 1 }}>
          <p className="energy-stat-label">Est. Monthly Cost</p>
          <p className="energy-stat-value">
            {estimatedCost === null
              ? <span className="loading-pulse">—</span>
              : <>₦{estimatedCost.toLocaleString('en-NG', { maximumFractionDigits: 0 })} <span className="energy-stat-unit">/ month</span></>
            }
          </p>
        </div>
        <button
          onClick={() => { setRateInput(rate.toString()); setShowRateEditor(true); }}
          title="Set electricity rate"
          style={{ background: 'none', border: 'none', color: 'var(--text-muted)', cursor: 'pointer', padding: '4px', borderRadius: '6px', transition: 'color 0.2s' }}
          onMouseOver={e => e.currentTarget.style.color = 'var(--accent-color)'}
          onMouseOut={e => e.currentTarget.style.color = 'var(--text-muted)'}
        >
          <Settings size={16} />
        </button>
      </div>

      {/* Rate Editor Modal */}
      {showRateEditor && (
        <div style={{ position: 'fixed', top: 0, left: 0, right: 0, bottom: 0, background: 'rgba(0,0,0,0.7)', zIndex: 1000, display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '1rem' }}>
          <div className="glass-panel" style={{ padding: '2rem', maxWidth: '360px', width: '100%', position: 'relative' }}>
            <button onClick={() => setShowRateEditor(false)} style={{ position: 'absolute', top: '16px', right: '16px', background: 'none', border: 'none', color: 'var(--text-muted)', cursor: 'pointer' }}>
              <X size={20} />
            </button>
            <h3 style={{ marginBottom: '0.5rem' }}>Electricity Rate</h3>
            <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', marginBottom: '1.5rem' }}>
              Set your tariff in Naira (₦) per kilowatt-hour (kWh). Used to estimate monthly cost.
            </p>
            <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
              <span style={{ fontSize: '1.5rem', color: 'var(--text-muted)' }}>₦</span>
              <input
                type="number"
                className="history-input"
                value={rateInput}
                onChange={e => setRateInput(e.target.value)}
                placeholder="225"
                min="1"
                style={{ flex: 1, fontSize: '1.4rem', fontWeight: '700', minWidth: 0, width: '100%' }}
                autoFocus
              />
              <span style={{ color: 'var(--text-muted)', whiteSpace: 'nowrap' }}>per kWh</span>
            </div>
            <button
              onClick={handleSaveRate}
              className="login-button"
              disabled={saving}
              style={{ width: '100%', padding: '14px', marginTop: '1.5rem', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px' }}
            >
              <Check size={18} /> {saving ? 'Saving...' : 'Save Rate'}
            </button>
          </div>
        </div>
      )}
    </div>
  );
};

export default EnergyStats;
