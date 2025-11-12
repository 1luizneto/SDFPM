from django.db import models
import uuid
from django.core.validators import FileExtensionValidator


class Project(models.Model):
    """Projeto que agrupa datasets e modelos"""

    STATUS_CHOICES = [
        ('creating', 'Creating'),
        ('ready', 'Ready'),
        ('processing', 'Processing'),
        ('error', 'Error'),
    ]

    id = models.UUIDField(primary_key=True, default=uuid.uuid4, editable=False)
    name = models.CharField(max_length=255)
    description = models.TextField(blank=True, null=True)
    status = models.CharField(max_length=20, choices=STATUS_CHOICES, default='creating')
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        ordering = ['-created_at']
        verbose_name = 'Project'
        verbose_name_plural = 'Projects'

    def __str__(self):
        return f"{self.name} ({self.status})"


class DataFile(models.Model):
    """Arquivo de dados associado a um projeto"""

    FILE_TYPE_CHOICES = [
        ('txt', 'Text File'),
        ('csv', 'CSV File'),
    ]

    id = models.UUIDField(primary_key=True, default=uuid.uuid4, editable=False)
    project = models.ForeignKey(Project, on_delete=models.CASCADE, related_name='data_files')
    file = models.FileField(
        upload_to='datasets/%Y/%m/%d/',
        validators=[FileExtensionValidator(allowed_extensions=['txt', 'csv'])]
    )
    file_type = models.CharField(max_length=3, choices=FILE_TYPE_CHOICES)
    delimiter = models.CharField(max_length=5, default=',')
    target_label = models.CharField(max_length=100, blank=True, null=True,
                                    help_text="Label para esta classe (ex: 'ligado', 'defeito')")
    has_target_column = models.BooleanField(
        default=False,
        help_text="Se True, o arquivo já contém a coluna target"
    )
    uploaded_at = models.DateTimeField(auto_now_add=True)
    rows_count = models.IntegerField(null=True, blank=True)
    columns = models.JSONField(default=dict, blank=True,
                               help_text="Metadados das colunas do arquivo")

    class Meta:
        ordering = ['uploaded_at']
        verbose_name = 'Data File'
        verbose_name_plural = 'Data Files'

    def __str__(self):
        return f"{self.project.name} - {self.file.name}"


class MLModel(models.Model):
    """Modelo de Machine Learning treinado"""

    STATUS_CHOICES = [
        ('pending', 'Pending'),
        ('training', 'Training'),
        ('completed', 'Completed'),
        ('failed', 'Failed'),
        ('deleted', 'Deleted'),
    ]

    id = models.UUIDField(primary_key=True, default=uuid.uuid4, editable=False)
    project = models.ForeignKey(Project, on_delete=models.CASCADE, related_name='models')
    name = models.CharField(max_length=255)
    version = models.CharField(max_length=50, default='1.0.0')
    status = models.CharField(max_length=20, choices=STATUS_CHOICES, default='pending')

    # Configurações de treinamento
    feature_columns = models.JSONField(
        default=list,
        help_text="Lista de colunas usadas como features"
    )
    target_column = models.CharField(max_length=100)
    epochs = models.IntegerField(default=100)
    batch_size = models.IntegerField(default=32)
    test_size = models.FloatField(default=0.2)

    # Arquivos do modelo
    keras_model_file = models.FileField(
        upload_to='models/%Y/%m/%d/',
        blank=True,
        null=True,
        help_text="Arquivo .h5 do modelo Keras"
    )
    tflite_model_file = models.FileField(
        upload_to='models/%Y/%m/%d/',
        blank=True,
        null=True,
        help_text="Arquivo .tflite para dispositivos embarcados"
    )
    scaler_file = models.FileField(
        upload_to='models/%Y/%m/%d/',
        blank=True,
        null=True,
        help_text="Arquivo .pkl do scaler"
    )

    # Métricas do modelo
    accuracy = models.FloatField(null=True, blank=True)
    loss = models.FloatField(null=True, blank=True)
    training_history = models.JSONField(default=dict, blank=True)
    confusion_matrix = models.JSONField(default=dict, blank=True)
    classification_report = models.JSONField(default=dict, blank=True)

    # Metadados
    model_size_kb = models.FloatField(null=True, blank=True)
    training_time_seconds = models.FloatField(null=True, blank=True)
    error_message = models.TextField(blank=True, null=True)

    created_at = models.DateTimeField(auto_now_add=True)
    trained_at = models.DateTimeField(null=True, blank=True)

    class Meta:
        ordering = ['-created_at']
        verbose_name = 'ML Model'
        verbose_name_plural = 'ML Models'

    def __str__(self):
        return f"{self.name} v{self.version} ({self.status})"
