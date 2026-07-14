import React, { useState, useEffect } from 'react';
import mqtt from 'mqtt';
import { 
  Thermometer, Droplets, Activity, Sun, Zap, Power,
  Wifi, WifiOff, BatteryCharging, CalendarDays, Lock
} from 'lucide-react';

import AnalogGauge from './components/AnalogGauge';
import HistoricalGraph from './components/HistoricalGraph';
import HistoryLookup from './components/HistoryLookup';

function App() {
  const MQTT_BROKER = 'wss://broker.hivemq.com:8884/mqtt';
  const MQTT_TOPIC_DATA = 'iot-hub/redphoenix25-v1-x8f9a2/data';
  const MQTT_TOPIC_CMD = 'iot-hub/redphoenix25-v1-x8f9a2/cmd';
  const MQTT_TOPIC_STATUS = 'iot-hub/redphoenix25-v1-x8f9a2/status';

  const [brokerConnected, setBrokerConnected] = useState(false);
  const [deviceConnected, setDeviceConnected] = useState(false);
  const [socket, setSocket] = useState(null);
  
  const [isAuthenticated, setIsAuthenticated] = useState(false);
  const [passwordInput, setPasswordInput] = useState('');
  const [loginError, setLoginError] = useState(false);
  
  const [envData, setEnvData] = useState({
    temperature: null, humidity: null, motion: null,
    lightLevel: null, voltage: null, mainCurrent: null
  });

  const [outlets, setOutlets] = useState([
    { id: 'socket1', name: 'Socket 1', state: false, power: 0, current: 0 },
    { id: 'socket2', name: 'Socket 2', state: false, power: 0, current: 0 },
    { id: 'socket3', name: 'Socket 3', state: false, power: 0, current: 0 },
    { id: 'socket4', name: 'Socket 4', state: false, power: 0, current: 0 }
  ]);

  useEffect(() => {
    const client = mqtt.connect(MQTT_BROKER);
    
    client.on('connect', () => {
      setBrokerConnected(true);
      client.subscribe(MQTT_TOPIC_DATA);
      client.subscribe(MQTT_TOPIC_STATUS);
    });

    client.on('message', (topic, message) => {
      if (topic === MQTT_TOPIC_STATUS) {
        setDeviceConnected(message.toString() === 'online');
      } else if (topic === MQTT_TOPIC_DATA) {
        try {
          const data = JSON.parse(message.toString());
          if (data.env) setEnvData(data.env);
          if (data.outlets) setOutlets(data.outlets);
        } catch (err) {}
      }
    });

    client.on('close', () => {
      setBrokerConnected(false);
      setDeviceConnected(false);
    });
    
    setSocket(client);
    return () => client.end();
  }, []);

  const toggleOutlet = (id, currentState) => {
    setOutlets(outlets.map(o => o.id === id ? { ...o, state: !currentState } : o));
    if (socket && brokerConnected) {
      socket.publish(MQTT_TOPIC_CMD, JSON.stringify({
        action: 'toggle', id: id, state: !currentState
      }));
    }
  };

  const handleLogin = (e) => {
    e.preventDefault();
    if (passwordInput === import.meta.env.VITE_APP_PASSWORD) {
      setIsAuthenticated(true);
      setLoginError(false);
    } else {
      setLoginError(true);
      setPasswordInput('');
    }
  };

  const outletsTotalPower = outlets.reduce((sum, o) => sum + o.power, 0);
  const trueTotalPower = (envData.mainCurrent !== null && envData.voltage !== null) 
    ? envData.mainCurrent * envData.voltage 
    : outletsTotalPower;

  if (!isAuthenticated) {
    return (
      <div className="login-container">
        <div className="glass-panel login-card">
          <div className="login-icon">
            <Lock size={32} />
          </div>
          <div>
            <h1>Command Center</h1>
            <p className="subtitle">Enter master password to access</p>
          </div>
          <form onSubmit={handleLogin} style={{ display: 'flex', flexDirection: 'column', gap: '1rem' }}>
            <input 
              type="password" 
              className="login-input" 
              placeholder="••••••••" 
              value={passwordInput}
              onChange={(e) => setPasswordInput(e.target.value)}
              autoFocus
            />
            {loginError && <div className="login-error">Incorrect password</div>}
            <button type="submit" className="login-button">Unlock</button>
          </form>
        </div>
      </div>
    );
  }

  return (
    <div className="dashboard-container">
      {/* Header */}
      <header className="header">
        <div>
          <h1>Command Center</h1>
          <p className="subtitle">Live Hub Overview</p>
        </div>
        <div className="connection-status">
          <div className={`status-indicator ${deviceConnected ? 'connected' : ''}`}></div>
          {deviceConnected ? (
            <span className="connected-text">Hub Online</span>
          ) : (
            <span>Hub Offline</span>
          )}
        </div>
      </header>

        <div className="grid-hero">
          <div className="glass-panel hero-card">
            <div>
              <p className="hero-label">Total Power Load</p>
              <div className="hero-value-container">
                <span className="hero-value">{trueTotalPower.toFixed(1)}</span>
                <span className="hero-unit">W</span>
              </div>
            </div>
            <div className="hero-secondary">
              <p className="hero-label">Main Voltage</p>
              <div className="hero-value-container" style={{justifyContent: 'flex-end'}}>
                <span className="hero-value" style={{color: 'var(--success-color)'}}>
                  {envData.voltage !== null ? envData.voltage.toFixed(0) : '--'}
                </span>
                <span className="hero-unit">V</span>
              </div>
            </div>
          </div>
        </div>

        {/* Live Wattage Gauge */}
        <div className="grid-overview">
          <AnalogGauge value={trueTotalPower} max={5000} />
        </div>

        {/* Historical Graph */}
        <HistoricalGraph />
        
        {/* Historical Lookup */}
        <HistoryLookup />

      {/* Environment 2x2 Grid */}
      <div className="grid-2col">
        <div className="glass-panel env-card">
          <div className="env-header">
            <div className="env-icon-wrapper" style={{ color: '#ef4444' }}><Thermometer size={20} /></div>
            Temperature
          </div>
          <div className="env-value">{envData.temperature === null ? '--' : `${envData.temperature.toFixed(1)}°`}</div>
        </div>

        <div className="glass-panel env-card">
          <div className="env-header">
            <div className="env-icon-wrapper" style={{ color: '#3b82f6' }}><Droplets size={20} /></div>
            Humidity
          </div>
          <div className="env-value">{envData.humidity === null ? '--' : `${envData.humidity.toFixed(1)}%`}</div>
        </div>

        <div className="glass-panel env-card">
          <div className="env-header">
            <div className="env-icon-wrapper" style={{ color: '#eab308' }}><Sun size={20} /></div>
            Light Level
          </div>
          <div className="env-value">{envData.lightLevel === null ? '--' : `${envData.lightLevel}%`}</div>
        </div>

        <div className="glass-panel env-card">
          <div className="env-header">
            <div className="env-icon-wrapper" style={{ color: envData.motion ? 'var(--success-color)' : 'var(--text-muted)' }}><Activity size={20} /></div>
            Motion
          </div>
          <div className="env-value" style={{ fontSize: '1.4rem' }}>
            {envData.motion === null ? '--' : (envData.motion ? 'Detected' : 'Clear')}
          </div>
        </div>
      </div>

      {/* Devices Section */}
      <div>
        <h2 style={{marginTop: '0.5rem'}}>Device Control</h2>
        <div className="device-list">
          {outlets.map((outlet) => (
            <div key={outlet.id} className={`glass-panel device-card ${outlet.state ? 'active' : ''}`}>
              <div className="device-info">
                <div className="device-icon-box">
                  <Power size={22} color={outlet.state ? 'var(--accent-color)' : 'var(--text-muted)'} />
                </div>
                <div>
                  <div className="device-name">{outlet.name}</div>
                  <div className="device-stats">
                    <span><Zap size={12}/> <span className="stat-highlight">{outlet.power.toFixed(1)}</span> W</span>
                    <span><Activity size={12}/> <span className="stat-highlight">{outlet.current.toFixed(2)}</span> A</span>
                  </div>
                </div>
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
          ))}
        </div>
      </div>

    </div>
  );
}

export default App;
