from django.shortcuts import render
from rest_framework import viewsets, status
from rest_framework.decorators import action
from rest_framework.response import Response
from rest_framework.parsers import MultiPartParser, FormParser, JSONParser
from django.shortcuts import get_object_or_404
from django.conf import settings
from drf_spectacular.utils import extend_schema, extend_schema_view, OpenApiParameter, OpenApiExample
from drf_spectacular.types import OpenApiTypes
import pandas as pd
import os

from .models import Project, DataFile, MLModel
from .serializers import (
    ProjectSerializer, ProjectListSerializer,
    DataFileSerializer,
    MLModelSerializer, MLModelListSerializer,
    TrainingConfigSerializer
)
from .services import GenericDataProcessor, GenericCNNTrainer


@extend_schema_view(
    list=extend_schema(
        summary="Listar todos os projetos",
        description="Retorna uma lista paginada de todos os projetos",
        tags=['Projects']
    ),
    retrieve=extend_schema(
        summary="Obter detalhes de um projeto",
        description="Retorna informações detalhadas de um projeto específico incluindo arquivos e modelos",
        tags=['Projects']
    ),
    create=extend_schema(
        summary="Criar novo projeto",
        description="Cria um novo projeto para agrupar datasets e modelos",
        tags=['Projects']
    ),
    update=extend_schema(
        summary="Atualizar projeto",
        description="Atualiza todas as informações de um projeto",
        tags=['Projects']
    ),
    partial_update=extend_schema(
        summary="Atualizar projeto parcialmente",
        description="Atualiza campos específicos de um projeto",
        tags=['Projects']
    ),
    destroy=extend_schema(
        summary="Deletar projeto",
        description="Remove um projeto e todos os seus arquivos e modelos associados",
        tags=['Projects']
    ),
)
class ProjectViewSet(viewsets.ModelViewSet):
    """ViewSet para gerenciar projetos"""

    queryset = Project.objects.all()

    def get_serializer_class(self):
        if self.action == 'list':
            return ProjectListSerializer
        return ProjectSerializer

    @extend_schema(
        summary="Upload de arquivo de dados",
        description="Faz upload de um arquivo CSV ou TXT para o projeto. O arquivo é processado automaticamente e as estatísticas são geradas.",
        tags=['Projects'],
        request={
            'multipart/form-data': {
                'type': 'object',
                'properties': {
                    'file': {'type': 'string', 'format': 'binary'},
                    'delimiter': {'type': 'string', 'default': ','},
                    'target_label': {'type': 'string', 'nullable': True},
                    'has_target_column': {'type': 'string', 'default': 'false'},
                }
            }
        },
        examples=[
            OpenApiExample(
                'Upload CSV sem target',
                value={
                    'file': 'arquivo.csv',
                    'delimiter': ',',
                    'target_label': 'classe_a',
                    'has_target_column': 'false'
                }
            ),
            OpenApiExample(
                'Upload TXT com target existente',
                value={
                    'file': 'dados.txt',
                    'delimiter': ';',
                    'has_target_column': 'true'
                }
            ),
        ]
    )
    @action(detail=True, methods=['post'], parser_classes=[MultiPartParser, FormParser])
    def upload(self, request, pk=None):
        """Endpoint para upload de arquivos de dados"""
        project = self.get_object()

        # Validar dados recebidos
        file = request.FILES.get('file')
        if not file:
            return Response(
                {'error': 'Nenhum arquivo foi enviado'},
                status=status.HTTP_400_BAD_REQUEST
            )

        # Detectar tipo de arquivo
        file_name = file.name.lower()
        if file_name.endswith('.csv'):
            file_type = 'csv'
        elif file_name.endswith('.txt'):
            file_type = 'txt'
        else:
            return Response(
                {'error': 'Tipo de arquivo não suportado. Use .csv ou .txt'},
                status=status.HTTP_400_BAD_REQUEST
            )

        # Criar objeto DataFile
        data_file = DataFile.objects.create(
            project=project,
            file=file,
            file_type=file_type,
            delimiter=request.data.get('delimiter', ','),
            target_label=request.data.get('target_label', None),
            has_target_column=request.data.get('has_target_column', 'false').lower() == 'true'
        )

        # Atualizar status do projeto
        project.status = 'processing'
        project.save()

        # Processar arquivo e extrair metadados
        try:
            processor = GenericDataProcessor()
            file_path = data_file.file.path

            df, stats = processor.process_datafile(
                file_path=file_path,
                delimiter=data_file.delimiter,
                target_label=data_file.target_label,
                has_target_column=data_file.has_target_column
            )

            # Atualizar metadados do arquivo
            data_file.rows_count = stats['total_rows']
            data_file.columns = stats
            data_file.save()

            # Atualizar status do projeto
            project.status = 'ready'
            project.save()

            print(f"✅ Arquivo processado: {data_file.rows_count} linhas, {stats['total_columns']} colunas")

        except Exception as e:
            project.status = 'error'
            project.save()
            return Response(
                {'error': f'Erro ao processar arquivo: {str(e)}'},
                status=status.HTTP_500_INTERNAL_SERVER_ERROR
            )

        serializer = DataFileSerializer(data_file, context={'request': request})
        return Response(serializer.data, status=status.HTTP_201_CREATED)

    @extend_schema(
        summary="Preview dos dados consolidados",
        description="Retorna uma prévia dos dados consolidados de todos os arquivos do projeto, incluindo estatísticas completas",
        tags=['Projects'],
        responses={200: {
            'type': 'object',
            'properties': {
                'project': {'type': 'object'},
                'statistics': {'type': 'object'},
                'preview_rows': {'type': 'array'},
                'total_rows': {'type': 'integer'},
                'columns': {'type': 'array'},
                'total_files': {'type': 'integer'},
            }
        }}
    )
    @action(detail=True, methods=['get'])
    def preview(self, request, pk=None):
        """Endpoint para preview dos dados consolidados do projeto"""
        project = self.get_object()

        if not project.data_files.exists():
            return Response(
                {'error': 'Nenhum arquivo foi carregado para este projeto'},
                status=status.HTTP_400_BAD_REQUEST
            )

        try:
            # Consolidar dados de todos os arquivos
            processor = GenericDataProcessor()

            file_configs = []
            for df_obj in project.data_files.all():
                file_configs.append({
                    'path': df_obj.file.path,
                    'delimiter': df_obj.delimiter,
                    'target_label': df_obj.target_label,
                    'has_target_column': df_obj.has_target_column
                })

            merged_df, stats = processor.consolidate_project_data(file_configs)

            # Retornar preview com primeiras linhas
            preview_data = merged_df.head(20).to_dict(orient='records')

            return Response({
                'project': ProjectSerializer(project, context={'request': request}).data,
                'statistics': stats,
                'preview_rows': preview_data,
                'total_rows': stats['total_rows'],
                'columns': stats['columns'],
                'total_files': len(file_configs)
            })

        except Exception as e:
            return Response(
                {'error': f'Erro ao consolidar dados: {str(e)}'},
                status=status.HTTP_500_INTERNAL_SERVER_ERROR
            )


