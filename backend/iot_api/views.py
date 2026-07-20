from rest_framework import generics
from rest_framework.views import APIView
from rest_framework.response import Response
from rest_framework import status
from django.views.generic import TemplateView
from django.utils import timezone
from .models import SensorData
from .serializers import SensorDataSerializer

class NexusControlView(TemplateView):
    template_name = 'iot_api/nexus_control.html'

class SensorDataCreateView(generics.CreateAPIView):
    queryset = SensorData.objects.all()
    serializer_class = SensorDataSerializer

class SensorDataListView(generics.ListAPIView):
    serializer_class = SensorDataSerializer
    
    def get_queryset(self):
        return SensorData.objects.all().order_by('-timestamp')[:50]

class ArduinoSensorReadingView(APIView):
    def post(self, request, sensor_id=1, *args, **kwargs):
        payload_data = request.data.copy() if isinstance(request.data, dict) else {}
        payload_data["sensor_id"] = sensor_id
        
        # Calculate if alert based on value/distance
        val = float(payload_data.get("value", payload_data.get("distance", 0)))
        threshold = float(payload_data.get("threshold", 25.0))
        is_alert = payload_data.get("is_alert", val < 10.0 or val > threshold)
        payload_data["is_alert"] = bool(is_alert)
        payload_data["distance"] = val

        data = {
            "payload": payload_data
        }
        
        serializer = SensorDataSerializer(data=data)
        if serializer.is_valid():
            serializer.save()
            return Response(serializer.data, status=status.HTTP_201_CREATED)
        return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)

class DashboardDataView(APIView):
    def get_data(self):
        readings = SensorData.objects.all().order_by('-timestamp')
        
        latest_readings = []
        total_readings = readings.count()
        total_alerts = 0
        total_distance = 0.0
        
        alerts_by_hour_map = {f"{h:02d}:00": 0 for h in range(24)}

        for r in readings[:100]:
            payload = r.payload if isinstance(r.payload, dict) else {}
            dist = float(payload.get("distance", payload.get("value", 0.0)))
            is_alert = bool(payload.get("is_alert", dist < 10.0 or dist > 25.0))
            sensor_id = payload.get("sensor_id", 1)
            
            if is_alert:
                total_alerts += 1
                local_time = timezone.localtime(r.timestamp)
                hour_key = local_time.strftime("%H:00")
                if hour_key in alerts_by_hour_map:
                    alerts_by_hour_map[hour_key] += 1
            
            total_distance += dist
            
            if len(latest_readings) < 20:
                latest_readings.append({
                    "id": r.id,
                    "timestamp": r.timestamp.isoformat(),
                    "distance": round(dist, 2),
                    "isAlert": is_alert,
                    "sensorId": sensor_id
                })

        avg_distance = round(total_distance / total_readings, 2) if total_readings > 0 else 0.0
        compliance_rate = round(((total_readings - total_alerts) / total_readings) * 100, 1) if total_readings > 0 else 100.0

        alerts_by_hour = [{"hour": k, "count": v} for k, v in alerts_by_hour_map.items()]

        payload_res = {
            "latestReadings": latest_readings,
            "stats": {
                "totalReadings": total_readings,
                "totalAlerts": total_alerts,
                "avgDistance": avg_distance,
                "complianceRate": compliance_rate
            },
            "alertsByHour": alerts_by_hour
        }
        
        # Return structured for both REST and GraphQL wrapper { "data": ... }
        return {
            "data": payload_res,
            **payload_res
        }

    def get(self, request, *args, **kwargs):
        return Response(self.get_data())

    def post(self, request, *args, **kwargs):
        return Response(self.get_data())
