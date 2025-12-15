from django.contrib import admin
from .models import Motor, Leitura


@admin.register(Motor)
class MotorAdmin(admin.ModelAdmin):
    list_display = ('nome', 'uid_hardware', 'localizacao', 'criado_em')
    search_fields = ('nome', 'uid_hardware')

@admin.register(Leitura)
class LeituraAdmin(admin.ModelAdmin):
    list_display = ('motor', 'eixo_x', 'eixo_y', 'eixo_z', 'rpm', 'em_falha', 'data_leitura')
    list_filter = ('em_falha', 'motor', 'data_leitura')