@extend_schema_view(
    list=extend_schema(
        summary="Listar arquivos de dados",
        description="Retorna lista de arquivos. Use ?project=<id> para filtrar por projeto",
        tags=['Data Files'],
        parameters=[
            OpenApiParameter(
                name='project',
                type=OpenApiTypes.UUID,
                location=OpenApiParameter.QUERY,
                description='ID do projeto para filtrar'
            )
        ]
    ),
    retrieve=extend_schema(
        summary="Obter detalhes de um arquivo",
        description="Retorna informações detalhadas de um arquivo específico",
        tags=['Data Files']
    ),
    destroy=extend_schema(
        summary="Deletar arquivo",
        description="Remove um arquivo de dados do projeto",
        tags=['Data Files']
    ),
)
class DataFileViewSet(viewsets.ModelViewSet):
    """ViewSet para gerenciar arquivos de dados"""

    queryset = DataFile.objects.all()
    serializer_class = DataFileSerializer

    def get_queryset(self):
        queryset = super().get_queryset()
        project_id = self.request.query_params.get('project', None)
        if project_id:
            queryset = queryset.filter(project_id=project_id)
        return queryset


@extend_schema_view(
    list=extend_schema(
        summary="Listar modelos",
        description="Retorna lista de modelos. Use ?project=<id> para filtrar por projeto",
        tags=['Models'],
        parameters=[
            OpenApiParameter(
                name='project',
                type=OpenApiTypes.UUID,
                location=OpenApiParameter.QUERY,
                description='ID do projeto para filtrar'
            )
        ]
    ),
    retrieve=extend_schema(
        summary="Obter detalhes de um modelo",
        description="Retorna informações completas de um modelo incluindo métricas e arquivos",
        tags=['Models']
    ),
    destroy=extend_schema(
        summary="Deletar modelo",
        description="Remove um modelo e todos os seus arquivos associados",
        tags=['Models']
    ),
)
class MLModelViewSet(viewsets.ModelViewSet):
    """ViewSet para gerenciar modelos de ML"""

    queryset = MLModel.objects.all()

    def get_serializer_class(self):
        if self.action == 'list':
            return MLModelListSerializer
        return MLModelSerializer

    def get_queryset(self):
        queryset = super().get_queryset()
        project_id = self.request.query_params.get('project', None)
        if project_id:
            queryset = queryset.filter(project_id=project_id)
        return queryset

    @extend_schema(
        summary="Treinar novo modelo",
        description="Inicia o treinamento de um modelo CNN 1D com os dados do projeto. O treinamento é síncrono e pode demorar dependendo do dataset e configurações.",
        tags=['Models'],
        request=TrainingConfigSerializer,
        responses={201: MLModelSerializer},
        examples=[
            OpenApiExample(
                'Treinamento completo',
                value={
                    "project_id": "abc-123-def-456",
                    "model_name": "Motor Classifier v1",
                    "version": "1.0.0",
                    "feature_columns": ["x", "y", "z", "adc_raw"],
                    "target_column": "target",
                    "epochs": 100,
                    "batch_size": 32,
                    "test_size": 0.2
                }
            ),
            OpenApiExample(
                'Treinamento rápido (teste)',
                value={
                    "project_id": "abc-123-def-456",
                    "model_name": "Test Model",
                    "feature_columns": ["col1", "col2", "col3"],
                    "target_column": "label",
                    "epochs": 10,
                    "batch_size": 16
                }
            ),
        ]
    )
    @action(detail=False, methods=['post'])
    def train(self, request):
        """Endpoint para iniciar treinamento de um modelo"""
        serializer = TrainingConfigSerializer(data=request.data)

        if not serializer.is_valid():
            return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)

        data = serializer.validated_data
        project = get_object_or_404(Project, id=data['project_id'])

        # Verificar se o projeto tem arquivos
        if not project.data_files.exists():
            return Response(
                {'error': 'Projeto não possui arquivos de dados'},
                status=status.HTTP_400_BAD_REQUEST
            )

        # Criar objeto do modelo
        ml_model = MLModel.objects.create(
            project=project,
            name=data['model_name'],
            version=data.get('version', '1.0.0'),
            feature_columns=data['feature_columns'],
            target_column=data['target_column'],
            epochs=data.get('epochs', 100),
            batch_size=data.get('batch_size', 32),
            test_size=data.get('test_size', 0.2),
            status='training'
        )

        # Iniciar treinamento
        try:
            # 1. Consolidar dados do projeto
            processor = GenericDataProcessor()
            file_configs = []

            for df_obj in project.data_files.all():
                file_configs.append({
                    'path': df_obj.file.path,
                    'delimiter': df_obj.delimiter,
                    'target_label': df_obj.target_label,
                    'has_target_column': df_obj.has_target_column
                })

            merged_df, stats = processor.consolidate_project_data(file_configs)

            print(f"📊 Dataset consolidado: {len(merged_df)} amostras")

            # 2. Treinar modelo
            trainer = GenericCNNTrainer(model_name=ml_model.name.replace(' ', '_'))

            # Definir diretório de salvamento
            save_dir = os.path.join(settings.MEDIA_ROOT, 'models', str(ml_model.id))
            os.makedirs(save_dir, exist_ok=True)

            # Pipeline completo de treinamento
            results = trainer.train_complete_pipeline(
                df=merged_df,
                feature_columns=data['feature_columns'],
                target_column=data['target_column'],
                save_dir=save_dir,
                epochs=data.get('epochs', 100),
                batch_size=data.get('batch_size', 32),
                test_size=data.get('test_size', 0.2),
                convert_tflite=True
            )

            # 3. Atualizar modelo com resultados
            ml_model.status = 'completed'
            ml_model.accuracy = results['accuracy']
            ml_model.loss = results['loss']
            ml_model.training_history = results['training_history']
            ml_model.confusion_matrix = results['confusion_matrix']
            ml_model.classification_report = results['classification_report']
            ml_model.training_time_seconds = results['training_time_seconds']
            ml_model.model_size_kb = results['model_size_kb']

            # Salvar caminhos dos arquivos (relativos ao MEDIA_ROOT)
            ml_model.keras_model_file.name = os.path.relpath(
                results['file_paths']['keras_model'],
                settings.MEDIA_ROOT
            )
            ml_model.tflite_model_file.name = os.path.relpath(
                results['file_paths']['tflite_model'],
                settings.MEDIA_ROOT
            )
            ml_model.scaler_file.name = os.path.relpath(
                results['file_paths']['scaler'],
                settings.MEDIA_ROOT
            )

            from django.utils import timezone
            ml_model.trained_at = timezone.now()
            ml_model.save()

            print(f"✅ Modelo treinado com sucesso! Acurácia: {results['accuracy']:.4f}")

            return Response(
                MLModelSerializer(ml_model, context={'request': request}).data,
                status=status.HTTP_201_CREATED
            )

        except Exception as e:
            ml_model.status = 'failed'
            ml_model.error_message = str(e)
            ml_model.save()

            print(f"❌ Erro ao treinar modelo: {str(e)}")

            return Response(
                {
                    'error': f'Erro ao treinar modelo: {str(e)}',
                    'model_id': str(ml_model.id)
                },
                status=status.HTTP_500_INTERNAL_SERVER_ERROR
            )

    @extend_schema(
        summary="Download do modelo TFLite",
        description="Retorna a URL para download do arquivo .tflite do modelo treinado",
        tags=['Models'],
        responses={200: {
            'type': 'object',
            'properties': {
                'download_url': {'type': 'string'},
                'file_name': {'type': 'string'},
                'size_kb': {'type': 'number'},
            }
        }}
    )
    @action(detail=True, methods=['get'])
    def download(self, request, pk=None):
        """Endpoint para download do modelo TFLite"""
        model = self.get_object()

        if not model.tflite_model_file:
            return Response(
                {'error': 'Modelo TFLite não disponível'},
                status=status.HTTP_404_NOT_FOUND
            )

        if model.status != 'completed':
            return Response(
                {'error': f'Modelo não está pronto. Status: {model.status}'},
                status=status.HTTP_400_BAD_REQUEST
            )

        # Retornar URL do arquivo
        file_url = request.build_absolute_uri(model.tflite_model_file.url)
        return Response({
            'download_url': file_url,
            'file_name': os.path.basename(model.tflite_model_file.name),
            'size_kb': model.model_size_kb
        })

    @extend_schema(
        summary="Métricas detalhadas do modelo",
        description="Retorna todas as métricas de avaliação do modelo incluindo histórico de treinamento, matriz de confusão e classification report",
        tags=['Models']
    )
    @action(detail=True, methods=['get'])
    def metrics(self, request, pk=None):
        """Endpoint para obter métricas detalhadas do modelo"""
        model = self.get_object()

        if model.status != 'completed':
            return Response(
                {'error': f'Modelo não está pronto. Status: {model.status}'},
                status=status.HTTP_400_BAD_REQUEST
            )

        return Response({
            'model_id': str(model.id),
            'model_name': model.name,
            'version': model.version,
            'accuracy': model.accuracy,
            'loss': model.loss,
            'training_history': model.training_history,
            'confusion_matrix': model.confusion_matrix,
            'classification_report': model.classification_report,
            'training_config': {
                'feature_columns': model.feature_columns,
                'target_column': model.target_column,
                'epochs': model.epochs,
                'batch_size': model.batch_size,
                'test_size': model.test_size,
            },
            'training_time_seconds': model.training_time_seconds,
            'model_size_kb': model.model_size_kb,
            'trained_at': model.trained_at
        })
