from rest_framework import serializers
from .models import Motor, Leitura

class LeituraSerializer(serializers.ModelSerializer):
    class Meta:
        model = Leitura
        fields = ['id', 'eixo_x', 'eixo_y', 'eixo_z', 'rpm', 'em_falha', 'data_leitura']

class MotorSerializer(serializers.ModelSerializer):
    #isto permite ver as últimas leituras dentro do JSON do motor

    leituras = LeituraSerializer(many=True, read_only=True)

    class Meta:
        model = Motor
        fields = ['id', 'nome', 'descricao', 'localizacao', 'uid_hardware', 'criado_em', 'leituras']