import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import api from '../services/api';
import { Save, ArrowLeft } from 'lucide-react';

const NovoMotor = () => 
{
    const navigate = useNavigate(); //hook para mudar de página
    const [formData, setFormData] = useState
    ({
        nome: '',
        descricao: '',
        localizacao: '',
        uid_hardware: ''
    });

    //atualiza o estaddo quando o usuário digita
    const handleChange = (e) =>
    {
        setFormData({ ...formData, [e.target.name]: e.target.value });
    };

    //envia para o backend
    const handleSubmit = async (e) =>
    {
        e.preventDefault();
        try
        {
            await api.post('motores/', formData);
            alert('Motor cadastrado com sucesso!');
            navigate('/');
        }
        catch (error)
        {
            console.error(error);
            alert('Erro ao cadastrar. Verifique se esse UID já existe');
        }
    };

    return (
        <div className="min-h-screen bg-gray-50 p-8 flex justify-center items-start">
            <div className="bg-white p-8 rounded-xl shadow-lg w-full max-w-md border border-gray-100">
                
                {/* Cabeçalho do Form */}
                <div className="flex items-center gap-4 mb-6">
                    <button onClick={() => navigate('/')} className="text-gray-400 hover:text-blue-600 transition">
                        <ArrowLeft />
                    </button>
                    <h1 className="text-2xl font-bold text-gray-800">Novo Motor</h1>
                </div>

                <form onSubmit={handleSubmit} className="space-y-4">
                    <div>
                        <label className="block text-sm font-medium text-gray-700 mb-1">Nome do Motor</label>
                        <input 
                            name="nome"
                            required
                            placeholder="Ex: Motor Esteira 02"
                            className="w-full p-2 border border-gray-300 rounded focus:ring-2 focus:ring-blue-500 outline-none"
                            onChange={handleChange}
                        />
                    </div>

                    <div>
                        <label className="block text-sm font-medium text-gray-700 mb-1">UID do Hardware (ESP32)</label>
                        <input 
                            name="uid_hardware"
                            required
                            placeholder="Ex: ESP32-S3-002"
                            className="w-full p-2 border border-gray-300 rounded focus:ring-2 focus:ring-blue-500 outline-none font-mono text-sm uppercase"
                            onChange={handleChange}
                        />
                        <p className="text-xs text-gray-400 mt-1">Este ID deve ser único e igual ao do código C.</p>
                    </div>

                    <div>
                        <label className="block text-sm font-medium text-gray-700 mb-1">Localização</label>
                        <input 
                            name="localizacao"
                            placeholder="Ex: Setor B"
                            className="w-full p-2 border border-gray-300 rounded focus:ring-2 focus:ring-blue-500 outline-none"
                            onChange={handleChange}
                        />
                    </div>

                    <div>
                        <label className="block text-sm font-medium text-gray-700 mb-1">Descrição</label>
                        <textarea 
                            name="descricao"
                            placeholder="Detalhes adicionais..."
                            className="w-full p-2 border border-gray-300 rounded focus:ring-2 focus:ring-blue-500 outline-none"
                            rows="3"
                            onChange={handleChange}
                        />
                    </div>

                    <button 
                        type="submit" 
                        className="w-full bg-blue-600 text-white py-2 rounded-lg hover:bg-blue-700 transition flex justify-center items-center gap-2 font-semibold shadow-md hover:shadow-lg"
                    >
                        <Save size={18} />
                        Salvar Motor
                    </button>
                </form>
            </div>
        </div>
    );
};

export default NovoMotor;