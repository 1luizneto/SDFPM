import { useEffect, useState } from 'react';
import api from '../services/api';
import MotorCard from '../components/MotorCard'; // Nosso card
import { LayoutDashboard, RefreshCw, Plus } from 'lucide-react';
import { Link } from 'react-router-dom';

function Dashboard() {
    const [motores, setMotores] = useState([]);
    const [loading, setLoading] = useState(true);

    // Função que busca os dados no Django
    const fetchMotores = async () => {
        try {
            const response = await api.get('motores/');
            setMotores(response.data);
        } catch (error) {
            console.error("Erro ao buscar motores:", error);
            alert("Erro ao conectar com o servidor!");
        } finally {
            setLoading(false);
        }
    };

    // Roda assim que a tela abre
    useEffect(() => {
        fetchMotores();

        // Opcional: Atualiza a cada 1 seg automaticamente
        const intervalo = setInterval(fetchMotores, 1000);
        return () => clearInterval(intervalo);
    }, []);

    return (
        // AJUSTE 1: Padding menor no mobile (p-4) e maior no PC (md:p-8)
        <div className="min-h-screen bg-gray-50 p-4 md:p-8">

            {/* AJUSTE 2: flex-col no mobile (empilha) e row no PC. Gap para separar. */}
            <div className="max-w-7xl mx-auto mb-8 flex flex-col md:flex-row justify-between items-center gap-6">

                {/* Título e Ícone */}
                <div className="flex items-center gap-3 w-full md:w-auto">
                    <div className="p-3 bg-blue-600 rounded-lg shadow-blue-200 shrink-0">
                        <LayoutDashboard className="text-white w-6 h-6 md:w-8 md:h-8" />
                    </div>
                    <div>
                        <h1 className="text-2xl md:text-3xl font-bold text-gray-800">Monitoramento</h1>
                        <p className="text-gray-500 text-sm md:text-base">Visão geral em tempo real</p>
                    </div>
                </div>

                {/* Botões */}
                {/* AJUSTE 3: Botões ocupam largura total no mobile para facilitar o toque */}
                <div className="flex gap-3 w-full md:w-auto">

                    <Link to="/novo" className="flex-1 md:flex-none justify-center items-center gap-2 px-4 py-2 bg-green-600 text-white rounded-lg hover:bg-green-700 transition-colors shadow-sm font-medium flex">
                        <Plus size={20} />
                        <span className="hidden md:inline">Novo Motor</span>
                        <span className="md:hidden">Novo</span>
                    </Link>

                    <button
                        onClick={fetchMotores}
                        className="flex-1 md:flex-none justify-center flex items-center gap-2 px-4 py-2 bg-white text-blue-600 border border-blue-200 rounded-lg hover:bg-blue-50 transition-colors shadow-sm"
                    >
                        <RefreshCw size={20} />
                        <span className="hidden md:inline">Atualizar</span>
                        <span className="md:hidden">Atualizar</span>
                    </button>
                </div>
            </div>

            {/* Grid de Motores */}
            {loading ? (
                <div className="flex flex-col items-center justify-center mt-20 text-gray-400 animate-pulse">
                    <RefreshCw className="animate-spin mb-2" size={32} />
                    <p>Buscando motores...</p>
                </div>
            ) : (
                // AJUSTE 4: O grid já era responsivo, mas garanti o gap correto
                <div className="max-w-7xl mx-auto grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
                    {motores.length > 0 ? (
                        motores.map((motor) => (
                            <MotorCard key={motor.id} motor={motor} />
                        ))
                    ) : (
                        <div className="col-span-full text-center py-20 bg-white rounded-xl border border-dashed border-gray-300">
                            <p className="text-gray-500 mb-4">Nenhum motor cadastrado.</p>
                            <Link to="/novo" className="text-blue-600 font-semibold hover:underline">
                                Cadastre o primeiro agora
                            </Link>
                        </div>
                    )}
                </div>
            )}
        </div>
    );
}

export default Dashboard;