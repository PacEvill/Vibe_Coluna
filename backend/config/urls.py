from django.contrib import admin
from django.urls import path, include
from iot_api.views import DashboardDataView

urlpatterns = [
    path('admin/', admin.site.urls),
    path('api/', include('iot_api.urls')),
    path('graphql/', DashboardDataView.as_view(), name='root-graphql-fallback'),
    path('dashboard/', DashboardDataView.as_view(), name='root-dashboard'),
]
