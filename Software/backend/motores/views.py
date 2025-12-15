from django.shortcuts import render
from rest_framework import viewsets
from rest_framework.decorators import action
from rest_framework.response import Response
from django.shortcuts import get_object_or_404
from .models import Motor, Leitura
from .serializers import MotorSerializer, LeituraSerializer

class MotorViewSet(viewsets.ModelViewSet):
    queryset = Motor.objects.all()
    serializer_class = MotorSerializer

class LeituraViewSet(viewsets.ModelViewSet):
    #ordena do mais antigo ao mais novo
    queryset = Leitura.objects.all().order_by('-data_leitura')
    serializer_class = LeituraSerializer

    #esta função extra permite que o ESP32 envie dados usando apenas o UID do motor
    #rota: POST /api/leituras/registrar/
    @action(detail=False, methods=['post'])
    def registrar(self, request):
        uid = request.data.get('uid_hardware')
        eixo_x = request.data.get('eixo_x')
        eixo_y = request.data.get('eixo_y')
        eixo_z = request.data.get('eixo_z')
        rpm = request.data.get('rpm')


        em_falha = request.data.get('em_falha', False)

        #tentar achar o motor pelo UID
        motor = get_object_or_404(Motor, uid_hardware=uid)

        #cria a leitura
        nova_leitura = Leitura.objects.create(
            motor=motor,
            eixo_x=eixo_x,
            eixo_y=eixo_y,
            eixo_z=eixo_z,
            rpm=rpm,
            #lógica simples de falha
            em_falha=em_falha
        )

        return Response({"status": "Leitura recebida", "id": nova_leitura.id})
