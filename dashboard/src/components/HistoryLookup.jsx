import React, { useState } from 'react';
import { ref, get } from 'firebase/database';
import { db } from '../firebase';
import { Search } from 'lucide-react';

const HistoryLookup = () => {
  const [date, setDate] = useState('');
  const [hour, setHour] = useState('12');
  const [result, setResult] = useState(null);
  const [loading, setLoading] = useState(false);

  const handleSearch = async () => {
    if (!date) return;
    setLoading(true);
    
    const [year, month, day] = date.split('-');
    
    // Path: energy_logs/YYYY/MM/DD/HH
    const dbRef = ref(db, `energy_logs/${year}/${parseInt(month)}/${parseInt(day)}/${parseInt(hour)}`);
    
    try {
      const snapshot = await get(dbRef);
      if (snapshot.exists()) {
        setResult(snapshot.val().wh);
      } else {
        setResult(0);
      }
    } catch (err) {
      console.error(err);
      setResult('Error');
    }
    
    setLoading(false);
  };

  return (
    <div className="glass-panel" style={{ marginTop: '2rem', gridColumn: '1 / -1' }}>
      <h3>Historical Lookup</h3>
      <p className="subtitle" style={{ marginBottom: '1rem' }}>Check power usage for a specific hour on any day.</p>
      
      <div style={{ display: 'flex', gap: '1rem', alignItems: 'center', flexWrap: 'wrap' }}>
        <input 
          type="date" 
          value={date} 
          onChange={(e) => setDate(e.target.value)} 
          className="history-input"
        />
        
        <select 
          value={hour} 
          onChange={(e) => setHour(e.target.value)}
          className="history-select"
        >
          {[...Array(24)].map((_, i) => (
            <option key={i} value={i}>{i}:00 - {i+1}:00</option>
          ))}
        </select>
        
        <button 
          onClick={handleSearch}
          className="login-button"
          style={{ width: 'auto', padding: '12px 24px', display: 'flex', alignItems: 'center', gap: '8px', margin: 0 }}
        >
          <Search size={18} /> {loading ? 'Searching...' : 'Search'}
        </button>
        
        {result !== null && (
          <div style={{ marginLeft: 'auto', background: 'rgba(0, 229, 255, 0.1)', padding: '12px 24px', borderRadius: '8px', border: '1px solid var(--accent-color)' }}>
            <span style={{ color: 'var(--text-main)', marginRight: '8px' }}>Usage: </span>
            <span style={{ fontWeight: '800', color: 'var(--accent-color)', fontSize: '1.2rem', fontFamily: "'Outfit', sans-serif" }}>{result} Wh</span>
          </div>
        )}
      </div>
    </div>
  );
};

export default HistoryLookup;
