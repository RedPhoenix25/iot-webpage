import { initializeApp } from "firebase/app";
import { getDatabase } from "firebase/database";

const firebaseConfig = {
  apiKey: "AIzaSyAZZAiaV8-uhg8ydDt7uZvtiBkhoxG6vMY",
  authDomain: "iot-energy-hub-eea5f.firebaseapp.com",
  databaseURL: "https://iot-energy-hub-eea5f-default-rtdb.firebaseio.com",
  projectId: "iot-energy-hub-eea5f",
  storageBucket: "iot-energy-hub-eea5f.firebasestorage.app",
  messagingSenderId: "582483093182",
  appId: "1:582483093182:web:18801573e3d6ada008e0ef"
};

const app = initializeApp(firebaseConfig);
export const db = getDatabase(app);
