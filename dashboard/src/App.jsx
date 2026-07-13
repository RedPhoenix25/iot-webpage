import React, { useState, useEffect } from 'react';
import { 
  Thermometer, 
  Droplets, 
  Activity, 
  Sun, 
  Zap, 
  Power,
  Wifi,
  WifiOff,
  BatteryCharging,
  CalendarDays
} from 'lucide-react';

const WEBSOCKET_URL = 'ws://192.168.137.240:81'; // Update this to the ESP32 IP address

function App() {
  const [connected, setConnected] = useState(false);
  const [socket, setSocket] = useState(null);
  
  // Default State
  const [envData, setEnvData] = useState({
    temperature: null,
    humidity: null,
    motion: null,
    lightLevel: null,
    voltage: null
  });

  const [energyData, setEnergyData] = useState({
    dailyWh: 0,
    weeklyWh: 0
  });

  const [outlets, setOutlets] = useState([
    { id: 'socket1', name: 'Socket 1', state: false, power: 0, current: 0 },
    { id: 'socket2', name: 'Socket 2', state: false, power: 0, current: 0 },
    { id: 'socket3', name: 'Socket 3', state: false, power: 0, current: 0 },
    { id: 'bulb', name: 'Smart Bulb', state: false, power: 0, current: 0 },
  ]);

  useEffect(() => {
    // Initialize WebSocket
    const ws = new WebSocket(WEBSOCKET_URL);
    
    ws.onopen = () => {
      console.log('Connected to WebSocket');
      setConnected(true);
    };
    
    ws.onclose = () => {
      console.log('Disconnected from WebSocket');
      setConnected(false);
      // Attempt reconnect logic can be added here
    };
    
    ws.onerror = (error) => {
      console.error('WebSocket Error: ', error);
    };
    
    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.env) setEnvData(data.env);
        if (data.energy) setEnergyData(data.energy);
        if (data.outlets) setOutlets(data.outlets);
      } catch (err) {
        console.error('Error parsing JSON:', err);
      }
    };
    
    setSocket(ws);
    
    return () => {
      ws.close();
    };
  }, []);

  const toggleOutlet = (id, currentState) => {
    // Optimistic UI update
    setOutlets(outlets.map(o => o.id === id ? { ...o, state: !currentState } : o));
    
    // Send to ESP32
    if (socket && socket.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify({
        action: 'toggle',
        id: id,
        state: !currentState
      }));
    }
  };

  const totalPower = outlets.reduce((sum, o) => sum + o.power, 0);

  return (
    <div className="dashboard-container">
      <header className="header">
        <div>
          <h1>IoT Energy Hub</h1>
          <p style={{ color: 'var(--text-muted)' }}>Real-time Monitoring & Management</p>
        </div>
        
        <div className="connection-status">
          <div className={`status-indicator ${connected ? 'connected' : ''}`}></div>
          {connected ? (
            <><Wifi size={16} /> <span>Connected</span></>
          ) : (
            <><WifiOff size={16} /> <span>Offline</span></>
          )}
        </div>
      </header>

      {/* Energy Reports Dashboard */}
      <section className="grid-env" style={{ marginBottom: '1.5rem' }}>
        <div className="glass-panel env-card" style={{ background: 'rgba(0, 230, 118, 0.05)', borderColor: 'rgba(0, 230, 118, 0.2)' }}>
          <div className="env-icon" style={{ color: 'var(--success-color)' }}>
            <BatteryCharging size={24} />
          </div>
          <div className="env-data">
            <h3>Daily Energy Consumed</h3>
            <p>{(energyData.dailyWh / 1000).toFixed(2)} kWh</p>
          </div>
        </div>

        <div className="glass-panel env-card" style={{ background: 'rgba(138, 43, 226, 0.05)', borderColor: 'rgba(138, 43, 226, 0.2)' }}>
          <div className="env-icon" style={{ color: '#8a2be2' }}>
            <CalendarDays size={24} />
          </div>
          <div className="env-data">
            <h3>Weekly Energy Consumed</h3>
            <p>{(energyData.weeklyWh / 1000).toFixed(2)} kWh</p>
          </div>
        </div>
      </section>

      {/* Environmental Dashboard */}
      <section className="grid-env">
        <div className="glass-panel env-card">
          <div className="env-icon" style={{ color: '#ff6b6b' }}>
            <Thermometer size={24} />
          </div>
          <div className="env-data">
            <h3>Temperature</h3>
            <p>{envData.temperature === null ? <span style={{ color: 'var(--text-muted)' }}>Offline</span> : `${envData.temperature.toFixed(1)}°C`}</p>
          </div>
        </div>

        <div className="glass-panel env-card">
          <div className="env-icon" style={{ color: '#4dabf7' }}>
            <Droplets size={24} />
          </div>
          <div className="env-data">
            <h3>Humidity</h3>
            <p>{envData.humidity === null ? <span style={{ color: 'var(--text-muted)' }}>Offline</span> : `${envData.humidity.toFixed(1)}%`}</p>
          </div>
        </div>

        <div className="glass-panel env-card">
          <div className="env-icon" style={{ color: '#fcc419' }}>
            <Sun size={24} />
          </div>
          <div className="env-data">
            <h3>Light Level</h3>
            <p>{envData.lightLevel === null ? <span style={{ color: 'var(--text-muted)' }}>Offline</span> : `${envData.lightLevel}%`}</p>
          </div>
        </div>

        <div className="glass-panel env-card">
          <div className="env-icon" style={{ color: envData.motion ? '#51cf66' : '#868e96' }}>
            <Activity size={24} />
          </div>
          <div className="env-data">
            <h3>Motion</h3>
            <p>{envData.motion === null ? <span style={{ color: 'var(--text-muted)' }}>Offline</span> : (envData.motion ? 'Detected' : 'Clear')}</p>
          </div>
        </div>

        <div className="glass-panel env-card" style={{ gridColumn: '1 / -1', background: 'rgba(0, 229, 255, 0.05)', borderColor: 'rgba(0, 229, 255, 0.2)' }}>
          <div className="env-icon" style={{ color: 'var(--accent-color)' }}>
            <Zap size={24} />
          </div>
          <div className="env-data power-summary">
            <div>
              <h3>Main Voltage</h3>
              <p>{envData.voltage === null ? <span style={{ color: 'var(--text-muted)' }}>Offline</span> : `${envData.voltage.toFixed(1)} V`}</p>
            </div>
            <div>
              <h3>Total Power Load</h3>
              <p>{totalPower.toFixed(1)} W</p>
            </div>
          </div>
        </div>
      </section>

      {/* Outlet Control */}
      <h2>Power Outlets</h2>
      <p style={{ color: 'var(--text-muted)', marginBottom: '1.5rem' }}>Control your sockets and monitor individual power draw.</p>
      
      <section className="grid-outlets">
        {outlets.map((outlet) => (
          <div key={outlet.id} className="glass-panel">
            <div className="outlet-header">
              <div className="outlet-title">
                <Power size={20} color={outlet.state ? 'var(--accent-color)' : 'var(--text-muted)'} />
                <h3 style={{ margin: 0, fontSize: '1.2rem' }}>{outlet.name}</h3>
              </div>
              <label className="toggle-switch">
                <input 
                  type="checkbox" 
                  checked={outlet.state}
                  onChange={() => toggleOutlet(outlet.id, outlet.state)}
                />
                <span className="slider"></span>
              </label>
            </div>
            
            <div className="outlet-stats">
              <div className="stat">
                <div className="stat-label">Status</div>
                <div className="stat-value" style={{ color: outlet.state ? 'var(--success-color)' : 'var(--text-muted)' }}>
                  {outlet.state ? 'ON' : 'OFF'}
                </div>
              </div>
              <div className="stat">
                <div className="stat-label">Power</div>
                <div className="stat-value">{outlet.power.toFixed(1)} W</div>
              </div>
              <div className="stat">
                <div className="stat-label">Current</div>
                <div className="stat-value">{outlet.current.toFixed(2)} A</div>
              </div>
            </div>
          </div>
        ))}
      </section>
    </div>
  );
}

export default App;
