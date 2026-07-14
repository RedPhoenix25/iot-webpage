import React, { useState, useEffect } from 'react';
import { AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { ref, onValue } from 'firebase/database';
import { db } from '../firebase';
import HistoryLookup from './HistoryLookup';

const HistoricalGraph = () => {
  const [timeRange, setTimeRange] = useState('hourly'); // 'hourly', 'daily', 'weekly', 'monthly'
  const [data, setData] = useState([]);

  useEffect(() => {
    const logsRef = ref(db, 'energy_logs');
    const unsubscribe = onValue(logsRef, (snapshot) => {
      const val = snapshot.val();
      if (!val) {
        setData([]);
        return;
      }
      
      const parsedData = processGraphData(val, timeRange);
      setData(parsedData);
    });

    return () => unsubscribe();
  }, [timeRange]);

  const processGraphData = (rawData, range) => {
    const now = new Date();
    let result = [];
    
    // Flatten YYYY/MM/DD/HH into array
    const flatLogs = [];
    for (const year in rawData) {
      for (const month in rawData[year]) {
        for (const day in rawData[year][month]) {
          for (const hour in rawData[year][month][day]) {
            const entry = rawData[year][month][day][hour];
            flatLogs.push({
              date: new Date(year, month - 1, day, hour),
              wh: entry.wh || 0
            });
          }
        }
      }
    }

    if (range === 'hourly') {
      const todayLogs = flatLogs.filter(l => 
        l.date.getDate() === now.getDate() && 
        l.date.getMonth() === now.getMonth() && 
        l.date.getFullYear() === now.getFullYear()
      );
      
      for (let i = 0; i < 24; i++) {
        const hourLogs = todayLogs.filter(l => l.date.getHours() === i);
        const totalWh = hourLogs.reduce((sum, l) => sum + l.wh, 0);
        result.push({ name: `${i}:00`, wh: totalWh });
      }
    } 
    else if (range === 'daily') {
      const monthLogs = flatLogs.filter(l => 
        l.date.getMonth() === now.getMonth() && 
        l.date.getFullYear() === now.getFullYear()
      );
      
      const daysInMonth = new Date(now.getFullYear(), now.getMonth() + 1, 0).getDate();
      for (let i = 1; i <= daysInMonth; i++) {
        const dayLogs = monthLogs.filter(l => l.date.getDate() === i);
        const totalWh = dayLogs.reduce((sum, l) => sum + l.wh, 0);
        result.push({ name: `Day ${i}`, wh: totalWh });
      }
    }
    else if (range === 'weekly') {
      // Last 4 weeks calculation based on today
      for(let i = 4; i >= 1; i--) {
        const start = new Date(now.getTime() - i * 7 * 24 * 60 * 60 * 1000);
        const end = new Date(now.getTime() - (i-1) * 7 * 24 * 60 * 60 * 1000);
        const weekLogs = flatLogs.filter(l => l.date >= start && l.date < end);
        const totalWh = weekLogs.reduce((sum, l) => sum + l.wh, 0);
        result.push({ name: `Week -${i}`, wh: totalWh });
      }
    }
    else if (range === 'monthly') {
      const yearLogs = flatLogs.filter(l => l.date.getFullYear() === now.getFullYear());
      const months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
      
      for (let i = 0; i < 12; i++) {
        const mLogs = yearLogs.filter(l => l.date.getMonth() === i);
        const totalWh = mLogs.reduce((sum, l) => sum + l.wh, 0);
        result.push({ name: months[i], wh: totalWh });
      }
    }

    return result;
  };

  return (
    <div className="glass-panel" style={{ marginTop: '2rem', gridColumn: '1 / -1' }}>
      <div className="graph-header" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '1.5rem', flexWrap: 'wrap', gap: '10px' }}>
        <h3>Energy Consumption (Wh)</h3>
        <div style={{ display: 'flex', gap: '0.5rem', flexWrap: 'wrap' }}>
          {['hourly', 'daily', 'weekly', 'monthly'].map(range => (
            <button 
              key={range}
              onClick={() => setTimeRange(range)}
              style={{
                background: timeRange === range ? 'var(--accent-color)' : 'rgba(255,255,255,0.1)',
                color: timeRange === range ? '#000' : 'var(--text-main)',
                border: 'none', padding: '6px 12px', borderRadius: '8px',
                cursor: 'pointer', fontFamily: 'Inter', textTransform: 'capitalize', fontWeight: '600'
              }}
            >
              {range}
            </button>
          ))}
        </div>
      </div>
      
      <div style={{ width: '100%', height: 300 }}>
        <ResponsiveContainer>
          <AreaChart data={data}>
            <defs>
              <linearGradient id="colorWh" x1="0" y1="0" x2="0" y2="1">
                <stop offset="5%" stopColor="var(--accent-color)" stopOpacity={0.5}/>
                <stop offset="95%" stopColor="var(--accent-color)" stopOpacity={0}/>
              </linearGradient>
            </defs>
            <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.05)" vertical={false} />
            <XAxis dataKey="name" stroke="var(--text-muted)" tick={{fill: 'var(--text-muted)'}} axisLine={false} tickLine={false} />
            <YAxis stroke="var(--text-muted)" tick={{fill: 'var(--text-muted)'}} axisLine={false} tickLine={false} />
            <Tooltip 
              contentStyle={{ background: '#090E17', border: '1px solid rgba(255,255,255,0.1)', borderRadius: '8px', color: 'white' }}
              itemStyle={{ color: 'var(--accent-color)' }}
            />
            <Area type="monotone" dataKey="wh" stroke="var(--accent-color)" strokeWidth={3} fillOpacity={1} fill="url(#colorWh)" />
          </AreaChart>
        </ResponsiveContainer>
      </div>

      {/* Embedded Historical Lookup */}
      <HistoryLookup />
    </div>
  );
};

export default HistoricalGraph;
