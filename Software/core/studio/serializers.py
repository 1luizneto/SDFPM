from rest_framework import serializers
from .models import Project, DataFile, MLModel


class DataFileSerializer(serializers.ModelSerializer):
    """Serializer para arquivos de dados"""

    file_url = serializers.SerializerMethodField()

    class Meta:
        model = DataFile
        fields = [
            'id', 'project', 'file', 'file_url', 'file_type',
            'delimiter', 'target_label', 'has_target_column',
            'uploaded_at', 'rows_count', 'columns'
        ]
        read_only_fields = ['id', 'uploaded_at', 'rows_count', 'columns']

    def get_file_url(self, obj):
        if obj.file:
            request = self.context.get('request')
            if request:
                return request.build_absolute_uri(obj.file.url)
        return None


class ProjectSerializer(serializers.ModelSerializer):
    """Serializer para projetos"""

    data_files = DataFileSerializer(many=True, read_only=True)
    data_files_count = serializers.SerializerMethodField()
    models_count = serializers.SerializerMethodField()

    class Meta:
        model = Project
        fields = [
            'id', 'name', 'description', 'status',
            'created_at', 'updated_at',
            'data_files', 'data_files_count', 'models_count'
        ]
        read_only_fields = ['id', 'created_at', 'updated_at']

    def get_data_files_count(self, obj):
        return obj.data_files.count()

    def get_models_count(self, obj):
        return obj.models.count()


class ProjectListSerializer(serializers.ModelSerializer):
    """Serializer simplificado para listagem de projetos"""

    data_files_count = serializers.SerializerMethodField()
    models_count = serializers.SerializerMethodField()

    class Meta:
        model = Project
        fields = [
            'id', 'name', 'description', 'status',
            'created_at', 'updated_at',
            'data_files_count', 'models_count'
        ]
        read_only_fields = ['id', 'created_at', 'updated_at']

    def get_data_files_count(self, obj):
        return obj.data_files.count()

    def get_models_count(self, obj):
        return obj.models.count()


class MLModelSerializer(serializers.ModelSerializer):
    """Serializer para modelos de ML"""

    project_name = serializers.CharField(source='project.name', read_only=True)
    keras_model_url = serializers.SerializerMethodField()
    tflite_model_url = serializers.SerializerMethodField()
    scaler_url = serializers.SerializerMethodField()

    class Meta:
        model = MLModel
        fields = [
            'id', 'project', 'project_name', 'name', 'version', 'status',
            'feature_columns', 'target_column', 'epochs', 'batch_size', 'test_size',
            'keras_model_file', 'keras_model_url',
            'tflite_model_file', 'tflite_model_url',
            'scaler_file', 'scaler_url',
            'accuracy', 'loss', 'training_history',
            'confusion_matrix', 'classification_report',
            'model_size_kb', 'training_time_seconds', 'error_message',
            'created_at', 'trained_at'
        ]
        read_only_fields = [
            'id', 'status', 'keras_model_file', 'tflite_model_file',
            'scaler_file', 'accuracy', 'loss', 'training_history',
            'confusion_matrix', 'classification_report',
            'model_size_kb', 'training_time_seconds', 'error_message',
            'created_at', 'trained_at'
        ]

    def get_keras_model_url(self, obj):
        if obj.keras_model_file:
            request = self.context.get('request')
            if request:
                return request.build_absolute_uri(obj.keras_model_file.url)
        return None

    def get_tflite_model_url(self, obj):
        if obj.tflite_model_file:
            request = self.context.get('request')
            if request:
                return request.build_absolute_uri(obj.tflite_model_file.url)
        return None

    def get_scaler_url(self, obj):
        if obj.scaler_file:
            request = self.context.get('request')
            if request:
                return request.build_absolute_uri(obj.scaler_file.url)
        return None


class MLModelListSerializer(serializers.ModelSerializer):
    """Serializer simplificado para listagem de modelos"""

    project_name = serializers.CharField(source='project.name', read_only=True)

    class Meta:
        model = MLModel
        fields = [
            'id', 'project', 'project_name', 'name', 'version', 'status',
            'accuracy', 'loss', 'model_size_kb',
            'created_at', 'trained_at'
        ]


class TrainingConfigSerializer(serializers.Serializer):
    """Serializer para configuração de treinamento"""

    project_id = serializers.UUIDField()
    model_name = serializers.CharField(max_length=255)
    version = serializers.CharField(max_length=50, default='1.0.0')
    feature_columns = serializers.ListField(
        child=serializers.CharField(),
        help_text="Lista de nomes das colunas a serem usadas como features"
    )
    target_column = serializers.CharField(
        help_text="Nome da coluna target/label"
    )
    epochs = serializers.IntegerField(default=100, min_value=1, max_value=1000)
    batch_size = serializers.IntegerField(default=32, min_value=1, max_value=256)
    test_size = serializers.FloatField(default=0.2, min_value=0.1, max_value=0.5)

    def validate_project_id(self, value):
        """Valida se o projeto existe"""
        if not Project.objects.filter(id=value).exists():
            raise serializers.ValidationError("Projeto não encontrado")
        return value

