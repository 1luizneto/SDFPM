from django.urls import path, include
from rest_framework.routers import DefaultRouter
from .views import ProjectViewSet, DataFileViewSet, MLModelViewSet

# Criar router do DRF
router = DefaultRouter()
router.register(r'projects', ProjectViewSet, basename='project')
router.register(r'datafiles', DataFileViewSet, basename='datafile')
router.register(r'models', MLModelViewSet, basename='mlmodel')

urlpatterns = [
    path('', include(router.urls)),
]

