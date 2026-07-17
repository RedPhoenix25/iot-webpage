import React, { useState } from 'react';
import { createPortal } from 'react-dom';
import { ref, get } from 'firebase/database';
import { db } from '../firebase';
import { Search, Calendar, FileDown, X, Activity, Zap, Bolt } from 'lucide-react';

const HistoryLookup = () => {
  const [isOpen, setIsOpen] = useState(false);
  const [activeTab, setActiveTab] = useState('lookup');
  
  // Lookup State
  const [date, setDate] = useState('');
  const [hour, setHour] = useState('12');
  const [result, setResult] = useState(null);
  const [loading, setLoading] = useState(false);

  // Daily Lookup State
  const [dailyDate, setDailyDate] = useState('');
  const [dailyResult, setDailyResult] = useState(null);
  const [loadingDaily, setLoadingDaily] = useState(false);

  // Statement State
  const [startDate, setStartDate] = useState('');
  const [endDate, setEndDate] = useState('');
  const [loadingCSV, setLoadingCSV] = useState(false);
  const [csvProgress, setCsvProgress] = useState('');
  const [toast, setToast] = useState({ show: false, message: '', type: 'info' });

  const showToast = (message, type = 'info') => {
    setToast({ show: true, message, type });
    setTimeout(() => setToast({ show: false, message: '', type: 'info' }), 5000);
  };

  const handleSearch = async () => {
    if (!date) return;
    setLoading(true);
    
    try {
      const [year, month, day] = date.split('-');
      const dbRef = ref(db, `energy_logs/${year}/${parseInt(month, 10)}/${parseInt(day, 10)}/${parseInt(hour, 10)}`);
      
      const snapshot = await get(dbRef);
      if (snapshot.exists()) {
        setResult(snapshot.val());
      } else {
        setResult({ total_wh: 0, s1_wh: 0, s2_wh: 0, s3_wh: 0, s4_wh: 0, v_avg: 0, v_min: 0, v_max: 0 });
      }
    } catch (err) {
      console.error(err);
      showToast("Search failed: " + err.message, 'error');
      setResult(null);
    }
    setLoading(false);
  };

  const handleDailySearch = async () => {
    if (!dailyDate) return;
    setLoadingDaily(true);
    
    try {
      const [year, month, day] = dailyDate.split('-');
      const dbRef = ref(db, `energy_logs/${year}/${parseInt(month, 10)}/${parseInt(day, 10)}/daily_summary`);
      
      const snapshot = await get(dbRef);
      if (snapshot.exists()) {
        setDailyResult(snapshot.val());
      } else {
        setDailyResult({ total_wh: 0, s1_wh: 0, s2_wh: 0, s3_wh: 0, s4_wh: 0, v_avg: 0, v_min: 0, v_max: 0 });
      }
    } catch (err) {
      console.error(err);
      showToast("Daily search failed: " + err.message, 'error');
      setDailyResult(null);
    }
    setLoadingDaily(false);
  };

  const handleDownloadCSV = async () => {
    if (!startDate || !endDate) return;
    setLoadingCSV(true);
    setCsvProgress('Preparing...');
    
    const start = new Date(startDate);
    const end = new Date(endDate);
    end.setHours(23, 59, 59, 999);

    const flatLogs = [];

    // Iterate month by month to avoid downloading the full DB at once
    const cursor = new Date(start.getFullYear(), start.getMonth(), 1);
    while (cursor <= end) {
      const year = cursor.getFullYear();
      const month = cursor.getMonth() + 1;
      setCsvProgress(`Fetching ${cursor.toLocaleString('default', { month: 'long' })} ${year}...`);
      
      try {
        const monthRef = ref(db, `energy_logs/${year}/${month}`);
        const snap = await get(monthRef);
        if (snap.exists()) {
          const days = snap.val();
          for (const day in days) {
            for (const hour in days[day]) {
              const entry = days[day][hour];
              const logDate = new Date(year, month - 1, parseInt(day), parseInt(hour));
              if (logDate >= start && logDate <= end) {
                flatLogs.push({
                  Date: `${year}-${String(month).padStart(2,'0')}-${String(day).padStart(2,'0')}`,
                  Time: `${String(hour).padStart(2,'0')}:00`,
                  Total_kWh: ((entry.total_wh || entry.wh || 0) / 1000).toFixed(4),
                  Socket1_kWh: ((entry.s1_wh || 0) / 1000).toFixed(4),
                  Socket2_kWh: ((entry.s2_wh || 0) / 1000).toFixed(4),
                  Socket3_kWh: ((entry.s3_wh || 0) / 1000).toFixed(4),
                  LightBulb_kWh: ((entry.s4_wh || 0) / 1000).toFixed(4),
                  Voltage_Avg_V: (entry.v_avg || 0).toFixed(2),
                  Voltage_Min_V: (entry.v_min || 0).toFixed(2),
                  Voltage_Max_V: (entry.v_max || 0).toFixed(2),
                });
              }
            }
          }
        }
      } catch (err) {
        console.error(`Error fetching ${year}/${month}:`, err);
      }

      // Advance to next month
      cursor.setMonth(cursor.getMonth() + 1);
    }
    
    if (flatLogs.length === 0) {
      setCsvProgress('');
      setLoadingCSV(false);
      alert('No data found in this date range. The system may not have been logging during this period.');
      return;
    }

    setCsvProgress(`Generating CSV (${flatLogs.length} records)...`);
    
    const headers = Object.keys(flatLogs[0]).join(',');
    const rows = flatLogs
      .sort((a, b) => `${a.Date}${a.Time}`.localeCompare(`${b.Date}${b.Time}`))
      .map(log => Object.values(log).join(','));
    const csvContent = 'data:text/csv;charset=utf-8,' + [headers, ...rows].join('\n');
    
    const link = document.createElement('a');
    link.setAttribute('href', encodeURI(csvContent));
    link.setAttribute('download', `iot_energy_statement_${startDate}_to_${endDate}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    
    setCsvProgress('');
    setLoadingCSV(false);
  };

  return (
    <>
      <div style={{ marginTop: '1.5rem', paddingTop: '1.5rem', borderTop: '1px solid rgba(255,255,255,0.1)', display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexWrap: 'wrap', gap: '10px' }}>
        <h4 style={{ color: 'var(--text-muted)', display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Calendar size={16} /> Advanced Analytics
        </h4>
        <button 
          onClick={() => setIsOpen(true)}
          className="login-button"
          style={{ width: 'auto', padding: '10px 20px', margin: 0, background: 'rgba(255,255,255,0.1)', color: 'white' }}
        >
          Open Detailed Reports
        </button>
      </div>

      {isOpen && createPortal(
        <div style={{ position: 'fixed', top: 0, left: 0, right: 0, bottom: 0, background: 'rgba(0,0,0,0.85)', zIndex: 999, display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '1rem' }}>
          <div className="glass-panel" style={{ width: '100%', maxWidth: '700px', maxHeight: '90vh', overflowY: 'auto', position: 'relative', padding: '2rem' }}>
            <button onClick={() => setIsOpen(false)} style={{ position: 'absolute', top: '20px', right: '20px', background: 'none', border: 'none', color: 'var(--text-muted)', cursor: 'pointer' }}><X size={24} /></button>
            
            <h2 style={{ marginBottom: '1.5rem' }}>Historical Reports</h2>
            
            <div style={{ display: 'flex', gap: '10px', marginBottom: '2rem', borderBottom: '1px solid rgba(255,255,255,0.1)', paddingBottom: '10px', flexWrap: 'wrap' }}>
              <button 
                onClick={() => setActiveTab('lookup')}
                style={{ background: 'none', border: 'none', color: activeTab === 'lookup' ? 'var(--accent-color)' : 'var(--text-muted)', fontSize: '1.1rem', fontWeight: 'bold', cursor: 'pointer' }}
              >Specific Hour Lookup</button>
              <button 
                onClick={() => setActiveTab('dailyLookup')}
                style={{ background: 'none', border: 'none', color: activeTab === 'dailyLookup' ? 'var(--accent-color)' : 'var(--text-muted)', fontSize: '1.1rem', fontWeight: 'bold', cursor: 'pointer' }}
              >Specific Day Lookup</button>
              <button 
                onClick={() => setActiveTab('statement')}
                style={{ background: 'none', border: 'none', color: activeTab === 'statement' ? 'var(--accent-color)' : 'var(--text-muted)', fontSize: '1.1rem', fontWeight: 'bold', cursor: 'pointer' }}
              >Download Statement</button>
            </div>

            {activeTab === 'lookup' && (
              <div>
                <p style={{ color: 'var(--text-muted)', marginBottom: '1rem' }}>Check precise power draw and voltage stats for any given hour.</p>
                <div style={{ display: 'flex', gap: '1rem', alignItems: 'center', flexWrap: 'wrap', marginBottom: '2rem' }}>
                  <input type="date" value={date} onChange={(e) => setDate(e.target.value)} className="history-input" />
                  <select value={hour} onChange={(e) => setHour(e.target.value)} className="history-select">
                    {[...Array(24)].map((_, i) => ( <option key={i} value={i}>{i}:00 - {i+1}:00</option> ))}
                  </select>
                  <button onClick={handleSearch} className="login-button" style={{ width: 'auto', padding: '12px 24px', margin: 0, display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <Search size={18} /> {loading ? 'Searching...' : 'Search'}
                  </button>
                </div>

                {result && (
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '1rem' }}>
                    <div style={{ background: 'rgba(0, 229, 255, 0.05)', padding: '1.5rem', borderRadius: '12px', border: '1px solid rgba(0, 229, 255, 0.2)' }}>
                      <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}><Bolt size={14} style={{display:'inline'}}/> Total System</p>
                      <h3 style={{ fontSize: '2rem', margin: 0, color: 'var(--accent-color)' }}>{((result.total_wh || result.wh || 0) / 1000).toFixed(3)} <span style={{fontSize:'1rem'}}>kWh</span></h3>
                    </div>
                    <div style={{ background: 'rgba(255,255,255,0.02)', padding: '1.5rem', borderRadius: '12px', border: '1px solid rgba(255,255,255,0.05)' }}>
                      <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}>Socket 1</p>
                      <h3 style={{ fontSize: '1.5rem', margin: 0 }}>{((result.s1_wh || 0) / 1000).toFixed(3)} <span style={{fontSize:'1rem'}}>kWh</span></h3>
                    </div>
                    <div style={{ background: 'rgba(255,255,255,0.02)', padding: '1.5rem', borderRadius: '12px', border: '1px solid rgba(255,255,255,0.05)' }}>
                      <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}>Socket 2</p>
                      <h3 style={{ fontSize: '1.5rem', margin: 0 }}>{((result.s2_wh || 0) / 1000).toFixed(3)} <span style={{fontSize:'1rem'}}>kWh</span></h3>
                    </div>
                    <div style={{ background: 'rgba(255,255,255,0.02)', padding: '1.5rem', borderRadius: '12px', border: '1px solid rgba(255,255,255,0.05)' }}>
                      <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}>Socket 3</p>
                      <h3 style={{ fontSize: '1.5rem', margin: 0 }}>{((result.s3_wh || 0) / 1000).toFixed(3)} <span style={{fontSize:'1rem'}}>kWh</span></h3>
                    </div>
                    <div style={{ background: 'rgba(255,255,255,0.02)', padding: '1.5rem', borderRadius: '12px', border: '1px solid rgba(255,255,255,0.05)' }}>
                      <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}>Light Bulb</p>
                      <h3 style={{ fontSize: '1.5rem', margin: 0 }}>{((result.s4_wh || 0) / 1000).toFixed(3)} <span style={{fontSize:'1rem'}}>kWh</span></h3>
                    </div>
                    <div style={{ background: 'rgba(16, 185, 129, 0.05)', padding: '1.5rem', borderRadius: '12px', border: '1px solid rgba(16, 185, 129, 0.2)', gridColumn: '1 / -1', display: 'flex', justifyContent: 'space-between' }}>
                      <div>
                        <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}><Activity size={14} style={{display:'inline'}}/> Avg Voltage</p>
                        <h3 style={{ fontSize: '1.5rem', margin: 0, color: 'var(--success-color)' }}>{result.v_avg || 0} V</h3>
                      </div>
                      <div>
                        <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}>Min Voltage</p>
                        <h3 style={{ fontSize: '1.5rem', margin: 0, color: 'var(--success-color)' }}>{result.v_min || 0} V</h3>
                      </div>
                      <div>
                        <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}>Peak Voltage</p>
                        <h3 style={{ fontSize: '1.5rem', margin: 0, color: 'var(--success-color)' }}>{result.v_max || 0} V</h3>
                      </div>
                    </div>
                  </div>
                )}
              </div>
            )}

            {activeTab === 'dailyLookup' && (
              <div>
                <p style={{ color: 'var(--text-muted)', marginBottom: '1rem' }}>Check precise power draw and voltage stats for an entire day.</p>
                <div style={{ display: 'flex', gap: '1rem', alignItems: 'center', flexWrap: 'wrap', marginBottom: '2rem' }}>
                  <input type="date" value={dailyDate} onChange={(e) => setDailyDate(e.target.value)} className="history-input" />
                  <button onClick={handleDailySearch} className="login-button" style={{ width: 'auto', padding: '12px 24px', margin: 0, display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <Search size={18} /> {loadingDaily ? 'Searching...' : 'Search'}
                  </button>
                </div>

                {dailyResult && (
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '1rem' }}>
                    <div style={{ background: 'rgba(0, 229, 255, 0.05)', padding: '1.5rem', borderRadius: '12px', border: '1px solid rgba(0, 229, 255, 0.2)' }}>
                      <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}><Bolt size={14} style={{display:'inline'}}/> Total System</p>
                      <h3 style={{ fontSize: '2rem', margin: 0, color: 'var(--accent-color)' }}>{((dailyResult.total_wh || dailyResult.wh || 0) / 1000).toFixed(3)} <span style={{fontSize:'1rem'}}>kWh</span></h3>
                    </div>
                    <div style={{ background: 'rgba(255,255,255,0.02)', padding: '1.5rem', borderRadius: '12px', border: '1px solid rgba(255,255,255,0.05)' }}>
                      <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}>Socket 1</p>
                      <h3 style={{ fontSize: '1.5rem', margin: 0 }}>{((dailyResult.s1_wh || 0) / 1000).toFixed(3)} <span style={{fontSize:'1rem'}}>kWh</span></h3>
                    </div>
                    <div style={{ background: 'rgba(255,255,255,0.02)', padding: '1.5rem', borderRadius: '12px', border: '1px solid rgba(255,255,255,0.05)' }}>
                      <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}>Socket 2</p>
                      <h3 style={{ fontSize: '1.5rem', margin: 0 }}>{((dailyResult.s2_wh || 0) / 1000).toFixed(3)} <span style={{fontSize:'1rem'}}>kWh</span></h3>
                    </div>
                    <div style={{ background: 'rgba(255,255,255,0.02)', padding: '1.5rem', borderRadius: '12px', border: '1px solid rgba(255,255,255,0.05)' }}>
                      <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}>Socket 3</p>
                      <h3 style={{ fontSize: '1.5rem', margin: 0 }}>{((dailyResult.s3_wh || 0) / 1000).toFixed(3)} <span style={{fontSize:'1rem'}}>kWh</span></h3>
                    </div>
                    <div style={{ background: 'rgba(255,255,255,0.02)', padding: '1.5rem', borderRadius: '12px', border: '1px solid rgba(255,255,255,0.05)' }}>
                      <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}>Light Bulb</p>
                      <h3 style={{ fontSize: '1.5rem', margin: 0 }}>{((dailyResult.s4_wh || 0) / 1000).toFixed(3)} <span style={{fontSize:'1rem'}}>kWh</span></h3>
                    </div>
                    <div style={{ background: 'rgba(16, 185, 129, 0.05)', padding: '1.5rem', borderRadius: '12px', border: '1px solid rgba(16, 185, 129, 0.2)', gridColumn: '1 / -1', display: 'flex', justifyContent: 'space-between' }}>
                      <div>
                        <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}><Activity size={14} style={{display:'inline'}}/> Avg Voltage</p>
                        <h3 style={{ fontSize: '1.5rem', margin: 0, color: 'var(--success-color)' }}>{dailyResult.v_avg || 0} V</h3>
                      </div>
                      <div>
                        <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}>Min Voltage</p>
                        <h3 style={{ fontSize: '1.5rem', margin: 0, color: 'var(--success-color)' }}>{dailyResult.v_min || 0} V</h3>
                      </div>
                      <div>
                        <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem', textTransform: 'uppercase', marginBottom: '8px' }}>Peak Voltage</p>
                        <h3 style={{ fontSize: '1.5rem', margin: 0, color: 'var(--success-color)' }}>{dailyResult.v_max || 0} V</h3>
                      </div>
                    </div>
                  </div>
                )}
              </div>
            )}

            {activeTab === 'statement' && (
              <div>
                <p style={{ color: 'var(--text-muted)', marginBottom: '1rem' }}>Generate a complete CSV bank-statement style spreadsheet of your energy usage across a given timeframe.</p>
                <div style={{ display: 'flex', gap: '1rem', alignItems: 'center', flexWrap: 'wrap', marginBottom: '2rem' }}>
                  <div style={{ display: 'flex', flexDirection: 'column' }}>
                    <label style={{ fontSize: '0.8rem', color: 'var(--text-muted)', marginBottom: '4px' }}>From</label>
                    <input type="date" value={startDate} onChange={(e) => setStartDate(e.target.value)} className="history-input" />
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column' }}>
                    <label style={{ fontSize: '0.8rem', color: 'var(--text-muted)', marginBottom: '4px' }}>To</label>
                    <input type="date" value={endDate} onChange={(e) => setEndDate(e.target.value)} className="history-input" />
                  </div>
                </div>
                <button 
                  onClick={handleDownloadCSV} 
                  className="login-button" 
                  style={{ width: '100%', padding: '16px', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px', fontSize: '1.1rem' }}
                  disabled={loadingCSV}
                >
                  <FileDown size={20} /> {loadingCSV ? (csvProgress || 'Starting...') : 'Download CSV Statement'}
                </button>
              </div>
            )}
          </div>
        </div>,
        document.body
      )}

      {toast.show && createPortal(
        <div style={{
          position: 'fixed', bottom: '30px', left: '50%', transform: 'translateX(-50%)',
          background: toast.type === 'error' ? 'rgba(239, 68, 68, 0.95)' : 'rgba(16, 185, 129, 0.95)',
          color: 'white', padding: '12px 24px', borderRadius: '30px',
          boxShadow: '0 4px 20px rgba(0,0,0,0.5)', zIndex: 10000,
          display: 'flex', alignItems: 'center', gap: '10px',
          fontWeight: '600', backdropFilter: 'blur(10px)', border: '1px solid rgba(255,255,255,0.1)'
        }}>
          {toast.message}
        </div>,
        document.body
      )}
    </>
  );
};

export default HistoryLookup;
