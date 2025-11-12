from django.contrib import admin
from .models import Project, DataFile, MLModel


@admin.register(Project)
class ProjectAdmin(admin.ModelAdmin):
    list_display = ['name', 'status', 'created_at', 'updated_at']
    list_filter = ['status', 'created_at']
    search_fields = ['name', 'description']
    readonly_fields = ['id', 'created_at', 'updated_at']


@admin.register(DataFile)
class DataFileAdmin(admin.ModelAdmin):
    list_display = ['project', 'file_type', 'target_label', 'rows_count', 'uploaded_at']
    list_filter = ['file_type', 'has_target_column', 'uploaded_at']
    search_fields = ['project__name', 'target_label']
    readonly_fields = ['id', 'uploaded_at']


@admin.register(MLModel)
class MLModelAdmin(admin.ModelAdmin):
    list_display = ['name', 'project', 'version', 'status', 'accuracy', 'created_at']
    list_filter = ['status', 'created_at']
    search_fields = ['name', 'project__name']
    readonly_fields = ['id', 'created_at', 'trained_at']
    fieldsets = (
        ('Informações Básicas', {
            'fields': ('id', 'project', 'name', 'version', 'status')
        }),
        ('Configurações de Treinamento', {
            'fields': ('feature_columns', 'target_column', 'epochs', 'batch_size', 'test_size')
        }),
        ('Arquivos do Modelo', {
            'fields': ('keras_model_file', 'tflite_model_file', 'scaler_file')
        }),
        ('Métricas', {
            'fields': ('accuracy', 'loss', 'training_history', 'confusion_matrix', 'classification_report')
        }),
        ('Metadados', {
            'fields': ('model_size_kb', 'training_time_seconds', 'error_message', 'created_at', 'trained_at')
        }),
    )
