import React from 'react';
import { Activity, AlertTriangle, CheckCircle } from 'lucide-react';
import { Link } from 'react-router-dom';

//Este componente recebe os dados de "motor" como propriedade
const MotorCard = ({ motor }) => {
    // Pega a última leitura (se existir)
    const ultimaLeitura = motor.leituras && motor.leituras.length > 0 ? motor.leituras[0] : null;
    
    // Verifica se está em falha (baseado na última leitura)
    const emFalha = ultimaLeitura ? ultimaLeitura.em_falha : false;

    return (
        <Link to={`/motor/${motor.id}`} className="block"> 
            <div className={`p-6 rounded-xl shadow-lg border-l-4 bg-white transition-transform hover:scale-105 ${
                emFalha ? 'border-red-500' : 'border-green-500'
            }`}>
                <div className="flex justify-between items-start mb-4">
                    <div>
                        <h3 className="text-xl font-bold text-gray-800">{motor.nome}</h3>
                        <p className="text-sm text-gray-500 font-mono">{motor.uid_hardware}</p>
                    </div>
                    {/* Ícone muda dependendo do status */}
                    {emFalha ? (
                        <AlertTriangle className="text-red-500 w-8 h-8" />
                    ) : (
                        <CheckCircle className="text-green-500 w-8 h-8" />
                    )}
                </div>

                <div className="space-y-2">
                    <p className="text-gray-600 text-sm">{motor.localizacao}</p>
                    
                    {/* Mostra dados técnicos se tiver leitura, senão avisa que está sem dados */}
                    <div className="mt-4 pt-4 border-t border-gray-100">
                        {ultimaLeitura ? (
                            <div className="grid grid-cols-2 gap-2 text-sm">
                                <div className="flex items-center gap-1">
                                    <Activity size={16} className="text-blue-500"/>
                                    <span className="font-semibold">RPM:</span> {ultimaLeitura.rpm}
                                </div>
                                <div className="flex items-center gap-1">
                                    <Activity size={16} className="text-purple-500"/>
                                    <span className="font-semibold">X:</span> {ultimaLeitura.eixo_x}
                                </div>
                            </div>
                        ) : (
                            <p className="text-xs text-gray-400 italic">Aguardando dados...</p>
                        )}
                    </div>
                </div>
                
                {/* Badge de Status */}
                <div className={`mt-4 text-center py-1 rounded text-xs font-bold uppercase ${
                    emFalha ? 'bg-red-100 text-red-700' : 'bg-green-100 text-green-700'
                }`}>
                    {emFalha ? 'CRÍTICO' : 'OPERACIONAL'}
                </div>
            </div>
        </Link>
    );
};

export default MotorCard;