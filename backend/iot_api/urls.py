from django.urls import path
from .views import (
    SensorDataCreateView, 
    SensorDataListView, 
    ArduinoSensorReadingView, 
    NexusControlView,
    DashboardDataView
)

urlpatterns = [
    path('data/', SensorDataListView.as_view(), name='data-list'),
    path('dashboard/', DashboardDataView.as_view(), name='dashboard-data'),
    path('graphql/', DashboardDataView.as_view(), name='graphql-data'),
    path('data/create/', SensorDataCreateView.as_view(), name='data-create'),
    path('v1/sensors/<int:sensor_id>/reading/', ArduinoSensorReadingView.as_view(), name='arduino-reading'),
    path('v1/sensors/reading/', ArduinoSensorReadingView.as_view(), name='arduino-reading-default'),
    path('nexus/', NexusControlView.as_view(), name='nexus-control'),
]
