import { useEffect, useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import api from '../services/api';
import { ArrowLeft, Activity, Trash2 } from 'lucide-react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from 'recharts';
import { format } from 'date-fns';

const DetalhesMotor = () => {
    const { id } = useParams();
    const navigate = useNavigate();
    const [motor, setMotor] = useState(null);
    const [loading, setLoading] = useState(true);

    // --- NOVA FUNÇÃO DE MAPEAMENTO DE STATUS ---
    const getStatusConfig = (codigo) => {
        // Se não tiver leitura ainda
        if (codigo === undefined || codigo === null) {
            return { label: 'SEM DADOS', style: 'bg-gray-100 text-gray-500' };
        }

        switch (Number(codigo)) {
            case 0:
                return { label: 'DESLIGADO', style: 'bg-gray-200 text-gray-600' };
            case 1:
                return { label: 'ALERTA (FALHA 1)', style: 'bg-yellow-100 text-yellow-800' };
            case 2:
                return { label: 'PERIGO (FALHA 2)', style: 'bg-orange-100 text-orange-800' };
            case 3:
                return { label: 'CRÍTICO (FALHA 3)', style: 'bg-red-100 text-red-800' };
            case 4:
                return { label: 'NORMAL', style: 'bg-green-100 text-green-800' };
            default:
                return { label: `STATUS ${codigo}`, style: 'bg-blue-100 text-blue-800' };
        }
    };

    useEffect(() => {
        const fetchDados = async () => {
            try {
                const response = await api.get(`motores/${id}/`);
                setMotor(response.data);
            } catch (error) {
                console.error("Erro ao buscar detalhes:", error);
                if (error.response && error.response.status === 404) {
                    alert("Motor não encontrado!");
                    navigate('/');
                }
            } finally {
                setLoading(false);
            }
        };

        fetchDados();
        const intervalo = setInterval(fetchDados, 1000);
        return () => clearInterval(intervalo);
    }, [id, navigate]);

    const handleDelete = async () => {
        const confirmacao = window.confirm("Tem certeza que deseja excluir este motor?");
        if (confirmacao) {
            try {
                await api.delete(`motores/${id}/`);
                alert("Motor excluído com sucesso!");
                navigate('/');
            } catch (error) {
                console.error("Erro ao deletar:", error);
                alert("Erro ao tentar excluir o motor.");
            }
        }
    };

    if (loading) return <p className="text-center mt-10">Carregando dados...</p>;
    if (!motor) return null;

    const dadosGrafico = motor.leituras ? motor.leituras.slice(0, 20).reverse().map(leitura => ({
        horario: format(new Date(leitura.data_leitura), 'HH:mm:ss'),
        rpm: leitura.rpm,
        eixo_x: leitura.eixo_x,
        eixo_z: leitura.eixo_z
    })) : [];

    // Pega o status da leitura mais recente
    const statusAtualCodigo = motor.leituras && motor.leituras.length > 0 ? motor.leituras[0].em_falha : null;
    const statusConfig = getStatusConfig(statusAtualCodigo);

    return (
        <div className="min-h-screen bg-gray-50 p-4 md:p-8">
            <div className="max-w-7xl mx-auto">

                {/* Cabeçalho */}
                <div className="flex flex-col md:flex-row md:items-center gap-4 mb-8 justify-between">
                    <div className="flex items-center gap-4">
                        <button onClick={() => navigate('/')} className="p-2 bg-white rounded-full shadow hover:bg-gray-100 transition">
                            <ArrowLeft className="text-gray-600" />
                        </button>
                        <div>
                            <h1 className="text-2xl md:text-3xl font-bold text-gray-800">{motor.nome}</h1>
                            <p className="text-gray-500 font-mono text-sm">{motor.uid_hardware} - {motor.localizacao}</p>
                        </div>
                    </div>

                    <div className="flex items-center gap-3 mt-2 md:mt-0">
                        
                        {/* AQUI ESTÁ A MUDANÇA PRINCIPAL: A BADGE DE STATUS DINÂMICA */}
                        <div className={`px-4 py-1 rounded-full text-sm font-bold ${statusConfig.style}`}>
                            {statusConfig.label}
                        </div>

                        <button onClick={handleDelete} className="p-2 bg-red-100 text-red-600 rounded-lg hover:bg-red-200 transition">
                            <Trash2 size={20} />
                        </button>
                    </div>
                </div>

                {/* Gráficos */}
                <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">

                    {/* Gráfico 1 - Vibração */}
                    <div className="bg-white p-4 md:p-6 rounded-xl shadow-lg border border-gray-100 min-w-0">
                        <h3 className="text-lg font-bold text-gray-700 mb-4 flex items-center gap-2">
                            <Activity size={20} className="text-purple-600" />
                            Vibração
                        </h3>
                        <div style={{ width: '100%', height: 300 }}>
                            <ResponsiveContainer width="100%" height="100%">
                                <LineChart data={dadosGrafico} margin={{ top: 5, right: 20, bottom: 5, left: 0 }}>
                                    <CartesianGrid strokeDasharray="3 3" />
                                    <XAxis dataKey="horario" tick={{ fontSize: 10 }} />
                                    <YAxis width={30} tick={{ fontSize: 10 }} />
                                    <Tooltip />
                                    <Legend wrapperStyle={{ fontSize: '12px' }} />
                                    <Line type="monotone" dataKey="eixo_x" stroke="#8884d8" dot={false} strokeWidth={2} />
                                    <Line type="monotone" dataKey="eixo_z" stroke="#82ca9d" dot={false} strokeWidth={2} />
                                </LineChart>
                            </ResponsiveContainer>
                        </div>
                    </div>

                    {/* Gráfico 2 - RPM */}
                    <div className="bg-white p-4 md:p-6 rounded-xl shadow-lg border border-gray-100 min-w-0">
                        <h3 className="text-lg font-bold text-gray-700 mb-4 flex items-center gap-2">
                            <Activity size={20} className="text-blue-600" />
                            Rotação (RPM)
                        </h3>
                        <div style={{ width: '100%', height: 300 }}>
                            <ResponsiveContainer width="100%" height="100%">
                                <LineChart data={dadosGrafico} margin={{ top: 5, right: 20, bottom: 5, left: 0 }}>
                                    <CartesianGrid strokeDasharray="3 3" />
                                    <XAxis dataKey="horario" tick={{ fontSize: 10 }} />
                                    <YAxis width={40} tick={{ fontSize: 10 }} />
                                    <Tooltip />
                                    <Legend wrapperStyle={{ fontSize: '12px' }} />
                                    <Line type="monotone" dataKey="rpm" stroke="#2563eb" dot={false} strokeWidth={2} />
                                </LineChart>
                            </ResponsiveContainer>
                        </div>
                    </div>

                </div>
            </div>
        </div>
    );
};

export default DetalhesMotor;