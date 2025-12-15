import { BrowserRouter, Routes, Route } from 'react-router-dom';
import Dashboard from './pages/Dashboard';
import NovoMotor from './pages/NovoMotor';
import DetalhesMotor from './pages/DetalhesMotor';

function App() {
  return (
    <BrowserRouter>
      <Routes>
        {/* Rota principal: Mostra o Dashboard */}
        <Route path="/" element={<Dashboard />} />  

        {/* Rota de cadastro */}
        <Route path="/novo" element={<NovoMotor />} />

        {/* Rota para o card do motor */}
        <Route path="/motor/:id" element={<DetalhesMotor />} />
      </Routes>
    </BrowserRouter>
  );
}

export default App;