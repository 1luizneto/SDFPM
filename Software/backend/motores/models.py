from django.db import models

class Motor(models.Model):
    nome = models.CharField(max_length=100)
    descricao = models.TextField(blank=True, null=True)
    localizacao = models.CharField(max_length=100, help_text="Ex: Setor A, Esteira 2")

    #O uid será o ID que você vai colocar no código do ESP32 para identificar este motor
    uid_hardware = models.CharField(max_length=50, unique=True, help_text="ID do ESP")

    criado_em = models.DateTimeField(auto_now_add=True)

    def __str__(self):
        return f"{self.nome} ({self.uid_hardware})"
    
class Leitura(models.Model):
    motor = models.ForeignKey(Motor, on_delete=models.CASCADE, related_name='leituras')

    #dados que o hardware vai enviar
    eixo_x = models.FloatField(help_text="Eixo X", default=0.0)
    eixo_y = models.FloatField(help_text="Eixo Y", default=0.0)
    eixo_z = models.FloatField(help_text="Eixo Z", default=0.0)
    rpm = models.FloatField(help_text="RPM", default=0.0)

    em_falha = models.IntegerField(default=0)

    data_leitura = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ['-data_leitura']

    def __str__(self):
        status = "FALHA" if self.em_falha else "OK"
        return f"{self.motor.nome} - {self.eixo_x} - {status}"


